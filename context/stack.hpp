#pragma once
#include <cstddef>

class Stack{
private:
    void* alloc_ptr=nullptr;
    size_t alloc_size=0;

    void* stack_ptr=nullptr;
    size_t stack_size=0;

private:
    bool is_page_in_memory(unsigned char page_from_mincore);
public:
    Stack();
    bool init(size_t size);
    ~Stack();

    void* get_stack_ptr(){
        return stack_ptr;
    }
    size_t get_stack_size(){
        return stack_size;
    }

    // 检测栈使用的内存是否小于等于num页（无法检测所有状况）
    bool check_stack_used(size_t num);
};