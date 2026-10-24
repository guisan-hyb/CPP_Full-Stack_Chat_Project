# server/StatusServer/CMakeLists.txt

# 1. 声明你的源文件 (推荐手动列出，不要用 aux_source_directory，避免漏掉文件)
set(STATUS_SERVER_SOURCES
    ConfigMgr.cpp
    AsioIOServicePool.cpp
    RedisMgr.cpp
    MysqlDao.cpp
    MysqlMgr.cpp
    StatusServer.cpp
    StatusServiceImpl.cpp
    ChatGrpcClient.cpp
)

# 2. 生成可执行文件
add_executable(StatusServer ${STATUS_SERVER_SOURCES})

# 确保无论依赖传递是否生效，编译器都能找到 proto 头文件
target_include_directories(StatusServer PRIVATE ${CMAKE_BINARY_DIR}/generated)

# 3. 链接 gRPC 和 Protobuf 库
target_link_libraries(StatusServer PRIVATE
    gRPC::grpc++
    protobuf::libprotobuf
    Boost::asio
    Boost::boost
    nlohmann_json::nlohmann_json
    chat_proto # 自己生成的chat_proto 库 -> message.proto
    redis++::redis++   # <-- 加上 Redis
    unofficial::mysql-connector-cpp::connector
)

# 每次构建前复制配置，支持在 configure 之后创建 config.ini
copy_config_to_target(StatusServer "${CMAKE_CURRENT_SOURCE_DIR}/config.ini")

