#include "common/ftp_api.h"

#include <iostream>
#include <string>

namespace {
    void printUsage(const char* programName) {
        std::cout
            << "Usage:\n"
            << "  Receive a file:\n"
            << "    " << programName << " receive <port> <output_file>\n\n"
            << "  Send a file:\n"
            << "    " << programName << " send <ip> <port> <input_file>\n";
    }

    bool parsePort(const std::string& text, int& port) {
        try {
            std::size_t used = 0;
            const int value = std::stoi(text, &used);
            if (used != text.size() || value <= 0 || value > 65535) {
                return false;
            }
            port = value;
            return true;
        }
        catch (...) {
            return false;
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    const std::string mode = argv[1];

    if (mode == "receive") {
        if (argc != 4) {
            printUsage(argv[0]);
            return 1;
        }

        int port = 0;
        if (!parsePort(argv[2], port)) {
            std::cerr << "Invalid port.\n";
            return 1;
        }

        return UDPData::receiveFile(argv[3], port) ? 0 : 2;
    }

    if (mode == "send") {
        if (argc != 5) {
            printUsage(argv[0]);
            return 1;
        }

        int port = 0;
        if (!parsePort(argv[3], port)) {
            std::cerr << "Invalid port.\n";
            return 1;
        }

        return UDPData::sendFile(argv[4], argv[2], port) ? 0 : 2;
    }

    printUsage(argv[0]);
    return 1;
}
