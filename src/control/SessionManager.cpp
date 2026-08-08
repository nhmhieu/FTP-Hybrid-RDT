#include "control/SessionManager.h"

ClientSession::ClientSession(SOCKET sock)
    : controlSocket(sock),
      authState(AuthState::UNAUTHENTICATED),
      currentDir(fs::current_path()),
      dataIP(""),
      dataPort(0),
      hasEndpoint(false),
      dataMode(DataMode::NONE),
      passiveSocket(INVALID_SOCKET),
      renameFromPath("") {
}

ClientSession::~ClientSession() { closePassiveSocket(); }


// =========================
// AUTHENTICATION
// =========================

AuthState ClientSession::getAuthState() const {
    return authState;
}

void ClientSession::setAuthState(AuthState state) {
    authState = state;
}

std::string ClientSession::getUsername() const {
    return username;
}

void ClientSession::setUsername(const std::string& name) {
    username = name;
}


// =========================
// CURRENT DIRECTORY
// =========================

fs::path& ClientSession::getCurrentDir() {
    return currentDir;
}

void ClientSession::setCurrentDir(const fs::path& path) {
    currentDir = path;
}


// =========================
// UDP DATA ENDPOINT
// =========================

void ClientSession::setDataEndpoint(
    const std::string& ip,
    int port
) {
    dataIP = ip;
    dataPort = port;
    hasEndpoint = true;
    dataMode = DataMode::ACTIVE;
    closePassiveSocket();
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
    dataMode = DataMode::NONE;
}

DataMode ClientSession::getDataMode() const { return dataMode; }
void ClientSession::setPassiveEndpoint(const std::string& ip, int port, SOCKET socket) {
    closePassiveSocket();
    dataIP = ip; dataPort = port; hasEndpoint = true;
    dataMode = DataMode::PASSIVE; passiveSocket = socket;
}
SOCKET ClientSession::getPassiveSocket() const { return passiveSocket; }
void ClientSession::closePassiveSocket() {
    if (passiveSocket != INVALID_SOCKET) {
        closesocket(passiveSocket);
        passiveSocket = INVALID_SOCKET;
    }
}


// =========================
// RNFR / RNTO STATE
// =========================

void ClientSession::setRenameFrom(
    const std::string& path
) {
    renameFromPath = path;
}

std::string ClientSession::getRenameFrom() const {
    return renameFromPath;
}

void ClientSession::clearRenameFrom() {
    renameFromPath = "";
}


// =========================
// CONTROL SOCKET
// =========================

SOCKET ClientSession::getControlSocket() const {
    return controlSocket;
}
