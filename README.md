# trpc-gateway-platform

基于 tRPC-Cpp 架构思想实现的高性能微服务网关与统一 RPC 调度平台。

## 技术栈

- C++17 / C++20
- CMake
- Protobuf
- HTTP / gRPC / tRPC 协议思想
- Redis
- MySQL
- Prometheus
- OpenTelemetry / Jaeger
- Docker Compose
- wrk / hey / JMeter

## 当前模块

### Module 1：项目初始化

已完成：

- CMake 工程初始化
- 目录结构设计
- YAML 配置文件设计
- spdlog 日志模块
- 程序启动入口

## 构建运行

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
./build/tgw_gateway --config=configs/gateway.yaml