#include "control/TCPClient.h"
#include <iostream>

TCPClient::TCPClient() : clientSocket(INVALID_SOCKET), isConnected(false) {
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cerr << "[-] WSAStartup failed with error: " << iResult << std::endl;
    }
}

TCPClient::~TCPClient() {
    disconnect();
    WSACleanup();
}

bool TCPClient::connectToServer(const std::string& ipAddress, int port) {
    clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) return false;

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, ipAddress.c_str(), &serverAddr.sin_addr);

    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        closesocket(clientSocket);
        clientSocket = INVALID_SOCKET;
        return false;
    }

    isConnected = true;
    return true;
}

bool TCPClient::sendData(const std::string& data) {
    if (!isConnected) return false;
    int bytesSent = send(clientSocket, data.c_str(), (int)data.length(), 0);
    return bytesSent != SOCKET_ERROR;
}

std::string TCPClient::receiveData() {
    if (!isConnected) return "";
    char buffer[1024] = { 0 };
    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesReceived > 0) {
        return std::string(buffer, bytesReceived);
    }
    return "";
}

void TCPClient::disconnect() {
    if (clientSocket != INVALID_SOCKET) {
        closesocket(clientSocket);
        clientSocket = INVALID_SOCKET;
    }
    isConnected = false;
}