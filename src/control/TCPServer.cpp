#include "control/TCPServer.h"
#include "control/CommandParser.h"
#include "control/FileSystem.h"
#include "control/SessionManager.h"
#include "common/ftp_api.h"
#include "common/protocol.h"
#include "common/checksum.h"
#include "common/representation.h"
#include <iostream>
#include <thread>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <array>
#include <sstream>

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
            bool uniqueStore = false;
            std::string uniqueStoreName;
            if (cmd.command == FTPCommand::STOU) {
                uniqueStore = true;
                fs::path requested = cmd.arg.empty() ? fs::path("unique") : fs::path(cmd.arg).filename();
                std::string stem = requested.stem().string();
                std::string extension = requested.extension().string();
                if (stem.empty()) stem = "unique";
                fs::path candidate = session.getCurrentDir() / (stem + extension);
                unsigned suffix = 1;
                std::error_code uniqueEc;
                while (fs::exists(candidate, uniqueEc)) {
                    candidate = session.getCurrentDir() /
                        (stem + "_" + std::to_string(suffix++) + extension);
                }
                uniqueStoreName = candidate.filename().string();
                cmd.command = FTPCommand::STOR;
                cmd.arg = uniqueStoreName;
            }

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

            case FTPCommand::PORT: {
                if (session.getAuthState() != AuthState::AUTHENTICATED) {
                    response = "530 Not logged in.\r\n";
                    break;
                }

                std::array<int, 6> parts{};
                std::istringstream input(cmd.arg);
                std::string token;
                bool valid = !cmd.arg.empty() &&
                    std::count(cmd.arg.begin(), cmd.arg.end(), ',') == 5;
                for (std::size_t i = 0; i < parts.size() && valid; ++i) {
                    if (!std::getline(input, token, ',') || token.empty()) {
                        valid = false;
                        break;
                    }
                    try {
                        std::size_t used = 0;
                        parts[i] = std::stoi(token, &used);
                        valid = used == token.size() && parts[i] >= 0 && parts[i] <= 255;
                    }
                    catch (...) {
                        valid = false;
                    }
                }
                if (std::getline(input, token, ',') || !valid) {
                    response = "501 Invalid PORT syntax.\r\n";
                    break;
                }

                const int dataPort = parts[4] * 256 + parts[5];
                if (dataPort <= 0) {
                    response = "501 Invalid PORT syntax.\r\n";
                    break;
                }

                const std::string dataIP =
                    std::to_string(parts[0]) + "." + std::to_string(parts[1]) + "." +
                    std::to_string(parts[2]) + "." + std::to_string(parts[3]);
                session.setDataEndpoint(dataIP, dataPort);
                response = "200 PORT command successful.\r\n";
                break;
            }

            case FTPCommand::PASV: {
                if (session.getAuthState() != AuthState::AUTHENTICATED) {
                    response = "530 Not logged in.\r\n";
                    break;
                }
                SOCKET passive = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
                sockaddr_in address{};
                address.sin_family = AF_INET;
                address.sin_addr.s_addr = htonl(INADDR_ANY);
                address.sin_port = 0;
                if (passive == INVALID_SOCKET || bind(passive,
                    reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
                    if (passive != INVALID_SOCKET) closesocket(passive);
                    response = "425 Cannot open passive data endpoint.\r\n";
                    break;
                }
                int addressLength = sizeof(address);
                getsockname(passive, reinterpret_cast<sockaddr*>(&address), &addressLength);
                sockaddr_in controlAddress{};
                int controlLength = sizeof(controlAddress);
                getsockname(clientSocket, reinterpret_cast<sockaddr*>(&controlAddress), &controlLength);
                char ipText[INET_ADDRSTRLEN]{};
                inet_ntop(AF_INET, &controlAddress.sin_addr, ipText, sizeof(ipText));
                const int passivePort = ntohs(address.sin_port);
                session.setPassiveEndpoint(ipText, passivePort, passive);
                std::string tuple = ipText;
                std::replace(tuple.begin(), tuple.end(), '.', ',');
                response = "227 Entering Passive Mode (" + tuple + "," +
                    std::to_string(passivePort / 256) + "," +
                    std::to_string(passivePort % 256) + ").\r\n";
                break;
            }

            case FTPCommand::TYPE: {
                if (session.getAuthState() != AuthState::AUTHENTICATED) {
                    response = "530 Not logged in.\r\n";
                } else if (cmd.arg == "I" || cmd.arg == "i") {
                    session.setTransferType(TransferType::BINARY);
                    response = "200 Type set to I.\r\n";
                } else if (cmd.arg == "A" || cmd.arg == "a") {
                    session.setTransferType(TransferType::ASCII);
                    response = "200 Type set to A.\r\n";
                } else response = "504 Unsupported TYPE.\r\n";
                break;
            }
            case FTPCommand::MODE: {
                if (session.getAuthState() != AuthState::AUTHENTICATED) response = "530 Not logged in.\r\n";
                else if (cmd.arg == "S" || cmd.arg == "s") { session.setTransferMode(TransferMode::STREAM); response = "200 Mode set to S.\r\n"; }
                else if (cmd.arg == "B" || cmd.arg == "b") { session.setTransferMode(TransferMode::BLOCK); response = "200 Mode set to B.\r\n"; }
                else if (cmd.arg == "C" || cmd.arg == "c") { session.setTransferMode(TransferMode::COMPRESSED); response = "200 Mode set to C.\r\n"; }
                else response = "504 Unsupported MODE.\r\n";
                break;
            }

            // =========================
            // RETR - active UDP download
            // =========================
            case FTPCommand::RETR: {
                if (session.getAuthState()
                    != AuthState::AUTHENTICATED) {

                    response =
                        "530 Not logged in.\r\n";
                    break;
                }

                if (cmd.arg.empty()) {
                    response = "501 Syntax error in parameters.\r\n";
                    break;
                }

                fs::path requestedFile(cmd.arg);
                if (requestedFile.is_absolute() || requestedFile.has_parent_path()) {
                    response = "550 Invalid file path.\r\n";
                    break;
                }
                if (!session.hasDataEndpoint()) {
                    response = "425 Use PORT first.\r\n";
                    break;
                }

                const fs::path filePath = session.getCurrentDir() / requestedFile.filename();
                std::error_code ec;
                if (!fs::is_regular_file(filePath, ec)) {
                    response = "550 File not found.\r\n";
                    break;
                }

                std::string destinationIP = session.getDataIp();
                int destinationPort = session.getDataPort();
                if (session.getDataMode() == DataMode::PASSIVE) {
                    SOCKET registrationSocket = session.getPassiveSocket();
                    DWORD timeout = 3000;
                    setsockopt(registrationSocket, SOL_SOCKET, SO_RCVTIMEO,
                        reinterpret_cast<const char*>(&timeout), sizeof(timeout));
                    udp_header_t registration{};
                    sockaddr_in source{};
                    int sourceLength = sizeof(source);
                    const int received = recvfrom(registrationSocket,
                        reinterpret_cast<char*>(&registration), sizeof(registration), 0,
                        reinterpret_cast<sockaddr*>(&source), &sourceLength);
                    sockaddr_in controlPeer{};
                    int peerLength = sizeof(controlPeer);
                    getpeername(clientSocket, reinterpret_cast<sockaddr*>(&controlPeer), &peerLength);
                    const bool validRegistration = received == sizeof(registration) &&
                        ntohs(registration.magic) == MAGIC_NUMBER &&
                        verify_checksum(reinterpret_cast<const std::uint8_t*>(&registration), sizeof(registration)) &&
                        registration.flags == FLAG_ACK && ntohs(registration.payloadLen) == 0 &&
                        ntohl(registration.seq) == 0 && ntohl(registration.ack) == 0 &&
                        source.sin_addr.s_addr == controlPeer.sin_addr.s_addr;
                    session.closePassiveSocket();
                    if (!validRegistration) {
                        session.clearDataEndpoint();
                        response = "425 Passive UDP registration timed out or invalid.\r\n";
                        break;
                    }
                    char learnedIP[INET_ADDRSTRLEN]{};
                    inet_ntop(AF_INET, &source.sin_addr, learnedIP, sizeof(learnedIP));
                    destinationIP = learnedIP;
                    destinationPort = ntohs(source.sin_port);
                }

                if (!sendAll("150 Opening UDP data connection for file download.\r\n")) {
                    closesocket(clientSocket);
                    return;
                }

                std::cout << "[RETR] Sending " << filePath.string() << " to "
                          << destinationIP << ":" << destinationPort << std::endl;
                fs::path sendPath = filePath;
                fs::path asciiTemp;
                bool prepared = true;
                if (session.getTransferType() == TransferType::ASCII) {
                    asciiTemp = fs::temp_directory_path() /
                        ("hybrid_ftp_ascii_" + std::to_string(reinterpret_cast<std::uintptr_t>(&session)) + ".tmp");
                    prepared = Representation::toAsciiWire(filePath, asciiTemp);
                    sendPath = asciiTemp;
                }
                fs::path modeTemp;
                if (prepared && session.getTransferMode() != TransferMode::STREAM) {
                    modeTemp = fs::temp_directory_path() /
                        ("hybrid_ftp_mode_" + std::to_string(reinterpret_cast<std::uintptr_t>(&session)) + ".tmp");
                    prepared = session.getTransferMode() == TransferMode::BLOCK
                        ? Representation::encodeBlock(sendPath, modeTemp)
                        : Representation::encodeRle(sendPath, modeTemp);
                    sendPath = modeTemp;
                }
                const bool success = prepared && UDPData::sendFile(
                    sendPath.string(), destinationIP, destinationPort);
                if (!asciiTemp.empty()) { std::error_code cleanup; fs::remove(asciiTemp, cleanup); }
                if (!modeTemp.empty()) { std::error_code cleanup; fs::remove(modeTemp, cleanup); }
                if (session.getDataMode() == DataMode::PASSIVE) session.clearDataEndpoint();
                response = success
                    ? "226 Transfer complete.\r\n"
                    : "426 Connection closed; transfer aborted.\r\n";

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
                const std::string preliminary = uniqueStore
                    ? "150 FILE: " + uniqueStoreName + "; opening UDP data connection.\r\n"
                    : "150 Opening UDP data connection for file upload.\r\n";
                if (!sendAll(preliminary)) {
                    closesocket(clientSocket);
                    return;
                }

                std::cout
                    << "[STOR] Receiving file: "
                    << savePath.string()
                    << " on UDP port "
                    << SERVER_STOR_UDP_PORT
                    << std::endl;

                int receivePort = SERVER_STOR_UDP_PORT;
                if (session.getDataMode() == DataMode::PASSIVE) {
                    receivePort = session.getDataPort();
                    session.closePassiveSocket();
                }
                bool success =
                    UDPData::receiveFile(
                        savePath.string(),
                        receivePort
                    );
                if (session.getDataMode() == DataMode::PASSIVE) session.clearDataEndpoint();

                if (success && session.getTransferMode() == TransferMode::BLOCK)
                    success = Representation::decodeBlock(savePath);
                else if (success && session.getTransferMode() == TransferMode::COMPRESSED)
                    success = Representation::decodeRle(savePath);
                if (success && session.getTransferType() == TransferType::ASCII) {
                    success = Representation::fromAsciiWire(savePath);
                }
                if (success) {
                    response = uniqueStore
                        ? "226 Transfer complete; FILE: " + uniqueStoreName + ".\r\n"
                        : "226 Transfer complete.\r\n";

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
// NLST
// =========================
case FTPCommand::NLST: {
    if (session.getAuthState()
        != AuthState::AUTHENTICATED) {

        response = "530 Not logged in.\r\n";
        break;
    }

    fs::path targetDir =
        session.getCurrentDir();

    if (!cmd.arg.empty()) {
        targetDir /= cmd.arg;
    }

    std::error_code ec;

    if (!fs::exists(targetDir, ec) ||
        !fs::is_directory(targetDir, ec)) {

        response = "550 Invalid directory.\r\n";
        break;
    }

    std::string nameList;

    for (const auto& entry :
         fs::directory_iterator(targetDir, ec)) {

        nameList +=
            entry.path().filename().string();

        if (entry.is_directory()) {
            nameList += "/";
        }

        nameList += "\r\n";
    }

    if (ec) {
        response =
            "550 Cannot list directory.\r\n";
    }
    else {
        response =
            "212 Name list follows.\r\n" +
            nameList;
    }

    break;
}


// =========================
// SIZE
// =========================
case FTPCommand::SIZE: {
    if (session.getAuthState()
        != AuthState::AUTHENTICATED) {

        response = "530 Not logged in.\r\n";
        break;
    }

    if (cmd.arg.empty()) {
        response =
            "501 Syntax error in parameters.\r\n";
        break;
    }

    if (!FileSystem::fileExists(
            session.getCurrentDir(),
            cmd.arg
        )) {

        response =
            "550 File not found or is a directory.\r\n";
        break;
    }

    uintmax_t fileSize =
        FileSystem::getFileSize(
            session.getCurrentDir(),
            cmd.arg
        );

    response =
        "213 " +
        std::to_string(fileSize) +
        "\r\n";

    break;
}


// =========================
// MDTM
// =========================
case FTPCommand::MDTM: {
    if (session.getAuthState()
        != AuthState::AUTHENTICATED) {

        response = "530 Not logged in.\r\n";
        break;
    }

    if (cmd.arg.empty()) {
        response =
            "501 Syntax error in parameters.\r\n";
        break;
    }

    std::string modifiedTime =
        FileSystem::getLastModifiedTime(
            session.getCurrentDir(),
            cmd.arg
        );

    if (modifiedTime.empty()) {
        response =
            "550 File not found.\r\n";
    }
    else {
        response =
            "213 " +
            modifiedTime +
            "\r\n";
    }

    break;
}


// =========================
// STAT
// =========================
case FTPCommand::STAT: {
    if (session.getAuthState()
        != AuthState::AUTHENTICATED) {

        response = "530 Not logged in.\r\n";
        break;
    }

    // STAT without argument:
    // return session/server status.
    if (cmd.arg.empty()) {

        std::string endpointInfo =
            session.hasDataEndpoint()
            ? (
                session.getDataIp() +
                ":" +
                std::to_string(
                    session.getDataPort()
                )
              )
            : "Not set";

        response =
            "211-Hybrid FTP Server Status:\r\n"
            " User: " +
            session.getUsername() +
            "\r\n"
            " Working Directory: " +
            session.getCurrentDir().generic_string() +
            "\r\n"
            " UDP Data Endpoint: " +
            endpointInfo +
            "\r\n"
            "211 End of status.\r\n";

        break;
    }

    fs::path targetPath =
        session.getCurrentDir() /
        cmd.arg;

    std::error_code ec;

    if (!fs::exists(targetPath, ec)) {
        response =
            "550 Path unavailable.\r\n";
        break;
    }

    // File metadata
    if (fs::is_regular_file(targetPath, ec)) {

        uintmax_t size =
            fs::file_size(targetPath, ec);

        if (ec) {
            response =
                "550 Cannot read file metadata.\r\n";
            break;
        }

        std::string modifiedTime =
            FileSystem::getLastModifiedTime(
                session.getCurrentDir(),
                cmd.arg
            );

        response =
            "213-File status follows:\r\n"
            " Name: " +
            targetPath.filename().string() +
            "\r\n"
            " Type: file\r\n"
            " Size: " +
            std::to_string(size) +
            "\r\n"
            " Modified: " +
            modifiedTime +
            "\r\n"
            "213 End of status.\r\n";

        break;
    }

    // Directory metadata/listing
    if (fs::is_directory(targetPath, ec)) {

        std::string listing =
            FileSystem::getDirectoryListing(
                targetPath
            );

        response =
            "213-Directory status follows:\r\n" +
            listing +
            "213 End of status.\r\n";

        break;
    }

    response =
        "550 Unsupported path type.\r\n";

    break;
}


// =========================
// DELE
// =========================
case FTPCommand::DELE: {
    if (session.getAuthState()
        != AuthState::AUTHENTICATED) {

        response = "530 Not logged in.\r\n";
        break;
    }

    if (cmd.arg.empty()) {
        response =
            "501 Syntax error in parameters.\r\n";
        break;
    }

    fs::path filePath =
        session.getCurrentDir() /
        cmd.arg;

    std::error_code ec;

    if (fs::is_regular_file(filePath, ec) &&
        fs::remove(filePath, ec)) {

        response =
            "250 File deleted successfully.\r\n";
    }
    else {
        response =
            "550 Delete operation failed.\r\n";
    }

    break;
}


// =========================
// RNFR
// =========================
case FTPCommand::RNFR: {
    if (session.getAuthState()
        != AuthState::AUTHENTICATED) {

        response = "530 Not logged in.\r\n";
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

    if (!fs::exists(targetPath, ec)) {
        response =
            "550 File or directory does not exist.\r\n";
        break;
    }

    session.setRenameFrom(cmd.arg);

    response =
        "350 Requested file action pending RNTO.\r\n";

    break;
}


// =========================
// RNTO
// =========================
case FTPCommand::RNTO: {
    if (session.getAuthState()
        != AuthState::AUTHENTICATED) {

        response = "530 Not logged in.\r\n";
        break;
    }

    if (session.getRenameFrom().empty()) {
        response =
            "503 Bad sequence of commands. "
            "Use RNFR first.\r\n";
        break;
    }

    if (cmd.arg.empty()) {
        response =
            "501 Syntax error in parameters.\r\n";
        break;
    }

    fs::path oldPath =
        session.getCurrentDir() /
        session.getRenameFrom();

    fs::path newPath =
        session.getCurrentDir() /
        cmd.arg;

    std::error_code ec;

    fs::rename(
        oldPath,
        newPath,
        ec
    );

    session.clearRenameFrom();

    if (ec) {
        response =
            "550 Rename operation failed.\r\n";
    }
    else {
        response =
            "250 File renamed successfully.\r\n";
    }

    break;
}


// =========================
// HELP
// =========================
case FTPCommand::HELP: {
    if (cmd.arg.empty()) {

        response =
            "214-Supported commands:\r\n"
            " USER PASS QUIT NOOP\r\n"
            " PWD CWD CDUP MKD RMD LIST NLST\r\n"
            " SIZE MDTM STAT DELE RNFR RNTO\r\n"
            " TYPE MODE PORT PASV\r\n"
            " RETR STOR STOU APPE\r\n"
            " HASH ABOR HELP\r\n"
            "214 End of HELP.\r\n";

        break;
    }

    std::string argUpper =
        cmd.arg;

    std::transform(
        argUpper.begin(),
        argUpper.end(),
        argUpper.begin(),
        [](unsigned char c) {
            return static_cast<char>(
                std::toupper(c)
            );
        }
    );

    if (argUpper == "NLST") {
        response =
            "214 Syntax: NLST [path]\r\n";
    }
    else if (argUpper == "SIZE") {
        response =
            "214 Syntax: SIZE <filename>\r\n";
    }
    else if (argUpper == "MDTM") {
        response =
            "214 Syntax: MDTM <filename>\r\n";
    }
    else if (argUpper == "STAT") {
        response =
            "214 Syntax: STAT [path]\r\n";
    }
    else if (argUpper == "DELE") {
        response =
            "214 Syntax: DELE <filename>\r\n";
    }
    else if (argUpper == "RNFR") {
        response =
            "214 Syntax: RNFR <old-name>\r\n";
    }
    else if (argUpper == "RNTO") {
        response =
            "214 Syntax: RNTO <new-name>\r\n";
    }
    else if (argUpper == "HELP") {
        response =
            "214 Syntax: HELP [command]\r\n";
    }
    else if (argUpper == "STOR") {
        response =
            "214 Syntax: STOR <filename>\r\n";
    }
    else if (argUpper == "RETR") {
        response =
            "214 Syntax: RETR <filename>\r\n";
    }
    else {
        response =
            "214 Command " +
            argUpper +
            " is supported.\r\n";
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
