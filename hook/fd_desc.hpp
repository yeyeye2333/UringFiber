#pragma once
#include <vector>
#include <atomic>
#include <shared_mutex>
#include <memory>

class FdLim;


class FdDesc {
public:
    enum FdType {
        tSocket,
        tOther
    };

private:
    int fd_=-1;
    FdType fdType_=tOther;
    // SocketAttribute sockAttr_;
    bool isNonBlocking_=false;
    long recvTimeout_=0;
    long sendTimeout_=0;

    std::shared_mutex mtx_;

public:
    std::atomic<long> offset=0;
    static FdLim fdLim_;

    // socketAttr
    int domain_ = -1;
    int type_ = -1;

public:
    FdDesc(int fd,FdType fdType, bool isNonBlocking=false);
    ~FdDesc();

    FdType GetFdType();

    void SetSocketAttr(int domain, int type);
    bool IsTcpSocket();

    bool IsNonBlocking();
    void SetNonBlocking(bool isNonBlocking);

    long GetSocketTimeout(int timeoutType);
    void SetSocketTimeout(int timeoutType, int microseconds);
};

FdLim FdDesc::fdLim_;

class FdLim {
public:
    int maxFdNums_=102400;

public:
    FdLim();
    ~FdLim();
};

std::vector<std::shared_ptr<FdDesc>> fds(FdDesc::fdLim_.maxFdNums_,nullptr);
void SetFd(int fd, std::shared_ptr<FdDesc> fdDesc);
