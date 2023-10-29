#ifndef PARALLEL_FUNC_H
#define PARALLEL_FUNC_H

#include <thread>
#include <future>
#include <list>
#include <algorithm>
#include <queue>
#include <memory>

template<typename F, typename A>
auto spawn_task(F &&f, A &&a) -> typename std::result_of<F(A&&)>::type{
    typedef typename std::result_of<F(A&&)>::type result_type;

    std::packaged_task<result_type(A&&)> task(std::move(f));
    std::future<result_type> res(task.get_future());

    std::thread t(std::move((task), std::move(a)));
    t.detach();
    return res;
}

// 线程安全队列
template<typename T>
class threadsafe_queue{
private:
    mutable std::mutex mut;
    std::queue<std::shared_ptr<T>> data_queue;
    std::condition_variable data_cond;
public:
    threadsafe_queue() {}

    void wait_and_pop(T &value){
        std::unique_lock<std::mutex> lk(mut);
        data_cond.wait(lk, [this]{return !data_queue.empty();});
        value = std::move(*data_queue.front());
        data_queue.pop();
    }

    bool try_pop(T& value){
        std::lock_guard<std::mutex> lk(mut);
        if(data_queue.empty()) return false;
        value = std::move(*data_queue.front());
        data_queue.pop();
        return true;
    }

    std::shared_ptr<T> wait_and_pop(){
        std::unique_lock<std::mutex> lk(mut);
        data_cond.wait(lk, [this]{return !data_queue.empty();});
        std::shared_ptr<T> res = data_queue.front();
        data_queue.pop();
        return res;
    }

    std::shared_ptr<T> try_pop(){
        std::unique_lock<std::mutex> lk(mut);
        if(data_queue.empty()) return {};
        auto res = data_queue.front();
        data_queue.pop();
        return res;
    }

    void push(T new_value) {
        std::shared_ptr<T> data(
                std::make_shared<T>(std::move(new_value)));
        std::lock_guard<std::mutex> lk(mut);
        data_queue.push(data);
        data_cond.notify_one();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lk(mut);
        return data_queue.empty();
    }
};

template<typename T>
class threadsafe_queue_on_list{
    struct node{
        std::shared_ptr<T> data;
        std::unique_ptr<node> next;
    };

    std::unique_ptr<node> head;
    node *tail;
    std::mutex head_mutex;
    std::mutex tail_mutex;
    std::condition_variable data_cond;

    node* get_tail(){
        std::lock_guard<std::mutex> tail_lock(tail_mutex);
        return tail;
    }

    std::unique_ptr<node> pop_head(){
        std::unique_ptr<node> old_head = std::move(head);
        head = std::move(old_head->next);
        return old_head;
    }

    std::unique_lock<std::mutex> wait_for_data(){
        std::unique_lock<std::mutex> head_lock(head_mutex);
        data_cond.wait(head_lock, [&]{return head.get() != get_tail();});
        return head_lock;
    }

    std::unique_ptr<node> wait_pop_head(){
        std::unique_lock<std::mutex> head_lock(wait_for_data());
        return pop_head();
    }

    std::unique_ptr<node> try_pop_head(){
        std::lock_guard<std::mutex> head_lock(head_mutex);
        if (head.get() == get_tail())
            return std::unique_ptr<node>();
        return pop_head();
    }

    std::unique_ptr<node> try_pop_head(T& value){
        std::lock_guard<std::mutex> head_lock(head_mutex);
        if(head.get() == get_tail())
            return std::unique_ptr<node>();
        value = std::move(*(head->data));
        return pop_head();
    }

public:
    threadsafe_queue_on_list(): head(new node), tail(head.get()) {}
    threadsafe_queue_on_list(const threadsafe_queue_on_list& other) = delete;
    threadsafe_queue_on_list& operator=(const threadsafe_queue_on_list &other) = delete;

    std::shared_ptr<T> try_pop(){
        auto old_head = try_pop_head();
        return old_head ? old_head->data : std::shared_ptr<T>();
    }

    bool try_pop(T& value){
        const auto old_head = try_pop_head(value);
        return (bool)old_head;
    }

    std::shared_ptr<T> wait_and_pop(){
        std::unique_ptr<node> const old_head = wait_pop_head();
        return old_head->data;
    }

    void push(T new_value){
        std::shared_ptr<T> new_data(std::make_shared<T>(std::move(new_value)));
        std::unique_ptr<node> p(new node);
        node *const new_tail = p.get();
        {
            std::lock_guard<std::mutex> tail_lock(tail_mutex);
            tail->data = new_data;
            tail->next = std::move(p);
            tail = new_tail;
        }
        data_cond.notify_one();
    }

    bool empty(){
        std::lock_guard<std::mutex> head_lock(head_mutex);
        return (head.get() == get_tail());
    }
};

template<typename T>
class lock_free_stack{
private:
    struct node{
        std::shared_ptr<T> data;
        node *next;
        node(T const &data_): data(std::make_shared<T>(data_)){}
    };
    std::atomic<node*> to_be_deleted; // 要被删除的节点
    std::atomic<unsigned> threads_in_pop; // 正在pop的线程数量

    static void delete_nodes(node *nodes){ // 回收被删除的节点
        while(nodes){
            node* next = nodes->next;
            delete nodes;
            nodes = next;
        }
    }

    void chain_pending_nodes(node *nodes){
        node *last = nodes;
        while (node* const next = last->next){
            last = next;
        }
        chain_pending_nodes(nodes, last);
    }

    void chain_pending_nodes(node *first, node *last){
        last->next = to_be_deleted;
        while (!to_be_deleted.compare_exchange_weak(last->next, first));
    }

    void chain_pending_node(node *n){
        chain_pending_nodes(n, n);
    }

    void try_reclaim(node *old_head){
        if (threads_in_pop == 1){
            node *nodes_to_delete = to_be_deleted.exchange(nullptr);
            if (!--threads_in_pop) {// 只有一个线程在调用时
                delete_nodes(nodes_to_delete);
            }
            else if (nodes_to_delete){
                chain_pending_nodes(nodes_to_delete);
            }
            delete old_head;
        }
        else{
            chain_pending_node(old_head);
            --threads_in_pop;
        }
    }

    std::atomic<node*> head;

public:
    void push(const T &data){
        node *const new_node = new node(data);
        new_node->next = head.load();
        while(!head.compare_exchange_weak(new_node->next, new_node));
    }

    std::shared_ptr<T> pop(){
        ++threads_in_pop;
        node *old_head = head.load();
        while(head.compare_exchange_weak(old_head, old_head->next));
        std::shared_ptr<T> res;
        if (old_head){
            res.swap(old_head->data);
        }
        try_reclaim(old_head);
        return res;
    }
};


#endif