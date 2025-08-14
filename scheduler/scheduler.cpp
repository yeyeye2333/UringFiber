#include <thread>
#include <iostream>
#include "scheduler.hpp"

Scheduler::Scheduler()=default;

Scheduler::~Scheduler()=default;

Scheduler& Scheduler::GetInstance(){
    static Scheduler sche;
    return sche;
}

void Scheduler::Start(int processer_num){
    assert(processer_num > 0 && processers_.empty());

    taskPool_=std::make_shared<TaskPool>();
    ioEngine_=std::make_shared<io_uring_engine>();
    
    for (int i = 0; i < processer_num; ++i){
        processers_.emplace_back(taskPool_);
        processers_.back()->Start();
    }

    for(auto& tk:tempTasks_){
        AddTask(tk);
    }

    while(!stop_){
        std::this_thread::sleep_for(std::chrono::seconds(1));
        LoadBalance();
    }
}

void Scheduler::Stop(){
    stop_ = true;

    for(auto& proc:processers_){
        proc->Stop();
    }

    WakeUpIOSubmit();
    WakeUpIOWait();
}

void Scheduler::AddTask(std::function<void()> func){
    static uint64_t idx=0;
    if (processers_.empty()){
        tempTasks_.push_back(func);
    }else if(auto proc=Processer::GetCurProcesser();proc!=nullptr){
        proc->AddTask(func);
    }else{
        processers_[idx++%processers_.size()]->AddTask(func);
    }
}

void Scheduler::AddTask(std::list<std::unique_ptr<Task>> &&tasks){
    assert(!processers_.empty());
    
    static uint64_t idx=0;
    processers_[idx++%processers_.size()]->AddTask(std::move(tasks));
}

void Scheduler::IOSubmit(std::unique_ptr<std::function<void(std::shared_ptr<io_uring_engine>)>> func){
    ioQueue_.push(std::move(func));
    if (ioWait_==true){
        WakeUpIOSubmit();
    }
}

void Scheduler::WakeUpIOWait(){
    // stop后不会再执行新协程，可传入错误协程指针
    io_uring_engine::result res; 
    __kernel_timespec timeout={0,0};
    ioEngine_->TimeOut(res,timeout);
}

void Scheduler::WakeUpIOSubmit(){
    std::unique_lock<std::mutex> lock(ioWaitMutex_);
    if (ioWait_==true){
        ioWait_=false;
        ioWaitCond_.notify_one();
    }
}

void Scheduler::IOWaitThread(){
    int64_t tk;
    while(!stop_){
        if(!ioEngine_->WaitResult(tk)){
            std::cerr<<"io_uring wait error"<<std::endl;
            continue;
        }
        auto task=reinterpret_cast<Task*>(tk);
        AddTask({std::make_unique<Task>(task)});
    }
}

void Scheduler::IOSubmitThread(){
    while(!stop_){
        auto ioFuncPtr=ioQueue_.pop();
        if(ioFuncPtr==nullptr){
            std::unique_lock<std::mutex> lock(ioWaitMutex_);
            ioWait_=true;
            ioFuncPtr=ioQueue_.pop();
            if (ioFuncPtr!=nullptr){
                ioWait_=false;
            }else{
                ioWaitCond_.wait(lock,[this]{return ioWait_==false;});
                continue;
            }
        }
        (*ioFuncPtr)(ioEngine_);
    }
}

void Scheduler::LoadBalance(){
    if(processers_.empty()) return;

    // 计算平均负载
    int totalTasks = 0;
    for(auto& proc : processers_){
        totalTasks += proc->TaskSize();
    }
    float avgTasks = static_cast<float>(totalTasks) / processers_.size();

    // 找出可以分配和需要分配的线程
    std::vector<Processer*> overloaded;
    std::vector<Processer*> underloaded;
    for(auto& proc : processers_){
        float tasks = proc->TaskSize();
        if(tasks > avgTasks * 1.1f){
            overloaded.push_back(proc);
        }else if(tasks < avgTasks * 0.3f){
            underloaded.push_back(proc);
        }
    }

    // 均衡负载
    for(auto* dst : underloaded){
        for(auto* src : overloaded){
            if(src->TaskSize() <= avgTasks * 1.1f) continue;
            if(dst->TaskSize() >= avgTasks * 0.3f) break;

            // 偷取最多1/4
            int stealNum = std::max(1, static_cast<int>((src->TaskSize() - avgTasks) / 4));
            if(stealNum > 0){
                auto stolenTasks = src->Steal(stealNum);
                if(!stolenTasks.empty()){
                    dst->AddTask(std::move(stolenTasks));
                }
            }
        }
    }
}