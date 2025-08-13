#include "processer.hpp"

Processer::Processer(Scheduler* scheduler,std::shared_ptr<TaskPool> taskPool):scheduler_(scheduler),taskPool_(taskPool){
    id_=NewProcessorID();
    mainTask_=std::make_unique<Task>(TaskFunc);// 实际执行流以swap后为准
}

Processer::~Processer(){
    auto proc=GetCurProcesser();
    if (proc==this){
        GetCurProcesser()=nullptr;
    }
}

Processer* & Processer::GetCurProcesser(){
    static thread_local Processer* proc=nullptr;
    return proc;
}

// 延迟执行上个协程的任务，防止上下文提前被其他线程切换
void Processer::AfterLastSche(){
    auto proc=Processer::GetCurProcesser();
    assert(proc!=nullptr);

    for(auto& todo:proc->LastScheToDos_){
        todo();
    }
    proc->LastScheToDos_.clear();
}

void Processer::TaskFunc(Task* task){
    AfterLastSche();
    task->realFunc_();
    task->status_=Task::Status::done;
    GetCurProcesser()->Yield();
}

void Processer::SuspendTask(){
    auto proc=Processer::GetCurProcesser();
    if (proc==nullptr){
        return;
    }

    proc->runningTask_->status_=Task::Status::block;
    proc->Yield();
}

void Processer::Start(){
    GetCurProcesser()=this;
    status_=Status::running;
    while(!stop_){
        runningTask_=runnableQueue_.pop();
        if (runningTask_==nullptr){
            runningTask_=Wait();
            if (runningTask_==nullptr){
                continue;;
            }
        }
        Task::Swap(*mainTask_,*runningTask_);
        AfterLastSche();
    }
}

void Processer::Stop(){
    stop_=true;
    WakeUp();
}
void Processer::AddTask(std::function<void()> func){
    Task* tk;
    taskPool_->Get(tk,func,TaskFunc);
    runnableQueue_.push(std::make_unique<Task>(tk));
    if (status_==Status::waiting){
        WakeUp();
    }
}

void Processer::AddTask(std::list<std::unique_ptr<Task>> &&tasks){
    for(auto& tk:tasks){
        runnableQueue_.push(std::move(tk));
    }
    if (status_==Status::waiting){
        WakeUp();
    }
}

std::list<std::unique_ptr<Task>> Processer::Steal(int num){
    assert(num>0);
    std::list<std::unique_ptr<Task>> stolenTasks_;
    for (int i=0;i<num;i++){
        auto task=runnableQueue_.pop();
        if (task!=nullptr){
            stolenTasks_.push_back(std::move(task));
        }else{
            break;
        }
    }
    return stolenTasks_;
}

std::unique_ptr<Task> Processer::Wait(){
    std::unique_lock<std::shared_mutex> lock(status_mtx_);
    status_=Status::waiting;
    auto next_task=runnableQueue_.pop();
    if (next_task!=nullptr){
        status_=Status::running;
        return next_task;
    }
    cv_.wait(lock,[this]{return status_!=Status::waiting;});
    return runnableQueue_.pop();
}

void Processer::WakeUp(){
    std::unique_lock<std::shared_mutex> lock(status_mtx_);
    if (status_==Status::waiting){
        status_=Status::running;
        cv_.notify_one();
    }
}

void Processer::AddToDo(std::function<void()> func){
    LastScheToDos_.push_back(func);
}

// 对称协程调度
void Processer::Yield(){
    auto cur_task=runningTask_.release();
    switch (runningTask_->status_){
    case Task::Status::runnable:
        LastScheToDos_.push_back([this,cur_task]{
            runnableQueue_.push(std::make_unique<Task>(cur_task));
        });
        break;

    case Task::Status::done:
        LastScheToDos_.push_back([this,cur_task]{
            taskPool_->Release(cur_task);
        });
        break;

    case Task::Status::block:
        break;
    }

    runningTask_=runnableQueue_.pop();
    if(runningTask_!=nullptr){
        // swap to runningTask_
        Task::Swap(*cur_task,*runningTask_);
    }else if(cur_task->status_==Task::Status::runnable){
        LastScheToDos_.pop_back();
    }else{
        // swap out
        Task::Swap(*cur_task,*mainTask_);
    }
    AfterLastSche();
}