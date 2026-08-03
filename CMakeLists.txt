cmake_minimum_required(VERSION 3.20)
project(UDPDay1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(udp_test
    src/main.cpp
    src/common/checksum.cpp
    src/common/ftp_api.cpp
    src/data/UDPSender.cpp
    src/data/UDPReceiver.cpp
)

target_include_directories(udp_test PRIVATE include)

target_link_libraries(udp_test PRIVATE ws2_32)
