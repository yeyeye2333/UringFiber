#include <malloc.h>

int main(){
    int *ptr = (int*)malloc(100);
    ptr[5]=1;
    free(ptr);
    return 0;
}