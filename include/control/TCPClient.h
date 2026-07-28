#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>

#pragma comment(lib, "Ws2_32.lib")

class TCPClient {
private:
    SOCKET clientSocket;
    bool isConnected;

public:
    TCPClient();
    ~TCPClient();

    // Kết nối tới Server bằng IP và Port
    bool connectToServer(const std::string& ipAddress, int port);

    // Gửi lệnh sang Server (ví dụ: "USER admin\r\n")
    bool sendData(const std::string& data);

    // Nhận phản hồi từ Server
    std::string receiveData();

    // Ngắt kết nối
    void disconnect();
};

#endif
