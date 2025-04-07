#include <iostream>
#include <cstring>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <openssl/hmac.h>

// Secret key used for HMAC computation
static const char* SECRET_KEY = "secret_key";
static const size_t KEY_LENGTH = 9;  // Length of the secret key

// Server configuration constants
constexpr const char* SERVER_IP = "127.0.0.1";   // IP address of the server
constexpr int PORT = 5001;                       // Port number for the connection
constexpr size_t MESSAGE_SIZE = 1024 * 1024;     // Size of each message: 1 MB
constexpr size_t HMAC_SIZE = 32;                 // Size of HMAC output for SHA-256 (32 bytes)
constexpr int NUM_MESSAGES = 100;                // Number of messages to send

int main() {
    // Allocate and initialize 1 MB of dummy data to send
    char* data = new char[MESSAGE_SIZE];
    memset(data, 'a', MESSAGE_SIZE); // Fill data with repeating 'a' characters

    // Create a TCP socket
    int sockFd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockFd < 0) {
        std::cerr << "Failed to create socket.\n";
        return 1;
    }

    // Set up the server address structure
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;                  // IPv4
    serverAddr.sin_port = htons(PORT);                // Convert port to network byte order
    inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr); // Convert IP address to binary form

    // Connect to the server
    if (connect(sockFd, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
        std::cerr << "Failed to connect to server.\n";
        close(sockFd);
        return 1;
    }

    std::cout << "Connected to server at " << SERVER_IP << ":" << PORT << std::endl;

    // Start timing the entire transmission process
    auto start = std::chrono::high_resolution_clock::now();
    size_t totalSent = 0; // Track total bytes sent

    // Loop to send NUM_MESSAGES messages
    for (int i = 0; i < NUM_MESSAGES; i++) {
        // 1) Compute HMAC of the current data block using SHA-256
        unsigned int hmacLen = 0;
        unsigned char* computedHmac = HMAC(
            EVP_sha256(),                   // Hash function
            SECRET_KEY, KEY_LENGTH,         // Secret key and length
            reinterpret_cast<unsigned char*>(data), MESSAGE_SIZE, // Data and size
            nullptr, &hmacLen               // Output buffer and length
        );

        // 2) Send the data block to the server
        ssize_t sentData = send(sockFd, data, MESSAGE_SIZE, 0);
        if (sentData < 0) {
            std::cerr << "Failed to send data for message " << i+1 << "\n";
            break;
        }
        totalSent += sentData;

        // 3) Send the HMAC immediately after the data
        ssize_t sentMac = send(sockFd, computedHmac, HMAC_SIZE, 0);
        if (sentMac < 0) {
            std::cerr << "Failed to send HMAC for message " << i+1 << "\n";
            break;
        }

        std::cout << "Sent message " << i+1 << "/" << NUM_MESSAGES << std::endl;
    }

    // Indicate to the server that no more data will be sent
    shutdown(sockFd, SHUT_WR);

    // Optionally receive an acknowledgment message from the server
    char ackBuffer[64];
    ssize_t ackSize = recv(sockFd, ackBuffer, sizeof(ackBuffer) - 1, 0);
    if (ackSize > 0) {
        ackBuffer[ackSize] = '\0'; // Null-terminate the received string
        std::cout << "Received acknowledgment: " << ackBuffer << std::endl;
    }

    // Stop the timer and calculate elapsed time
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    // Calculate throughput in KB/s
    double throughputKBs = (totalSent / elapsed.count()) / 1024.0;

    std::cout << "Sent " << totalSent << " bytes in " << elapsed.count() << " seconds.\n";
    std::cout << "Throughput: " << throughputKBs << " KB/s\n";

    // Clean up: close socket and free memory
    close(sockFd);
    delete[] data;

    return 0;
}
