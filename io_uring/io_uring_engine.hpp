#pragma once
#include<functional>
#include<memory>
#include<optional>
#include<cstring> 

#include<liburing.h>

//非线程安全
class io_uring_engine{
private:
    io_uring ring;
        
public:
    struct result{
        void *user_data;// 传出
        int *ret;// 内部修改
    };

    enum op_type{
        NORMAl,
        TIMEOUT
    };
    struct ring_data{
        op_type type;
        result res;
    };

private:
    io_uring_sqe* GetSqe();
    int Submit();
    int WaitCqe(io_uring_cqe**cqe_ptr);
    void CqeSeen();

    bool Exec(std::function<void(io_uring_sqe*)>func,result& res,struct __kernel_timespec timeout);

public:
    io_uring_engine(unsigned entries=1024,std::optional<io_uring_params> p=std::nullopt);
    ~io_uring_engine();

    bool Read(int fd,void* buf,size_t count,off_t offset,result res,struct __kernel_timespec timeout={0,0});
    bool Write(int fd,const void* buf,size_t count,off_t offset,result res,struct __kernel_timespec timeout={0,0});

    bool WaitResult(void *&user_data);
};
