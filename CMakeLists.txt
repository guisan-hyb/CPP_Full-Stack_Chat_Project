cmake_minimum_required(VERSION 3.15)
project(FullStackChat)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 只寻找 Server 需要的库
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

# 只进入 server 目录构建
add_subdirectory(server)

# client 使用qmake构建
