#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

constexpr int PORT = 8080;
constexpr size_t BUFFER_SIZE = 3;

int main(int argc, char** argv) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        std::cerr << "Failed to create socket, errno=" << sock << "\n";
        return 1;
    }

    // Socket address boilerplate
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // Bind to all available interfaces
    server_addr.sin_port = htons(PORT);       // Convert port to network byte order

    // Bind to port
    if (bind(sock, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
        std::cerr << "Bind failed, errno=" << sock << "\n";
        close(sock);
        return 1;
    }
    std::cout << "UDP receiver listening on port " << PORT << "...\n";

    // Receive data
    char buffer[BUFFER_SIZE];
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);

    while (true) {
        ssize_t bytes_received = recvfrom(
            sock,
            buffer,
            BUFFER_SIZE - 1,
            0,
            reinterpret_cast<sockaddr*>(&client_addr),
            &client_len
        );

        if (bytes_received < 0) {
            std::cerr << "Failed to receive bytes, errno=" << bytes_received << "\n";
            break;
        }

        // Null-terminate data
        buffer[bytes_received] = '\0';

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);

        std::cout << "Received " << bytes_received << " bytes from " 
                  << client_ip << ":" << ntohs(client_addr.sin_port) 
                  << " -> " << buffer << "\n";
    }

    // Cleanup
    close(sock);
    return 0;
}
