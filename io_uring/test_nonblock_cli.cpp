#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <string>

int main() {
    // 创建TCP socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return 1;
    }

    // 设置服务器地址
    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8080);
    
    // 转换为网络字节序
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address/ Address not supported" << std::endl;
        close(sock);
        return 1;
    }

    // 连接到服务器
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Connection Failed" << std::endl;
        close(sock);
        return 1;
    }

    std::cout << "Connected to server" << std::endl;

    // 准备测试数据
    const std::string message = "Test data from client";
    size_t total_bytes = message.size();
    size_t bytes_sent = 0;
    
    // 确保完整发送数据
    while (bytes_sent < total_bytes) {
        ssize_t result = send(sock, message.data() + bytes_sent, total_bytes - bytes_sent, 0);
        if (result <= 0) {
            std::cerr << "Failed to send data: " << strerror(errno) << std::endl;
            break;
        }
        bytes_sent += result;
    }

    if (bytes_sent == total_bytes) {
        std::cout << "Successfully sent all " << total_bytes << " bytes of data" << std::endl;
    } else {
        std::cerr << "Only sent " << bytes_sent << " of " << total_bytes << " bytes" << std::endl;
    }

    sleep(3);
    // 确保数据完全发送
    shutdown(sock, SHUT_WR);
    
    // 关闭连接
    close(sock);
    return 0;
}
