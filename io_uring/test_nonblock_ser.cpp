#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>

#include "io_uring_engine.hpp"

int main(int argc, char* argv[]) {
    // 创建TCP socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return 1;
    }

    // 设置socket选项
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "Failed to set socket options" << std::endl;
        close(server_fd);
        return 1;
    }

    // 绑定地址和端口
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Failed to bind socket" << std::endl;
        close(server_fd);
        return 1;
    }

    // 开始监听
    if (listen(server_fd, 10) < 0) {
        std::cerr << "Failed to listen on socket" << std::endl;
        close(server_fd);
        return 1;
    }

    std::cout << "Server listening on port 8080..." << std::endl;

    // 接受客户端连接
    sockaddr_in client_addr{};
    socklen_t client_addr_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_addr_len);
    if (client_fd < 0) {
        std::cerr << "Failed to accept client connection" << std::endl;
        close(server_fd);
        return 1;
    }

    std::cout << "Client connected" << std::endl;

    // 设置客户端套接字为非阻塞
    int flags = fcntl(client_fd, F_GETFL, 0);
    if (flags == -1) {
        std::cerr << "Failed to get socket flags" << std::endl;
        close(client_fd);
        close(server_fd);
        return 1;
    }
    if (fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        std::cerr << "Failed to set socket to non-blocking" << std::endl;
        close(client_fd);
        close(server_fd);
        return 1;
    }

    // 准备读取缓冲区
    char buffer[1024];
    ssize_t bytes_read = 0;

    if (argc > 1) {
        // 使用io_uring引擎读取
        io_uring_engine engine;
        io_uring_engine::result res;

        if (!engine.Read(client_fd, buffer, sizeof(buffer), 0, nullptr)) {
            std::cerr << "Failed to submit read request" << std::endl;
            close(client_fd);
            close(server_fd);
            return 1;
        }

        if (!engine.WaitResult(res)) {
            std::cerr << "Failed to wait for read result" << std::endl;
        } else {
            bytes_read = res.res;
        }
    } else {
        // 使用系统调用read
        bytes_read = read(client_fd, buffer, sizeof(buffer));
    }

    // 处理读取结果
    if (bytes_read == 0) {
        std::cout << "Client closed connection without sending data" << std::endl;
    } else if (bytes_read < 0) {
        std::cerr << "Read error: " << strerror(-bytes_read) << std::endl;
    } else {
        std::cout << "Received " << bytes_read << " bytes of data" << std::endl;
    }

    // 清理
    close(client_fd);
    close(server_fd);
    return 0;
}