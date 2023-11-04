#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <memory>
#include <thread>
#include <vector>
#include <atomic>
#include <future>
#include <functional>
#include <deque>
#include <semaphore>
#include <numeric>

#include "threadsafe_deque.hpp"
#include <condition_variable>
#include "spdlog/spdlog.h"

typedef std::function<void()> task_type;

class ethread{
    int work_id;  // 当前对象id 未指定为-1

    std::thread t;  // 管理的线程
    mutable std::binary_semaphore blocker;  // 信号量 用于挂起和释放线程

    threadsafe_deque<task_type> works;  // 任务队列

    bool busy;  // 是否正在执行任务
    std::atomic_bool finish;  // 结束标志

    void work(){
        while (!finish){
            task_type task;
            if(works.pop_front(task)){
                busy = (true);
                spdlog::info("ethread {} start working", work_id);
                task();
                busy = (false);
                spdlog::info("ethread {} work done", work_id);
            }
            else if(finish){
                continue;
            }
            else{
                spdlog::info("ethread {} sleep", work_id);
                blocker.acquire();
                spdlog::info("ethread {} wake", work_id);
            }
        }
    }

public:
    explicit ethread(int id = -1)
        : work_id(id), t(), blocker(0), busy(false), finish(false) {
        start();
        spdlog::info("ethread {} created", work_id);
    }

    ~ethread() noexcept{
        finish = true;
        wake_up();
        if (t.joinable()) {
            t.join();
        }
        spdlog::info("ethread {} deleted", work_id);
    }
    inline void start(){
        t = std::thread(&ethread::work, this);
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
        spdlog::info("ethread {} get task", work_id);
        works.push_back(std::move(task));
        wake_up();
    }

    bool steal_task(task_type &task){
        spdlog::info("ethread {} steal task");
        return works.pop_back(task);
    }

    inline bool sleep() const {return blocker.try_acquire();}

    inline void wake_up() const {blocker.release();}

    inline bool isbusy() const {return busy;}

    inline int task_num() const {return busy + works.size();}

    bool operator<(const ethread &other) const {return this->task_num() < other.task_num();}

    ethread(ethread&&)=delete;
    ethread& operator=(ethread&&)=delete;
    ethread(const ethread&)=delete;
    ethread& operator=(const ethread&)=delete;
};

class thread_pool{
    std::atomic_int max_size;   // 最大线程数量
    std::atomic_int min_size;
    std::atomic_bool finish;

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
                continue;
                std::shared_ptr<ethread> max_task;
                std::shared_ptr<ethread> min_task;
                int size = 0;
                for (auto &e : workers){
                    if (!e) continue;
                    if (max_task < e)
                        max_task = e;
                    if (!(min_task < e))
                        min_task = e;
                    size += e->task_num();
                }
                if (max_task && min_task &&
                        max_task->task_num() * workers.size()> size + 2 * workers.size()){
                    max_task->steal_task(task);
                    min_task->push_task(task);
                }
            }
            std::this_thread::yield();
        }
    }

public:
    explicit thread_pool(int min_size_ = 10):
            max_size((int)std::thread::hardware_concurrency()),     // unsigned => int
            min_size(std::min(max_size.load(), min_size_ + 1)),
            finish(false),
            thread_handler(&thread_pool::handler, this)
        {
        for(int i = 1; i < min_size; ++i){
            workers.emplace_back(std::make_shared<ethread>(i));
        }

    }

    ~thread_pool(){
        finish = true;
        if (thread_handler.joinable())
            thread_handler.join();
    }

    template<typename Func, typename ...Args>
    auto submit(Func &&f, Args &&... args) -> std::future<typename std::invoke_result_t<Func, Args...>>{
        typedef typename std::invoke_result_t<Func, Args...> result_type;
        std::function<result_type()> row = [&]{return std::forward<Func>(f)(std::forward<Args>(args)...);};
        auto task = std::make_shared<std::packaged_task<result_type()>>(std::move(row));
        auto res = task->get_future();
        pool_work_que.push_back(std::move([task]{(*task)();}));
        return res;
    }
};

#endif