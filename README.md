# Hybrid FTP Application – TCP Control + Reliable UDP Data

Ứng dụng FTP lai (Hybrid FTP) kết hợp điều khiển qua giao thức TCP và truyền dữ liệu tệp tin qua giao thức UDP đáng tin cậy (Reliable Data Transfer - RDT). Đồ án được xây dựng trên nền ngôn ngữ C++ (chuẩn C++17) và WinSock2 trên hệ điều hành Windows.

---

## 1. Tổng quan hệ thống

Hệ thống ứng dụng mô hình Client-Server chia làm 2 kênh truyền biệt lập:
- **Kênh điều khiển (TCP Control Channel)**: Đảm nhận việc gửi lệnh FTP từ Client, xử lý phiên làm việc (Session), xác thực người dùng và phản hồi mã trạng thái (Response Code) theo chuẩn FTP.
- **Kênh dữ liệu (UDP Data Channel)**: Đảm nhận việc truyền nội dung tệp tin hoặc danh sách thư mục (LIST). Do bản chất UDP không tin cậy, nhóm tự thiết kế và cài đặt giao thức RDT (Reliable Data Transfer) ở tầng ứng dụng.

---

## 2. Các thành phần chính

- `TCPServer`: Quản lý socket lắng nghe TCP, chấp nhận đa kết nối Client và khởi tạo luồng xử lý cho mỗi phiên.
- `TCPClient`: Giao diện dòng lệnh phía Client, gửi câu lệnh TCP và phối hợp kích hoạt truyền nhận dữ liệu UDP.
- `ClientSession`: Quản lý trạng thái phiên của từng Client (Xác thực, thư mục hiện tại, cấu hình PORT/PASV, TYPE, MODE).
- `UDPSender` & `UDPReceiver`: Bộ gửi/nhận dữ liệu UDP RDT tích hợp cơ chế đóng gói header, tính toán checksum, timeout và gửi lại (retransmission).
- `CommandParser`: Phân tích chuỗi lệnh TCP đầu vào từ Client.
- `FileSystem`: Thao tác hệ thống tệp tin local và sandbox thư mục an toàn.
- `checksum`: Hàm tính và kiểm tra 16-bit Internet Checksum cho từng gói dữ liệu UDP.
- `sha256`: Tính toán mã băm SHA-256 toàn bộ tệp tin để kiểm tra độ toàn vẹn sau khi truyền.

---

## 3. Yêu cầu hệ thống & Yêu cầu build

- **Hệ điều hành**: Windows 10/11.
- **Trình biên dịch**: MSVC (Visual Studio 2019/2022) hỗ trợ C++17.
- **Công cụ build**: CMake version >= 3.15.

---

## 4. Hướng dẫn Biên dịch (Build)

Mở **Command Prompt** hoặc **PowerShell** tại thư mục gốc của project và chạy các lệnh sau:

```bash
# Tạo thư mục và cấu hình build bằng CMake
cmake -S . -B build

# Biên dịch ứng dụng
cmake --build build
```

Sau khi biên dịch thành công, file thực thi sẽ nằm tại: `build/Debug/hybrid_ftp.exe`.

---

## 5. Hướng dẫn Chạy ứng dụng

### 5.1. Chạy Server
Mở Terminal 1 và thực thi lệnh:
```bash
build\Debug\hybrid_ftp.exe server
```
*(Nếu chạy `hybrid_ftp.exe` không tham số, ứng dụng mặc định khởi chạy ở chế độ Server trên cổng TCP 8080).*

### 5.2. Chạy Client
Mở Terminal 2 và kết nối tới Server:
```bash
build\Debug\hybrid_ftp.exe client 127.0.0.1
```

---

## 6. Tài khoản đăng nhập Demo

- **Username**: `admin`
- **Password**: `123`

---

## 7. Danh sách Lệnh hỗ trợ

| Lệnh | Cú pháp ví dụ | Mô tả |
| :--- | :--- | :--- |
| `USER` | `USER admin` | Gửi tên đăng nhập |
| `PASS` | `PASS 123` | Gửi mật khẩu |
| `PWD` | `PWD` | Hiển thị đường dẫn thư mục hiện tại |
| `LIST` | `LIST` | Liệt kê danh sách tệp tin/thư mục qua kênh dữ liệu |
| `PASV` | `PASV` | Yêu cầu Server mở cổng thụ động (Passive Mode) cho UDP |
| `PORT` | `PORT 127,0,0,1,31,144` | Khai báo IP và cổng phía Client lắng nghe (Active Mode) |
| `STOR` | `STOR test.txt` | Tải tệp tin từ Client lên Server |
| `RETR` | `RETR test.txt` | Tải tệp tin từ Server về Client |
| `HASH` | `HASH test.txt` | Xem mã băm SHA-256 của tệp tin trên Server |
| `QUIT` | `QUIT` | Ngắt kết nối và thoát phiên làm việc |

---

## 8. Giới hạn hiện tại

- Cơ chế RDT trên UDP đang áp dụng mô hình **Stop-and-Wait** đơn giản (Timeout & Retransmit), chưa triển khai cửa sổ trượt (Sliding Window - GBN/SR) và kiểm soát tắc nghẽn (Congestion Control).
- Tải tệp tin về Client (`RETR`) hỗ trợ đầy đủ cả 2 chế độ **Active Mode** (qua lệnh `PORT`) và **Passive Mode** (qua lệnh `PASV`).
- Tải tệp tin lên Server (`STOR`) hoạt động trên chế độ **Passive Mode**. Nếu Client đã phát lệnh `PORT` trước đó mà tiến hành `STOR`, Client sẽ tự động thông báo và chuyển sang Passive mode (`PASV`) để hoàn tất truyền dữ liệu.
