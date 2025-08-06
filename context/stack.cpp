#include "stack.hpp"
#include <sys/mman.h>
#include <unistd.h>
#include <iostream>

Stack::Stack()=default;

bool Stack::init(size_t size){
    auto pagesize=getpagesize();
    int protecte_page_num=1; // 栈顶保护页数
    // 使size向上取值，对齐系统页大小
    stack_size = (size + pagesize - 1) & ~(pagesize - 1);
    alloc_size=stack_size+ pagesize * protecte_page_num;

    if ((alloc_ptr = mmap(nullptr, alloc_size, PROT_READ | PROT_WRITE, 
            MAP_ANONYMOUS | MAP_PRIVATE, -1, 0)) == MAP_FAILED){
        std::cerr<<"mmap failed"<<std::endl;
        return false;
    }
    stack_ptr=alloc_ptr + pagesize*protecte_page_num;

    if (mprotect(alloc_ptr,pagesize * protecte_page_num,PROT_NONE) == -1){
        std::cerr<<"mprotect failed"<<std::endl;
        return false;
    }
    
    std::cout<<"alloc_ptr="<<alloc_ptr<<", stack_ptr="<<stack_ptr<<std::endl;
    return true;
}

Stack::~Stack(){
    // 释放栈
    if (alloc_ptr && munmap(alloc_ptr, alloc_size) == -1){
        std::cerr<<"munmap failed"<<std::endl;
    }
}

bool Stack::is_page_in_memory(unsigned char page_from_mincore){
    return (page_from_mincore)&1;
}

bool Stack::check_stack_used(size_t num){
    unsigned char vec[num+1];
    // 检测栈底（高地址）往下num+1个页面是否全部在内存中
    auto pagesize=getpagesize();
    auto size=pagesize*(num+1);
    auto start=stack_ptr+stack_size-size;
    if (mincore(start, size, vec) == -1){
        std::cerr<<"mincore failed"<<std::endl;
        return false;
    }

    // 只检查栈底页，和对应地址最低页
    if (is_page_in_memory(vec[0]) && !is_page_in_memory(vec[num+1])){
        return true;
    }
    return false;
}