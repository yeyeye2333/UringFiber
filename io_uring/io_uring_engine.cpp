#include "io_uring_engine.hpp"
#include <assert.h>
#include <iostream>

// 构造/析构函数
io_uring_engine::io_uring_engine(unsigned entries,std::optional<io_uring_params> p){
    if(!p){
        io_uring_params _p;
        memset(&_p,0,sizeof(_p));
        _p.flags=IORING_SETUP_SQPOLL;
        p=_p;
    }
    io_uring_queue_init_params(entries,&ring,&p.value());
}

io_uring_engine::~io_uring_engine(){
    io_uring_queue_exit(&ring);
}

// 主要执行流
io_uring_sqe* io_uring_engine::GetSqe(){
    return io_uring_get_sqe(&ring);
}

int io_uring_engine::Submit(){
    return io_uring_submit(&ring);
}

int io_uring_engine::WaitCqe(io_uring_cqe**cqe_ptr){
    return io_uring_wait_cqe(&ring,cqe_ptr);
}

void io_uring_engine::CqeSeen(){
    io_uring_cq_advance(&ring,1);
}

// 执行函数
bool io_uring_engine::Exec(std::function<void(io_uring_sqe*)>func,result &res,struct __kernel_timespec timeout){
    assert(timeout.tv_sec>=0 && timeout.tv_nsec>=0);
    io_uring_sqe* sqe=GetSqe();
    if (!sqe){
        return false;
    }
    func(sqe);
    io_uring_sqe_set_data(sqe,new ring_data{NORMAl,res});

    if(timeout.tv_sec>0 || timeout.tv_nsec>0){
        io_uring_sqe_set_flags(sqe, IOSQE_IO_LINK);
        sqe=GetSqe();
        if (!sqe){
            return false;
        }
        io_uring_prep_link_timeout(sqe, &timeout, 0);
        io_uring_sqe_set_data(sqe, new ring_data{TIMEOUT,res});
    }
    
    Submit();
    return true;
}

bool io_uring_engine::Read(int fd,void* buf,size_t count,off_t offset,result res,struct __kernel_timespec timeout){
    return Exec(std::bind(io_uring_prep_read,std::placeholders::_1,fd,buf,count,offset),res,timeout);
}

bool io_uring_engine::Write(int fd,const void* buf,size_t count,off_t offset,result res,struct __kernel_timespec timeout){
    return Exec(std::bind(io_uring_prep_write,std::placeholders::_1,fd,buf,count,offset),res,timeout);
}

bool io_uring_engine::WaitResult(void *&user_data){
    io_uring_cqe* cqe;
    if(WaitCqe(&cqe)<0){
        return false;
    }
    CqeSeen();

    ring_data* data=static_cast<ring_data*>(io_uring_cqe_get_data(cqe));
    if (data->type==TIMEOUT){
        switch (cqe->res){
        case -ETIME:
            *(data->res.ret)=EAGAIN;
            break;

        case -ENOENT:
        case -ECANCELED:
            return WaitResult(user_data);

        default:
            // 其他错误
            return false;
        }
    }else{
        *(data->res.ret)=-cqe->res;
    }

    user_data=data->res.user_data;
    delete data;

    return true;
}