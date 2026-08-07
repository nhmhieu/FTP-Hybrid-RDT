#include "control/TCPServer.h"
#include "control/CommandParser.h"
#include "control/FileSystem.h"
#include "control/SessionManager.h"
#include <string>
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

void TCPServer::sendResponse(SOCKET clientSocket, const std::string& response) {
    send(clientSocket, response.c_str(), static_cast<int>(response.length()), 0);
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
        std::cout << "[+] New client connected from: " << clientIP << ":" << ntohs(clientAddr.sin_port) << std::endl;

        std::thread clientThread(&TCPServer::handleClient, this, clientSocket);
        clientThread.detach();
    }
}

void TCPServer::handleClient(SOCKET clientSocket) {
    ClientSession session(clientSocket);
    std::string pendingData;
    char buffer[1024];

    sendResponse(clientSocket, "220 Welcome to FTP Server\r\n");

    while (true) {
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);

        if (bytesReceived <= 0) {
            std::cout << "[-] Client disconnected." << std::endl;
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
                sendResponse(clientSocket, response);
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


            case FTPCommand::SIZE: {
                if (session.getAuthState() != AuthState::AUTHENTICATED) {
                    response = "530 Not logged in.\r\n";
                    break;
                }
                if (cmd.arg.empty()) {
                    response = "501 Syntax error in parameters.\r\n";
                    break;
                }
                if (FileSystem::fileExists(session.getCurrentDir(), cmd.arg)) {
                    uintmax_t fileSize = FileSystem::getFileSize(session.getCurrentDir(), cmd.arg);
                    response = "213 " + std::to_string(fileSize) + "\r\n";
                }
                else {
                    response = "550 File not found or is a directory.\r\n";
                }
                break;
            }


            case FTPCommand::DELE: {
                if (session.getAuthState() != AuthState::AUTHENTICATED) {
                    response = "530 Not logged in.\r\n";
                    break;
                }
                if (cmd.arg.empty()) {
                    response = "501 Syntax error in parameters.\r\n";
                    break;
                }
                fs::path filePath = session.getCurrentDir() / cmd.arg;
                std::error_code ec;
                if (fs::is_regular_file(filePath, ec) && fs::remove(filePath, ec)) {
                    response = "250 File deleted successfully.\r\n";
                }
                else {
                    response = "550 Delete operation failed (file missing or permission denied).\r\n";
                }
                break;
            }


            case FTPCommand::NLST: {
                if (session.getAuthState() != AuthState::AUTHENTICATED) {
                    response = "530 Not logged in.\r\n";
                    break;
                }

                fs::path targetDir = session.getCurrentDir();
                if (!cmd.arg.empty()) {
                    targetDir = targetDir / cmd.arg;
                }

                std::error_code ec;
                if (!fs::exists(targetDir, ec) || !fs::is_directory(targetDir, ec)) {
                    response = "550 Invalid directory.\r\n";
                    break;
                }

                std::string nameList = "";
                for (const auto& entry : fs::directory_iterator(targetDir, ec)) {
                    nameList += entry.path().filename().string() + "\r\n";
                }

                response = "226 Name list status follows.\r\n" + nameList;
                break;
            }


            case FTPCommand::RNFR: {
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
                if (fs::exists(targetPath, ec)) {
                    session.setRenameFrom(cmd.arg);
                    response = "350 Requested file action pending RNTO.\r\n";
                }
                else {
                    response = "550 File or directory does not exist.\r\n";
                }
                break;
            }


            case FTPCommand::RNTO: {
                if (session.getAuthState() != AuthState::AUTHENTICATED) {
                    response = "530 Not logged in.\r\n";
                    break;
                }
                if (session.getRenameFrom().empty()) {
                    response = "503 Bad sequence of commands. Call RNFR first.\r\n";
                    break;
                }
                if (cmd.arg.empty()) {
                    response = "501 Syntax error in parameters.\r\n";
                    break;
                }

                fs::path oldPath = session.getCurrentDir() / session.getRenameFrom();
                fs::path newPath = session.getCurrentDir() / cmd.arg;

                std::error_code ec;
                fs::rename(oldPath, newPath, ec);

                if (!ec) {
                    response = "250 File renamed successfully.\r\n";
                }
                else {
                    response = "550 Rename operation failed.\r\n";
                }

                session.clearRenameFrom(); // Reset trạng thái rename
                break;
            }
                                 // === LỆNH 1: MDTM (Lấy thời điểm sửa đổi file) ===
            case FTPCommand::MDTM: {
                if (session.getAuthState() != AuthState::AUTHENTICATED) {
                    response = "530 Not logged in.\r\n";
                    break;
                }
                if (cmd.arg.empty()) {
                    response = "501 Syntax error in parameters.\r\n";
                    break;
                }

                std::string mtime = FileSystem::getLastModifiedTime(session.getCurrentDir(), cmd.arg);
                if (!mtime.empty()) {
                    response = "213 " + mtime + "\r\n";
                }
                else {
                    response = "550 File not found.\r\n";
                }
                break;
            }

                                 // === LỆNH 2: STAT (Trạng thái phiên làm việc hoặc đường dẫn) ===
            case FTPCommand::STAT: {
                if (session.getAuthState() != AuthState::AUTHENTICATED) {
                    response = "530 Not logged in.\r\n";
                    break;
                }

                if (cmd.arg.empty()) {
                    // Trả về thông tin trạng thái Server & Phiên kết nối hiện tại
                    std::string endpointInfo = session.hasDataEndpoint()
                        ? (session.getDataIp() + ":" + std::to_string(session.getDataPort()))
                        : "Not set";

                    response = "211-Hybrid FTP Server Status:\r\n"
                        " Connected User: " + session.getUsername() + "\r\n"
                        " Working Dir: " + session.getCurrentDir().generic_string() + "\r\n"
                        " UDP Data Endpoint: " + endpointInfo + "\r\n"
                        "211 End of status.\r\n";
                }
                else {
                    // Trả về danh sách file qua kênh control TCP (không dùng kênh data)
                    fs::path targetPath = session.getCurrentDir() / cmd.arg;
                    std::string listing = FileSystem::getDirectoryListing(targetPath);
                    if (!listing.empty()) {
                        response = "213-Status follows:\r\n" + listing + "213 End of status.\r\n";
                    }
                    else {
                        response = "550 Could not get status for specified path.\r\n";
                    }
                }
                break;
            }

                                 // === LỆNH 3: HELP (Trợ giúp sử dụng lệnh) ===
            case FTPCommand::HELP: {
                if (cmd.arg.empty()) {
                    response = "214-Supported commands:\r\n"
                        " USER PASS QUIT NOOP PWD CWD CDUP MKD RMD LIST NLST\r\n"
                        " SIZE DELE RNFR RNTO PORT STOR RETR MDTM STAT HELP\r\n"
                        "214 Help OK.\r\n";
                }
                else {
                    std::string argUpper = cmd.arg;
                    std::transform(argUpper.begin(), argUpper.end(), argUpper.begin(), ::toupper);

                    if (argUpper == "USER") response = "214 Syntax: USER <username>\r\n";
                    else if (argUpper == "PASS") response = "214 Syntax: PASS <password>\r\n";
                    else if (argUpper == "MDTM") response = "214 Syntax: MDTM <filename> -> Returns YYYYMMDDhhmmss\r\n";
                    else if (argUpper == "STAT") response = "214 Syntax: STAT [path]\r\n";
                    else if (argUpper == "STOR") response = "214 Syntax: STOR <filename> (UDP Upload)\r\n";
                    else if (argUpper == "RETR") response = "214 Syntax: RETR <filename> (UDP Download)\r\n";
                    else response = "214 Command " + argUpper + " is supported.\r\n";
                }
                break;
            }
            default:
                response = "500 Unknown command.\r\n";
                break;
            }

            sendResponse(clientSocket, response);
        }
    }

    closesocket(clientSocket);
}