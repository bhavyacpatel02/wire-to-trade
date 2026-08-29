#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <iostream>

constexpr int PORT = 31337;
constexpr size_t BUFFER_SIZE = 1024;

int main() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        std::cerr << "Failed to create socket, errno=" << errno << "\n";
        return 1;
    }

    // Socket address boilerplate
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr =
        INADDR_ANY;                      // Bind to all available interfaces
    server_addr.sin_port = htons(PORT);  // Convert port to network byte order

    // Bind to port
    if (bind(sock, reinterpret_cast<sockaddr*>(&server_addr),
             sizeof(server_addr)) < 0) {
        std::cerr << "Bind failed, errno=" << errno << "\n";
        close(sock);
        return 1;
    }
    std::cout << "UDP receiver listening on port " << PORT << "...\n";

    // Receive data
    char buffer[BUFFER_SIZE];
    sockaddr_in client_addr{};

    // Some boilerplate stuff for recvmsg
    iovec iov{};
    iov.iov_base = buffer;
    iov.iov_len = BUFFER_SIZE - 1;

    msghdr msg{};
    msg.msg_name = &client_addr;
    msg.msg_namelen = sizeof(client_addr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    while (true) {
        // Reset msg_namelen before each call because recvmsg updates it
        msg.msg_namelen = sizeof(client_addr);

        ssize_t bytes_received = recvmsg(sock, &msg, 0);

        if (bytes_received < 0) {
            std::cerr << "Failed to receive bytes, errno=" << errno
                      << "\n";
            break;
        }

        // Check macOS BSD truncation flag
        if (msg.msg_flags & MSG_TRUNC) {
            std::cerr << "WARNING: DATA TRUNCATED!\n";
        }

        // Null-terminate data
        buffer[bytes_received] = '\0';

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);

        std::cout << "Received " << bytes_received << " bytes from "
                  << client_ip << ":" << ntohs(client_addr.sin_port) << " -> "
                  << buffer << "\n";
    }

    // Cleanup
    close(sock);
    return 0;
}
