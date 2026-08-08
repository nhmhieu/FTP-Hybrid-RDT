cmake_minimum_required(VERSION 3.20)

project(FTP_Hybrid_RDT LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(COMMON_SOURCES
    src/common/checksum.cpp
    src/common/ftp_api.cpp
    src/common/representation.cpp
    src/common/sha256.cpp
)

set(UDP_SOURCES
    src/data/UDPSender.cpp
    src/data/UDPReceiver.cpp
)

set(TCP_SOURCES
    src/control/TCPServer.cpp
    src/control/TCPClient.cpp
    src/control/CommandParser.cpp
    src/control/SessionManager.cpp
    src/control/FileSystem.cpp
)

add_executable(udp_test
    src/udp_test_main.cpp
    ${COMMON_SOURCES}
    ${UDP_SOURCES}
)

add_executable(hybrid_ftp
    src/main.cpp
    ${COMMON_SOURCES}
    ${UDP_SOURCES}
    ${TCP_SOURCES}
)

target_include_directories(udp_test PRIVATE include)
target_include_directories(hybrid_ftp PRIVATE include)

if(MSVC)
    target_compile_options(udp_test PRIVATE /utf-8)
    target_compile_options(hybrid_ftp PRIVATE /utf-8)
endif()

if(WIN32)
    target_link_libraries(udp_test PRIVATE ws2_32 bcrypt)
    target_link_libraries(hybrid_ftp PRIVATE ws2_32 bcrypt)
endif()
