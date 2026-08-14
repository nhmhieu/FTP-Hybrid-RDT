#include "control/UserManager.h"


bool UserManager :: loadUsers(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Khong the mo file : " << filename << '\n'; 
        return false;
    }

    std::string line; 
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue; 
        std::stringstream ss(line); 
        UserAccount acc; 

        if (ss >> acc.username >> acc.password >> acc.homeDir) {
            userList.push_back(acc);
            
            std::string fullPath = acc.homeDir; 
            if (!std::filesystem::exists(fullPath)) {
                std::filesystem::create_directories(fullPath); 
                std::cout << "Da tu dong tao thu muc cho user [" << acc.username << "] : " << acc.homeDir << '\n'; 
            }
        }
    }

    file.close(); 
    std::cout << "Tai thanh cong danh sach user " << userList.size() << " tai khoan tu " << filename << std::endl; 
    return true; 
}

bool UserManager :: checkIfUserExist(const std::string& username) {
    for (const auto& acc : userList) {
        if (acc.username == username) return true; 
    }
    return false; 
}

bool UserManager :: checkUserPassword(const std::string& username, const std::string& password) {
    for (const auto& acc : userList) {
        if (acc.username == username && acc.password == password) return true; 
    }
    return false; 
}

std::string UserManager::getUserHomeDir(const std::string& username) {
    for (const auto& acc : userList) {
        if (acc.username == username) return acc.homeDir;
    }
    return "";
}
