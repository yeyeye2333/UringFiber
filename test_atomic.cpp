#include <atomic>
#include<memory>
#include<iostream>
struct test_atomic
{
    /* data */
    int64_t a;
    int32_t b;
};

int main(){
    std::atomic<test_atomic> test_atomic={};
    if (std::atomic_is_lock_free(&test_atomic)){
        printf("The test_atomic is lock free\n");
    }else{
        printf("The test_atomic is not lock free\n");
    }
}