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

    std::string dataIP;
    int dataPort;
    bool hasEndpoint;

    std::string renameFromPath;

public:
    ClientSession(SOCKET sock);

    // Authentication
    void setUsername(const std::string& user);
    std::string getUsername() const;

    AuthState getAuthState() const;
    void setAuthState(AuthState state);

    // Directory
    fs::path& getCurrentDir();
    void setCurrentDir(const fs::path& path);

    // UDP endpoint
    void setDataEndpoint(
        const std::string& ip,
        int port
    );

    std::string getDataIp() const;
    int getDataPort() const;
    bool hasDataEndpoint() const;
    void clearDataEndpoint();

    // Rename state
    void setRenameFrom(const std::string& path);
    std::string getRenameFrom() const;
    void clearRenameFrom();

    SOCKET getControlSocket() const;
};