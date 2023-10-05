cmake_minimum_required(VERSION 3.19)
project(NTorrent)

set(CMAKE_CXX_STANDARD 20)

include_directories(NTorrent C:/Projects/NTrorrent/libs/asio-1.28.0/include/)
link_libraries(ws2_32 wsock32)
add_executable(NTorrent main.cpp)