#include "data/UDPSender.h"
#include "common/checksum.h"
#include "common/protocol.h"

#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

UDPSender::UDPSender()
    : sock(INVALID_SOCKET),
    destAddr{},
    timeoutMs(700),
    maxRetries(5),
    winsockStarted(false) {

    WSADATA wsaData{};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "[UDP Sender] WSAStartup failed.\n";
        return;
    }
    winsockStarted = true;

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        std::cerr << "[UDP Sender] Failed to create UDP socket. Error="
            << WSAGetLastError() << '\n';
    }
}

UDPSender::~UDPSender() {
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
    }
    if (winsockStarted) {
        WSACleanup();
    }
}

bool UDPSender::isReady() const {
    return winsockStarted && sock != INVALID_SOCKET;
}

bool UDPSender::sendFile(
    const std::string& filePath,
    const std::string& destIP,
    int destPort
) {
    if (!isReady()) {
        std::cerr << "[UDP Sender] Socket is not ready.\n";
        return false;
    }

    if (destPort <= 0 || destPort > 65535) {
        std::cerr << "[UDP Sender] Invalid port.\n";
        return false;
    }

    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[UDP Sender] Failed to open input file: " << filePath << '\n';
        return false;
    }

    destAddr = {};
    destAddr.sin_family = AF_INET;
    destAddr.sin_port = htons(static_cast<u_short>(destPort));

    if (inet_pton(AF_INET, destIP.c_str(), &destAddr.sin_addr) != 1) {
        std::cerr << "[UDP Sender] Invalid IPv4 address: " << destIP << '\n';
        return false;
    }

    std::array<std::uint8_t, UDP_PAYLOAD_MAX> payload{};
    std::uint32_t seq = 0;
    std::uint64_t totalBytes = 0;

    while (file) {
        file.read(
            reinterpret_cast<char*>(payload.data()),
            static_cast<std::streamsize>(payload.size())
        );

        const std::streamsize bytesRead = file.gcount();
        if (bytesRead <= 0) {
            break;
        }

        if (!sendPacketAndWaitAck(
            payload.data(),
            static_cast<std::size_t>(bytesRead),
            seq,
            FLAG_DATA)) {
            std::cerr << "[UDP Sender] Failed to send packet seq=" << seq << '\n';
            return false;
        }

        totalBytes += static_cast<std::uint64_t>(bytesRead);
        ++seq;
    }

    // FIN giải quyết đúng cả file rỗng và file có kích thước bội số 1024.
    if (!sendPacketAndWaitAck(nullptr, 0, seq, FLAG_FIN)) {
        std::cerr << "[UDP Sender] FIN-ACK was not received.\n";
        return false;
    }

    std::cout << "[UDP Sender] File sent successfully. Total bytes="
        << totalBytes << '\n';
    return true;
}

bool UDPSender::sendPacketAndWaitAck(
    const std::uint8_t* payload,
    std::size_t payloadLen,
    std::uint32_t seq,
    std::uint8_t flags
) {
    if (payloadLen > UDP_PAYLOAD_MAX) {
        return false;
    }

    std::array<std::uint8_t, sizeof(udp_header_t) + UDP_PAYLOAD_MAX> packet{};
    auto* header = reinterpret_cast<udp_header_t*>(packet.data());

    header->magic = htons(MAGIC_NUMBER);
    header->flags = flags;
    header->reserved = 0;
    header->seq = htonl(seq);
    header->ack = 0;
    header->payloadLen = htons(static_cast<u_short>(payloadLen));
    header->checksum = 0;

    if (payloadLen > 0 && payload != nullptr) {
        std::memcpy(packet.data() + sizeof(udp_header_t), payload, payloadLen);
    }

    const std::size_t packetLen = sizeof(udp_header_t) + payloadLen;
    header->checksum = htons(calculate_checksum(packet.data(), packetLen));

    for (int attempt = 1; attempt <= maxRetries; ++attempt) {
        const int sent = sendto(
            sock,
            reinterpret_cast<const char*>(packet.data()),
            static_cast<int>(packetLen),
            0,
            reinterpret_cast<const sockaddr*>(&destAddr),
            sizeof(destAddr)
        );

        if (sent == SOCKET_ERROR) {
            std::cerr << "[UDP Sender] sendto failed. Error="
                << WSAGetLastError() << '\n';
            continue;
        }

        std::cout << "[UDP Sender] Sending seq=" << seq
            << ", bytes=" << payloadLen
            << ", attempt=" << attempt << '\n';

        const bool expectFinAck = (flags & FLAG_FIN) != 0;
        if (waitForAck(seq, expectFinAck)) {
            return true;
        }

        std::cout << "[UDP Sender] Timeout seq=" << seq
            << ", retransmitting.\n";
    }

    return false;
}

bool UDPSender::waitForAck(std::uint32_t expectedSeq, bool expectFinAck) {
    DWORD timeout = static_cast<DWORD>(timeoutMs);
    if (setsockopt(
        sock,
        SOL_SOCKET,
        SO_RCVTIMEO,
        reinterpret_cast<const char*>(&timeout),
        sizeof(timeout)) == SOCKET_ERROR) {
        std::cerr << "[UDP Sender] Failed to set receive timeout. Error="
            << WSAGetLastError() << '\n';
        return false;
    }

    std::array<std::uint8_t, sizeof(udp_header_t)> ackPacket{};
    sockaddr_in fromAddr{};
    int fromLen = sizeof(fromAddr);

    while (true) {
        const int recvLen = recvfrom(
            sock,
            reinterpret_cast<char*>(ackPacket.data()),
            static_cast<int>(ackPacket.size()),
            0,
            reinterpret_cast<sockaddr*>(&fromAddr),
            &fromLen
        );

        if (recvLen == SOCKET_ERROR) {
            const int error = WSAGetLastError();
            if (error == WSAETIMEDOUT) {
                return false;
            }
            std::cerr << "[UDP Sender] recvfrom failed. Error=" << error << '\n';
            return false;
        }

        if (recvLen != static_cast<int>(sizeof(udp_header_t))) {
            continue;
        }

        // Chỉ nhận ACK từ đúng IP và port đã gửi tới.
        if (fromAddr.sin_addr.s_addr != destAddr.sin_addr.s_addr ||
            fromAddr.sin_port != destAddr.sin_port) {
            continue;
        }

        const auto* header =
            reinterpret_cast<const udp_header_t*>(ackPacket.data());

        if (ntohs(header->magic) != MAGIC_NUMBER) {
            continue;
        }
        if (!verify_checksum(ackPacket.data(), ackPacket.size())) {
            continue;
        }
        if ((header->flags & FLAG_ACK) == 0) {
            continue;
        }
        if (expectFinAck && (header->flags & FLAG_FIN) == 0) {
            continue;
        }
        if (ntohl(header->ack) != expectedSeq) {
            continue;
        }

        std::cout << "[UDP Sender] Received ACK for seq=" << expectedSeq << '\n';
        return true;
    }
}
