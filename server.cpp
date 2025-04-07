#include <iostream>
#include <cstring>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <openssl/hmac.h>
 
static const char* SECRET_KEY = "secret_key";
static const size_t KEY_LENGTH = 9;  // length of "secret_key"
 
constexpr int PORT = 5001;
constexpr size_t MESSAGE_SIZE = 1024 * 1024; // 1 MB
constexpr size_t HMAC_SIZE = 32;             // 32 bytes for SHA-256
constexpr int NUM_MESSAGES = 100;
 
// Helper to read exactly n bytes from a socket
bool readNBytes(int sock, void* buffer, size_t n) {
    size_t totalRead = 0;
    char* buf = static_cast<char*>(buffer);
    while (totalRead < n) {
        ssize_t bytesRead = recv(sock, buf + totalRead, n - totalRead, 0);
        if (bytesRead <= 0) {
            return false;  // Connection closed or error
        }
        totalRead += bytesRead;
    }
    return true;
}
 
int main() {
    // Create a socket
    int serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd < 0) {
        std::cerr << "Failed to create socket.\n";
        return 1;
    }
 
    // Allow quick reuse of the port
    int opt = 1;
    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
 
    // Bind to port
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY; // Listen on all interfaces
    serverAddr.sin_port = htons(PORT);
 
    if (bind(serverFd, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
        std::cerr << "Failed to bind.\n";
        close(serverFd);
        return 1;
    }
 
    // Start listening
    if (listen(serverFd, 1) < 0) {
        std::cerr << "Failed to listen.\n";
        close(serverFd);
        return 1;
    }
 
    std::cout << "Server listening on port " << PORT << std::endl;
 
    // Accept a single client connection
    sockaddr_in clientAddr{};
    socklen_t clientLen = sizeof(clientAddr);
    int clientFd = accept(serverFd, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
    if (clientFd < 0) {
        std::cerr << "Failed to accept connection.\n";
        close(serverFd);
        return 1;
    }
    std::cout << "Client connected.\n";
 
    // Start timing
    auto start = std::chrono::high_resolution_clock::now();
    size_t totalReceived = 0;
 
    // Read NUM_MESSAGES from client
    for (int i = 0; i < NUM_MESSAGES; i++) {
        // 1) Read exactly 1 MB of data
        char* buffer = new char[MESSAGE_SIZE];
        if (!readNBytes(clientFd, buffer, MESSAGE_SIZE)) {
            std::cerr << "Failed to read message " << i+1 << "\n";
            delete[] buffer;
            break;
        }
 
        // 2) Read the 32-byte HMAC
        unsigned char receivedHmac[HMAC_SIZE];
        if (!readNBytes(clientFd, receivedHmac, HMAC_SIZE)) {
            std::cerr << "Failed to read HMAC " << i+1 << "\n";
            delete[] buffer;
            break;
        }
 
        totalReceived += MESSAGE_SIZE;
 
        // 3) Compute the expected HMAC
        unsigned int len = 0;
        unsigned char* computedHmac = HMAC(
            EVP_sha256(),
            SECRET_KEY, KEY_LENGTH,
            reinterpret_cast<unsigned char*>(buffer), MESSAGE_SIZE,
            nullptr, &len
        );
 
        // 4) Compare HMACs
        if (CRYPTO_memcmp(computedHmac, receivedHmac, HMAC_SIZE) == 0) {
            std::cout << "Message " << i+1 << " verified successfully.\n";
        } else {
            std::cerr << "Message " << i+1 << " HMAC verification failed!\n";
        }
 
        delete[] buffer;
    }
 
    // End timing
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    double throughputKBs = (totalReceived / elapsed.count()) / 1024.0;
 
    std::cout << "Received " << totalReceived << " bytes in " 
<< elapsed.count() << " seconds.\n";
    std::cout << "Throughput: " << throughputKBs << " KB/s\n";
 
    // Send ACK
    const std::string ack = "ACK";
    send(clientFd, ack.c_str(), ack.size(), 0);
 
    close(clientFd);
    close(serverFd);
    return 0;
}
