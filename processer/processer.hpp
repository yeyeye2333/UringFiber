#pragma once
#include <list>
#include <deque>
#include <atomic>
#include <condition_variable>
#include <shared_mutex>
#include "lockfree_queue.hpp"
#include "task_pool.hpp"

class Processer{
private:
    static std::atomic<uint64_t> processorID_;

    std::shared_ptr<TaskPool> taskPool_;

    std::shared_mutex status_mtx_;
    std::condition_variable_any cv_;

    bool stop_=false;

    std::unique_ptr<Task> mainTask_;
    std::unique_ptr<Task> runningTask_;
    LFQueue<Task> runnableQueue_;

    std::list<std::function<void()>> LastScheToDos_;

public:
    enum Status{
        running,
        waiting
    };

    uint64_t id_=-1;
    Status status_=Status::waiting;

private:
    uint64_t NewProcessorID(){ return processorID_.fetch_add(1); }

    static void AfterLastSche();

    static void TaskFunc(Task* arg);
    static void SuspendTask();

    std::unique_ptr<Task>  Wait();

public:
    Processer(std::shared_ptr<TaskPool> taskPool);
    ~Processer();

    static Processer* & GetCurProcesser();

    Processer(const Processer&) = delete;
    Processer(Processer&&) = delete;
    Processer& operator=(const Processer&) = delete;
    Processer& operator=(Processer&&) = delete;

    void Start();
    void Stop();

    void WakeUp();

    int TaskSize(){ return runnableQueue_.size; }
    void AddTask(std::function<void()> func);
    void AddTask(std::list<std::unique_ptr<Task>> &&tasks);
    std::list<std::unique_ptr<Task>>  Steal(int num);

    void Yield();
};

std::atomic<uint64_t> Processer::processorID_=0;