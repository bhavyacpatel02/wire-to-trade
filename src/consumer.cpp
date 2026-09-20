#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>

#include "constants.h"
#include "message.h"

// Bunch of signal handling boilerplate
//
// g_stop can stay a plain volatile sig_atomic_t: only the receive thread
// accepts SIGINT/SIGTERM (see stop_signal_set), so the handler always runs on
// the receive thread, and only the receive thread reads the flag.
static volatile sig_atomic_t g_stop = 0;
extern "C" void handle_stop(int) { g_stop = 1; }
static void install_signal_handlers() {
    struct sigaction sa{};
    sa.sa_handler = handle_stop;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;  // NOT SA_RESTART — we want recvmsg to return EINTR
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
}

// The signals that mean "shut down". main blocks them before spawning any
// thread (a new thread inherits its creator's mask), and the receive thread
// unblocks them for itself. The kernel then has exactly one thread it can
// deliver them to: the one sitting in recvmsg, where EINTR is what we want.
static sigset_t stop_signal_set() {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    return set;
}

struct ReadCounters {
    uint32_t max_seq_num = 0;
    uint32_t dropped = 0;
};

void receive_messages(int sock, std::queue<Message>& queue, std::mutex& mutex,
                      std::atomic<bool>& receiver_done) {
    // Only this thread accepts the shutdown signals
    sigset_t set = stop_signal_set();
    pthread_sigmask(SIG_UNBLOCK, &set, nullptr);

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
            if (errno == EINTR) {
                if (g_stop) break;  // Shutdown requested
                continue;           // Some other signal, resume waiting
            }
            std::cerr << "Failed to recevied bytes, errno=" << errno << " ("
                      << std::strerror(errno) << ")\n";
            break;
        }

        // Check macOS BSD truncation flag
        if (msg.msg_flags & MSG_TRUNC) {
            std::cerr << "WARNING: Data truncated!\n";
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);

        if (bytes_received != sizeof(Message)) {
            std::cerr << "WARNING: Bytes received does not match Message size, "
                         "skipping packet.\n";
            continue;
        }

        Message m;
        memcpy(&m, buffer, sizeof(m));  // Interpret buffer's bytes as a Message

        // Add to queue of work items
        {
            std::scoped_lock lock(mutex);
            queue.push(m);
        }
    }

    // Used to release the consumer thread
    receiver_done = true;
}

void consume_messages(std::queue<Message>& queue, std::mutex& mutex,
                      std::atomic<bool>& receiver_done,
                      ReadCounters& read_counts) {
    // NOTE: Right now, we are busy spinning and grabbing the lock every time,
    // this is pretty terrible
    while (true) {
        // Consume work item (if one exists)
        Message msg{};
        bool popped = false;
        {
            std::scoped_lock lock(mutex);
            if (!queue.empty()) {
                msg = queue.front();
                queue.pop();
                popped = true;
            } else if (receiver_done) {
                // Exit when queue is drained and receiver thread has exited
                break;
            }
        }

        // Update stastics if message popped
        if (popped) {
            if (msg.seq_num - read_counts.max_seq_num > 1) {
                read_counts.dropped +=
                    msg.seq_num - read_counts.max_seq_num - 1;
            }
            read_counts.max_seq_num = msg.seq_num;
        }
    }
}

int main() {
    // Shared structures to faciliate communication across two threads
    std::queue<Message> queue;
    std::mutex mutex;
    std::atomic<bool> receiver_done{false};

    install_signal_handlers();

    // Block the shutdown signals in main before any thread exists. Both threads
    // inherit the blocked mask; the receive thread unblocks for itself.
    sigset_t set = stop_signal_set();
    pthread_sigmask(SIG_BLOCK, &set, nullptr);

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

    // Spawn threads
    ReadCounters read_counts{};
    std::thread r(receive_messages, sock, std::ref(queue), std::ref(mutex),
                  std::ref(receiver_done));
    std::thread c(consume_messages, std::ref(queue), std::ref(mutex),
                  std::ref(receiver_done), std::ref(read_counts));

    // Cleanup
    r.join();
    c.join();
    close(sock);

    // Print statistics
    std::cout << "Received final sequence number " << read_counts.max_seq_num
              << ", with " << read_counts.dropped << " dropped messages.\n";

    return 0;
}
