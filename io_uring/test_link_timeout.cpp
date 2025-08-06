#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <liburing.h>

int main() {
    enum { CONNECT, TIMEOUT };
    // 创建测试用的监听socket但不接受连接
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("server socket creation failed");
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8080);

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(server_fd);
        return EXIT_FAILURE;
    }

    if (listen(server_fd, 1) < 0) {
        perror("listen failed");
        close(server_fd);
        return EXIT_FAILURE;
    }

    // 创建客户端socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("client socket creation failed");
        close(server_fd);
        return EXIT_FAILURE;
    }

    struct io_uring ring;
    io_uring_queue_init(3, &ring, 0);
    
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    io_uring_prep_connect(sqe, sockfd, (struct sockaddr*)&addr, sizeof(addr));
    io_uring_sqe_set_data(sqe, (void *)CONNECT);
    io_uring_sqe_set_flags(sqe, IOSQE_IO_LINK );
    sqe = io_uring_get_sqe(&ring);
    io_uring_prep_send(sqe, sockfd,"hello",5,0);
    // io_uring_prep_recv(sqe, sockfd, NULL, 1, 0);
    io_uring_sqe_set_data(sqe, (void *)CONNECT);
    io_uring_sqe_set_flags(sqe, IOSQE_IO_LINK );
    sqe = io_uring_get_sqe(&ring);
    struct __kernel_timespec timeout= {.tv_sec=0,.tv_nsec = 1};
    io_uring_prep_link_timeout(sqe, &timeout, 0);
    io_uring_sqe_set_data(sqe, (void *)TIMEOUT);
    io_uring_submit(&ring);
    
    struct io_uring_cqe *cqe;
    for (int i = 0; i < 3; i++) {
        io_uring_wait_cqe(&ring, &cqe);
        printf("25: Get %s\n", io_uring_cqe_get_data(cqe) == (void *)CONNECT ? "CONNECT" : "TIMEOUT");
        if (cqe->res < 0) {
            printf("27: %s%s\n", strerror(-cqe->res), -cqe->res == ENOENT ? " (ENOENT)" : "");
            return EXIT_FAILURE;
        }
    io_uring_cqe_seen(&ring, cqe);
    }
    io_uring_queue_exit(&ring);
    close(sockfd);
    close(server_fd);
    return EXIT_SUCCESS;
}