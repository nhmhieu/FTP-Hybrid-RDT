#include "control/TCPServer.h"
#include "control/CommandParser.h"
#include "control/SessionManager.h"
#include "control/FileSystem.h"
#include <iostream>
#include <thread>
#include <vector>

#pragma comment(lib, "Ws2_32.lib")

int main(int argc, char* argv[]) {
    // Khởi tạo Winsock Server ở cổng 21 (cổng FTP chuẩn) hoặc 8080 để test
    int port = 8080;

    std::cout << "========================================" << std::endl;
    std::cout << "      HYBRID FTP SERVER - STARTING      " << std::endl;
    std::cout << "========================================" << std::endl;

    TCPServer server(port);

    if (server.start()) {
        std::cout << "[+] Server Listening on port " << port << "..." << std::endl;
        std::cout << "[+] Waiting for incoming connections..." << std::endl;

        // Vòng lặp lắng nghe và tạo Thread riêng xử lý từng Client kết nối tới
        server.acceptClients();
    }
    else {
        std::cerr << "[-] Failed to start FTP Server!" << std::endl;
        return 1;
    }

    return 0;
}
