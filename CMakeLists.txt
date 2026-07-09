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

FetchContent_MakeAvailable(spdlog yaml-cpp)

find_package(Boost REQUIRED COMPONENTS system)

add_executable(tgw_gateway
    src/main.cc
    src/common/config.cc
    src/common/logger.cc
    src/gateway/router.cc
    src/gateway/http_server.cc
)

target_include_directories(tgw_gateway
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(tgw_gateway
    PRIVATE
        spdlog::spdlog
        yaml-cpp
        Boost::system
        pthread
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(tgw_gateway PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()