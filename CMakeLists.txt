cmake_minimum_required(VERSION 3.15)
project(FullStackChat)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==========================================
# 新增：屏蔽 MSVC 的弃用警告
# ==========================================
# ==========================================
# 强制关闭 MSVC 的 "将警告视为错误" 并屏蔽弃用警告
# ==========================================
if(MSVC)
    # 1. 屏蔽 C4996 弃用警告
    add_compile_options(/wd4996)
    
    # 2. 显式告诉编译器：不要把警告当成错误！
    # /WX- 的意思是关闭 "Treat Warnings as Errors"
    add_compile_options(/WX-)
    
    # 3. 如果 CMAKE 里之前被偷偷塞入了 /WX，用正则删掉它
    string(REGEX REPLACE "/WX" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REGEX REPLACE "/WX" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REGEX REPLACE "/WX" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")

    # 强制 MSVC 编译器按 UTF-8 处理源码和执行字符集 
    add_compile_options(/utf-8)
endif()

# 寻找库...
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(Boost REQUIRED)

# 进入子目录...
add_subdirectory(server)

# Qt 使用qmake构建
