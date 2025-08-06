#include <thread>
#include <iostream>
#include <assert.h>
#include "lockfree_queue.hpp"
#define TESTCOUNT 100000

void TestLockFreeQueMultiPushPop() {
    LFQueue<int>  que;
    std::thread t1([&]() {
        for (int i = 0; i < TESTCOUNT * 100; i++) {
            que.push(i);
            // std::cout << "push data is " << i << std::endl;
            // std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        std::cout << "t1 over"<< std::endl;
        });

    std::thread t4([&]() {
        for (int i = TESTCOUNT*100; i < TESTCOUNT * 200; i++) {
            que.push(i);
            // std::cout << "push data is " << i << std::endl;
            // std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        std::cout << "t4 over"<< std::endl;
        });

    std::thread t2([&]() {
        for (int i = 0; i < TESTCOUNT * 100;) {
            auto p = que.pop();
            if (p == nullptr) {
                // std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            i++;
            // std::cout << "pop data is " << *p << std::endl;
        }
        std::cout << "t2 over"<< std::endl;
        });

    std::thread t3([&]() {
        for (int i = 0; i < TESTCOUNT * 100;) {
            auto p = que.pop();
            if (p == nullptr) {
                // std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            i++;
            // std::cout << "pop data is " << *p << std::endl;
        }
        std::cout << "t3 over"<< std::endl;
        });

    std::this_thread::sleep_for(std::chrono::seconds(5));
    t1.join();
    std::cout<< "size is "<< que.size<<std::endl;
    std::cout << "destruct count is " << que.destruct_count << std::endl;
    t2.join();
    std::cout<< "size is "<< que.size<<std::endl;
    std::cout << "destruct count is " << que.destruct_count << std::endl;
    t3.join();
    std::cout<< "size is "<< que.size<<std::endl;
    std::cout << "destruct count is " << que.destruct_count << std::endl;
    t4.join();
    std::cout<< "size is "<< que.size<<std::endl;
    std::cout << "destruct count is " << que.destruct_count << std::endl;
    assert(que.destruct_count == TESTCOUNT * 200);
}

void TestSimple(){
    LFQueue<int>  que;
    std::thread t1([&]() {
        std::cout<<std::this_thread::get_id()<<"push\n";
            que.push(1);
        });
    std::thread t2([&]() {
            std::this_thread::sleep_for(std::chrono::seconds(2));
        std::cout<<std::this_thread::get_id()<<"pop\n";
            auto p = que.pop();
            assert(*p == 1);
        });
    t1.join();
    t2.join();
    assert(que.destruct_count == 1);
}

int main(){
    TestLockFreeQueMultiPushPop();
    // TestSimple();
}