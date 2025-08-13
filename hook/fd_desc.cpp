#include <sys/resource.h>
#include <sys/socket.h>
#include <mutex>
#include <assert.h>

#include "fd_desc.hpp"

FdDesc::FdDesc(int fd,FdType fdType,bool isNonBlocking):fd_(fd), fdType_(fdType), isNonBlocking_(isNonBlocking){}

FdDesc::~FdDesc()=default;

FdDesc::FdType FdDesc::GetFdType() {
    std::shared_lock<std::shared_mutex> lock(mtx_);
    return fdType_;
}

void FdDesc::SetSocketAttr(int domain, int type) {
    std::unique_lock<std::shared_mutex> lock(mtx_);
    domain_ = domain;
    type_ = type;
}

bool FdDesc::IsTcpSocket() {
    std::shared_lock<std::shared_mutex> lock(mtx_);
    return fdType_ == tSocket && type_ == SOCK_STREAM;
}

bool FdDesc::IsNonBlocking() {
    std::shared_lock<std::shared_mutex> lock(mtx_);
    return isNonBlocking_;
}

void FdDesc::SetNonBlocking(bool isNonBlocking) {
    std::unique_lock<std::shared_mutex> lock(mtx_);
    isNonBlocking_ = isNonBlocking;
}

long FdDesc::GetSocketTimeout(int timeoutType) {
    std::shared_lock<std::shared_mutex> lock(mtx_);
    return timeoutType == SO_RCVTIMEO ? recvTimeout_ : sendTimeout_;
}

void FdDesc::SetSocketTimeout(int timeoutType, int microseconds) {
    std::unique_lock<std::shared_mutex> lock(mtx_);
    if (timeoutType == SO_RCVTIMEO) {
        recvTimeout_ = microseconds;
    } else if (timeoutType == SO_SNDTIMEO) {
        sendTimeout_ = microseconds;
    }
}

FdLim::FdLim(){
    struct rlimit64 rlim;
    if(getrlimit64(RLIMIT_NOFILE, &rlim)==-1){
        maxFdNums_=102400;
    }
    maxFdNums_ = rlim.rlim_cur;
}

FdLim::~FdLim()=default;