#include "control/FileSystem.h"
#include <sstream>

bool FileSystem::changeDirectory(fs::path& currentDir, const std::string& targetPath) {
    if (targetPath.empty()) return false;

    fs::path pathObj = fs::path(targetPath);
    if (pathObj.is_relative()) {
        pathObj = currentDir / pathObj;
    }

    std::error_code ec;
    pathObj = fs::canonical(pathObj, ec);

    if (!ec && fs::exists(pathObj) && fs::is_directory(pathObj)) {
        currentDir = pathObj;
        return true;
    }
    return false;
}

bool FileSystem::changeToParentDir(fs::path& currentDir) {
    if (currentDir.has_parent_path()) {
        currentDir = currentDir.parent_path();
        return true;
    }
    return false;
}

bool FileSystem::createDirectory(const fs::path& currentDir, const std::string& dirName) {
    if (dirName.empty()) return false;
    fs::path targetPath = currentDir / dirName;
    std::error_code ec;
    return fs::create_directory(targetPath, ec);
}

bool FileSystem::removeDirectory(const fs::path& currentDir, const std::string& dirName) {
    if (dirName.empty()) return false;
    fs::path targetPath = currentDir / dirName;
    std::error_code ec;
    if (fs::is_directory(targetPath, ec) && fs::remove(targetPath, ec)) {
        return true;
    }
    return false;
}

std::string FileSystem::getDirectoryListing(const fs::path& currentDir) {
    std::stringstream ss;
    std::error_code ec;

    if (!fs::exists(currentDir) || !fs::is_directory(currentDir)) {
        return "";
    }

    for (const auto& entry : fs::directory_iterator(currentDir, ec)) {
        auto status = entry.status();

        // Định dạng kiểu Unix ls -l đơn giản: d cho directory, - cho file
        if (fs::is_directory(status)) {
            ss << "drwxr-xr-x 1 owner group 0 Jan 1 00:00 " << entry.path().filename().string() << "\r\n";
        }
        else {
            ss << "-rw-r--r-- 1 owner group " << entry.file_size() << " Jan 1 00:00 " << entry.path().filename().string() << "\r\n";
        }
    }
    return ss.str();
}

bool FileSystem::fileExists(const fs::path& currentDir, const std::string& fileName) {
    fs::path filePath = currentDir / fileName;
    std::error_code ec;
    return fs::exists(filePath, ec) && fs::is_regular_file(filePath, ec);
}

uintmax_t FileSystem::getFileSize(const fs::path& currentDir, const std::string& fileName) {
    fs::path filePath = currentDir / fileName;
    std::error_code ec;
    if (fileExists(currentDir, fileName)) {
        return fs::file_size(filePath, ec);
    }
    return 0;
}