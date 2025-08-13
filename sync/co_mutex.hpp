#pragma once
#include <atomic>

#include "lockfree_queue.hpp"
#include "processer.hpp"
#include "task.hpp"

class CoMutex {
private:
    struct state{
        unsigned locked:1;
        unsigned waiters:31;
    };

private:
    std::atomic<state> mState={state{0, 0}};
    LFQueue<Task> waiterQueue_;

public:
    CoMutex() = default;
    ~CoMutex() = default;

    void lock(){
        for(;;){
            auto oldS=mState.load();
            auto newS=oldS;
            if(oldS.locked == 0){
                newS.locked=1;
            }else{
                newS.waiters++;
            }

            if(mState.compare_exchange_strong(oldS,newS)){
                if(newS.locked == 1){
                    // 加锁成功
                    break;
                }
                // 增加等待者数量并延迟加入等待队列
                auto curTask=Processer::GetCurProcesser()->GetRunningTask();
                Processer::GetCurProcesser()->AddToDo([&]{
                    waiterQueue_.push(std::make_unique<Task>(curTask));
                });
                Processer::SuspendTask();
            }
        }    
    }

    void unlock(){
        auto oldS=mState.load();
        while(!mState.compare_exchange_strong(oldS,state{0,oldS.waiters}));

        oldS.locked=0;
        if(oldS.waiters > 0 && mState.compare_exchange_strong(oldS,state{oldS.locked, oldS.waiters-1U})){
            for(;;){
                auto task=waiterQueue_.pop();
                if(task != nullptr){
                    Processer::GetCurProcesser()->AddTask(std::list<std::unique_ptr<Task>>{std::move(task)});
                    break;
                }
            }
        }
    }

    bool try_lock(){
        auto oldS=mState.load();
        if(oldS.locked == 0){
            return mState.compare_exchange_strong(oldS,state{1, oldS.waiters});
        }
    }
};