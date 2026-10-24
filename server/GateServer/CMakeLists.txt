# server/GateServer/CMakeLists.txt

# 1. 声明你的源文件 (推荐手动列出，不要用 aux_source_directory，避免漏掉文件)
set(GATE_SERVER_SOURCES
    GateServer.cpp
    CServer.cpp
    HttpConnection.cpp
    LogicSystem.cpp
    VerifyGrpcClient.cpp
    ConfigMgr.cpp
    AsioIOServicePool.cpp
    RedisMgr.cpp
    MysqlDao.cpp
    MysqlMgr.cpp
    StatusGrpcClient.cpp
    PasswordHasher.cpp
)

# 2. 生成可执行文件
add_executable(GateServer ${GATE_SERVER_SOURCES})

# 确保无论依赖传递是否生效，编译器都能找到 proto 头文件
target_include_directories(GateServer PRIVATE ${CMAKE_BINARY_DIR}/generated)

find_package(unofficial-sodium CONFIG REQUIRED)

# 3. 链接 gRPC 和 Protobuf 库
target_link_libraries(GateServer PRIVATE
    gRPC::grpc++
    protobuf::libprotobuf
    Boost::asio
    Boost::boost
    nlohmann_json::nlohmann_json
    chat_proto # 自己生成的chat_proto 库 -> message.proto
    redis++::redis++   # <-- 加上 Redis
    unofficial::mysql-connector-cpp::connector
    unofficial-sodium::sodium
)

# 每次构建前复制配置，支持在 configure 之后创建 config.ini
copy_config_to_target(GateServer "${CMAKE_CURRENT_SOURCE_DIR}/config.ini")
