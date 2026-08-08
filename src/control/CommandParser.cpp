#include "control/CommandParser.h"
#include <sstream>
#include <algorithm>

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

    std::stringstream ss(cleanStr);
    std::string cmdStr;
    ss >> cmdStr;

    std::getline(ss, result.arg);
    result.arg = trim(result.arg);

    std::transform(cmdStr.begin(), cmdStr.end(), cmdStr.begin(), ::toupper);

    // --- NHÓM QUẢN LÝ PHIÊN ---
    if (cmdStr == "USER")        result.command = FTPCommand::USER;
    else if (cmdStr == "PASS")   result.command = FTPCommand::PASS;
    else if (cmdStr == "QUIT")   result.command = FTPCommand::QUIT;
    else if (cmdStr == "NOOP")   result.command = FTPCommand::NOOP;

    // --- NHÓM ĐIỀU HƯỚNG THƯ MỤC (BỔ SUNG) ---
    else if (cmdStr == "PWD")    result.command = FTPCommand::PWD;
    else if (cmdStr == "CWD")    result.command = FTPCommand::CWD;
    else if (cmdStr == "CDUP")   result.command = FTPCommand::CDUP;
    else if (cmdStr == "MKD")    result.command = FTPCommand::MKD;
    else if (cmdStr == "RMD")    result.command = FTPCommand::RMD;

    else if (cmdStr == "LIST")   result.command = FTPCommand::LIST;
    else if (cmdStr == "RETR")   result.command = FTPCommand::RETR;
    else if (cmdStr == "STOR")   result.command = FTPCommand::STOR;
    else if (cmdStr == "STOU")   result.command = FTPCommand::STOU;
    else if (cmdStr == "PORT")   result.command = FTPCommand::PORT;
    else if (cmdStr == "PASV")   result.command = FTPCommand::PASV;
    else if (cmdStr == "TYPE")   result.command = FTPCommand::TYPE;
    else if (cmdStr == "MODE")   result.command = FTPCommand::MODE;

// --- FILE / METADATA COMMANDS ---
    else if (cmdStr == "NLST")   result.command = FTPCommand::NLST;
    else if (cmdStr == "SIZE")   result.command = FTPCommand::SIZE;
    else if (cmdStr == "MDTM")   result.command = FTPCommand::MDTM;
    else if (cmdStr == "STAT")   result.command = FTPCommand::STAT;
    else if (cmdStr == "DELE")   result.command = FTPCommand::DELE;
    else if (cmdStr == "RNFR")   result.command = FTPCommand::RNFR;
    else if (cmdStr == "RNTO")   result.command = FTPCommand::RNTO;
    else if (cmdStr == "HELP")   result.command = FTPCommand::HELP;

    else {
        result.command = FTPCommand::UNKNOWN;
    }

    return result;
}

std::string CommandParser::getArg(const std::string& rawCmd) {
    ParsedCommand parsed = parse(rawCmd);
    return parsed.arg;
}
