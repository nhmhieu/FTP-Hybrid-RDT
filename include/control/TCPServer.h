#ifndef TCP_SERVER_H
#define TCP_SERVER_H


#include <winsock2.h>
#include <ws2tcpip.h>

class TCPServer {
private:
    int port;
    SOCKET listenSocket; // Socket dùng để lắng nghe kết nối

    // Hàm xử lý dữ liệu truyền/nhận riêng cho từng client
    void handleClient(SOCKET clientSocket);

public:
    // Constructor (Khởi tạo)
    TCPServer(int serverPort);

    // Destructor (Dọn dẹp)
    ~TCPServer();

    // 1. Hàm khởi tạo Winsock, tạo socket, bind và listen
    bool start();

    // 2. Vòng lặp nhận kết nối (accept) và tạo thread mới cho mỗi client
    void acceptClients();
};

#pragma comment(lib, "Ws2_32.lib")

#endif // !TCP_SERVER_H