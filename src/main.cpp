#include <iostream>
#include <string>
#include "control/TCPServer.h"// Nhúng header Server của bạn vào đây
#pragma comment(lib, "Ws2_32.lib")


int main(int argc, char* argv[]) {
    bool isServer = false;
    bool isClient = false;
    int port = 8080;           // Cổng mặc định
    std::string host = "127.0.0.1"; // IP mặc định

    // 1. Vòng lặp duyệt và phân tích các tham số dòng lệnh (argv)
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-s") {
            isServer = true;
        }
        else if (arg == "-c") {
            isClient = true;
        }
        else if (arg == "-p" && i + 1 < argc) {
            port = std::stoi(argv[++i]); // Đọc giá trị port phía sau -p
        }
        else if (arg == "-h" && i + 1 < argc) {
            host = argv[++i];            // Đọc giá trị host phía sau -h
        }
    }

    // 2. Ràng buộc logic kiểm tra đầu vào
    if (isServer && isClient) {
        std::cerr << "Lỗi: Không thể chạy vừa là Server vừa là Client cùng lúc!\n";
        return 1;
    }

    // 3. Điều hướng thực thi
    if (isServer) {
        std::cout << "--> Dang khoi chạy TCP Server o Port: " << port << "...\n";
        // TCPServer server(port);
        // server.start();
        // server.acceptClients();
    }
    else if (isClient) {
        std::cout << "--> Dang khoi chạy TCP Client ket noi toi " << host << ":" << port << "...\n";
        // TCPClient client(host, port);
        // client.connectToServer();
    }
    else {
        std::cout << "Cú pháp sử dụng:\n";
        std::cout << "  Chạy Server: " << argv[0] << " -s -p <port>\n";
        std::cout << "  Chạy Client: " << argv[0] << " -c -h <host> -p <port>\n";
    }

    return 0;
}

