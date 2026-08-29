#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

constexpr const char* SERVER_IP = "127.0.0.1";
constexpr int PORT = 8080;

int main() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        std::cerr << "Failed to create socket, errno=" << sock << "\n";
        return 1;
    }

    // Socket address boilerplate
    sockaddr_in dest_addr{};
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);

    // Convert IP address string to binary format
    if (inet_pton(AF_INET, SERVER_IP, &dest_addr.sin_addr) <= 0) {
        std::cerr << "Invalid or unsupported destination IP address\n";
        close(sock);
        return 1;
    }

    // Messages to send
    static constexpr std::array<std::string_view, 2> messages = {
        "Hello",
        "World!"
    };

    for (const auto msg : messages) {
        ssize_t bytes_sent = sendto(
            sock,
            msg.data(), // Pointer to contiguous byte buffer
            msg.size(), // Exact size in bytes
            0,
            reinterpret_cast<sockaddr*>(&dest_addr),
            sizeof(dest_addr)
        );
    }

    return 0;
}
