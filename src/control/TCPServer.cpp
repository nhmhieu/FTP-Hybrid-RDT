#include "control/TCPServer.h"
#include "control/CommandParser.h"
#include "control/FileSystem.h"
#include "control/SessionManager.h"
#include <iostream>
#include <thread>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

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

    // Khởi tạo Root/Current Directory riêng cho Client này
    fs::path currentDir = fs::current_path();

    // 1. Gửi Welcome Message chuẩn (dùng welcomeMsg.c_str() và welcomeMsg.length())
    std::string welcomeMsg = "220 Welcome to FTP Server\r\n";
    send(clientSocket, welcomeMsg.c_str(), static_cast<int>(welcomeMsg.length()), 0);

    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

        if (bytesReceived > 0) {
            std::string rawCommand(buffer);
            ParsedCommand cmd = CommandParser::parse(rawCommand);
            std::string response = "";

            switch (cmd.command) {
                // === NHÓM QUẢN LÝ PHIÊN ===
            case FTPCommand::USER:
                response = "331 Password required for " + cmd.arg + "\r\n";
                break;

            case FTPCommand::PASS:
                response = "230 User logged in, proceed.\r\n";
                break;

            case FTPCommand::NOOP:
                response = "200 NOOP ok.\r\n";
                break;

            case FTPCommand::QUIT:
                response = "221 Goodbye.\r\n";
                send(clientSocket, response.c_str(), static_cast<int>(response.length()), 0);
                closesocket(clientSocket);
                return;

                // === NHÓM ĐIỀU HƯỚNG THƯ MỤC ===
            case FTPCommand::PWD: {
                // Output chuẩn: 257 "C:/path/to/dir" is current directory.
                response = "257 \"" + currentDir.generic_string() + "\" is current directory.\r\n";
                break;
            }

            case FTPCommand::CWD: {
                if (cmd.arg.empty()) {
                    response = "501 Syntax error in parameters.\r\n";
                    break;
                }

                // Hỗ trợ cả đường dẫn tương đối và tuyệt đối
                fs::path targetPath = fs::path(cmd.arg);
                if (targetPath.is_relative()) {
                    targetPath = currentDir / targetPath;
                }

                // Chuẩn hóa đường dẫn (giải quyết các kí tự . hoặc ..)
                std::error_code ec;
                targetPath = fs::canonical(targetPath, ec);

                if (!ec && fs::exists(targetPath) && fs::is_directory(targetPath)) {
                    currentDir = targetPath;
                    response = "250 Directory successfully changed.\r\n";
                }
                else {
                    response = "550 Failed to change directory.\r\n";
                }
                break;
            }

            case FTPCommand::CDUP: {
                // Lùi về thư mục cha của currentDir
                if (currentDir.has_parent_path()) {
                    currentDir = currentDir.parent_path();
                    response = "200 Directory changed to parent.\r\n";
                }
                else {
                    response = "550 Cannot move above root.\r\n";
                }
                break;
            }

            case FTPCommand::MKD: {
                if (cmd.arg.empty()) {
                    response = "501 Syntax error in parameters.\r\n";
                    break;
                }

                fs::path targetPath = currentDir / cmd.arg;
                std::error_code ec;
                if (fs::create_directory(targetPath, ec)) {
                    response = "257 \"" + cmd.arg + "\" directory created.\r\n";
                }
                else {
                    response = "550 Create directory failed.\r\n";
                }
                break;
            }

            case FTPCommand::RMD: {
                if (cmd.arg.empty()) {
                    response = "501 Syntax error in parameters.\r\n";
                    break;
                }

                fs::path targetPath = currentDir / cmd.arg;
                std::error_code ec;
                // fs::remove chỉ xóa nếu là file hoặc folder RỖNG
                if (fs::is_directory(targetPath) && fs::remove(targetPath, ec)) {
                    response = "250 Directory removed.\r\n";
                }
                else {
                    response = "550 Remove directory failed (dir may not be empty or exist).\r\n";
                }
                break;
            }

            default:
                response = "500 Unknown command.\r\n";
                break;
            }

            // Gửi response chuỗi chuẩn qua socket (dùng response.c_str())
            send(clientSocket, response.c_str(), static_cast<int>(response.length()), 0);
        }
        else if (bytesReceived <= 0) {
            std::cout << "[-] Client ngắt kết nối." << std::endl;
            break;
        }
    }

    closesocket(clientSocket);
}