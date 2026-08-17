#include "control/TCPClient.h"
#include "common/ftp_api.h"
#include "common/representation.h"
#include "common/DataMode.h"

#include <iostream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <atomic>
#include <algorithm>
#include <array>
#include <sstream>
#include <cctype>

namespace fs = std::filesystem;

// ========================================
// Helper Private: Dọn dẹp Data Channel
// ========================================
void TCPClient::cleanupDataChannel() {
    this->dataMode = DataMode::NONE;
    this->passiveIP.clear();
    this->passivePort = 0;
    this->activeIP.clear();
    this->activePort = 0;
}

// ========================================
// Constructor & Destructor
// ========================================
TCPClient::TCPClient()
    : clientSocket(INVALID_SOCKET),
      isConnected(false), dataMode(DataMode::NONE), passivePort(0), activePort(0), asciiType(false), transferMode('S') {

    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cerr << "[-] WSAStartup failed with error: " << iResult << std::endl;
    }
}

TCPClient::~TCPClient() {
    disconnect();
    WSACleanup();
}

// ========================================
// Connect to TCP server
// ========================================
bool TCPClient::connectToServer(const std::string& ipAddress, int port) {
    clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "[-] Cannot create TCP socket." << std::endl;
        return false;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (inet_pton(AF_INET, ipAddress.c_str(), &serverAddr.sin_addr) != 1) {
        std::cerr << "[-] Invalid server IP address." << std::endl;
        closesocket(clientSocket);
        clientSocket = INVALID_SOCKET;
        return false;
    }

    if (connect(clientSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "[-] TCP connection failed. Error=" << WSAGetLastError() << std::endl;
        closesocket(clientSocket);
        clientSocket = INVALID_SOCKET;
        return false;
    }

    isConnected = true;
    std::cout << "[CLIENT] Connected to " << ipAddress << ":" << port << std::endl;
    return true;
}

// ========================================
// Send & Receive TCP Data
// ========================================
bool TCPClient::sendData(const std::string& data) {
    if (!isConnected) return false;

    int totalSent = 0;
    const int dataLength = static_cast<int>(data.size());

    while (totalSent < dataLength) {
        int bytesSent = send(clientSocket, data.c_str() + totalSent, dataLength - totalSent, 0);
        if (bytesSent == SOCKET_ERROR || bytesSent == 0) {
            std::cerr << "[-] TCP send failed. Error=" << WSAGetLastError() << std::endl;
            return false;
        }
        totalSent += bytesSent;
    }
    return true;
}

std::string TCPClient::receiveData() {
    if (!isConnected) return "";

    auto readLine = [&]() -> std::string {
        while (true) {
            const auto end = receiveBuffer.find("\r\n");
            if (end != std::string::npos) {
                std::string line = receiveBuffer.substr(0, end + 2);
                receiveBuffer.erase(0, end + 2);
                return line;
            }
            char buffer[1024];
            const int received = recv(clientSocket, buffer, sizeof(buffer), 0);
            if (received <= 0) return {};
            receiveBuffer.append(buffer, received);
        }
    };

    std::string first = readLine();
    if (first.empty()) return {};
    std::string response = first;
    if (first.size() >= 4 && std::isdigit(static_cast<unsigned char>(first[0])) &&
        std::isdigit(static_cast<unsigned char>(first[1])) &&
        std::isdigit(static_cast<unsigned char>(first[2])) && first[3] == '-') {
        const std::string terminator = first.substr(0, 3) + " ";
        while (true) {
            std::string line = readLine();
            if (line.empty()) return {};
            response += line;
            if (line.rfind(terminator, 0) == 0) break;
        }
    }
    return response;
}

// ========================================
// Mode Settings
// ========================================
bool TCPClient::enterPassiveMode() {
    if (!sendData("PASV\r\n")) return false;
    const std::string response = receiveData();
    std::cout << response;

    const auto open = response.find('('), close = response.find(')', open);
    if (response.rfind("227", 0) != 0 || open == std::string::npos || close == std::string::npos) return false;

    std::array<int, 6> values{};
    std::istringstream input(response.substr(open + 1, close - open - 1));
    std::string token;
    for (int& value : values) {
        if (!std::getline(input, token, ',')) return false;
        try { value = std::stoi(token); } catch (...) { return false; }
        if (value < 0 || value > 255) return false;
    }

    passiveIP = std::to_string(values[0]) + "." + std::to_string(values[1]) + "." +
                std::to_string(values[2]) + "." + std::to_string(values[3]);
    passivePort = values[4] * 256 + values[5];

    this->dataMode = DataMode::PASSIVE;
    return true;
}

bool TCPClient::enterActiveMode(const std::string& portArgs) {
    if (portArgs.empty()) {
        std::cout << "[-] Missing PORT arguments (e.g., PORT 127,0,0,1,31,144)\n";
        return false;
    }

    if (!sendData("PORT " + portArgs + "\r\n")) return false;

    std::string response = receiveData();
    std::cout << response;

    if (response.rfind("200", 0) == 0) {
        std::array<int, 6> values{};
        std::istringstream input(portArgs);
        std::string token;

        for (int& value : values) {
            if (!std::getline(input, token, ',')) return false;
            try { 
                value = std::stoi(token); 
                if (value < 0 || value > 255) return false;
            } catch (...) { 
                return false; 
            }
        }

        this->activeIP = std::to_string(values[0]) + "." + std::to_string(values[1]) + "." +
                         std::to_string(values[2]) + "." + std::to_string(values[3]);
        this->activePort = values[4] * 256 + values[5];
        this->dataMode = DataMode::ACTIVE;
        return true;
    }

    return false; 
}

bool TCPClient::setTransferType(const std::string& type) {
    if (type != "A" && type != "a" && type != "I" && type != "i") return false;
    if (!sendData("TYPE " + type + "\r\n")) return false;
    const std::string response = receiveData();
    std::cout << response;
    if (response.rfind("200", 0) != 0) return false;
    asciiType = (type == "A" || type == "a");
    return true;
}

bool TCPClient::setTransferMode(const std::string& mode) {
    if (mode.size() != 1 || std::string("SsBbCc").find(mode[0]) == std::string::npos) return false;
    if (!sendData("MODE " + mode + "\r\n")) return false;
    const std::string response = receiveData(); 
    std::cout << response;
    if (response.rfind("200", 0) != 0) return false;
    transferMode = static_cast<char>(std::toupper(static_cast<unsigned char>(mode[0]))); 
    return true;
}

// ========================================
// STOR / STOU / APPE - Upload file
// ========================================
bool TCPClient::uploadFile(
    const std::string& localFilePath,
    const std::string& remoteFileName,
    const std::string& commandName
) {
    if (!isConnected) return false;

    fs::path localPath(localFilePath);
    if (!fs::exists(localPath) || !fs::is_regular_file(localPath) || remoteFileName.empty()) {
        std::cerr << "[STOR] Invalid local path or remote filename." << std::endl;
        return false;
    }

    // Đảm bảo luôn thiết lập Passive Mode mới cho mỗi lượt upload
    if (this->dataMode == DataMode::ACTIVE) {
        std::cout << "[STOR] Active mode is not supported. Switching to Passive mode..." << std::endl;
    }
    
    if (!enterPassiveMode()) {
        std::cerr << "[STOR] Cannot negotiate a passive UDP endpoint." << std::endl;
        cleanupDataChannel();
        return false;
    }

    std::string uploadIP = passiveIP;
    int uploadPort = passivePort;

    const std::string command = commandName + " " + remoteFileName + "\r\n";
    std::cout << "[STOR] Requesting upload: " << remoteFileName << std::endl;

    if (!sendData(command)) {
        cleanupDataChannel();
        return false;
    }

    std::string response = receiveData();
    if (response.empty() || response.rfind("150", 0) != 0) {
        std::cerr << "[STOR] Server is not ready for UDP transfer." << std::endl;
        cleanupDataChannel();
        return false;
    }
    std::cout << response;

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Chuẩn bị file mã hóa tạm
    fs::path sendPath = localPath;
    fs::path asciiTemp, modeTemp;

    if (asciiType) {
        asciiTemp = fs::temp_directory_path() / "hybrid_ftp_client_ascii.tmp";
        if (!Representation::toAsciiWire(localPath, asciiTemp)) {
            cleanupDataChannel();
            return false;
        }
        sendPath = asciiTemp;
    }

    if (transferMode != 'S') {
        modeTemp = fs::temp_directory_path() / "hybrid_ftp_client_mode.tmp";
        bool encoded = (transferMode == 'B') 
            ? Representation::encodeBlock(sendPath, modeTemp)
            : Representation::encodeRle(sendPath, modeTemp);
        
        if (!encoded) {
            std::error_code ec;
            if (!asciiTemp.empty()) fs::remove(asciiTemp, ec);
            cleanupDataChannel();
            return false;
        }
        sendPath = modeTemp;
    }

    // Gửi dữ liệu qua UDP
    std::cout << "[STOR] Sending file over UDP to " << uploadIP << ":" << uploadPort << std::endl;
    bool udpSuccess = UDPData::sendFile(sendPath.string(), uploadIP, uploadPort);

    // Dọn dẹp file tạm và reset Data Channel
    std::error_code ec;
    if (!asciiTemp.empty()) fs::remove(asciiTemp, ec);
    if (!modeTemp.empty()) fs::remove(modeTemp, ec);
    cleanupDataChannel();

    if (!udpSuccess) {
        std::cerr << "[STOR] UDP file transfer failed." << std::endl;
        std::string finalResponse = receiveData();
        if (!finalResponse.empty()) std::cout << finalResponse;
        return false;
    }

    response = receiveData();
    if (response.empty()) return false;
    std::cout << response;

    return response.rfind("226", 0) == 0;
}

// ========================================
// RETR - Download file
// ========================================
bool TCPClient::downloadFile(
    const std::string& remoteFileName,
    const std::string& localFilePath,
    const std::string& clientIP,
    int udpPort
) {
    if (!isConnected || remoteFileName.empty() || localFilePath.empty()) {
        return false;
    }

    // 1. Tự động chuyển về Passive nếu chưa chọn Mode nào
    if (this->dataMode == DataMode::NONE) {
        if (!enterPassiveMode()) {
            std::cerr << "[RETR] Failed to negotiate Passive Mode.\n";
            cleanupDataChannel();
            return false;
        }
    }

    const bool usePassive = (this->dataMode == DataMode::PASSIVE);
    std::string response;

    // 2. Cập nhật port chuẩn nếu đang ở Active Mode (nhập từ lệnh PORT trước đó)
    int actualUdpPort = udpPort;
    if (!usePassive && this->dataMode == DataMode::ACTIVE && this->activePort > 0) {
        actualUdpPort = this->activePort; 
    }

    // 3. CHỈ gửi lệnh PORT nếu người dùng CHƯA chạy lệnh PORT thủ công trước đó
    if (!usePassive && this->activePort == 0) {
        if (actualUdpPort <= 0 || actualUdpPort > 65535) return false;

        const int p1 = actualUdpPort / 256;
        const int p2 = actualUdpPort % 256;
        std::string portAddress = clientIP;

        if (portAddress.empty()) {
            sockaddr_in localAddr{};
            int localAddrLength = sizeof(localAddr);
            char localAddressText[INET_ADDRSTRLEN]{};
            if (getsockname(clientSocket, reinterpret_cast<sockaddr*>(&localAddr), &localAddrLength) == SOCKET_ERROR ||
                inet_ntop(AF_INET, &localAddr.sin_addr, localAddressText, sizeof(localAddressText)) == nullptr) {
                return false;
            }
            portAddress = localAddressText;
        }
        std::replace(portAddress.begin(), portAddress.end(), '.', ',');

        if (!sendData("PORT " + portAddress + "," + std::to_string(p1) + "," + std::to_string(p2) + "\r\n")) {
            return false;
        }
        response = receiveData();
        std::cout << response;
        if (response.rfind("200", 0) != 0) return false;
    }

    // 4. Mở UDP Receiver với actualUdpPort (51234)
    std::atomic<int> receiverState{0};
    bool receiveSuccess = false;
    std::thread receiver([&]() {
        receiveSuccess = usePassive
            ? UDPData::receivePassiveFile(localFilePath, 0, passiveIP, passivePort, receiverState)
            : UDPData::receiveFile(localFilePath, actualUdpPort, receiverState);
    });

    while (receiverState.load() == 0) {
        std::this_thread::yield();
    }

    if (receiverState.load() < 0) {
        receiver.join();
        cleanupDataChannel();
        return false;
    }

    if (!sendData("RETR " + remoteFileName + "\r\n")) {
        receiver.join();
        cleanupDataChannel();
        return false;
    }

    response = receiveData();
    std::cout << response;
    if (response.rfind("150", 0) != 0) {
        receiver.join();
        cleanupDataChannel();
        return false;
    }

    receiver.join();
    cleanupDataChannel();

    response = receiveData();
    std::cout << response;

    if (receiveSuccess && transferMode == 'B') receiveSuccess = Representation::decodeBlock(localFilePath);
    else if (receiveSuccess && transferMode == 'C') receiveSuccess = Representation::decodeRle(localFilePath);
    if (receiveSuccess && asciiType) receiveSuccess = Representation::fromAsciiWire(localFilePath);

    return receiveSuccess && response.rfind("226", 0) == 0;
}

// ========================================
// Disconnect
// ========================================
void TCPClient::disconnect() {
    if (clientSocket != INVALID_SOCKET) {
        closesocket(clientSocket);
        clientSocket = INVALID_SOCKET;
    }
    isConnected = false;
    cleanupDataChannel();
}