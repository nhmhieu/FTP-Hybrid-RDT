#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include <string>

// Danh sách các lệnh FTP hỗ trợ
enum class FTPCommand {
    USER,
    PASS,
    QUIT,
    NOOP,
    UNKNOWN // Lệnh không hợp lệ hoặc chưa hỗ trợ
};

// Cấu trúc lưu thông tin kết quả phân tích
struct ParsedCommand {
    FTPCommand command;
    std::string arg;
};

class CommandParser {
public:
    // Phân tích chuỗi lệnh từ client gửi lên (ví dụ: "USER admin\r\n")
    static ParsedCommand parse(const std::string& rawCmd);

    // Lấy riêng tham số từ câu lệnh
    static std::string getArg(const std::string& rawCmd);
};

#endif // COMMAND_PARSER_H
