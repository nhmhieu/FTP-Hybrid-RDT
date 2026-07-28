include <iostream>
#include <string>
#include "control/TCPServer.h"

#pragma comment(lib, "Ws2_32.lib")

int main(int argc, char* argv[]) {
    int port = 8080; // Cổng mặc định

    // 1. Phân tích tham số dòng lệnh (Command Line Arguments)
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-p" && i + 1 < argc) {
            port = std::stoi(argv[++i]); // Đọc giá trị port phía sau -p
        }
    }

    // 2. Khởi chạy Server
    std::cout << "===> Dang khoi chay TCP Server o Port: " << port << "...\n";

    TCPServer server(port);
    if (server.start()) {
        server.acceptClients();
    }
    else {
        std::cerr << "Khong the khoi tao Server!\n";
        return 1;
    }

    return 0;
}