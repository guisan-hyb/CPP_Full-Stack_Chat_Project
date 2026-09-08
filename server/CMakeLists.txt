# server/CMakeLists.txt
cmake_minimum_required(VERSION 3.15)

find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(Boost REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# ------------------------------------------------------------
# 1. 获取 protoc 可执行文件（使用 Protobuf 提供的变量）
# ------------------------------------------------------------
if(NOT Protobuf_PROTOC_EXECUTABLE)
    message(FATAL_ERROR "Protobuf_PROTOC_EXECUTABLE not set. Check Protobuf installation.")
endif()
message(STATUS "Using protoc: ${Protobuf_PROTOC_EXECUTABLE}")

# 2. 获取 grpc_cpp_plugin
#    a) 优先从 gRPC 目标获取（如果存在）
#    b) 否则用 find_program 在常见位置查找
if(TARGET gRPC::grpc_cpp_plugin)
    get_target_property(GRPC_CPP_PLUGIN_EXECUTABLE gRPC::grpc_cpp_plugin IMPORTED_LOCATION)
    if(NOT GRPC_CPP_PLUGIN_EXECUTABLE)
        get_target_property(GRPC_CPP_PLUGIN_EXECUTABLE gRPC::grpc_cpp_plugin LOCATION)
    endif()
endif()

if(NOT GRPC_CPP_PLUGIN_EXECUTABLE)
    find_program(GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin
        HINTS ${gRPC_DIR}/../../../tools/grpc
              ${gRPC_DIR}/../../tools/grpc
        REQUIRED
    )
endif()
message(STATUS "Using grpc_cpp_plugin: ${GRPC_CPP_PLUGIN_EXECUTABLE}")

# ------------------------------------------------------------
# 3. 定义 proto 文件列表（方便扩展）
# ------------------------------------------------------------
set(PROTO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/proto)
set(PROTO_FILES
    ${PROTO_DIR}/message.proto
    # 未来可增加其他 .proto 文件
)

set(GENERATED_DIR ${CMAKE_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIR})

# 生成源文件列表（用于 add_library）
foreach(PROTO ${PROTO_FILES})
    get_filename_component(BASENAME ${PROTO} NAME_WE)
    list(APPEND GENERATED_SRCS
        ${GENERATED_DIR}/${BASENAME}.pb.cc
        ${GENERATED_DIR}/${BASENAME}.pb.h
        ${GENERATED_DIR}/${BASENAME}.grpc.pb.cc
        ${GENERATED_DIR}/${BASENAME}.grpc.pb.h
    )
endforeach()

# ------------------------------------------------------------
# 4. 添加自定义命令，生成 .pb 和 .grpc.pb 文件
# ------------------------------------------------------------
add_custom_command(
    OUTPUT ${GENERATED_SRCS}
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
        --proto_path=${PROTO_DIR}
        --cpp_out=${GENERATED_DIR}
        --grpc_out=${GENERATED_DIR}
        --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN_EXECUTABLE}
        ${PROTO_FILES}
    DEPENDS ${PROTO_FILES}
    COMMENT "Running protoc with gRPC plugin"
    VERBATIM
)

# ------------------------------------------------------------
# 5. 创建静态库，包含生成的代码
# ------------------------------------------------------------
add_library(chat_proto STATIC ${GENERATED_SRCS})
target_include_directories(chat_proto PUBLIC ${GENERATED_DIR})
target_link_libraries(chat_proto PUBLIC
    gRPC::grpc++
    protobuf::libprotobuf
)

# ------------------------------------------------------------
# 6. 引入子目录 GateServer
# ------------------------------------------------------------
add_subdirectory(GateServer)

