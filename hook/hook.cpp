#include <dlfcn.h>
#include <stdarg.h>

#include "scheduler.hpp"
#include "processer.hpp"
#include "fd_desc.hpp"
#include "hook.hpp"

extern "C" {
static open_t open_f=(open_t)dlsym(RTLD_NEXT,"open");
static socket_t socket_f=(socket_t)dlsym(RTLD_NEXT,"socket");
static close_t close_f=(close_t)dlsym(RTLD_NEXT,"close");
static connect_t connect_f=(connect_t)dlsym(RTLD_NEXT,"connect");
static read_t read_f=(read_t)dlsym(RTLD_NEXT,"read");
static write_t write_f=(write_t)dlsym(RTLD_NEXT,"write");
static recv_t recv_f=(recv_t)dlsym(RTLD_NEXT,"recv");
static send_t send_f=(send_t)dlsym(RTLD_NEXT,"send");
static recvmsg_t recvmsg_f=(recvmsg_t)dlsym(RTLD_NEXT,"recvmsg");
static sendmsg_t sendmsg_f=(sendmsg_t)dlsym(RTLD_NEXT,"sendmsg");
static getsockopt_t getsockopt_f=(getsockopt_t)dlsym(RTLD_NEXT,"getsockopt");
static setsockopt_t setsockopt_f=(setsockopt_t)dlsym(RTLD_NEXT,"setsockopt");
static accept_t accept_f=(accept_t)dlsym(RTLD_NEXT,"accept");
static accept4_t accept4_f=(accept4_t)dlsym(RTLD_NEXT,"accept4");
static sleep_t sleep_f=(sleep_t)dlsym(RTLD_NEXT,"sleep");
static usleep_t usleep_f=(usleep_t)dlsym(RTLD_NEXT,"usleep");
static nanosleep_t nanosleep_f=(nanosleep_t)dlsym(RTLD_NEXT,"nanosleep");
static dup_t dup_f=(dup_t)dlsym(RTLD_NEXT,"dup");
static dup2_t dup2_f=(dup2_t)dlsym(RTLD_NEXT,"dup2");
static fcntl_t fcntl_f=(fcntl_t)dlsym(RTLD_NEXT,"fcntl");



bool IOSubmit(io_uring_engine::result& res,std::function<void(std::shared_ptr<io_uring_engine>)> func){
    if(auto proc=Processer::GetCurProcesser(); proc!=nullptr){
        res.user_data=reinterpret_cast<int64_t>(Processer::GetCurProcesser()->GetRunningTask());

        proc->AddToDo([&]{
            proc->GetScheduler()->IOSubmit(std::make_unique<std::function<void(std::shared_ptr<io_uring_engine>)>>(func));
        });
        Processer::SuspendTask();

        if(res.ret<0){
            errno=-res.ret;
            res.ret=-1;
        }
        return true;
    }
    return false;
}

int open(const char *pathname, int oflags, ...){
    va_list ap;
    va_start(ap, oflags);

    int fd=open_f(pathname,oflags,va_arg(ap, mode_t));

    if(fd>=0){
        fds[fd]=std::make_shared<FdDesc>(fd,FdDesc::tOther,oflags & O_NONBLOCK);
    }

    va_end(ap);
    return fd;
}

int socket(int domain, int type, int protocol){
    int fd=socket_f(domain,type,protocol);

    if(fd>=0){
        fds[fd]=std::make_shared<FdDesc>(fd,FdDesc::tSocket,0);
        fds[fd]->SetSocketAttr(domain,type);
    }

    return fd;
}

int close(int fd){
    int ret=close_f(fd);

    if(ret>=0){
        fds[fd]=nullptr;
    }
    
    return ret;
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen){
    io_uring_engine::result res;

    auto fdPtr=fds[sockfd];
    if(fdPtr==nullptr || fdPtr->GetFdType()!=FdDesc::tSocket || fdPtr->IsNonBlocking() || !fdPtr->IsTcpSocket() ||
        !IOSubmit(res,[&](std::shared_ptr<io_uring_engine> ioEngine){
            ioEngine->Connect(sockfd,addr,addrlen,res);
        }))
    {
        return connect_f(sockfd,addr,addrlen);
    }
    return res.ret;
}


int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen){
    return accept4(sockfd,addr,addrlen,0);
}

int accept4(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags){
    io_uring_engine::result res;

    auto fdPtr=fds[sockfd];
    if(fdPtr==nullptr || fdPtr->GetFdType()!=FdDesc::tSocket || !IOSubmit(res,[&](std::shared_ptr<io_uring_engine> ioEngine){
            ioEngine->Accept(sockfd,addr,addrlen,flags,res);
        }))
    {
        return accept4_f(sockfd,addr,addrlen,flags);
    }
    if(res.ret>=0){
        int fd=res.ret;
        fds[fd]=std::make_shared<FdDesc>(fd,FdDesc::tSocket,flags & SOCK_NONBLOCK);
        fds[fd]->SetSocketAttr(fdPtr->domain_,fdPtr->type_);
    }
    return res.ret;
}

ssize_t read(int fd, void *buf, size_t count){
    io_uring_engine::result res;

    auto fdPtr=fds[fd];
    if(fdPtr==nullptr || fdPtr->IsNonBlocking() || !IOSubmit(res,[&](std::shared_ptr<io_uring_engine> ioEngine){
            switch (fdPtr->GetFdType())
            {
            case FdDesc::tOther:
                ioEngine->Read(fd,buf,count,fdPtr->offset,res);
                if(res.ret>=0){
                    fdPtr->offset+=res.ret;
                }
                break;
            
            case FdDesc::tSocket:
                struct __kernel_timespec timeout={0,0};
                timeout.tv_sec=fdPtr->GetSocketTimeout(SO_RCVTIMEO)/1000000;
                timeout.tv_nsec=fdPtr->GetSocketTimeout(SO_RCVTIMEO)%1000000*1000;
                ioEngine->Recv(fd,buf,count,0,res,timeout);
                break;
            }
        }))
    {
        return read_f(fd,buf,count);
    }
    return res.ret;
}

ssize_t write(int fd, const void *buf, size_t count){
    io_uring_engine::result res;

    auto fdPtr=fds[fd];
    if(fdPtr==nullptr || fdPtr->IsNonBlocking() || !IOSubmit(res,[&](std::shared_ptr<io_uring_engine> ioEngine){
            switch (fdPtr->GetFdType())
            {
            case FdDesc::tOther:
                ioEngine->Write(fd,buf,count,fdPtr->offset,res);
                if(res.ret>=0){
                    fdPtr->offset+=res.ret;
                }
                break;

            case FdDesc::tSocket:
                struct __kernel_timespec timeout={0,0};
                timeout.tv_sec=fdPtr->GetSocketTimeout(SO_SNDTIMEO)/1000000;
                timeout.tv_nsec=fdPtr->GetSocketTimeout(SO_SNDTIMEO)%1000000*1000;
                ioEngine->Send(fd,buf,count,0,res,timeout);
                break;
            }
        }))
    {
        return write_f(fd,buf,count);
    }
    return res.ret;
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags){
    io_uring_engine::result res;

    auto fdPtr=fds[sockfd];
    if(fdPtr==nullptr || fdPtr->GetFdType()!=FdDesc::tSocket || fdPtr->IsNonBlocking() || flags & MSG_DONTWAIT ||
        !IOSubmit(res,[&](std::shared_ptr<io_uring_engine> ioEngine){
            struct __kernel_timespec timeout={0,0};
            timeout.tv_sec=fdPtr->GetSocketTimeout(SO_RCVTIMEO)/1000000;
            timeout.tv_nsec=fdPtr->GetSocketTimeout(SO_RCVTIMEO)%1000000*1000;
            ioEngine->Recv(sockfd,buf,len,flags,res,timeout);
        }))
    {
        return recv_f(sockfd,buf,len,flags);
    }
    return res.ret;
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags){
    io_uring_engine::result res;

    auto fdPtr=fds[sockfd];
    if(fdPtr==nullptr || fdPtr->GetFdType()!=FdDesc::tSocket || fdPtr->IsNonBlocking() || flags & MSG_DONTWAIT ||
        !IOSubmit(res,[&](std::shared_ptr<io_uring_engine> ioEngine){
            struct __kernel_timespec timeout={0,0};
            timeout.tv_sec=fdPtr->GetSocketTimeout(SO_SNDTIMEO)/1000000;
            timeout.tv_nsec=fdPtr->GetSocketTimeout(SO_SNDTIMEO)%1000000*1000;
            ioEngine->Send(sockfd,buf,len,flags,res,timeout);
        }))
    {
        return send_f(sockfd,buf,len,flags);
    }
    return res.ret;
}

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags){
    io_uring_engine::result res;

    auto fdPtr=fds[sockfd];
    if(fdPtr==nullptr || fdPtr->GetFdType()!=FdDesc::tSocket || fdPtr->IsNonBlocking() || flags & MSG_DONTWAIT ||
        !IOSubmit(res,[&](std::shared_ptr<io_uring_engine> ioEngine){
            struct __kernel_timespec timeout={0,0};
            timeout.tv_sec=fdPtr->GetSocketTimeout(SO_RCVTIMEO)/1000000;
            timeout.tv_nsec=fdPtr->GetSocketTimeout(SO_RCVTIMEO)%1000000*1000;
            ioEngine->Recvmsg(sockfd,msg,flags,res,timeout);
        }))
    {
        return recvmsg_f(sockfd,msg,flags);
    }
    return res.ret;
}

ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags){
    io_uring_engine::result res;

    auto fdPtr=fds[sockfd];
    if(fdPtr==nullptr || fdPtr->GetFdType()!=FdDesc::tSocket || fdPtr->IsNonBlocking() || flags & MSG_DONTWAIT ||
        !IOSubmit(res,[&](std::shared_ptr<io_uring_engine> ioEngine){
            struct __kernel_timespec timeout={0,0};
            timeout.tv_sec=fdPtr->GetSocketTimeout(SO_SNDTIMEO)/1000000;
            timeout.tv_nsec=fdPtr->GetSocketTimeout(SO_SNDTIMEO)%1000000*1000;
            ioEngine->Sendmsg(sockfd,msg,flags,res,timeout);
        }))
    {
        return sendmsg_f(sockfd,msg,flags);
    }
    return res.ret;
}

unsigned int sleep(unsigned int seconds){
    io_uring_engine::result res;

    struct __kernel_timespec timeout={0,0};// 放在协程栈，确保取出时还存在
    if(seconds<0 && !IOSubmit(res,[&](std::shared_ptr<io_uring_engine> ioEngine){
        timeout.tv_sec=seconds;
        ioEngine->TimeOut(res,timeout);
    })){
        return sleep_f(seconds);
    }
    return res.ret;
}

int usleep(useconds_t usec){
    io_uring_engine::result res;

    struct __kernel_timespec timeout={0,0};// 放在协程栈，确保取出时还存在
    if(usec<0 && !IOSubmit(res,[&](std::shared_ptr<io_uring_engine> ioEngine){
        timeout.tv_sec=usec/1000000;
        timeout.tv_nsec=usec%1000000*1000;
        ioEngine->TimeOut(res,timeout);
    })){
        return usleep_f(usec);
    }
    return res.ret;
}

int nanosleep(const struct timespec *req, struct timespec *rem){
    io_uring_engine::result res;

    struct __kernel_timespec timeout={0,0};// 放在协程栈，确保取出时还存在
    if(req->tv_sec<0 && !IOSubmit(res,[&](std::shared_ptr<io_uring_engine> ioEngine){
        timeout.tv_sec=req->tv_sec;
        timeout.tv_nsec=req->tv_nsec;
        ioEngine->TimeOut(res,timeout);
    })){
        return nanosleep_f(req,rem);
    }
    return res.ret;
}

int fcntl(int __fd, int __cmd, ...){
    va_list ap;
    va_start(ap, __cmd);
    
    switch (__cmd) {
        case F_DUPFD:
        case F_DUPFD_CLOEXEC:
            {
                int fd = va_arg(ap, int);
                va_end(ap);

                int newfd = fcntl_f(__fd, __cmd, fd);
                if (newfd >= 0) {
                    fds[newfd]=fds[__fd];
                }

                return newfd;
            }
        
        case F_SETFD:
        case F_SETOWN:
        case F_SETSIG:
        case F_SETLEASE:
        case F_NOTIFY:
        case F_SETPIPE_SZ:
            {
                int arg = va_arg(ap, int);
                va_end(ap);
                return fcntl_f(__fd, __cmd, arg);
            }
        case F_SETFL:
            {
                int flags = va_arg(ap, int);
                va_end(ap);
                
                if (fds[__fd]) {
                    fds[__fd]->SetNonBlocking(flags & O_NONBLOCK);
                }

                return fcntl_f(__fd, __cmd, flags);
            }

        case F_GETLK:
        case F_SETLK:
        case F_SETLKW:
            {
                struct flock* arg = va_arg(ap, struct flock*);
                va_end(ap);
                return fcntl_f(__fd, __cmd, arg);
            }

        case F_GETOWN_EX:
        case F_SETOWN_EX:
            {
                struct f_owner_exlock* arg = va_arg(ap, struct f_owner_exlock*);
                va_end(ap);
                return fcntl_f(__fd, __cmd, arg);
            }

        case F_GETFL:
            {
                va_end(ap);
                return fcntl_f(__fd, __cmd);
            }

        case F_GETFD:
        case F_GETOWN:
        case F_GETSIG:
        case F_GETLEASE:
        case F_GETPIPE_SZ:
        default:
            {
                va_end(ap);
                return fcntl_f(__fd, __cmd);
            }
    }
}

int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen){
    return getsockopt_f(sockfd,level,optname,optval,optlen);
}

int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen){
    int ret=setsockopt_f(sockfd,level,optname,optval,optlen);

    if(ret>=0 && level==SOL_SOCKET && (optname==SO_RCVTIMEO || optname==SO_SNDTIMEO)){
        auto fdPtr=fds[sockfd];
        if(fdPtr!=nullptr){
            const timeval & tv = *(const timeval*)optval;
            int microseconds = tv.tv_sec * 1000000 + tv.tv_usec;
            fdPtr->SetSocketTimeout(optname, microseconds);
        }
    }

    return ret;
}

int dup(int oldfd){
    int newfd=dup_f(oldfd);

    if(newfd>=0){
        fds[newfd]=fds[oldfd];
    }

    return newfd;
}

int dup2(int oldfd, int newfd){
    if (oldfd == newfd) return dup2_f(oldfd, newfd);

    int ret = dup2_f(oldfd, newfd);
    if (ret >= 0){
        fds[newfd]=fds[oldfd];
    }

    return ret;
}

}