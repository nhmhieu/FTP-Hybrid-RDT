#pragma once

#include <string>
#include <filesystem>
#include <winsock2.h>

#include "common/DataMode.h"

namespace fs = std::filesystem;

enum class AuthState {
    UNAUTHENTICATED,
    WAITING_FOR_PASS,
    AUTHENTICATED
};

// enum class DataMode { NONE, ACTIVE, PASSIVE };
enum class TransferType { BINARY, ASCII };
enum class TransferMode { STREAM, BLOCK, COMPRESSED };

class ClientSession {
private:
    SOCKET controlSocket;

    AuthState authState;
    std::string username;

    fs::path currentDir;
    fs::path rootDir;

    std::string dataIP;
    int dataPort;
    bool hasEndpoint;
    DataMode dataMode;
    SOCKET passiveSocket;
    TransferType transferType;
    TransferMode transferMode;

    std::string renameFromPath;

public:
    ClientSession(SOCKET sock);
    ~ClientSession();

    // Authentication
    void setUsername(const std::string& user);
    std::string getUsername() const;

    AuthState getAuthState() const;
    void setAuthState(AuthState state);

    // Directory
    fs::path& getCurrentDir();
    void setCurrentDir(const fs::path& path);
    const fs::path& getRootDir() const;

    // UDP endpoint
    void setDataEndpoint(
        const std::string& ip,
        int port
    );

    std::string getDataIp() const;
    int getDataPort() const;
    bool hasDataEndpoint() const;
    void clearDataEndpoint();
    DataMode getDataMode() const;
    void setPassiveEndpoint(const std::string& ip, int port, SOCKET socket);
    SOCKET getPassiveSocket() const;
    void closePassiveSocket();
    TransferType getTransferType() const;
    void setTransferType(TransferType type);
    TransferMode getTransferMode() const;
    void setTransferMode(TransferMode mode);

    // Rename state
    void setRenameFrom(const std::string& path);
    std::string getRenameFrom() const;
    void clearRenameFrom();

    SOCKET getControlSocket() const;
};
