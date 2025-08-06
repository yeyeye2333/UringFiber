#pragma once
#include <functional>
#include <memory>
#include <assert.h>
#include "task.hpp"
#include "../thread_safe/lockfree_queue.hpp"

class TaskPool{
private:
    int stack_size_;
    uint64_t maxIdleTask_;
    std::atomic<uint64_t> count_;
    LFQueue<Task> taskQueue_;

public:
    // 栈使用页数小于等于low_page_时，可回收
    size_t low_page_=2;

private:
    Task* Create(Task::Func func){
        return new Task(func,stack_size_);
    }
    void Delete(Task* tk){
        delete tk;
    }

    
    void Put(Task* tk){
        if(++count_>maxIdleTask_){
            --count_;
            Delete(tk);
            return;
        }
        taskQueue_.push(std::make_unique<Task>(tk));
    }

    std::shared_ptr<Task> Out(Task* tk){
        if (!tk) return nullptr;

        return std::shared_ptr<Task>(tk, [this](Task* ptr){
            this->Release(ptr);
        });
    }

public:
    TaskPool(int stack_size=1*1024*1024,uint64_t maxIdleTask=-1):stack_size_(stack_size),maxIdleTask_(maxIdleTask){}
    ~TaskPool()=default;

    TaskPool(const TaskPool&)=delete;
    TaskPool(TaskPool&&)=delete;
    TaskPool& operator=(const TaskPool&)=delete;
    TaskPool& operator=(TaskPool&&)=delete;

    void Release(Task* tk){
        if (!Task::Check_stack_used(*tk,low_page_)){
            this->Delete(tk);
            return;
        }
        Put(tk);
    }

    void Get(Task* out,std::function<void()> realFunc,Task::Func taskFunc){
        auto tk=taskQueue_.pop();
        if (tk==nullptr){
            tk.reset(Create(taskFunc));
        }
        tk->realFunc_=realFunc;
        out=tk.release();
    }

    std::shared_ptr<Task> Get(std::function<void()> realFunc,Task::Func taskFunc){
        auto tk=taskQueue_.pop();
        if (tk==nullptr){
            tk.reset(Create(taskFunc));
        }
        tk->realFunc_=realFunc;
        tk->status_=Task::Status::runnable;
        return Out(tk.release());
    }
};

