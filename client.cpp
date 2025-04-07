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
 
constexpr const char* SERVER_IP = "127.0.0.1";

constexpr int PORT = 5001;

constexpr size_t MESSAGE_SIZE = 1024 * 1024; // 1 MB

constexpr size_t HMAC_SIZE = 32;             // 32 bytes for SHA-256

constexpr int NUM_MESSAGES = 100;
 
int main() {

    // Prepare 1 MB of dummy data

    char* data = new char[MESSAGE_SIZE];

    memset(data, 'a', MESSAGE_SIZE);
 
    // Create socket

    int sockFd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockFd < 0) {

        std::cerr << "Failed to create socket.\n";

        return 1;

    }
 
    // Connect to server

    sockaddr_in serverAddr{};

    serverAddr.sin_family = AF_INET;

    serverAddr.sin_port = htons(PORT);

    inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);
 
    if (connect(sockFd, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {

        std::cerr << "Failed to connect to server.\n";

        close(sockFd);

        return 1;

    }

    std::cout << "Connected to server at " << SERVER_IP << ":" << PORT << std::endl;
 
    // Start timing

    auto start = std::chrono::high_resolution_clock::now();

    size_t totalSent = 0;
 
    // Send NUM_MESSAGES to the server

    for (int i = 0; i < NUM_MESSAGES; i++) {

        // 1) Compute HMAC for this data block

        unsigned int hmacLen = 0;

        unsigned char* computedHmac = HMAC(

            EVP_sha256(),

            SECRET_KEY, KEY_LENGTH,

            reinterpret_cast<unsigned char*>(data), MESSAGE_SIZE,

            nullptr, &hmacLen

        );
 
        // 2) Send data

        ssize_t sentData = send(sockFd, data, MESSAGE_SIZE, 0);

        if (sentData < 0) {

            std::cerr << "Failed to send data for message " << i+1 << "\n";

            break;

        }

        totalSent += sentData;
 
        // 3) Send HMAC

        ssize_t sentMac = send(sockFd, computedHmac, HMAC_SIZE, 0);

        if (sentMac < 0) {

            std::cerr << "Failed to send HMAC for message " << i+1 << "\n";

            break;

        }
 
        std::cout << "Sent message " << i+1 << "/" << NUM_MESSAGES << std::endl;

    }
 
    // Indicate we are done sending

    shutdown(sockFd, SHUT_WR);
 
    // Optionally receive ACK

    char ackBuffer[64];

    ssize_t ackSize = recv(sockFd, ackBuffer, sizeof(ackBuffer) - 1, 0);

    if (ackSize > 0) {

        ackBuffer[ackSize] = '\0';

        std::cout << "Received acknowledgment: " << ackBuffer << std::endl;

    }
 
    // End timing

    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> elapsed = end - start;

    double throughputKBs = (totalSent / elapsed.count()) / 1024.0;
 
    std::cout << "Sent " << totalSent << " bytes in " 
<< elapsed.count() << " seconds.\n";

    std::cout << "Throughput: " << throughputKBs << " KB/s\n";
 
    close(sockFd);

    delete[] data;

    return 0;

}
 
