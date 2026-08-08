#include "control/FileSystem.h"

#include <sstream>
#include <chrono>
#include <ctime>
#include <iomanip>

bool FileSystem::resolveWithinRoot(const fs::path& root, const fs::path& current,
    const std::string& requested, fs::path& resolved) {
    if (requested.empty()) { resolved = current; return true; }
    const fs::path input(requested);
    if (input.is_absolute() || input.has_root_name() || input.has_root_directory()) return false;
    std::error_code ec;
    resolved = fs::weakly_canonical(current / input, ec);
    if (ec) return false;
    const fs::path relative = fs::relative(resolved, root, ec);
    if (ec || relative.is_absolute()) return false;
    for (const auto& component : relative) {
        if (component == "..") return false;
    }
    return true;
}

std::string FileSystem::virtualPath(const fs::path& root, const fs::path& current) {
    std::error_code ec;
    const fs::path relative = fs::relative(current, root, ec);
    if (ec || relative.empty() || relative == ".") return "/";
    return "/" + relative.generic_string();
}

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
        const auto fileTime = entry.last_write_time(ec);
        std::string timestamp = "Jan 01 00:00";
        if (!ec) {
            const auto systemTime = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                fileTime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
            const std::time_t value = std::chrono::system_clock::to_time_t(systemTime);
            std::tm local{};
            if (localtime_s(&local, &value) == 0) {
                std::ostringstream formatted;
                formatted << std::put_time(&local, "%b %d %H:%M");
                timestamp = formatted.str();
            }
        }
        ec.clear();

        // Định dạng kiểu Unix ls -l đơn giản: d cho directory, - cho file
        if (fs::is_directory(status)) {
            ss << "drwxr-xr-x 1 owner group 0 " << timestamp << " " << entry.path().filename().string() << "\r\n";
        }
        else {
            ss << "-rw-r--r-- 1 owner group " << entry.file_size() << " " << timestamp << " " << entry.path().filename().string() << "\r\n";
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

std::string FileSystem::getLastModifiedTime(
    const fs::path& currentDir,
    const std::string& fileName
) {
    fs::path filePath =
        currentDir / fileName;

    std::error_code ec;

    if (!fs::exists(filePath, ec) ||
        !fs::is_regular_file(filePath, ec)) {
        return "";
    }

    auto fileTime =
        fs::last_write_time(filePath, ec);

    if (ec) {
        return "";
    }

    auto systemTime =
        std::chrono::time_point_cast<
            std::chrono::system_clock::duration
        >(
            fileTime -
            fs::file_time_type::clock::now() +
            std::chrono::system_clock::now()
        );

    std::time_t time =
        std::chrono::system_clock::to_time_t(
            systemTime
        );

    std::tm utcTime{};

    if (gmtime_s(&utcTime, &time) != 0) {
        return "";
    }

    char buffer[32];

    if (std::strftime(
            buffer,
            sizeof(buffer),
            "%Y%m%d%H%M%S",
            &utcTime
        ) == 0) {
        return "";
    }

    return std::string(buffer);
}
