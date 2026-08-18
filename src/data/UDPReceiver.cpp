#include "data/UDPReceiver.h"
#include "common/checksum.h"
#include "common/protocol.h"

#include <array>
#include <fstream>
#include <iostream>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

UDPReceiver::UDPReceiver()
    : sock(INVALID_SOCKET),
    localAddr{},
    expectedSeq(0),
    winsockStarted(false) {

    WSADATA wsaData{};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "[UDP Receiver] WSAStartup failed.\n";
        return;
    }
    winsockStarted = true;

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        std::cerr << "[UDP Receiver] Failed to create UDP socket. Error="
            << WSAGetLastError() << '\n';
    }
}

UDPReceiver::~UDPReceiver() {
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
    }
    if (winsockStarted) {
        WSACleanup();
    }
}

bool UDPReceiver::isReady() const {
    return winsockStarted && sock != INVALID_SOCKET;
}

bool UDPReceiver::receiveFile(
    const std::string& savePath,
    int listenPort,
    std::atomic<int>* readyState,
    const std::atomic<bool>* cancel
) {
    if (!isReady()) {
        if (readyState != nullptr) readyState->store(-1);
        std::cerr << "[UDP Receiver] Socket is not ready.\n";
        return false;
    }

    if (listenPort <= 0 || listenPort > 65535) {
        if (readyState != nullptr) readyState->store(-1);
        std::cerr << "[UDP Receiver] Invalid port.\n";
        return false;
    }

    expectedSeq = 0;
    localAddr = {};
    localAddr.sin_family = AF_INET;
    localAddr.sin_port = htons(static_cast<u_short>(listenPort));
    localAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    sockaddr_in boundAddr{};
    int boundLength = sizeof(boundAddr);
    const bool alreadyBound = getsockname(sock, reinterpret_cast<sockaddr*>(&boundAddr),
        &boundLength) == 0 && ntohs(boundAddr.sin_port) != 0;
    if (!alreadyBound && bind(
        sock,
        reinterpret_cast<sockaddr*>(&localAddr),
        sizeof(localAddr)) == SOCKET_ERROR) {
        std::cerr << "[UDP Receiver] Bind failed. Error="
            << WSAGetLastError() << '\n';
        if (readyState != nullptr) readyState->store(-1);
        return false;
    }

    std::ofstream outFile(savePath, std::ios::binary | std::ios::trunc);
    if (!outFile.is_open()) {
        std::cerr << "[UDP Receiver] Failed to open output file: "
            << savePath << '\n';
        if (readyState != nullptr) readyState->store(-1);
        return false;
    }

    if (readyState != nullptr) {
        readyState->store(1);
    }

    // Receiver không chờ vô hạn nếu sender biến mất.
    DWORD timeout = 100;
    setsockopt(
        sock,
        SOL_SOCKET,
        SO_RCVTIMEO,
        reinterpret_cast<const char*>(&timeout),
        sizeof(timeout)
    );

    constexpr int MAX_IDLE_TIMEOUTS = 100;
    int idleTimeouts = 0;
    std::uint64_t totalBytes = 0;
    bool peerSelected = false;
    sockaddr_in selectedPeer{};

    std::array<std::uint8_t, sizeof(udp_header_t) + UDP_PAYLOAD_MAX> packet{};

    std::cout << "[UDP Receiver] Listening on port " << listenPort << "...\n";

    while (idleTimeouts < MAX_IDLE_TIMEOUTS) {
        if (cancel && cancel->load()) return false;
        sockaddr_in clientAddr{};
        int addrLen = sizeof(clientAddr);

        const int recvLen = recvfrom(
            sock,
            reinterpret_cast<char*>(packet.data()),
            static_cast<int>(packet.size()),
            0,
            reinterpret_cast<sockaddr*>(&clientAddr),
            &addrLen
        );

        if (recvLen == SOCKET_ERROR) {
            const int error = WSAGetLastError();
            if (error == WSAETIMEDOUT) {
                if (cancel && cancel->load()) return false;
                ++idleTimeouts;
                continue;
            }

            std::cerr << "[UDP Receiver] recvfrom failed. Error=" << error << '\n';
            return false;
        }

        idleTimeouts = 0;

        if (!isPacketValid(packet.data(), static_cast<std::size_t>(recvLen))) {
            std::cout << "[UDP Receiver] Invalid packet received. Ignoring it.\n";
            continue;
        }
        if (peerSelected && (clientAddr.sin_addr.s_addr != selectedPeer.sin_addr.s_addr ||
            clientAddr.sin_port != selectedPeer.sin_port)) continue;
        if (!peerSelected) { selectedPeer = clientAddr; peerSelected = true; }

        const auto* header = reinterpret_cast<const udp_header_t*>(packet.data());
        const std::uint32_t seq = ntohl(header->seq);
        const std::size_t payloadLen = ntohs(header->payloadLen);
        const bool isData = (header->flags & FLAG_DATA) != 0;
        const bool isFin = (header->flags & FLAG_FIN) != 0;

        if (isFin) {
            if (seq == expectedSeq) {
                sendAck(seq, FLAG_FIN, clientAddr);
                outFile.flush();
                outFile.close();

                // Nếu FIN-ACK đầu tiên bị mất, re-ACK FIN được gửi lại.
                lingerForDuplicateFin(seq, clientAddr);

                std::cout << "[UDP Receiver] File received successfully. Total bytes="
                    << totalBytes << ", saved to " << savePath << '\n';
                return true;
            }

            // FIN chưa đúng thứ tự thì chưa được kết thúc.
            continue;
        }

        if (!isData) {
            continue;
        }

        if (seq == expectedSeq) {
            outFile.write(
                reinterpret_cast<const char*>(packet.data() + sizeof(udp_header_t)),
                static_cast<std::streamsize>(payloadLen)
            );

            if (!outFile.good()) {
                std::cerr << "[UDP Receiver] Failed to write to output file.\n";
                return false;
            }

            totalBytes += payloadLen;
            sendAck(seq, 0, clientAddr);
            ++expectedSeq;

            std::cout << "[UDP Receiver] Received seq=" << seq
                << ", bytes=" << payloadLen << '\n';
        }
        else if (seq < expectedSeq) {
            // Packet trùng do ACK trước đó bị mất: không ghi lại, chỉ ACK lại.
            sendAck(seq, 0, clientAddr);
            std::cout << "[UDP Receiver] Duplicate packet seq=" << seq
                << ", ACK sent again.\n";
        }
        // seq > expectedSeq: bỏ qua. Stop-and-Wait bình thường không tạo trường hợp này.
    }

    std::cerr << "[UDP Receiver] Receiver timed out. File transfer aborted.\n";
    return false;
}

bool UDPReceiver::receivePassiveFile(const std::string& savePath, int listenPort,
    const std::string& serverIP, int serverPort, std::atomic<int>* readyState,
    const std::atomic<bool>* cancel) {
    if (!isReady() || listenPort < 0 || listenPort > 65535 ||
        serverPort <= 0 || serverPort > 65535) {
        if (readyState) readyState->store(-1);
        return false;
    }
    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    local.sin_port = htons(static_cast<u_short>(listenPort));
    if (bind(sock, reinterpret_cast<sockaddr*>(&local), sizeof(local)) == SOCKET_ERROR) {
        if (readyState) readyState->store(-1);
        return false;
    }
    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(static_cast<u_short>(serverPort));
    if (inet_pton(AF_INET, serverIP.c_str(), &server.sin_addr) != 1) {
        if (readyState) readyState->store(-1);
        return false;
    }
    udp_header_t registration{};
    registration.magic = htons(MAGIC_NUMBER);
    registration.flags = FLAG_ACK;
    registration.checksum = htons(calculate_checksum(
        reinterpret_cast<const std::uint8_t*>(&registration), sizeof(registration)));
    if (sendto(sock, reinterpret_cast<const char*>(&registration), sizeof(registration), 0,
        reinterpret_cast<const sockaddr*>(&server), sizeof(server)) == SOCKET_ERROR) {
        if (readyState) readyState->store(-1);
        return false;
    }
    sockaddr_in actual{};
    int actualLength = sizeof(actual);
    if (getsockname(sock, reinterpret_cast<sockaddr*>(&actual), &actualLength) == SOCKET_ERROR) {
        if (readyState) readyState->store(-1);
        return false;
    }
    return receiveFile(savePath, ntohs(actual.sin_port), readyState, cancel);
}

bool UDPReceiver::sendAck(
    std::uint32_t ackSeq,
    std::uint8_t extraFlags,
    const sockaddr_in& clientAddr
) {
    udp_header_t ack{};
    ack.magic = htons(MAGIC_NUMBER);
    ack.flags = static_cast<std::uint8_t>(FLAG_ACK | extraFlags);
    ack.reserved = 0;
    ack.seq = 0;
    ack.ack = htonl(ackSeq);
    ack.payloadLen = 0;
    ack.checksum = 0;
    ack.checksum = htons(calculate_checksum(
        reinterpret_cast<const std::uint8_t*>(&ack),
        sizeof(ack)
    ));

    const int sent = sendto(
        sock,
        reinterpret_cast<const char*>(&ack),
        sizeof(ack),
        0,
        reinterpret_cast<const sockaddr*>(&clientAddr),
        sizeof(clientAddr)
    );

    return sent != SOCKET_ERROR;
}

bool UDPReceiver::isPacketValid(
    const std::uint8_t* packet,
    std::size_t len
) const {
    if (packet == nullptr || len < sizeof(udp_header_t)) {
        return false;
    }

    const auto* header = reinterpret_cast<const udp_header_t*>(packet);
    if (ntohs(header->magic) != MAGIC_NUMBER) {
        return false;
    }

    const std::size_t payloadLen = ntohs(header->payloadLen);
    if (payloadLen > UDP_PAYLOAD_MAX) {
        return false;
    }

    if (len != sizeof(udp_header_t) + payloadLen) {
        return false;
    }

    if (header->reserved != 0 || ntohl(header->ack) != 0) return false;
    if (header->flags == FLAG_FIN && payloadLen != 0) return false;
    if (header->flags == FLAG_DATA && payloadLen == 0) return false;
    if (header->flags != FLAG_DATA && header->flags != FLAG_FIN) return false;

    return verify_checksum(packet, len);
}

void UDPReceiver::lingerForDuplicateFin(
    std::uint32_t finSeq,
    const sockaddr_in& originalClientAddr
) {
    DWORD timeout = 300;
    setsockopt(
        sock,
        SOL_SOCKET,
        SO_RCVTIMEO,
        reinterpret_cast<const char*>(&timeout),
        sizeof(timeout)
    );

    std::array<std::uint8_t, sizeof(udp_header_t)> packet{};

    // Chờ ngắn để ACK lại FIN nếu FIN-ACK đầu tiên bị mất.
    for (int i = 0; i < 4; ++i) {
        sockaddr_in clientAddr{};
        int addrLen = sizeof(clientAddr);

        const int recvLen = recvfrom(
            sock,
            reinterpret_cast<char*>(packet.data()),
            static_cast<int>(packet.size()),
            0,
            reinterpret_cast<sockaddr*>(&clientAddr),
            &addrLen
        );

        if (recvLen == SOCKET_ERROR) {
            continue;
        }

        if (clientAddr.sin_addr.s_addr != originalClientAddr.sin_addr.s_addr ||
            clientAddr.sin_port != originalClientAddr.sin_port) {
            continue;
        }

        if (!isPacketValid(packet.data(), static_cast<std::size_t>(recvLen))) {
            continue;
        }

        const auto* header = reinterpret_cast<const udp_header_t*>(packet.data());
        if ((header->flags & FLAG_FIN) != 0 && ntohl(header->seq) == finSeq) {
            sendAck(finSeq, FLAG_FIN, clientAddr);
        }
    }
}
