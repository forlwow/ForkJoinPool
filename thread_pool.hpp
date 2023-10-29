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
#include <iostream>

// 用于汇聚线程
class join_threads{
    std::vector<std::thread> &threads;
    std::condition_variable &thread_blocker;
    std::unique_lock<std::mutex> &condition;

public:
    explicit join_threads(std::vector<std::thread> &threads_, std::condition_variable& blocker, std::unique_lock<std::mutex> &l)
    : threads(threads_), thread_blocker(blocker), condition(l) {}

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

class thread_pool{
    typedef std::function<void()> task_type;
    typedef std::unique_lock<std::mutex> unique_lock;

    std::atomic_bool done;
    std::atomic_uint max_size;   // 最大线程数量
    std::atomic_uint min_size;

    std::condition_variable thread_blocker; // 条件变量 用于唤醒线程
    std::mutex condition_lock;  // 条件变量内的锁
    unique_lock condition; // 条件变量内的锁

    unsigned handler_id;
    std::vector<std::thread> threads;
    std::vector<unsigned> work_ids;
    threadsafe_deque<task_type> pool_work_que;

    std::vector<threadsafe_deque<task_type>> thread_work_deques;
    inline thread_local static threadsafe_deque<task_type> *thread_work_que= nullptr;
    inline thread_local static unsigned work_id=0;

    join_threads joiner;

    static inline void log(unsigned id){
        std::cout << "working on " << id << std::endl;
    }
    static inline void log(){
        std::cout << "done" << std::endl;
    }

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
//                log(work_id);
                task();
//                log();
            }
            else if(done){
                continue;
            }
            else
                thread_blocker.wait(self_u, [this]{return judge();});
        }
    }

    void thread_handler(unsigned id){
        work_id = id;
        while(!done){
            task_type task;
            if(pool_work_que.pop_front(task)){
                int index = -1;
                int size = INT_MAX;
                int v_size = thread_work_deques.size();
                for (auto i = 1; i < v_size; ++i){
                    if (int tmp = thread_work_deques[i].size() < size){
                        size = tmp;
                        index = i;
                    }
                }
                if(index != -1){
                    thread_work_deques[index].push_back(std::move(task));
                    thread_blocker.notify_all();
                }
                else{
                    std::this_thread::yield();
                }
            }
        }
    }

public:
    explicit thread_pool(unsigned min_size_ = 10):
            done(false),
            max_size(std::thread::hardware_concurrency()), min_size(std::min(max_size.load(), min_size_)),
            thread_blocker(),
            condition_lock(), condition(condition_lock),
            thread_work_deques(min_size),
            joiner(threads, thread_blocker, condition){

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