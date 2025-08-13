#include<dlfcn.h>
#include<unistd.h>
#include"malloc.h"

static void *tmp=dlsym(RTLD_NEXT,"malloc");
static void *(*sys_malloc) (size_t __size)=(void *(*) (size_t __size))(tmp);
// extern void *__malloc (size_t __size);
        void *malloc (size_t __size){
        write(1,"malloc hook success",20);
        if (sys_malloc){
            return sys_malloc(__size);
        }else{
            // write(1,"can't dlsym",12);
            // return __malloc(__size);
            return 0;
        }
    }