#pragma once
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

class FileSystem {
public:
    // Kiểm tra và chuyển thư mục làm việc (CWD)
    static bool changeDirectory(fs::path& currentDir, const std::string& targetPath);

    // Lấy thư mục cha (CDUP)
    static bool changeToParentDir(fs::path& currentDir);

    // Tạo thư mục (MKD)
    static bool createDirectory(const fs::path& currentDir, const std::string& dirName);

    // Xóa thư mục rỗng (RMD)
    static bool removeDirectory(const fs::path& currentDir, const std::string& dirName);

    // Lấy danh sách file/thư mục để chuẩn bị cho lệnh LIST/NLST
    static std::string getDirectoryListing(const fs::path& currentDir);

    // Kiểm tra file có tồn tại để truyền nhận dữ liệu hay không (dành cho STOR/RETR)
    static bool fileExists(const fs::path& currentDir, const std::string& fileName);
    static uintmax_t getFileSize(const fs::path& currentDir, const std::string& fileName);

    static std::string getLastModifiedTime(
    const fs::path& currentDir,
    const std::string& fileName
);
};
