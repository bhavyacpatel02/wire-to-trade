#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <iostream>
#include <vector>

#include "constants.h"
#include "message.h"

int main(int argc, char* argv[]) {
    // Parse arguments
    if (argc < 2) {
        std::cerr << "Missing CLI argument for number of messages\n";
        return 1;
    }
    int num_msgs = std::stoi(argv[1]);

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

    // Create messages to send
    std::vector<Message> messages;
    messages.reserve(num_msgs);
    for (int i = 0; i < num_msgs; ++i) {
        messages.emplace_back(i, 10 * i, 100 * i);
    }

    for (const auto msg : messages) {
        ssize_t bytes_sent = sendto(sock,
                                    &msg,  // Pointer to contiguous byte buffer
                                    sizeof(msg),  // Exact size in bytes
                                    0, reinterpret_cast<sockaddr*>(&dest_addr),
                                    sizeof(dest_addr));

        if (bytes_sent < 0) {
            std::cerr << "Failed to send bytes to consumer\n";
        }
    }

    return 0;
}
