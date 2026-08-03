#ifndef FTP_API_H
#define FTP_API_H

#include <string>

namespace UDPData {
    // Hàm gửi file: gọi khi client yêu cầu tải file (RETR)
    bool sendFile(const std::string& filePath, const std::string& destIP, int destPort);

    // Hàm nhận file: gọi khi client upload file (STOR)
    bool receiveFile(const std::string& savePath, int listenPort);
}

#endif