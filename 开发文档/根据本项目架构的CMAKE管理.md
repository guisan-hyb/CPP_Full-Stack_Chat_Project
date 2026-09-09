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



---

## 回顾多层CMake

这是一个非常经典的 **“洋葱式”分层 CMake 结构**。每一层都有明确的“职责边界”，互不越界，又通过 CMake 的**作用域（Scope）**规则紧密配合。

为了帮你彻底理清，我们不只看“它写了什么”，更要看 **“它为什么在这里写，而不是写在上层或下层”**。

我用一个**建筑公司**的比喻来帮你回顾这三层架构：

---

### 第 0 层（最外层）：`CMakePresets.json` —— “公司营业执照与资质证书”
- **位置**：项目根目录。
- **职责**：**环境适配**。它不定义任何代码或库，只告诉 CMake“我这台电脑的编译环境长什么样”。
- **核心动作**：
  - 指定生成器（`Visual Studio 17 2022`）。
  - 指定架构（`x64`）。
  - **最关键**：指定 `vcpkg` 的工具链文件。这相当于告诉 CMake：“所有 `find_package` 都去 C:/vcpkg 里找，别去系统默认路径”。
- **为什么放最外层**：因为环境是全局的，无论是 Server 还是 Client，都得用同一套 vcpkg。

---

### 第 1 层（根目录）：`CMakeLists.txt` —— “集团总公司”
- **位置**：项目根目录。
- **职责**：**全局政策与项目入口**。它不管具体业务，只管“统一思想”和“引入子部门”。
- **核心动作**：
  1. `cmake_minimum_required` 和 `project`：确定工程名，开启 C++17 标准。
  2. `find_package(Protobuf ...)` 和 `find_package(gRPC ...)`：**注意！这里查找了，但并没有立刻用**。它的作用是**探测环境**，把 `Protobuf_PROTOC_EXECUTABLE` 等变量缓存到内存里，方便子目录直接使用。
  3. `add_subdirectory(server)`：把控制权交给子目录。它完全不关心 Server 里面是生成 proto，还是写业务逻辑。
- **为什么这样设计**：如果将来你新增一个 `TestServer`，你只需要在根目录加一行 `add_subdirectory(test_server)`，根目录的通用设置（C++标准、vcpkg环境）会自动继承，不需要重复写。

---

### 第 2 层（`server/CMakeLists.txt`）—— “基础设施研发部”
- **位置**：`server` 目录下。
- **职责**：**定义公共基础设施（通信协议库）**。它把 `.proto` 文件变成 C++ 代码，并打包成供所有人调用的“库”。
- **核心动作（这是你最关心的）**：
  1. **再次 `find_package`**：为了安全（如果根目录没找，这里能兜底），并利用之前缓存的路径。
  2. **查找工具链**：动态寻找 `protoc` 和 `grpc_cpp_plugin`（你改了动态查找，这里就体现了价值）。
  3. **定义生成逻辑**：`add_custom_command` 规定怎么把 `.proto` 变成 `.cc/.h`。
  4. **创建“库目标”**：`add_library(chat_proto STATIC ...)`。它把生成的文件编译成 `chat_proto.lib`。
  5. **传递依赖**：`target_include_directories(... PUBLIC ${GENERATED_DIR})`。它负责“生”出文件，并承诺“谁用我，我就把 proto 头文件路径送给他”。
  6. **引入子目录**：`add_subdirectory(GateServer)`。
- **为什么放这一层**：`chat_proto` 是所有服务器（GateServer、LoginServer）都要用的底层通信协议。放在这里，所有子服务器都能共享这一个库，避免重复编译 `.proto`，极大提速。

---

### 第 3 层（`server/GateServer/CMakeLists.txt`）—— “具体业务项目部”
- **位置**：`server/GateServer` 目录下。
- **职责**：**实现具体业务功能**。只关心“我这个网关服务器需要哪些 .cpp 文件，我要链接哪些库”。
- **核心动作**：
  1. `set(GATE_SERVER_SOURCES ...)`：列出手写的业务逻辑代码（GateServer.cpp、LogicSystem.cpp 等）。
  2. `add_executable(GateServer ...)`：声明要生成一个可执行文件。
  3. **手动加包含路径（你刚做的）**：`target_include_directories(GateServer PRIVATE ${CMAKE_BINARY_DIR}/generated)`。虽然理论上第 2 层的 `PUBLIC` 能传递，但在 VS 生成器中为了绝对保险，你在这一层**显式声明自己的包含路径**（这是一种“防御性编程”）。
  4. `target_link_libraries`：**这里才是真正“组装”的地方**。把 `chat_proto`、`Boost`、`nlohmann_json` 全部链接进来。
- **为什么放这一层**：GateServer 只负责处理 HTTP/WebSocket 业务逻辑，它不需要知道 `protoc` 怎么调用，也不需要知道 `vcpkg` 怎么配。它只需要拿到 `chat_proto.lib` 和头文件，然后专注写 `GetVerifyCode` 的实现就行了。

---

### 跨层级的“变量传递”法则（帮你理解作用域）

你可能注意到有些变量写了 `PARENT_SCOPE`，有些没有。这里有一个铁律：

1. **向下传递（父→子）**：**默认自动**。根目录定义的 `CMAKE_CXX_STANDARD`，`server/CMakeLists.txt` 和 `GateServer/CMakeLists.txt` 都能直接读到。根目录 `find_package` 找到的变量，子目录直接就能用。
2. **向上传递（子→父）**：**必须手动**。如果 `server/CMakeLists.txt` 定义了个变量想让根目录用，必须用 `set(... PARENT_SCOPE)`（你原来代码里写过，后来优化去掉了，因为不需要了）。
3. **同级传递（兄→弟）**：**通过 Target（目标）**。`server` 定义了 `chat_proto` 目标，`GateServer` 通过 `target_link_libraries` 来“认大哥”，从而自动继承大哥的 PUBLIC 属性和头文件路径。

---

### 总结记忆卡片（便于复习）

| 层级        | 文件位置             | 角色定位     | 核心命令                                  | 这一层关心什么？                                 |
| :---------- | :------------------- | :----------- | :---------------------------------------- | :----------------------------------------------- |
| **第 0 层** | 根目录               | **环境配置** | `CMakePresets.json`                       | 编译器在哪？vcpkg 在哪？                         |
| **第 1 层** | 根目录               | **总指挥部** | `project`, `add_subdirectory`             | 项目叫什么？用 C++ 几？有几个大模块？            |
| **第 2 层** | `/server`            | **部件工厂** | `add_custom_command`, `add_library`       | `.proto` 怎么变成 C++？`chat_proto` 怎么造出来？ |
| **第 3 层** | `/server/GateServer` | **组装车间** | `add_executable`, `target_link_libraries` | 业务代码在哪？需要链接哪些库才能跑起来？         |

以后你看到任何大型 CMake 工程，试着用这 4 层去套。**绝大多数项目都不超过 4 层**，超过 4 层就是过度设计了。你现在已经能清晰地分层看问题了，这比背 100 条 API 指令都管用！👍



