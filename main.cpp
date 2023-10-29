#include "iostream"
#include "threadsafe_deque.hpp"
#include <vector>
#include <queue>
#include <algorithm>
#include <random>
#include <thread>
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
    myclass test;
    auto t1 = chrono::high_resolution_clock::now();
    thread_pool p(4);
    auto res1 = p.submit(fun1);
    auto res2 = p.submit(fun1);
    auto res5 = p.submit(fun1);
    auto res6 = p.submit(fun1);
    auto res3 = p.submit(fun2);
    auto res4 = p.submit(fun2);
    auto res7 = p.submit(fun2);
    auto res8 = p.submit(fun2);
    while(!res1.valid() || !res2.valid() || !res3.valid() || !res4.valid());
    res1.get(); res2.get(); res3.get(); res4.get();
    auto t2 = chrono::high_resolution_clock::now();
    auto d1 = chrono::duration_cast<chrono::milliseconds>(t2 - t1).count();
    cout << d1 << endl;
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
    cout << d2 << endl;
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

