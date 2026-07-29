#include "control/SessionManager.h"

// Constructor: Khởi tạo các giá trị ban đầu cho một Client Session mới
ClientSession::ClientSession(SOCKET sock)
    : controlSocket(sock),
    authState(AuthState::UNAUTHENTICATED),
    currentDir(fs::current_path()),
    dataPort(0) {
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

// Lấy Socket Điều khiển TCP
SOCKET ClientSession::getControlSocket() const {
    return controlSocket;
}