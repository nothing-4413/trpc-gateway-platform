cmake_minimum_required(VERSION 3.20)

# 项目名称
project(trpc_gateway_platform VERSION 0.1.0 LANGUAGES CXX)

# 使用 C++17。后续如果要用 coroutine，可以切换到 C++20。
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 生成 compile_commands.json，方便 VSCode / clangd 进行代码跳转和补全
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 默认构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# 第三方依赖拉取
include(FetchContent)

# spdlog：高性能 C++ 日志库
FetchContent_Declare(
    spdlog
    URL https://github.com/gabime/spdlog/archive/refs/tags/v1.14.1.tar.gz
)

# yaml-cpp：读取 YAML 配置文件
FetchContent_Declare(
    yaml-cpp
    URL https://github.com/jbeder/yaml-cpp/archive/refs/tags/0.8.0.tar.gz
)

FetchContent_MakeAvailable(spdlog yaml-cpp)

# 主程序
add_executable(tgw_gateway
    src/main.cc
    src/common/config.cc
    src/common/logger.cc
)

# 头文件目录
target_include_directories(tgw_gateway
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

# 链接第三方库
target_link_libraries(tgw_gateway
    PRIVATE
        spdlog::spdlog
        yaml-cpp
)

# 编译选项
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(tgw_gateway PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()