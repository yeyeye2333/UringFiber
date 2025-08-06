#pragma once
#include <optional>
#include <stdint.h>
#include <atomic>
#include "../context/context.hpp"

class Task{
private:
    static std::atomic<uint64_t> taskID_;
    Context ctx_;

public:
    typedef void(*Func)(Task*);
    enum Status{
        runnable,
        block,
        done
    };

    std::function<void()> realFunc_;
    Status status_=Status::runnable;
    uint64_t id_=-1;

private:
    uint64_t NewTaskID(){
        return taskID_.fetch_add(1);
    }

public:
    Task(Func func,int stack_size=1*1024*1024);
    ~Task();

    // 检测栈使用的内存是否小于等于num页
    static bool Check_stack_used(Task& task,size_t num);

    static void Swap(Task& cur,Task& next);
};

std::atomic<uint64_t> Task::taskID_=0;