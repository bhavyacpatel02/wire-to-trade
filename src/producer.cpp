#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <iostream>
#include <string_view>

#include "constants.h"
#include "message.h"

int main() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        std::cerr << "Failed to create socket, errno=" << errno << "\n";
        return 1;
    }

    // Socket address boilerplate
    sockaddr_in dest_addr{};
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);

    // Convert IP address string to binary format
    if (inet_pton(AF_INET, SERVER_IP.data(), &dest_addr.sin_addr) <= 0) {
        std::cerr << "Invalid or unsupported destination IP address\n";
        close(sock);
        return 1;
    }

    // Messages to send
    static constexpr std::array<Message, 2> messages = {
        Message{.seq_num = 1, .quantity = 10, .price = 500},
        Message{.seq_num = 2, .quantity = 20, .price = 600}
    };

    for (const auto msg : messages) {
        ssize_t bytes_sent = sendto(
            sock,
            &msg,  // Pointer to contiguous byte buffer
            sizeof(msg),  // Exact size in bytes
            0, reinterpret_cast<sockaddr*>(&dest_addr), sizeof(dest_addr));
    }

    return 0;
}
