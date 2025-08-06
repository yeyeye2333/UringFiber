#include <optional>
#include <stdexcept>
#include <assert.h>

#include "context.hpp"

Context::Context(Func func,void* arg,std::optional<Context> next_ctx,int stack_size){
    assert(stack_size > 0);
    if(!stk_.init(stack_size)){
        throw std::runtime_error("stack init failed");
    }
    getcontext(&ctx_);
    ctx_.uc_stack.ss_sp = stk_.get_stack_ptr();
    ctx_.uc_stack.ss_size = stk_.get_stack_size();
    if (next_ctx){
        ctx_.uc_link = &(next_ctx->ctx_);
    }
    makecontext(&ctx_,reinterpret_cast<void(*)()>(func),1,arg);
}

bool Context::Check_stack_used(size_t num){
    stk_.check_stack_used(num);
}

void Context::Reset(Func func,void* arg,std::optional<Context> next_ctx){
    if (next_ctx){
        ctx_.uc_link = &(next_ctx->ctx_);
    }
    makecontext(&ctx_,reinterpret_cast<void(*)()>(func),1,arg);
}

void Context::Swap(Context& cur,Context& next){
    swapcontext(&cur.ctx_,&next.ctx_);
}

