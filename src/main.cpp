#include "control/TCPServer.h"
#include "control/TCPClient.h"

#include <iostream>
#include <string>
#include <sstream>
#include <filesystem>

#pragma comment(lib, "Ws2_32.lib")

namespace fs = std::filesystem;


// ========================================
// Run server
// ========================================
int runServer(int port) {
    std::cout
        << "========================================\n"
        << "      HYBRID FTP SERVER - STARTING      \n"
        << "========================================\n";

    TCPServer server(port);

    if (!server.start()) {
        std::cerr
            << "[-] Failed to start FTP Server!"
            << std::endl;

        return 1;
    }

    std::cout
        << "[+] Server Listening on port "
        << port
        << "..."
        << std::endl;

    std::cout
        << "[+] Waiting for incoming connections..."
        << std::endl;

    server.acceptClients();

    return 0;
}


// ========================================
// Run client
// ========================================
int runClient(
    const std::string& serverIP,
    int serverPort
) {
    constexpr int CLIENT_RETR_UDP_PORT = 8082;

    TCPClient client;

    std::cout
        << "========================================\n"
        << "      HYBRID FTP CLIENT - STARTING      \n"
        << "========================================\n";

    if (!client.connectToServer(
            serverIP,
            serverPort
        )) {

        std::cerr
            << "[-] Cannot connect to server "
            << serverIP
            << ":"
            << serverPort
            << std::endl;

        return 1;
    }

    // Read welcome message: 220
    std::string welcome =
        client.receiveData();

    if (!welcome.empty()) {
        std::cout << welcome;
    }

    std::cout
        << "\nCommands:\n"
        << "  USER admin\n"
        << "  PASS 123\n"
        << "  PWD\n"
        << "  LIST\n"
        << "  STOR <local-file-path>\n"
        << "  QUIT\n"
        << std::endl;

    while (true) {
        std::cout << "ftp> ";

        std::string input;

        if (!std::getline(std::cin, input)) {
            break;
        }

        if (input.empty()) {
            continue;
        }

        // ====================================
        // Special handling for STOR
        // ====================================
        std::istringstream iss(input);

        std::string command;
        iss >> command;

        if (command == "ABOR") {
            if (client.isTransferRunning()) {
                client.cancelTransfer();
            } else {
                if (client.sendData("ABOR\r\n")) {
                    std::string response = client.receiveData();
                    if (!response.empty()) std::cout << response;
                }
            }
            continue;
        }

        if (client.isTransferRunning()) {
            std::cout << "[CLIENT] Transfer in progress. Use ABOR to cancel.\n";
            continue;
        }

        if (command == "PASV") {
            std::cout << (client.enterPassiveMode() ? "[CLIENT] Passive mode selected.\n" :
                "[CLIENT] PASV failed.\n");
            continue;
        }

        if (command == "PORT") {
            std::string portArgs;
            std::getline(iss, portArgs);

            // Xóa khoảng trắng thừa ở đầu chuỗi
            if (!portArgs.empty() && portArgs.front() == ' ') {
                portArgs.erase(0, 1);
            }

            std::cout << (client.enterActiveMode(portArgs) 
                ? "[CLIENT] Active mode set.\n" 
                : "[CLIENT] PORT failed.\n");
            continue;
        }

        if (command == "TYPE") {
            std::string type;
            iss >> type;
            std::cout << (client.setTransferType(type) ? "[CLIENT] Transfer type updated.\n" :
                "[CLIENT] TYPE failed.\n");
            continue;
        }
        if (command == "MODE") {
            std::string mode; iss >> mode;
            std::cout << (client.setTransferMode(mode) ? "[CLIENT] Transfer mode updated.\n" : "[CLIENT] MODE failed.\n");
            continue;
        }

        if (command == "STOR" || command == "STOU" || command == "APPE") {
            std::string localFilePath;

            std::getline(iss, localFilePath);

            // Remove first space
            if (!localFilePath.empty() &&
                localFilePath.front() == ' ') {

                localFilePath.erase(0, 1);
            }

            if (localFilePath.empty()) {
                std::cout
                    << "Usage: " << command << " <local-file-path>"
                    << std::endl;

                continue;
            }

            fs::path localPath(localFilePath);

            // Server only receives the filename,
            // not the client's local directory.
            std::string remoteFileName =
                localPath.filename().string();

            client.startUploadAsync(
                localFilePath,
                remoteFileName,
                command
            );

            continue;
        }

        if (command == "RETR") {
            std::string remoteFileName;
            iss >> remoteFileName;
            if (remoteFileName.empty()) {
                std::cout << "Usage: RETR <remote-file-name>" << std::endl;
                continue;
            }

            const fs::path downloadDir = fs::current_path() / "temp_downloads";
            std::error_code ec;
            fs::create_directories(downloadDir, ec);
            if (ec) {
                std::cerr << "[-] Cannot create temp_downloads directory." << std::endl;
                continue;
            }

            const fs::path localPath = downloadDir / fs::path(remoteFileName).filename();
            client.startDownloadAsync(
                remoteFileName, localPath.string(), "", CLIENT_RETR_UDP_PORT);
            continue;
        }

        // ====================================
        // Normal TCP commands
        // ====================================
        if (!client.sendData(
                input + "\r\n"
            )) {

            std::cerr
                << "[-] Failed to send command."
                << std::endl;

            break;
        }

        std::string response =
            client.receiveData();

        if (response.empty()) {
            std::cerr
                << "[-] Server disconnected."
                << std::endl;

            break;
        }

        std::cout << response;

        if (command == "QUIT") {
            break;
        }
    }

    client.disconnect();

    return 0;
}


// ========================================
// Main
// ========================================
int main(int argc, char* argv[]) {
    constexpr int DEFAULT_TCP_PORT = 8080;

    /*
     * No argument:
     *     hybrid_ftp.exe
     *
     * Keep old behavior -> run server.
     */
    if (argc == 1) {
        return runServer(DEFAULT_TCP_PORT);
    }

    std::string mode = argv[1];

    // ====================================
    // SERVER MODE
    // ====================================
    if (mode == "server") {
        return runServer(DEFAULT_TCP_PORT);
    }

    // ====================================
    // CLIENT MODE
    // ====================================
    if (mode == "client") {
        std::string serverIP = "127.0.0.1";

        if (argc >= 3) {
            serverIP = argv[2];
        }

        return runClient(
            serverIP,
            DEFAULT_TCP_PORT
        );
    }

    std::cout
        << "Usage:\n"
        << "  hybrid_ftp.exe server\n"
        << "  hybrid_ftp.exe client [server-ip]\n";

    return 1;
}
