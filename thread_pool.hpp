#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <memory>
#include <thread>
#include <vector>
#include <atomic>
#include <future>
#include <functional>
#include <deque>

#include "threadsafe_deque.hpp"
#include <condition_variable>
#include "spdlog/spdlog.h"

// 用于汇聚线程
class join_threads{
    std::vector<std::thread> &threads;
    std::condition_variable &thread_blocker;

public:
    explicit join_threads(std::vector<std::thread> &threads_, std::condition_variable& blocker)
    : threads(threads_), thread_blocker(blocker) {}

    // 由于线程空闲时会阻塞 需要将其唤醒才能汇入
    ~join_threads(){
        for(auto & thread : threads){
            if (thread.joinable()) {
                thread_blocker.notify_all();
                thread.join();
            }
        }
    }
};

class ethread{
    std::thread t;
    std::mutex wake;
    std::unique_lock<std::mutex> unique_wake;

    inline thread_local static

public:
    template<typename F, typename ...Args>
    ehtread(F &&f, Args &&...args)
        : t(std::forward<F>(f), std::forward<Args>(args)...),
        wake(), unique_wake(wake) {}

    void join() {t.join();}

    bool joinable() {return t.joinable();}


}

class thread_pool{
    typedef std::function<void()> task_type;
    typedef std::unique_lock<std::mutex> unique_lock;

    std::atomic_bool done;
    std::atomic_uint max_size;   // 最大线程数量
    std::atomic_uint min_size;

    std::condition_variable thread_blocker; // 条件变量 用于唤醒线程
    std::mutex condition_lock;  // 条件变量内的锁
    unique_lock condition; // 条件变量内的锁

    unsigned handler_id;  // 负责调度的线程的id
    std::vector<std::thread> threads;   // 所有线程
    std::vector<unsigned> work_ids; // 线程id列表
    threadsafe_deque<task_type> pool_work_que;  // 公共任务队列

    std::vector<threadsafe_deque<task_type>> thread_work_deques;
    inline thread_local static threadsafe_deque<task_type> *thread_work_que= nullptr;
    inline thread_local static unsigned work_id=0;

    join_threads joiner;

    inline bool judge(){
        bool t = done.load() || !thread_work_que->empty();
        return t;
    }

    void thread_worker(unsigned id){
        std::mutex self_lock;
        unique_lock self_u(self_lock);
        work_id = id;
        thread_work_que = &thread_work_deques[work_id];
        while(!done){
            task_type task;
            if(thread_work_que->pop_front(task)) {
                spdlog::info("work on {}", work_id);
                task();
                spdlog::info("done work on {}", work_id);
            }
            else if(done){
                continue;
            }
            else {
                spdlog::info("wait on {}", work_id);
                thread_blocker.wait(self_u, [this] { return judge(); });
                spdlog::info("thread {} wake", work_id);
            }
        }
    }

    void thread_handler(unsigned id){
        work_id = id;
        while(!done){
            task_type task;
            if(pool_work_que.pop_front(task)){
                unsigned index = MAXUINT;
                int size = INT_MAX;
                auto v_size = thread_work_deques.size();
                for (unsigned i = 1; i < v_size; ++i){
                    int tmp = thread_work_deques[i].size();
                    if (tmp < size){
                        size = tmp;
                        index = i;
                    }
                }
                if(index != MAXUINT){
                    spdlog::info("push task {}", index);
                    thread_work_deques[index].push_back(std::move(task));
                    thread_blocker.notify_all();
                }
                else{
                    return;
                    std::this_thread::yield();
                }
            }
        }
    }

public:
    explicit thread_pool(unsigned min_size_ = 10):
            done(false),
            max_size(std::thread::hardware_concurrency()), min_size(std::min(max_size.load(), min_size_ + 1)),
            thread_blocker(),
            condition_lock(), condition(condition_lock),
            thread_work_deques(min_size),
            joiner(threads, thread_blocker){

        handler_id = 0;
        threads.emplace_back(&thread_pool::thread_handler, this, handler_id);
        work_ids.emplace_back(handler_id);
        for (unsigned i = 1; i < min_size; ++i){
            threads.emplace_back(&thread_pool::thread_worker, this, i);
            work_ids.emplace_back(i);
        }
    }

    ~thread_pool(){
        done = true;
        condition_lock.unlock();
        thread_blocker.notify_all();
    }

    template<typename Func, typename ...Args>
    auto submit(Func &&f, Args &&... args) -> std::future<typename std::invoke_result_t<Func, Args...>>{
        typedef typename std::invoke_result_t<Func, Args...> result_type;
        std::function<result_type()> row = [&]{return std::forward<Func>(f)(std::forward<Args>(args)...);};
        auto task = std::make_shared<std::packaged_task<result_type()>>(std::move(row));
        auto res = task->get_future();
        pool_work_que.push_back(std::move([task]{(*task)();}));
        thread_blocker.notify_all();
        return res;
    }
};

#endif