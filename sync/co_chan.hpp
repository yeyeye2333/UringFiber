#pragma once
#include <deque>
#include <stdexcept>

#include "co_mutex.hpp"

template <typename T>
class Channel {
private:
    struct SendItem{
        bool& isError;
        Task* task;
        T const& data;
    };
    struct RecvItem{
        bool& isError;
        Task* task;
        T& data;
    };
    
    
private:
    CoMutex mtx_;

    bool closed_ = false;

    unsigned int capacity_=0;
    std::deque<T> buffer_;

    std::deque<SendItem> sendQueue_;
    std::deque<RecvItem> recvQueue_;

public:
    Channel(unsigned int capacity = 0) : capacity_(capacity) {}
    ~Channel() {
        Close();
    }

    Channel(Channel const&) = delete;
    Channel(Channel&&) = delete;
    Channel& operator=(Channel const&) = delete;
    Channel& operator=(Channel&&) = delete;

    void Close(){
        std::unique_lock<CoMutex> lock(mtx_);
        if(closed_ == false){
            closed_ = true;

            // 唤醒所有等待发送的协程
            for(auto& item:sendQueue_){
                item.isError = true;
                Processer::GetCurProcesser()->AddTask({std::make_unique<Task>(item.task)});
            }
            sendQueue_.clear();

            // 唤醒所有等待接收的协程
            for(auto& item:recvQueue_){
                item.isError = true;
                Processer::GetCurProcesser()->AddTask({std::make_unique<Task>(item.task)});
            }
            recvQueue_.clear();
        }
    }

    // 发送数据
    Channel& operator<<(T const& data){
        std::unique_lock<CoMutex> lock(mtx_);
        
        if(closed_){
            throw std::runtime_error("channel closed");
        }

        if(!recvQueue_.empty()){
            // 有等待接收的协程，直接交给首个协程
            RecvItem front = recvQueue_.front();
            recvQueue_.pop_front();
            front.data = data;
            Processer::GetCurProcesser()->AddTask({std::make_unique<Task>(front.task)});
            return *this;
        }

        if(buffer_.size()<capacity_){
            // 缓冲区存在且未满，直接放入缓冲区
            buffer_.push_back(data);
            return *this;
        }

        bool isError = false;
        sendQueue_.push_back(SendItem{isError, Processer::GetCurProcesser()->GetRunningTask(), data});
        Processer::SuspendTask();
        if(isError){
            std::runtime_error("channel closed");
        }
        return *this;
    }

    // 接收数据
    Channel& operator>>(T& data){
        std::unique_lock<CoMutex> lock(mtx_);
    
        if(closed_ && buffer_.empty()){
            throw std::runtime_error("channel closed");
        }

        if(buffer_.size()>0){
            // 缓冲区存在数据，直接取出
            data = buffer_.front();
            buffer_.pop_front();

            // 存在等待发送的协程，将首个协程数据放入缓冲区
            if(!sendQueue_.empty()){
                SendItem front = sendQueue_.front();
                sendQueue_.pop_front();
                buffer_.push_back(front.data);
                Processer::GetCurProcesser()->AddTask({std::make_unique<Task>(front.task)});
            }
            return *this;
        }

        if(capacity_ == 0 && !sendQueue_.empty()){
            // 无缓冲区，且存在等待发送的协程，直接获取首个协程
            SendItem front = sendQueue_.front();
            sendQueue_.pop_front();
            data = front.data;
            Processer::GetCurProcesser()->AddTask({std::make_unique<Task>(front.task)});
            return *this;
        }

        bool isError = false;
        recvQueue_.push_back(RecvItem{isError, Processer::GetCurProcesser()->GetRunningTask(), data});
        Processer::SuspendTask();
        if(isError){
            std::runtime_error("channel closed");
        }
        return *this;
    }
};

// 可拷贝
template <typename T>
class CoChan {
private:
    std::shared_ptr<Channel<T>> chan_;

public:
    CoChan(unsigned int capacity = 0) : chan_(std::make_shared<Channel<T>>(capacity)) {}
    ~CoChan() = default;

    void Close() {
        chan_->Close();
    }

    CoChan& operator<<(T const& data) {
        (*chan_) << data;
        return *this;
    }

    CoChan& operator>>(T& data) {
        (*chan_) >> data;
        return *this;
    }
};