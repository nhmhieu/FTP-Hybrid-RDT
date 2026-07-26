#include "control/TCPServer.h"
#include "control/CommandParser.h"
#include <iostream>
#include <thread>
#include <vector>

// Thêm dòng này để linker tự động liên kết Winsock library
#pragma comment(lib, "Ws2_32.lib")

// Constructor: Khởi tạo các giá trị ban đầu
TCPServer::TCPServer(int serverPort) : port(serverPort), listenSocket(INVALID_SOCKET) {}

// Destructor: Dọn dẹp tài nguyên khi Server dừng
TCPServer::~TCPServer() {
    if (listenSocket != INVALID_SOCKET) {
        closesocket(listenSocket);
    }
    WSACleanup(); // Dọn dẹp môi trường Winsock
}

// 1. Hàm khởi tạo Winsock, Bind và Listen
bool TCPServer::start() {
    WSADATA wsaData;
    // Khởi tạo Winsock phiên bản 2.2
    int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (res != 0) {
        std::cerr << "[LỖI] WSAStartup thất bại: " << res << std::endl;
        return false;
    }

    // Tạo socket lắng nghe
    listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        std::cerr << "[LỖI] Không thể tạo socket: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return false;
    }

    // Cấu hình địa chỉ Server (IPv4, Port, chấp nhận kết nối từ mọi IP)
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    // Bind socket với địa chỉ và port
    if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "[LỖI] Bind thất bại: " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return false;
    }

    // Chuyển socket sang chế độ lắng nghe (Hàng chờ tối đa SOMAXCONN)
    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "[LỖI] Listen thất bại: " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return false;
    }

    std::cout << "[SERVER] Đã khởi tạo thành công trên cổng " << port << std::endl;
    return true;
}

// 2. Vòng lặp liên tục chờ và nhận kết nối từ Client
void TCPServer::acceptClients() {
    std::cout << "[SERVER] Đang chờ Client kết nối..." << std::endl;

    while (true) {
        sockaddr_in clientAddr;
        int clientAddrSize = sizeof(clientAddr);

        // Đợi Client kết nối tới (Hàm này sẽ block cho đến khi có client mới)
        SOCKET clientSocket = accept(listenSocket, (sockaddr*)&clientAddr, &clientAddrSize);

        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "[LỖI] Accept thất bại: " << WSAGetLastError() << std::endl;
            continue; // Tiếp tục chờ client khác
        }

        // Lấy thông tin IP của Client vừa kết nối
        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
        std::cout << "[+] Client mới kết nối từ: " << clientIP << ":" << ntohs(clientAddr.sin_port) << std::endl;

        // Tạo Thread riêng để xử lý Client này độc lập
        std::thread clientThread(&TCPServer::handleClient, this, clientSocket);
        clientThread.detach(); // Tách thread ra để tự chạy ngầm và dọn dẹp khi xong
    }
}

// 3. Hàm xử lý truyền/nhận dữ liệu với từng Client
void TCPServer::handleClient(SOCKET clientSocket) {
    char buffer[1024];

    // Gửi thông điệp chào mừng chuẩn FTP (Mã 220) khi client vừa kết nối
    std::string welcomeMsg = "220 Welcome to FTP Server\r\n";
    send(clientSocket, welcomeMsg.c_str(), welcomeMsg.length(), 0);

    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

        if (bytesReceived > 0) {
            std::string rawCommand(buffer);

            // 1. Dùng CommandParser để giải mã câu lệnh
            ParsedCommand cmd = CommandParser::parse(rawCommand);
            std::string response = "";

            // 2. Xử lý các lệnh cơ bản
            switch (cmd.command) {
            case FTPCommand::USER:
                std::cout << "[USER]: " << cmd.arg << std::endl;
                response = "331 Password required for " + cmd.arg + "\r\n";
                break;

            case FTPCommand::PASS:
                std::cout << "[PASS]: " << cmd.arg << std::endl;
                response = "230 User logged in, proceed.\r\n";
                break;

            case FTPCommand::NOOP:
                response = "200 NOOP ok.\r\n";
                break;

            case FTPCommand::QUIT:
                response = "221 Goodbye.\r\n";
                send(clientSocket, response.c_str(), response.length(), 0);
                closesocket(clientSocket);
                return; // Thoát thread

            default:
                response = "500 Unknown command.\r\n";
                break;
            }

            // Gửi phản hồi lại cho Client
            send(clientSocket, response.c_str(), response.length(), 0);

        }
        else if (bytesReceived == 0) {
            std::cout << "[-] Client ngắt kết nối." << std::endl;
            break;
        }
        else {
            break;
        }
    }

    closesocket(clientSocket);
}