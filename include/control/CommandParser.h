#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include <string>

// Danh sách các lệnh FTP hỗ trợ
enum class FTPCommand {
    USER, PASS, QUIT, NOOP, PWD, CWD, CDUP, MKD, RMD,
    LIST, NLST, STAT, SIZE, MDTM, TYPE, MODE, PORT, PASV,
    RETR, STOR, STOU, APPE, DELE, RNFR, RNTO, HASH, ABOR, HELP,
    UNKNOWN
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
