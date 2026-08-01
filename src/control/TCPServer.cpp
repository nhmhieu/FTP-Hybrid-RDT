#include "control/TCPServer.h"
#include "control/CommandParser.h"
#include "control/FileSystem.h"
#include "control/SessionManager.h"
#include <iostream>
#include <thread>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

#pragma comment(lib, "Ws2_32.lib")

TCPServer::TCPServer(int serverPort) : port(serverPort), listenSocket(INVALID_SOCKET) {}

TCPServer::~TCPServer() {
    if (listenSocket != INVALID_SOCKET) {
        closesocket(listenSocket);
    }
    WSACleanup();
}

bool TCPServer::start() {
    WSADATA wsaData;
    int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (res != 0) {
        std::cerr << "[LỖI] WSAStartup thất bại: " << res << std::endl;
        return false;
    }

    listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        std::cerr << "[LỖI] Không thể tạo socket: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return false;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "[LỖI] Bind thất bại: " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return false;
    }

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "[LỖI] Listen thất bại: " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return false;
    }

    std::cout << "[SERVER] Đã khởi tạo thành công trên cổng " << port << std::endl;
    return true;
}

void TCPServer::acceptClients() {
    std::cout << "[SERVER] Đang chờ Client kết nối..." << std::endl;

    while (true) {
        sockaddr_in clientAddr;
        int clientAddrSize = sizeof(clientAddr);

        SOCKET clientSocket = accept(listenSocket, (sockaddr*)&clientAddr, &clientAddrSize);

        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "[LỖI] Accept thất bại: " << WSAGetLastError() << std::endl;
            continue;
        }

        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
        std::cout << "[+] Client mới kết nối từ: " << clientIP << ":" << ntohs(clientAddr.sin_port) << std::endl;

        std::thread clientThread(&TCPServer::handleClient, this, clientSocket);
        clientThread.detach();
    }
}

void TCPServer::handleClient(SOCKET clientSocket) {
    ClientSession session(clientSocket);
    std::string pendingData;
    char buffer[1024];

    // Gửi Welcome Message chuẩn
    std::string welcomeMsg = "220 Welcome to FTP Server\r\n";
    send(clientSocket, welcomeMsg.c_str(), static_cast<int>(welcomeMsg.length()), 0);

    while (true) {
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);

        if (bytesReceived <= 0) {
            std::cout << "[-] Client ngắt kết nối." << std::endl;
            break;
        }

        pendingData.append(buffer, bytesReceived);

        std::size_t endPos;
        // Xử lý từng lệnh phân tách bởi \r\n (TCP Framing)
        while ((endPos = pendingData.find("\r\n")) != std::string::npos) {
            std::string commandLine = pendingData.substr(0, endPos);
            pendingData.erase(0, endPos + 2);

            ParsedCommand cmd = CommandParser::parse(commandLine);
            std::string response = "";

            switch (cmd.command) {
                // === AUTHENTICATION STATE MACHINE ===
            case FTPCommand::USER:
                if (cmd.arg.empty()) {
                    response = "501 Missing username.\r\n";
                }
                else {
                    session.setUsername(cmd.arg);
                    session.setAuthState(AuthState::WAITING_FOR_PASS);
                    response = "331 Username OK, need password.\r\n";
                }
                break;

            case FTPCommand::PASS:
                if (session.getAuthState() != AuthState::WAITING_FOR_PASS) {
                    response = "503 Login with USER first.\r\n";
                }
                else if (session.getUsername() == "admin" && cmd.arg == "123") {
                    session.setAuthState(AuthState::AUTHENTICATED);
                    response = "230 Login successful.\r\n";
                }
                else {
                    session.setAuthState(AuthState::UNAUTHENTICATED);
                    response = "530 Login incorrect.\r\n";
                }
                break;

            case FTPCommand::NOOP:
                response = "200 NOOP ok.\r\n";
                break;

            case FTPCommand::QUIT:
                response = "221 Goodbye.\r\n";
                send(clientSocket, response.c_str(), static_cast<int>(response.length()), 0);
                closesocket(clientSocket);
                return;

                // === CÁC LỆNH ĐẦU BỎ QUA NẾU CHƯA LOGGED IN ===
            case FTPCommand::PWD: {
                if (session.getAuthState() != AuthState::AUTHENTICATED) {
                    response = "530 Not logged in.\r\n";
                    break;
                }
                response = "257 \"" + session.getCurrentDir().generic_string() + "\" is current directory.\r\n";
                break;
            }

            case FTPCommand::CWD: {
                if (session.getAuthState() != AuthState::AUTHENTICATED) {
                    response = "530 Not logged in.\r\n";
                    break;
                }
                if (cmd.arg.empty()) {
                    response = "501 Syntax error in parameters.\r\n";
                    break;
                }

                fs::path targetPath = fs::path(cmd.arg);
                if (targetPath.is_relative()) {
                    targetPath = session.getCurrentDir() / targetPath;
                }

                std::error_code ec;
                targetPath = fs::canonical(targetPath, ec);

                if (!ec && fs::exists(targetPath) && fs::is_directory(targetPath)) {
                    session.setCurrentDir(targetPath);
                    response = "250 Directory successfully changed.\r\n";
                }
                else {
                    response = "550 Failed to change directory.\r\n";
                }
                break;
            }

            case FTPCommand::CDUP: {
                if (session.getAuthState() != AuthState::AUTHENTICATED) {
                    response = "530 Not logged in.\r\n";
                    break;
                }
                if (session.getCurrentDir().has_parent_path()) {
                    session.setCurrentDir(session.getCurrentDir().parent_path());
                    response = "200 Directory changed to parent.\r\n";
                }
                else {
                    response = "550 Cannot move above root.\r\n";
                }
                break;
            }

            case FTPCommand::MKD: {
                if (session.getAuthState() != AuthState::AUTHENTICATED) {
                    response = "530 Not logged in.\r\n";
                    break;
                }
                if (cmd.arg.empty()) {
                    response = "501 Syntax error in parameters.\r\n";
                    break;
                }

                fs::path targetPath = session.getCurrentDir() / cmd.arg;
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
                if (session.getAuthState() != AuthState::AUTHENTICATED) {
                    response = "530 Not logged in.\r\n";
                    break;
                }
                if (cmd.arg.empty()) {
                    response = "501 Syntax error in parameters.\r\n";
                    break;
                }

                fs::path targetPath = session.getCurrentDir() / cmd.arg;
                std::error_code ec;
                if (fs::is_directory(targetPath) && fs::remove(targetPath, ec)) {
                    response = "250 Directory removed.\r\n";
                }
                else {
                    response = "550 Remove directory failed (dir may not be empty or exist).\r\n";
                }
                break;
            }

            case FTPCommand::LIST: {
                if (session.getAuthState() != AuthState::AUTHENTICATED) {
                    response = "530 Not logged in.\r\n";
                    break;
                }

                std::error_code ec;
                std::string listing;
                for (const auto& entry : fs::directory_iterator(session.getCurrentDir(), ec)) {
                    listing += entry.path().filename().string();
                    listing += entry.is_directory() ? "/\r\n" : "\r\n";
                }

                response = ec ? "550 Cannot list directory.\r\n"
                    : "212 Directory status follows.\r\n" + listing;
                break;
            }

            case FTPCommand::RETR:
            case FTPCommand::STOR:
                if (session.getAuthState() != AuthState::AUTHENTICATED) {
                    response = "530 Not logged in.\r\n";
                }
                else {
                    response = "502 Command not implemented.\r\n";
                }
                break;

            default:
                response = "500 Unknown command.\r\n";
                break;
            }

            send(clientSocket, response.c_str(), static_cast<int>(response.length()), 0);
        }
    }

    closesocket(clientSocket);
}