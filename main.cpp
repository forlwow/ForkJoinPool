#include "iostream"
#include "threadsafe_deque.hpp"
#include <vector>
#include <queue>
#include <chrono>
#include "thread_pool.hpp"
#include "parallel_func.hpp"

using namespace std;

queue<int> q2;
atomic_bool done= false;

void fun1();
void fun2();
int fun3(int);
int fun4(int);
void fun5();

#define vsize 1e8

threadsafe_deque<int> a;
threadsafe_queue<int> b;
vector<int> v1(vsize);
vector<int> v2(vsize);

class myclass{
public:
    myclass()=default;
    myclass(myclass&&)=default;
    myclass(const myclass&)=delete;
    myclass& operator=(myclass&&)=default;
    myclass& operator=(const myclass&)=delete;

    void operator()(){
        cout << this << endl;
    }

    int operator()(int i){
        cout << i << endl;
        return i * 2;
    }
};

int main(){
    thread_pool p(10);
    this_thread::sleep_for(chrono::seconds(1));
    auto t1 = chrono::high_resolution_clock::now();
    vector<future<void>> res;
    res.emplace_back(p.submit(fun1));
    res.emplace_back(p.submit(fun1));
    res.emplace_back(p.submit(fun1));
    res.emplace_back(p.submit(fun1));
    res.emplace_back(p.submit(fun2));
    res.emplace_back(p.submit(fun2));
    res.emplace_back(p.submit(fun2));
    res.emplace_back(p.submit(fun2));
    for(auto &tmp : res){
        while(!tmp.valid());
        tmp.get();
    }
    auto t2 = chrono::high_resolution_clock::now();
    auto d1 = chrono::duration_cast<chrono::milliseconds>(t2 - t1).count();
    spdlog::info(d1);
    auto t3 = chrono::high_resolution_clock::now();
    fun1();
    fun1();
    fun1();
    fun1();
    fun2();
    fun2();
    fun2();
    fun2();
    auto t4 = chrono::high_resolution_clock::now();
    auto d2 = chrono::duration_cast<chrono::milliseconds>(t4 - t3).count();
    spdlog::info(d2);
}

void fun1(){
    for (int i = 0; i < vsize/2; ++i) {
//        a.push_front(v1[i]);
            v1[i] = 0;
    }
}

void fun2(){
    for (int i = vsize/2; i < vsize; ++i) {
//        a.push_back(v1[i]);
            v1[i] = 0;
    }
}

int fun3(int j){
    for (int i = 0; i < vsize*10; ++i) {
        a.pop_back(j);
    }
    return 0;
}

int fun4(int j){
    for (int i = 0; i < vsize; ++i) {
        a.pop_front(j);
    }
    return j;
}

void fun5(int &&p){
    cout << &p << endl;
}

