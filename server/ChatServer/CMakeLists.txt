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
    ChatGrpcClient.cpp
    UserMgr.cpp
    ChatServiceImpl.cpp
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

# 每次构建前复制配置，支持在 configure 之后创建 config.ini
copy_config_to_target(ChatServer "${CMAKE_CURRENT_SOURCE_DIR}/config.ini")

