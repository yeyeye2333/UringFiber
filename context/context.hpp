#pragma once
#include <ucontext.h>
#include "stack.hpp"

class Context{
private:
    ucontext_t ctx_;
    Stack stk_;
public:
    typedef void(*Func)(void*);
public:
    Context(Func func,void* arg,std::optional<Context>  next_ctx=std::nullopt,int stack_size=1*1024*1024);
    ~Context()=default;

    Context(Context const&) = delete;
    Context(Context&&) = delete;
    Context& operator=(Context const&) = delete;
    Context& operator=(Context&&) = delete;

    // 检测栈使用的内存是否小于等于num页（无法检测所有状况）
    bool Check_stack_used(size_t num);

    void Reset(Func func,void* arg,std::optional<Context>  next_ctx=std::nullopt);

    static void Swap(Context& cur,Context& next);
};