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

# ==========================================
# 自动拷贝配置文件到运行目录
# ==========================================
# 1. 定义配置文件的源路径 (config.ini 放在 StatusServer 目录下)
set(CONFIG_FILE ${CMAKE_CURRENT_SOURCE_DIR}/config.ini)

# 2. 添加自定义命令：在编译 StatusServer 之后，将 config.ini 拷贝到 .exe 所在目录
add_custom_command(TARGET StatusServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CONFIG_FILE}
    $<TARGET_FILE_DIR:StatusServer>
)

