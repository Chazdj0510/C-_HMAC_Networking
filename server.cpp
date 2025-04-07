#include <iostream>
#include <cstring>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <openssl/hmac.h>

static const char* SECRET_KEY = "secret_key";
static const size_t KEY_LENGTH = 9;

constexpr int PORT = 5001;
constexpr size_t MESSAGE_SIZE = 1024 * 1024; // 1 MB
constexpr size_t HMAC_SIZE = 32;             // SHA-256 output size
constexpr int NUM_MESSAGES = 100;

// Reads exactly 'n' bytes from the socket into buffer
bool readNBytes(int sock, void* buffer, size_t n) {
    size_t totalRead = 0;
    char* buf = static_cast<char*>(buffer);
    while (totalRead < n) {
        ssize_t bytesRead = recv(sock, buf + totalRead, n - totalRead, 0);
        if (bytesRead <= 0) return false;
        totalRead += bytesRead;
    }
    return true;
}

int main() {
    // Create socket and allow reuse
    int serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd < 0) return std::cerr << "Socket creation failed.\n", 1;
    int opt = 1;
    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Bind to all interfaces on specified port
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);
    if (bind(serverFd, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0)
        return std::cerr << "Bind failed.\n", close(serverFd), 1;

    // Listen and accept one connection
    if (listen(serverFd, 1) < 0)
        return std::cerr << "Listen failed.\n", close(serverFd), 1;

    std::cout << "Server listening on port " << PORT << std::endl;
    sockaddr_in clientAddr{};
    socklen_t clientLen = sizeof(clientAddr);
    int clientFd = accept(serverFd, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
    if (clientFd < 0)
        return std::cerr << "Accept failed.\n", close(serverFd), 1;
    std::cout << "Client connected.\n";

    // Start timer and prepare for receiving messages
    auto start = std::chrono::high_resolution_clock::now();
    size_t totalReceived = 0;

    for (int i = 0; i < NUM_MESSAGES; i++) {
        char* buffer = new char[MESSAGE_SIZE];
        if (!readNBytes(clientFd, buffer, MESSAGE_SIZE)) {
            std::cerr << "Failed to read message " << i+1 << "\n";
            delete[] buffer;
            break;
        }

        unsigned char receivedHmac[HMAC_SIZE];
        if (!readNBytes(clientFd, receivedHmac, HMAC_SIZE)) {
            std::cerr << "Failed to read HMAC " << i+1 << "\n";
            delete[] buffer;
            break;
        }

        totalReceived += MESSAGE_SIZE;

        unsigned int len = 0;
        unsigned char* computedHmac = HMAC(EVP_sha256(), SECRET_KEY, KEY_LENGTH,
                                           reinterpret_cast<unsigned char*>(buffer), MESSAGE_SIZE,
                                           nullptr, &len);

        if (CRYPTO_memcmp(computedHmac, receivedHmac, HMAC_SIZE) == 0)
            std::cout << "Message " << i+1 << " verified successfully.\n";
        else
            std::cerr << "Message " << i+1 << " HMAC verification failed!\n";

        delete[] buffer;
    }

    // End timer and report throughput
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    double throughputKBs = (totalReceived / elapsed.count()) / 1024.0;
    std::cout << "Received " << totalReceived << " bytes in " << elapsed.count() << " seconds.\n";
    std::cout << "Throughput: " << throughputKBs << " KB/s\n";

    // Send acknowledgment and clean up
    send(clientFd, "ACK", 3, 0);
    close(clientFd);
    close(serverFd);
    return 0;
}

