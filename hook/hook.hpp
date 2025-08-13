#pragma once

#include <unistd.h>
#include <resolv.h>
#include <poll.h>

extern "C" {
typedef int (*open_t)(const char *__path, int __oflag, ...);

typedef int (*socket_t)(int domain, int type, int protocol);

typedef int(*close_t)(int);

typedef int(*connect_t)(int, const struct sockaddr *, socklen_t);

typedef int(*accept_t)(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

typedef int(*accept4_t)(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags);

typedef ssize_t(*read_t)(int, void *, size_t);

typedef ssize_t(*recv_t)(int sockfd, void *buf, size_t len, int flags);

typedef ssize_t(*recvmsg_t)(int sockfd, struct msghdr *msg, int flags);

typedef ssize_t(*write_t)(int, const void *, size_t);

typedef ssize_t(*send_t)(int sockfd, const void *buf, size_t len, int flags);

typedef ssize_t(*sendmsg_t)(int sockfd, const struct msghdr *msg, int flags);

typedef unsigned int(*sleep_t)(unsigned int seconds);

typedef int (*usleep_t)(useconds_t usec);

typedef int(*nanosleep_t)(const struct timespec *req, struct timespec *rem);

typedef int(*fcntl_t)(int __fd, int __cmd, ...);

typedef int (*getsockopt_t)(int sockfd, int level, int optname, void *optval, socklen_t *optlen);

typedef int (*setsockopt_t)(int sockfd, int level, int optname, const void *optval, socklen_t optlen);

typedef int(*dup_t)(int);

typedef int(*dup2_t)(int, int);

}