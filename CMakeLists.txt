cmake_minimum_required(VERSION 3.20)

project(trpc_gateway_platform VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

include(FetchContent)

FetchContent_Declare(
    spdlog
    URL https://github.com/gabime/spdlog/archive/refs/tags/v1.14.1.tar.gz
)

FetchContent_Declare(
    yaml-cpp
    URL https://github.com/jbeder/yaml-cpp/archive/refs/tags/0.8.0.tar.gz
)

FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/archive/refs/tags/v3.11.3.tar.gz
)

FetchContent_MakeAvailable(spdlog yaml-cpp nlohmann_json)

find_package(Boost REQUIRED COMPONENTS system)
find_package(Protobuf REQUIRED)
find_package(Threads REQUIRED)

set(PROTO_FILES
    ${PROJECT_SOURCE_DIR}/proto/common.proto
    ${PROJECT_SOURCE_DIR}/proto/user.proto
    ${PROJECT_SOURCE_DIR}/proto/file_meta.proto
    ${PROJECT_SOURCE_DIR}/proto/task.proto
)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

add_library(tgw_proto
    ${PROTO_SRCS}
    ${PROTO_HDRS}
)

target_include_directories(tgw_proto
    PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(tgw_proto
    PUBLIC
        protobuf::libprotobuf
)

add_library(tgw_rpc_core
    src/service/user_repository.cc
    src/service/user_service.cc
    src/service/file_meta_service.cc
    src/service/task_service.cc
)

target_include_directories(tgw_rpc_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(tgw_rpc_core
    PUBLIC
        tgw_proto
        spdlog::spdlog
)

add_executable(tgw_gateway
    src/main.cc
    src/admin/admin_handler.cc
    src/auth/auth_filter.cc
    src/auth/token_service.cc
    src/common/config.cc
    src/common/logger.cc
    src/gateway/router.cc
    src/gateway/http_server.cc
    src/gateway/route_rule.cc
    src/gateway/upstream_client.cc
    src/gateway/gateway_handler.cc
    src/gateway/local_rpc_upstream_client.cc
    src/governance/rate_limit_filter.cc
    src/governance/governance_upstream_client.cc
    src/observability/metrics.cc
    src/observability/tracing.cc
    src/service/remote_user_service_client.cc
)

target_include_directories(tgw_gateway
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(tgw_gateway
    PRIVATE
        spdlog::spdlog
        yaml-cpp
        nlohmann_json::nlohmann_json
        Boost::system
        Threads::Threads
        tgw_rpc_core
)

add_executable(tgw_rpc_demo
    src/rpc/rpc_demo.cc
    src/common/logger.cc
)

target_include_directories(tgw_rpc_demo
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(tgw_rpc_demo
    PRIVATE
        tgw_rpc_core
        tgw_proto
        spdlog::spdlog
)

add_executable(tgw_user_service
    src/service/user_service_server_main.cc
    src/common/logger.cc
)

target_include_directories(tgw_user_service
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(tgw_user_service
    PRIVATE
        tgw_rpc_core
        tgw_proto
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        Boost::system
        Threads::Threads
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(tgw_gateway PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    target_compile_options(tgw_rpc_demo PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    target_compile_options(tgw_user_service PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()
