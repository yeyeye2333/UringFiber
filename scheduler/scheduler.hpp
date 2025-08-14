#pragma once
#include <functional>
#include <deque>
#include "processer.hpp"
#include "io_uring_engine.hpp"
#include "lockfree_queue.hpp"

class Scheduler{
private:
    bool stop_=false;

    LFQueue<std::function<void(std::shared_ptr<io_uring_engine>)>> ioQueue_;
    bool ioWait_=false;
    std::mutex ioWaitMutex_;
    std::condition_variable ioWaitCond_;

    std::shared_ptr<io_uring_engine> ioEngine_;

    std::shared_ptr<TaskPool> taskPool_;

    std::deque<Processer*> processers_;

    std::deque<std::function<void()>> tempTasks_;

private:
    void LoadBalance();

    void WakeUpIOWait();
    void WakeUpIOSubmit();
    void IOWaitThread();
    void IOSubmitThread();// 同时负责负载均衡

    void AddTask(std::list<std::unique_ptr<Task>> &&tasks);

public:
    Scheduler(/* args */);
    ~Scheduler();

    static Scheduler& GetInstance();

    Scheduler(Scheduler const&) = delete;
    Scheduler(Scheduler &&) = delete;
    Scheduler& operator=(Scheduler const&) = delete;
    Scheduler& operator=(Scheduler &&) = delete;

    void Start(int processer_num=4);
    void Stop();

    void AddTask(std::function<void()> func);

    void IOSubmit(std::unique_ptr<std::function<void(std::shared_ptr<io_uring_engine>)>> func);
};
