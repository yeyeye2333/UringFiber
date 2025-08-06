#include "task.hpp"

Task::Task(Func func,int stack_size):ctx_(Context::Func(func),this,std::nullopt,stack_size){
    id_=NewTaskID();
}

Task::~Task()=default;

bool Task::Check_stack_used(Task& task,size_t num){
    return task.ctx_.Check_stack_used(num);
}

void Task::Swap(Task& cur,Task& next){
    Context::Swap(cur.ctx_,next.ctx_);
}