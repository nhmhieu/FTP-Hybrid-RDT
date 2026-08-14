#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include "common/User.h"

class UserManager {
private:
    // Biến static lưu dữ liệu dùng chung (C++17 trở lên dùng inline static rất gọn)
    static inline std::vector<UserAccount> userList;

public:
    static bool loadUsers(const std::string& filename);
    static bool checkIfUserExist(const std::string& username);
    static bool checkUserPassword(const std::string& username, const std::string& password);
    static std::string getUserHomeDir(const std::string& username);
};