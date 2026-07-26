#include "control/CommandParser.h"
#include <sstream>
#include <algorithm>

// Hàm hỗ trợ: Cắt khoảng trắng và ký tự newline (\r, \n) ở hai đầu chuỗi
static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

ParsedCommand CommandParser::parse(const std::string& rawCmd) {
    ParsedCommand result;
    result.command = FTPCommand::UNKNOWN;
    result.arg = "";

    std::string cleanStr = trim(rawCmd);
    if (cleanStr.empty()) return result;

    // Cắt chuỗi theo khoảng trắng đầu tiên
    std::stringstream ss(cleanStr);
    std::string cmdStr;
    ss >> cmdStr; // Lấy từ đầu tiên (Tên lệnh)

    // Lấy phần còn lại làm tham số
    std::getline(ss, result.arg);
    result.arg = trim(result.arg);

    // Chuyển tên lệnh thành chữ hoa để so sánh (không phân biệt hoa/thường)
    std::transform(cmdStr.begin(), cmdStr.end(), cmdStr.begin(), ::toupper);

    // So sánh lệnh và gán Enum tương ứng
    if (cmdStr == "USER") {
        result.command = FTPCommand::USER;
    }
    else if (cmdStr == "PASS") {
        result.command = FTPCommand::PASS;
    }
    else if (cmdStr == "QUIT") {
        result.command = FTPCommand::QUIT;
    }
    else if (cmdStr == "NOOP") {
        result.command = FTPCommand::NOOP;
    }
    else {
        result.command = FTPCommand::UNKNOWN;
    }

    return result;
}

std::string CommandParser::getArg(const std::string& rawCmd) {
    ParsedCommand parsed = parse(rawCmd);
    return parsed.arg;
}