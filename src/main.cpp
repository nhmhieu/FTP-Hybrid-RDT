#include <iostream>
#include <string>
#include "control/TCPServer.h"
#include "control/TCPClient.h"

#pragma comment(lib, "Ws2_32.lib")

int main(int argc, char* argv[]) {
    bool isServer = false;
    bool isClient = false;
    int port = 8080;                // Port mặc định
    std::string host = "127.0.0.1"; // Host mặc định (localhost)

    // 1. Phân tích tham số dòng lệnh (Command Line Parser)
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-s") {
            isServer = true;
        }
        else if (arg == "-c") {
            isClient = true;
        }
        else if (arg == "-p" && i + 1 < argc) {
            port = std::stoi(argv[++i]); // Lấy giá trị port
        }
        else if (arg == "-h" && i + 1 < argc) {
            host = argv[++i];            // Lấy địa chỉ IP host
        }
    }

    // 2. Ràng buộc kiểm tra tính hợp lệ của tham số
    if (isServer && isClient) {
        std::cerr << "[LỖI] Không thể chạy đồng thời vừa là Server vừa là Client!\n";
        return 1;
    }

    // 3. Điều hướng chạy Server
    if (isServer) {
        std::cout << "===> Đang khởi chạy TCP FTP Server trên Port: " << port << "...\n";
        TCPServer server(port);
        if (server.start()) {
            server.acceptClients(); // Hàm này sẽ block để liên tục nhận kết nối
        }
        else {
            std::cerr << "[LỖI] Khởi tạo Server thất bại!\n";
            return 1;
        }
    }
    // 4. Điều hướng chạy Client
    else if (isClient) {
        std::cout << "===> Đang kết nối tới FTP Server (" << host << ":" << port << ")...\n";
        TCPClient client;

        if (client.connectToServer(host, port)) {
            std::cout << "[+] Kết nối thành công!\n";

            // Nhận và in thông điệp chào mừng (Mã 220) từ Server gửi qua
            std::string welcomeMsg = client.receiveData();
            std::cout << welcomeMsg;

            // Vòng lặp giao tiếp CLI cho Client
            std::string userCommand;
            while (true) {
                std::cout << "ftp> ";
                if (!std::getline(std::cin, userCommand) || userCommand.empty()) {
                    continue;
                }

                // Gửi câu lệnh sang Server (Đảm bảo có ký tự xuống dòng \r\n chuẩn FTP)
                client.sendData(userCommand + "\r\n");

                // Nhận và hiển thị phản hồi từ Server
                std::string response = client.receiveData();
                if (response.empty()) {
                    std::cout << "[-] Mất kết nối tới Server.\n";
                    break;
                }
                std::cout << response;

                // Nếu người dùng gõ QUIT thì thoát chương trình Client
                if (userCommand == "QUIT" || userCommand == "quit") {
                    break;
                }
            }
            client.disconnect();
        }
        else {
            std::cerr << "[LỖI] Không thể kết nối tới Server " << host << ":" << port << "\n";
            return 1;
        }
    }
    // 5. Trường hợp người dùng không truyền -s hay -c
    else {
        std::cout << "================ CÚ PHÁP SỬ DỤNG ================\n";
        std::cout << "Chạy Server: " << argv[0] << " -s [-p <port>]\n";
        std::cout << "Chạy Client: " << argv[0] << " -c [-h <host>] [-p <port>]\n";
        std::cout << "Ví dụ:\n";
        std::cout << "  " << argv[0] << " -s -p 8080\n";
        std::cout << "  " << argv[0] << " -c -h 127.0.0.1 -p 8080\n";
        std::cout << "=================================================\n";
    }

    return 0;
}