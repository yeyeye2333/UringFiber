#include <iostream>
#include <optional>
#include <functional>
#include "context.hpp"

std::function<void()> swap1_2;
std::function<void()> swap2_1;

void func1(void* ) {
    std::cout << "Entering func1" << std::endl;
    swap1_2();
    std::cout << "Exiting func1" << std::endl;
}

void func2(void* ) {
    char stack2[3000];
    // printf("func2 %p\n", &stack2[0]);
    // std::cout << "Entering func2" << std::endl;
    swap2_1();
    // std::cout << "Exiting func2" << std::endl;
}

Context ctx2(func2, nullptr, std::nullopt, 4*1024);
Context ctx1(func1, (void*)&ctx2, std::nullopt, 4*1024);


int main() {
    swap1_2 = [&]() {
        std::cout << "Swapping from 1 to 2" << std::endl;
        ctx1.Swap(ctx1, ctx2);
    };
    swap2_1 = [&]() {
        std::cout << "Swapping from 2 to 1" << std::endl;
        ctx2.Swap(ctx2, ctx1);
    };
    std::cout << "Starting Context switch test" << std::endl;
    ctx1.Swap(ctx1, ctx2);// ctx1已被改为main函数上下文
    std::cout << "Back to main Context" << std::endl;
    
    return 0;
}
