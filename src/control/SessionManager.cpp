#include "control/SessionManager.h"

// Constructor: Khởi tạo các giá trị ban đầu cho một Client Session mới
ClientSession::ClientSession(SOCKET sock)
    : controlSocket(sock),
    authState(AuthState::UNAUTHENTICATED),
    currentDir(fs::current_path()),
    dataIP(""),
    dataPort(0),
    hasEndpoint(false) {
}

// Quản lý Trạng thái Xác thực (Authentication State)
AuthState ClientSession::getAuthState() const {
    return authState;
}

void ClientSession::setAuthState(AuthState state) {
    authState = state;
}

// Quản lý Tên đăng nhập (Username)
std::string ClientSession::getUsername() const {
    return username;
}

void ClientSession::setUsername(const std::string& name) {
    username = name;
}

// Quản lý Thư mục làm việc hiện tại
fs::path& ClientSession::getCurrentDir() {
    return currentDir;
}

void ClientSession::setCurrentDir(const fs::path& path) {
    currentDir = path;
}
// === QUẢN LÝ KÊNH DỮ LIỆU UDP ===
void ClientSession::setDataEndpoint(const std::string& ip, int port) {
    dataIP = ip;
    dataPort = port;
    hasEndpoint = true;
}

std::string ClientSession::getDataIp() const {
    return dataIP;
}

int ClientSession::getDataPort() const {
    return dataPort;
}

bool ClientSession::hasDataEndpoint() const {
    return hasEndpoint;
}

void ClientSession::clearDataEndpoint() {
    dataIP = "";
    dataPort = 0;
    hasEndpoint = false;
}
// Lấy Socket Điều khiển TCP
SOCKET ClientSession::getControlSocket() const {
    return controlSocket;
}

void ClientSession::setRenameFrom(const std::string& path) {
    renameFromPath = path;
}

std::string ClientSession::getRenameFrom() const {
    return renameFromPath;
}

void ClientSession::clearRenameFrom() {
    renameFromPath = "";
}