# server/ChatServer/CMakeLists.txt

# 1.声明源文件
set(CHAT_SERVER_SOURCES
	ChatServer.cpp
    ConfigMgr.cpp
    MsgNode.cpp
    AsioIOServicePool.cpp
    CServer.cpp
    CSession.cpp
    LogicSystem.cpp
    MysqlDao.cpp
    MysqlMgr.cpp
    RedisMgr.cpp
    StatusGrpcClient.cpp
)

# 2.生成可执行文件
add_executable(ChatServer ${CHAT_SERVER_SOURCES})

# 确保无论依赖传递是否生效，编译器都能找到 proto 头文件
target_link_directories(ChatServer PRIVATE ${CMAKE_BINARY_DIR}/generated)

# 3.链接库文件
target_link_libraries(ChatServer
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
# 1. 定义配置文件的源路径 (config.ini 放在 ChatServer 目录下)
set(CONFIG_FILE ${CMAKE_CURRENT_SOURCE_DIR}/config.ini)

# 2. 添加自定义命令：在编译 ChatServer 之后，将 config.ini 拷贝到 .exe 所在目录
add_custom_command(TARGET ChatServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CONFIG_FILE}
    $<TARGET_FILE_DIR:ChatServer>
)

