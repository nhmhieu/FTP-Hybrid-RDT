#pragma once 

#include <iostream>

struct UserAccount{
    std::string username;
    std::string password;
    std::string homeDir; // Đường dẫn thư mục tương ứng trong storage
};