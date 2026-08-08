#include "control/TCPClient.h"
#include "common/ftp_api.h"

#include <iostream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <atomic>
#include <algorithm>

namespace fs = std::filesystem;


// ========================================
// Constructor
// ========================================
TCPClient::TCPClient()
    : clientSocket(INVALID_SOCKET),
      isConnected(false) {

    WSADATA wsaData;

    int iResult =
        WSAStartup(MAKEWORD(2, 2), &wsaData);

    if (iResult != 0) {
        std::cerr
            << "[-] WSAStartup failed with error: "
            << iResult
            << std::endl;
    }
}


// ========================================
// Destructor
// ========================================
TCPClient::~TCPClient() {
    disconnect();
    WSACleanup();
}


// ========================================
// Connect to TCP server
// ========================================
bool TCPClient::connectToServer(
    const std::string& ipAddress,
    int port
) {
    clientSocket =
        socket(
            AF_INET,
            SOCK_STREAM,
            IPPROTO_TCP
        );

    if (clientSocket == INVALID_SOCKET) {
        std::cerr
            << "[-] Cannot create TCP socket."
            << std::endl;

        return false;
    }

    sockaddr_in serverAddr{};

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (inet_pton(
            AF_INET,
            ipAddress.c_str(),
            &serverAddr.sin_addr
        ) != 1) {

        std::cerr
            << "[-] Invalid server IP address."
            << std::endl;

        closesocket(clientSocket);
        clientSocket = INVALID_SOCKET;

        return false;
    }

    if (connect(
            clientSocket,
            reinterpret_cast<sockaddr*>(&serverAddr),
            sizeof(serverAddr)
        ) == SOCKET_ERROR) {

        std::cerr
            << "[-] TCP connection failed. Error="
            << WSAGetLastError()
            << std::endl;

        closesocket(clientSocket);
        clientSocket = INVALID_SOCKET;

        return false;
    }

    isConnected = true;

    std::cout
        << "[CLIENT] Connected to "
        << ipAddress
        << ":"
        << port
        << std::endl;

    return true;
}


// ========================================
// Send TCP data completely
// ========================================
bool TCPClient::sendData(
    const std::string& data
) {
    if (!isConnected) {
        return false;
    }

    int totalSent = 0;
    const int dataLength =
        static_cast<int>(data.size());

    while (totalSent < dataLength) {

        int bytesSent =
            send(
                clientSocket,
                data.c_str() + totalSent,
                dataLength - totalSent,
                0
            );

        if (bytesSent == SOCKET_ERROR ||
            bytesSent == 0) {

            std::cerr
                << "[-] TCP send failed. Error="
                << WSAGetLastError()
                << std::endl;

            return false;
        }

        totalSent += bytesSent;
    }

    return true;
}


// ========================================
// Receive TCP response
// ========================================
std::string TCPClient::receiveData() {
    if (!isConnected) {
        return "";
    }

    char buffer[1024] = { 0 };

    int bytesReceived =
        recv(
            clientSocket,
            buffer,
            sizeof(buffer) - 1,
            0
        );

    if (bytesReceived > 0) {
        return std::string(
            buffer,
            bytesReceived
        );
    }

    return "";
}


// ========================================
// STOR - Upload file through UDP
// ========================================
bool TCPClient::uploadFile(
    const std::string& localFilePath,
    const std::string& remoteFileName,
    const std::string& serverIP,
    int udpPort
) {
    // 1. TCP connection must exist
    if (!isConnected) {
        std::cerr
            << "[STOR] TCP client is not connected."
            << std::endl;

        return false;
    }

    // 2. Check local file
    fs::path localPath(localFilePath);

    if (!fs::exists(localPath) ||
        !fs::is_regular_file(localPath)) {

        std::cerr
            << "[STOR] Local file not found: "
            << localFilePath
            << std::endl;

        return false;
    }

    // 3. Check remote filename
    if (remoteFileName.empty()) {
        std::cerr
            << "[STOR] Remote filename is empty."
            << std::endl;

        return false;
    }

    // 4. Send STOR command through TCP
    const std::string command =
        "STOR " +
        remoteFileName +
        "\r\n";

    std::cout
        << "[STOR] Requesting upload: "
        << remoteFileName
        << std::endl;

    if (!sendData(command)) {
        std::cerr
            << "[STOR] Failed to send STOR command."
            << std::endl;

        return false;
    }

    // 5. Wait for server's preliminary response
    std::string response =
        receiveData();

    if (response.empty()) {
        std::cerr
            << "[STOR] Server did not respond."
            << std::endl;

        return false;
    }

    std::cout << response;

    // Server must reply with 150
    if (response.rfind("150", 0) != 0) {
        std::cerr
            << "[STOR] Server is not ready "
            << "for UDP transfer."
            << std::endl;

        return false;
    }

   
    std::this_thread::sleep_for(
        std::chrono::milliseconds(100)
    );

    // 6. Send actual file through Reliable UDP
    std::cout
        << "[STOR] Sending file over UDP to "
        << serverIP
        << ":"
        << udpPort
        << std::endl;

    bool udpSuccess =
        UDPData::sendFile(
            localFilePath,
            serverIP,
            udpPort
        );

    if (!udpSuccess) {
        std::cerr
            << "[STOR] UDP file transfer failed."
            << std::endl;

        /*
         * Server may still return 426.
         * Read it so the TCP control stream
         * remains synchronized.
         */
        std::string finalResponse =
            receiveData();

        if (!finalResponse.empty()) {
            std::cout << finalResponse;
        }

        return false;
    }

    // 7. UDP completed.
    // Wait for server's final FTP reply.
    response =
        receiveData();

    if (response.empty()) {
        std::cerr
            << "[STOR] Missing final server response."
            << std::endl;

        return false;
    }

    std::cout << response;

    // 8. Successful transfer must end with 226
    if (response.rfind("226", 0) == 0) {

        std::cout
            << "[STOR] Upload successful."
            << std::endl;

        return true;
    }

    std::cerr
        << "[STOR] Upload failed."
        << std::endl;

    return false;
}


// ========================================
// Disconnect
// ========================================
void TCPClient::disconnect() {
    if (clientSocket != INVALID_SOCKET) {

        closesocket(clientSocket);

        clientSocket =
            INVALID_SOCKET;
    }

    isConnected = false;
}

bool TCPClient::downloadFile(
    const std::string& remoteFileName,
    const std::string& localFilePath,
    const std::string& clientIP,
    int udpPort
) {
    if (!isConnected || remoteFileName.empty() || localFilePath.empty() ||
        udpPort <= 0 || udpPort > 65535) {
        return false;
    }

    const int p1 = udpPort / 256;
    const int p2 = udpPort % 256;
    std::string portAddress = clientIP;
    if (portAddress.empty()) {
        sockaddr_in localAddr{};
        int localAddrLength = sizeof(localAddr);
        char localAddressText[INET_ADDRSTRLEN]{};
        if (getsockname(clientSocket, reinterpret_cast<sockaddr*>(&localAddr),
                        &localAddrLength) == SOCKET_ERROR ||
            inet_ntop(AF_INET, &localAddr.sin_addr, localAddressText,
                      sizeof(localAddressText)) == nullptr) {
            return false;
        }
        portAddress = localAddressText;
    }
    std::replace(portAddress.begin(), portAddress.end(), '.', ',');

    if (!sendData("PORT " + portAddress + "," + std::to_string(p1) + "," +
                  std::to_string(p2) + "\r\n")) {
        return false;
    }

    std::string response = receiveData();
    std::cout << response;
    if (response.rfind("200", 0) != 0) {
        return false;
    }

    std::atomic<int> receiverState{0};
    bool receiveSuccess = false;
    std::thread receiver([&]() {
        receiveSuccess = UDPData::receiveFile(localFilePath, udpPort, receiverState);
    });

    while (receiverState.load() == 0) {
        std::this_thread::yield();
    }
    if (receiverState.load() < 0) {
        receiver.join();
        return false;
    }

    if (!sendData("RETR " + remoteFileName + "\r\n")) {
        receiver.join();
        return false;
    }

    response = receiveData();
    std::cout << response;
    if (response.rfind("150", 0) != 0) {
        receiver.join();
        return false;
    }

    receiver.join();
    response = receiveData();
    std::cout << response;
    return receiveSuccess && response.rfind("226", 0) == 0;
}
