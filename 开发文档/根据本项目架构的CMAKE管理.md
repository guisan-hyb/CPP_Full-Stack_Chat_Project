## 根据本项目架构的CMAKE管理

针对你的项目架构（Client 用 QMake，Server 用 CMake + vcpkg 管理多个 gRPC 微服务），最佳实践是**采用“顶层 CMake 统筹 + 子目录 CMake 分治”的模块化结构**。

既然你后续还会开发其他服务器（如 LogicServer、ChatServer 等），我们需要把公共的依赖（gRPC、Protobuf）提取到 `server` 目录下的主 CMake 中，然后每个具体的 Server 作为一个独立的子目录。

以下是详细的解决方案和完整的代码文件：

---

### 第1步：项目目录结构规划

为了清晰的模块化，建议你的 `server` 目录结构如下：

```text
C++_Full-Stack_Chat_Project/
├── CMakePresets.json          <-- 顶层配置文件 (连接 vcpkg)
├── CMakeLists.txt             <-- 顶层 CMake (仅做全局设置)
├── client/                    <-- (QMake 管理，CMake 忽略它)
└── server/
    ├── CMakeLists.txt         <-- 服务端主 CMake (寻找 gRPC/Protobuf等公共库)
    ├── proto/                 <-- (推荐) 存放所有的 .proto 通信协议文件
    │   └── message.proto
    └── GateServer/            <-- 网关服务器
        ├── CMakeLists.txt     <-- GateServer 专属 CMake
        ├── GateServer.cpp
        ├── Server.cpp
        └── ...
```

---

### 第2步：配置顶层 `CMakePresets.json`

在项目根目录 `C++_Full-Stack_Chat_Project/` 下创建（或修改）`CMakePresets.json`。
这个文件的作用是告诉 VS2022 和 CMake：“去哪里找 vcpkg”。

```json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "windows-vcpkg",
      "generator": "Visual Studio 17 2022",
      "architecture": {
        "value": "x64",
        "strategy": "set"
      },
      "binaryDir": "${sourceDir}/build",
      "cacheVariables": {
        "CMAKE_TOOLCHAIN_FILE": "C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
      }
    }
  ]
}
```
*(注意：请确认 `C:/vcpkg/...` 路径与你电脑上实际的 vcpkg 安装路径一致)*

---

### 第3步：配置顶层 `CMakeLists.txt`

在项目根目录 `C++_Full-Stack_Chat_Project/` 下创建 `CMakeLists.txt`。
这个文件非常简单，只负责设置 C++ 标准并进入 `server` 目录。

```cmake
cmake_minimum_required(VERSION 3.15)
project(FullStackChat)

# 全局设置 C++17 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 注意：千万不要写 add_subdirectory(client)，因为它是 QMake
# 只进入 server 目录
add_subdirectory(server)
```

---

### 第4步：配置 `server/CMakeLists.txt`（核心）

在 `server/` 目录下创建 `CMakeLists.txt`。这个文件负责寻找所有的公共库（gRPC、Protobuf），并提取出生成代码所需的工具路径，供子服务器使用。

```cmake
# server/CMakeLists.txt

# 寻找 Protobuf 和 gRPC
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

# 提取 protoc 和 grpc_cpp_plugin 的绝对路径 (处理 Windows 下 Debug/Release 路径问题)
get_target_property(_protoc_path protobuf::protoc IMPORTED_LOCATION_RELEASE)
if(NOT EXISTS "${_protoc_path}")
    get_target_property(_protoc_path protobuf::protoc IMPORTED_LOCATION_DEBUG)
endif()

get_target_property(_grpc_plugin_path gRPC::grpc_cpp_plugin IMPORTED_LOCATION_RELEASE)
if(NOT EXISTS "${_grpc_plugin_path}")
    get_target_property(_grpc_plugin_path gRPC::grpc_cpp_plugin IMPORTED_LOCATION_DEBUG)
endif()

# 将路径暴露给子目录 (GateServer 等)
set(PROTOC_BIN ${_protoc_path} PARENT_SCOPE)
set(GRPC_PLUGIN_BIN ${_grpc_plugin_path} PARENT_SCOPE)

# 进入 GateServer 目录
add_subdirectory(GateServer)

# 以后开发了新的服务器，在这里加一行即可：
# add_subdirectory(LogicServer)
```

---

### 第5步：配置 `server/GateServer/CMakeLists.txt`

在 `server/GateServer/` 目录下创建 `CMakeLists.txt`。这里定义你的可执行文件，并链接相关的库。

```cmake
# server/GateServer/CMakeLists.txt

# 1. 声明你的源文件 (推荐手动列出，不要用 aux_source_directory，避免漏掉文件)
set(GATE_SERVER_SOURCES
    GateServer.cpp
    # Server.cpp
    # Session.cpp
)

# 2. 生成可执行文件
add_executable(GateServer ${GATE_SERVER_SOURCES})

# 3. 链接 gRPC 和 Protobuf 库
target_link_libraries(GateServer PRIVATE
    gRPC::grpc++
    protobuf::libprotobuf
)

# 4. (进阶) 如果你的 GateServer 需要解析 proto 文件，加上以下代码生成代码：
# 假设你的 proto 文件放在了 server/proto/message.proto
# set(PROTO_FILE ${CMAKE_SOURCE_DIR}/server/proto/message.proto)
# set(GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
# file(MAKE_DIRECTORY ${GENERATED_DIR})

# add_custom_command(
#     OUTPUT ${GENERATED_DIR}/message.pb.cc ${GENERATED_DIR}/message.grpc.pb.cc
#     COMMAND ${PROTOC_BIN} --proto_path=${CMAKE_SOURCE_DIR}/server/proto 
#             --cpp_out=${GENERATED_DIR} ${PROTO_FILE}
#     COMMAND ${PROTOC_BIN} --proto_path=${CMAKE_SOURCE_DIR}/server/proto 
#             --grpc_out=${GENERATED_DIR} --plugin=protoc-gen-grpc=${GRPC_PLUGIN_BIN} ${PROTO_FILE}
#     DEPENDS ${PROTO_FILE}
# )

# 将生成的代码加入编译目标
# target_sources(GateServer PRIVATE 
#     ${GENERATED_DIR}/message.pb.cc 
#     ${GENERATED_DIR}/message.grpc.pb.cc
# )
# target_include_directories(GateServer PRIVATE ${GENERATED_DIR})
```
*(注：上面第5步中关于 proto 生成的部分我先注释掉了，如果你现在还没有写 `.proto` 文件，先把注释的部分去掉，等后续需要用到时再放开。)*

---

### 第6步：在 VS2022 中加载项目

1. 打开 VS2022，选择 **“打开本地文件夹”**。
2. 选中你的根目录 `C++_Full-Stack_Chat_Project`。
3. VS2022 会自动读取 `CMakePresets.json`。如果没自动读取，在顶部菜单栏点击 `项目 -> CMake 设置`，在“配置预设”下拉框中选择 `windows-vcpkg`。
4. 等待右下角的进度条跑完（CMake 正在配置），输出窗口显示 `Configuring done` 和 `Generating done` 即代表成功。
5. 在右侧“解决方案资源管理器”中，你会看到 `GateServer` 这个可执行目标，右键点击它可以设为启动项目，然后按 F5 即可编译运行。

### 日常开发工作流总结：
* **写客户端**：打开 QtCreator，打开 `client/client.pro`，正常用 QMake 编译运行。
* **写服务端**：打开 VS2022，打开根目录文件夹，修改 `GateServer.cpp` 代码，直接 F5 编译运行。VS 会自动调用 vcpkg 下载好的 gRPC 和 Protobuf 库进行链接。



