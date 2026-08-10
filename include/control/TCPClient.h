#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include "common/DataMode.h"


#pragma comment(lib, "Ws2_32.lib")

class TCPClient {
private:
    SOCKET clientSocket;
    bool isConnected;
    // bool passiveMode;
    std::string passiveIP;
    int passivePort;
    bool asciiType;
    char transferMode;
    std::string receiveBuffer;


    DataMode dataMode ; 
    std::string activeIP;
    int activePort;


public:
    TCPClient();
    ~TCPClient();

    // Kết nối tới Server bằng IP và Port
    bool connectToServer(const std::string& ipAddress, int port);

    // Gửi lệnh sang Server (ví dụ: "USER admin\r\n")
    bool sendData(const std::string& data);

    // Nhận phản hồi từ Server
    std::string receiveData();
    bool enterPassiveMode();
    bool enterActiveMode(const std::string& portArgs);
    bool setTransferType(const std::string& type);
    bool setTransferMode(const std::string& mode);

    // Upload file lên Server
    bool uploadFile(
        const std::string& localFilePath,
        const std::string& remoteFileName,
        const std::string& commandName = "STOR"
    );

    bool downloadFile(
        const std::string& remoteFileName,
        const std::string& localFilePath,
        const std::string& clientIP,
        int udpPort
    );

    // Ngắt kết nối
    void disconnect();
    void cleanupDataChannel() ; 
};

#endif
