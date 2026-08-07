#include "control/TCPServer.h"
#include "control/CommandParser.h"
#include "control/FileSystem.h"
#include "control/SessionManager.h"
#include "common/ftp_api.h"
#include <iostream>
#include <thread>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;
namespace {
    constexpr int SERVER_STOR_UDP_PORT = 8081; //Tạm khai báo port UDP cho STOR
}

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
        std::cerr << "[ERROR] WSAStartup failed: " << res << std::endl;
        return false;
    }

    listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        std::cerr << "[ERROR] Cannot create socket: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return false;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "[ERROR] Bind failed: " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return false;
    }

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "[ERROR] Listen failed: " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return false;
    }

    std::cout << "[SERVER] Start up succeeded on port " << port << std::endl;
    return true;
}

void TCPServer::acceptClients() {
    std::cout << "[SERVER] Waiting for client to connect..." << std::endl;

    while (true) {
        sockaddr_in clientAddr;
        int clientAddrSize = sizeof(clientAddr);

        SOCKET clientSocket = accept(listenSocket, (sockaddr*)&clientAddr, &clientAddrSize);

        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "[ERROR] Accept failed: " << WSAGetLastError() << std::endl;
            continue;
        }

        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
        std::cout << "[+] New client connnected from: " << clientIP << ":" << ntohs(clientAddr.sin_port) << std::endl;

        std::thread clientThread(&TCPServer::handleClient, this, clientSocket);
        clientThread.detach();
    }
}

void TCPServer::handleClient(SOCKET clientSocket) {
    ClientSession session(clientSocket);
    std::string pendingData;
    char buffer[1024];

    // Helper gửi đầy đủ response qua TCP.
    // Đặt local ở đây nên KHÔNG cần khai báo trong TCPServer.h.
    auto sendAll = [clientSocket](const std::string& message) -> bool {
        int totalSent = 0;
        const int messageLength =
            static_cast<int>(message.size());

        while (totalSent < messageLength) {
            int sent = send(
                clientSocket,
                message.c_str() + totalSent,
                messageLength - totalSent,
                0
            );

            if (sent == SOCKET_ERROR || sent == 0) {
                return false;
            }

            totalSent += sent;
        }

        return true;
    };

    // Welcome message
    if (!sendAll("220 Welcome to FTP Server\r\n")) {
        closesocket(clientSocket);
        return;
    }

    while (true) {
        int bytesReceived =
            recv(clientSocket, buffer, sizeof(buffer), 0);

        if (bytesReceived <= 0) {
            std::cout
                << "[-] Client disconnected."
                << std::endl;
            break;
        }

        pendingData.append(buffer, bytesReceived);

        std::size_t endPos;

        // TCP framing: mỗi command kết thúc bằng \r\n
        while ((endPos = pendingData.find("\r\n"))
            != std::string::npos) {

            std::string commandLine =
                pendingData.substr(0, endPos);

            pendingData.erase(0, endPos + 2);

            ParsedCommand cmd =
                CommandParser::parse(commandLine);

            std::string response;

            switch (cmd.command) {

            // =========================
            // USER
            // =========================
            case FTPCommand::USER: {
                if (cmd.arg.empty()) {
                    response =
                        "501 Missing username.\r\n";
                }
                else {
                    session.setUsername(cmd.arg);

                    session.setAuthState(
                        AuthState::WAITING_FOR_PASS
                    );

                    response =
                        "331 Username OK, need password.\r\n";
                }

                break;
            }

            // =========================
            // PASS
            // =========================
            case FTPCommand::PASS: {
                if (session.getAuthState()
                    != AuthState::WAITING_FOR_PASS) {

                    response =
                        "503 Login with USER first.\r\n";
                }
                else if (
                    session.getUsername() == "admin" &&
                    cmd.arg == "123"
                ) {
                    session.setAuthState(
                        AuthState::AUTHENTICATED
                    );

                    response =
                        "230 Login successful.\r\n";
                }
                else {
                    session.setAuthState(
                        AuthState::UNAUTHENTICATED
                    );

                    response =
                        "530 Login incorrect.\r\n";
                }

                break;
            }

            // =========================
            // NOOP
            // =========================
            case FTPCommand::NOOP: {
                response = "200 NOOP ok.\r\n";
                break;
            }

            // =========================
            // QUIT
            // =========================
            case FTPCommand::QUIT: {
                sendAll("221 Goodbye.\r\n");
                closesocket(clientSocket);
                return;
            }

            // =========================
            // PWD
            // =========================
            case FTPCommand::PWD: {
                if (session.getAuthState()
                    != AuthState::AUTHENTICATED) {

                    response =
                        "530 Not logged in.\r\n";
                    break;
                }

                response =
                    "257 \"" +
                    session.getCurrentDir().generic_string() +
                    "\" is current directory.\r\n";

                break;
            }

            // =========================
            // CWD
            // =========================
            case FTPCommand::CWD: {
                if (session.getAuthState()
                    != AuthState::AUTHENTICATED) {

                    response =
                        "530 Not logged in.\r\n";
                    break;
                }

                if (cmd.arg.empty()) {
                    response =
                        "501 Syntax error in parameters.\r\n";
                    break;
                }

                fs::path targetPath =
                    fs::path(cmd.arg);

                if (targetPath.is_relative()) {
                    targetPath =
                        session.getCurrentDir() /
                        targetPath;
                }

                std::error_code ec;

                targetPath =
                    fs::canonical(targetPath, ec);

                if (!ec &&
                    fs::exists(targetPath) &&
                    fs::is_directory(targetPath)) {

                    session.setCurrentDir(targetPath);

                    response =
                        "250 Directory successfully changed.\r\n";
                }
                else {
                    response =
                        "550 Failed to change directory.\r\n";
                }

                break;
            }

            // =========================
            // CDUP
            // =========================
            case FTPCommand::CDUP: {
                if (session.getAuthState()
                    != AuthState::AUTHENTICATED) {

                    response =
                        "530 Not logged in.\r\n";
                    break;
                }

                if (session.getCurrentDir()
                    .has_parent_path()) {

                    session.setCurrentDir(
                        session.getCurrentDir()
                        .parent_path()
                    );

                    response =
                        "200 Directory changed to parent.\r\n";
                }
                else {
                    response =
                        "550 Cannot move above root.\r\n";
                }

                break;
            }

            // =========================
            // MKD
            // =========================
            case FTPCommand::MKD: {
                if (session.getAuthState()
                    != AuthState::AUTHENTICATED) {

                    response =
                        "530 Not logged in.\r\n";
                    break;
                }

                if (cmd.arg.empty()) {
                    response =
                        "501 Syntax error in parameters.\r\n";
                    break;
                }

                fs::path targetPath =
                    session.getCurrentDir() /
                    cmd.arg;

                std::error_code ec;

                if (fs::create_directory(
                        targetPath,
                        ec
                    )) {

                    response =
                        "257 \"" +
                        cmd.arg +
                        "\" directory created.\r\n";
                }
                else {
                    response =
                        "550 Create directory failed.\r\n";
                }

                break;
            }

            // =========================
            // RMD
            // =========================
            case FTPCommand::RMD: {
                if (session.getAuthState()
                    != AuthState::AUTHENTICATED) {

                    response =
                        "530 Not logged in.\r\n";
                    break;
                }

                if (cmd.arg.empty()) {
                    response =
                        "501 Syntax error in parameters.\r\n";
                    break;
                }

                fs::path targetPath =
                    session.getCurrentDir() /
                    cmd.arg;

                std::error_code ec;

                if (fs::is_directory(targetPath) &&
                    fs::remove(targetPath, ec)) {

                    response =
                        "250 Directory removed.\r\n";
                }
                else {
                    response =
                        "550 Remove directory failed "
                        "(dir may not be empty or exist).\r\n";
                }

                break;
            }

            // =========================
            // LIST
            // =========================
            case FTPCommand::LIST: {
                if (session.getAuthState()
                    != AuthState::AUTHENTICATED) {

                    response =
                        "530 Not logged in.\r\n";
                    break;
                }

                std::error_code ec;
                std::string listing;

                for (const auto& entry :
                     fs::directory_iterator(
                         session.getCurrentDir(),
                         ec
                     )) {

                    listing +=
                        entry.path()
                        .filename()
                        .string();

                    listing +=
                        entry.is_directory()
                        ? "/\r\n"
                        : "\r\n";
                }

                if (ec) {
                    response =
                        "550 Cannot list directory.\r\n";
                }
                else {
                    response =
                        "212 Directory status follows.\r\n" +
                        listing;
                }

                break;
            }

            // =========================
            // RETR - chưa làm
            // =========================
            case FTPCommand::RETR: {
                if (session.getAuthState()
                    != AuthState::AUTHENTICATED) {

                    response =
                        "530 Not logged in.\r\n";
                }
                else {
                    response =
                        "502 Command not implemented.\r\n";
                }

                break;
            }

            // =========================
            // STOR - Upload bằng UDP
            // =========================
            case FTPCommand::STOR: {
                if (session.getAuthState()
                    != AuthState::AUTHENTICATED) {

                    response =
                        "530 Not logged in.\r\n";
                    break;
                }

                if (cmd.arg.empty()) {
                    response =
                        "501 Syntax error in parameters.\r\n";
                    break;
                }

                fs::path requestedFile(cmd.arg);

                if (requestedFile.is_absolute() ||
                    requestedFile.has_parent_path()) {

                    response =
                        "550 Invalid file path.\r\n";
                    break;
                }

                fs::path savePath =
                    session.getCurrentDir() /
                    requestedFile.filename();

                // 150 phải gửi ngay trước khi chờ UDP
                if (!sendAll(
                    "150 Opening UDP data connection "
                    "for file upload.\r\n"
                )) {
                    closesocket(clientSocket);
                    return;
                }

                std::cout
                    << "[STOR] Receiving file: "
                    << savePath.string()
                    << " on UDP port "
                    << SERVER_STOR_UDP_PORT
                    << std::endl;

                bool success =
                    UDPData::receiveFile(
                        savePath.string(),
                        SERVER_STOR_UDP_PORT
                    );

                if (success) {
                    response =
                        "226 Transfer complete.\r\n";

                    std::cout
                        << "[STOR] Upload completed: "
                        << savePath.string()
                        << std::endl;
                }
                else {
                    response =
                        "426 Connection closed; "
                        "transfer aborted.\r\n";

                    std::cout
                        << "[STOR] Upload failed."
                        << std::endl;
                }

                break;
            }

            // =========================
            // UNKNOWN
            // =========================
            default: {
                response =
                    "500 Unknown command.\r\n";
                break;
            }

            } // end switch

            // Gửi response cuối cùng
            if (!response.empty()) {
                if (!sendAll(response)) {
                    std::cout
                        << "[-] Failed to send response."
                        << std::endl;

                    closesocket(clientSocket);
                    return;
                }
            }

        } // end command framing loop

    } // end recv loop

    closesocket(clientSocket);
}