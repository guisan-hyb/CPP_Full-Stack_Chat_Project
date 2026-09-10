# server/CMakeLists.txt
cmake_minimum_required(VERSION 3.20)  # 提到 3.20 以更好支持 generator expression

find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(Boost REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# ------------------------------------------------------------ 
# 1. protoc 路径（用 generator expression 更可靠）
# ------------------------------------------------------------
if(NOT Protobuf_PROTOC_EXECUTABLE)
    # vcpkg 也导出了 protobuf::protoc 这个 imported target
    set(Protobuf_PROTOC_EXECUTABLE $<TARGET_FILE:protobuf::protoc>)
endif()
message(STATUS "Using protoc: ${Protobuf_PROTOC_EXECUTABLE}")

# 不再手动 get_target_property，直接在 add_custom_command 里用 $<TARGET_FILE:...>

# ------------------------------------------------------------ 
# 2. proto 文件列表
# ------------------------------------------------------------
set(PROTO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/proto)
set(PROTO_FILES
    ${PROTO_DIR}/message.proto
)

set(GENERATED_DIR ${CMAKE_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIR})

# 生成源文件列表
set(GENERATED_SRCS "")
set(GENERATED_HDRS "")
foreach(PROTO ${PROTO_FILES})
    get_filename_component(BASENAME ${PROTO} NAME_WE)
    list(APPEND GENERATED_SRCS
        ${GENERATED_DIR}/${BASENAME}.pb.cc
        ${GENERATED_DIR}/${BASENAME}.grpc.pb.cc
    )
    list(APPEND GENERATED_HDRS
        ${GENERATED_DIR}/${BASENAME}.pb.h
        ${GENERATED_DIR}/${BASENAME}.grpc.pb.h
    )
endforeach()

# ------------------------------------------------------------ 
# 3. 自定义命令：用 generator expression 拿 plugin 路径
# ------------------------------------------------------------
add_custom_command(
    OUTPUT ${GENERATED_SRCS} ${GENERATED_HDRS}
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
        "--proto_path=${PROTO_DIR}"
        "--cpp_out=${GENERATED_DIR}"
        "--grpc_out=${GENERATED_DIR}"
        "--plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>"
        ${PROTO_FILES}
    DEPENDS ${PROTO_FILES} gRPC::grpc_cpp_plugin
    COMMENT "Running protoc with gRPC plugin"
    VERBATIM
)

# ------------------------------------------------------------ 
# 4. chat_proto 静态库
# ------------------------------------------------------------
add_library(chat_proto STATIC ${GENERATED_SRCS})

target_include_directories(chat_proto PUBLIC ${GENERATED_DIR})

target_link_libraries(chat_proto PUBLIC
    gRPC::grpc++
    protobuf::libprotobuf
)

# ------------------------------------------------------------ 
# 5. 子目录
# ------------------------------------------------------------
add_subdirectory(GateServer)
# 不用加VerifyServer，因为它是node.js
