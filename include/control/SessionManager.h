#pragma once
#include <string>
#include <filesystem>
#include <winsock2.h>

namespace fs = std::filesystem;

enum class AuthState {
    UNAUTHENTICATED,
    WAITING_FOR_PASS,
    AUTHENTICATED
};

class ClientSession {
private:
    SOCKET controlSocket;
    AuthState authState;
    std::string username;
    fs::path currentDir;

    // Các thông tin chuẩn bị cho Kênh Data UDP của Hiếu sau này
    std::string dataIP;
    int dataPort;

public:
    ClientSession(SOCKET sock);

    // Quản lý Authentication
    void setUsername(const std::string& user);
    std::string getUsername() const;
    AuthState getAuthState() const;
    void setAuthState(AuthState state);

    // Quản lý Thư mục làm việc
    fs::path& getCurrentDir();
    void setCurrentDir(const fs::path& path);

    // Quản lý kênh dữ liệu UDP
    void setDataEndpoint(const std::string& ip, int port);
    int getDataPort() const;

    SOCKET getControlSocket() const;
};