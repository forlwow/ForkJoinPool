#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <memory>
#include <thread>
#include <utility>
#include <vector>
#include <atomic>
#include <future>
#include <functional>
#include <deque>
#include <numeric>

#include "threadsafe_deque.hpp"
#include <condition_variable>
#include "spdlog/spdlog.h"

typedef std::function<void()> task_type;
class thread_pool;

class ethread{
    friend thread_pool;

    int work_id;  // 当前对象id 未指定为-1
    bool lite;

    std::thread t;  // 管理的线程
    mutable std::mutex wait_lock; // 用于挂起
    mutable std::condition_variable block;

    threadsafe_deque<task_type> works;  // 任务队列

    bool busy;  // 是否正在执行任务
    std::atomic_bool finish;  // 结束标志

    std::function<bool(ethread *, task_type &taskType)> get_task;  // 用于获取任务的回调

    inline void do_task(task_type &task){
        busy = (true);
        spdlog::debug("ethread {} start working", work_id);
        task();
        busy = (false);
        spdlog::debug("ethread {} work done", work_id);
    }

    void work(){
        while (!finish){
            task_type task;
            if(get_task && get_task(this, task)){
                do_task(task);
            }
            else if(finish){
                continue;
            }
            else{
                if (lite) std::this_thread::yield();
                else {
                    spdlog::debug("ethread {} sleep", work_id);
                    std::unique_lock lk(wait_lock);
                    block.wait(lk);
                    spdlog::debug("ethread {} wake", work_id);
                }
            }
        }
    }

public:
    explicit ethread(int id = -1, bool lite_ = false)
        : work_id(id), lite(lite_), t(), wait_lock(), block(), works(),
        busy(false), finish(false), get_task(nullptr) {
        spdlog::debug("ethread {} created", work_id);
    }

    ~ethread() noexcept{
        finish = true;
        wake_up();
        if (t.joinable()) {
            t.join();
        }
        spdlog::debug("ethread {} deleted", work_id);
    }
    inline void start(){
        t = std::thread(&ethread::work, this);
    }

    inline void set_callback(std::function<bool(ethread *, task_type &taskType)> call_back){
        get_task = std::move(call_back);
    }

    template<typename F, typename ... Args>
    auto submit(F &&f, Args &&...args) -> std::future<typename std::invoke_result_t<F, Args...>>{
        typedef std::invoke_result_t<F, Args...> result_tpye;
        std::function<result_tpye()> row = [&]{return std::forward<F>(f)(std::forward<Args>(args)...);};
        auto task = std::make_shared<std::packaged_task<result_tpye()>>(std::move(row));
        auto res = task->get_future();
        works.push_back(std::move([task]{(*task)();}));
        wake_up();
        return res;
    }

    void push_task(task_type task){
        spdlog::debug("ethread {} get task", work_id);
        works.push_back(std::move(task));
        wake_up();
    }

    bool steal_task(task_type &task){
        spdlog::debug("ethread {} was stolen task", work_id);
        return works.pop_back(task);
    }

    inline void wake_up() const {block.notify_all();}

    inline bool isbusy() const {return busy;}

    int task_num() const {return isbusy() + works.size();}

    inline int get_work_id() const {return work_id;}

    ethread(ethread&&)=delete;
    ethread& operator=(ethread&&)=delete;
    ethread(const ethread&)=delete;
    ethread& operator=(const ethread&)=delete;
};

class thread_pool{
    std::atomic_int max_size;   // 最大线程数量
    std::atomic_int min_size;
    std::atomic_bool finish;

    bool lite;

    std::thread thread_handler;

    std::vector<std::shared_ptr<ethread>> workers;
    threadsafe_deque<task_type> pool_work_que;  // 公共任务队列

    void handler(){
        while (!finish){
            task_type task;
            if(pool_work_que.pop_front(task)) {
                auto min_t = workers.at(0);
                for(auto &e : workers){
                    auto i = e->task_num();
                    auto j = min_t->task_num();
                    if (i < j)
                        min_t = e;
                }
                min_t->push_task(task);
            }
            else{
                int size = 0;
                std::shared_ptr<ethread> max_task = workers[0];
                std::shared_ptr<ethread> min_task = workers[0];
                for (auto &e : workers){
                    if (!e) continue;
                    if (max_task->task_num() < e->task_num())
                        max_task = e;
                    else if (min_task->task_num() >= e->task_num())
                        min_task = e;
                    size += e->task_num();
                }
                if (max_task.get() != min_task.get() &&
                        max_task->task_num() * workers.size()> size + workers.size()){
                    max_task->steal_task(task);
                    if(task) {
                        min_task->push_task(task);
                        spdlog::debug("work from {} to {}", max_task->get_work_id(), min_task->get_work_id());
                    }
                }
            }
            std::this_thread::yield();
        }
    }

    inline bool pop_from_pool(task_type &task){
        return pool_work_que.pop_front(task);
    }

    inline bool steal_task(task_type &task, unsigned id = 0){
        for (unsigned i = 0; i < workers.size(); ++i){
            unsigned index = (id + i + 1) % workers.size();
            if(workers[index]->works.pop_back(task))
                return true;
        }
        return false;
    }

    bool get_task(ethread *main, task_type &task) {
        if (main->works.pop_front(task))
            return true;
        else if(!lite || finish) // 防止访问被析构的ethread
            return false;
        else if(pop_from_pool(task))
            return true;
        else if(steal_task(task, main->work_id))
            return true;
        else
            return false;
    }

public:
    explicit thread_pool(int min_size_ = 10, bool islite = true):
            max_size((int)std::thread::hardware_concurrency()),     // unsigned => int
            min_size(std::min(max_size.load(), std::max(min_size_ + 1, 0))),
            finish(false),
            lite(islite)
        {
        for(int i = 1; i < min_size; ++i){
            workers.emplace_back(std::make_shared<ethread>(i, lite));
            workers[i-1]->set_callback(
                    [&](ethread *main, task_type &task){return this->get_task(main, task);}
                    );
        }
        for (auto &e : workers)
            e->start();
        if (!lite)
            // 构建完工作线程再构建调度线程
            thread_handler = std::thread(&thread_pool::handler, this);
    }

    ~thread_pool(){
        finish = true;
        if (!lite && thread_handler.joinable())
            thread_handler.join();
        spdlog::debug("thread pool delete");
    }

    template<typename Func, typename ...Args>
    auto submit(Func &&f, Args &&... args) -> std::future<typename std::invoke_result_t<Func, Args...>>{
        typedef typename std::invoke_result_t<Func, Args...> result_type;
        std::function<result_type()> row = [&]{return std::forward<Func>(f)(std::forward<Args>(args)...);};
        auto task = std::make_shared<std::packaged_task<result_type()>>(std::move(row));
        auto res = task->get_future();
        pool_work_que.push_back(([task]{(*task)();}));
        return res;
    }

    void wait_all(){
        unsigned flag = 0;
        for (unsigned i = 0; i < workers.size(); ++i){
            flag |= 1 << i;
        }
        while (flag || !pool_work_que.empty()){
            for (unsigned i = 0; i < workers.size(); ++i){
                if (workers[i]->task_num())
                    flag |= 1 << i;
                else
                    flag &= ~(1 << i);
            }
        }
    }
};

#endif