# TGW Gateway Platform

基于 C++17 的高性能微服务网关与统一 RPC 调度平台。

本项目参考 tRPC-Cpp 的分层架构思想，围绕微服务网关核心场景实现：

- HTTP 接入
- 配置化路由
- 统一响应封装
- Protobuf 服务接口
- 本地 RPC 调度
- JWT 风格鉴权
- 固定窗口限流
- 重试、超时、熔断、降级
- Prometheus 指标
- TraceId 链路追踪
- Docker Compose 部署
- hey / wrk 压测

## 1. 架构图

```text
Client
  |
  v
Boost.Beast HTTP Server
  |
  v
Router
  |
  v
GatewayHandler
  |
  +--> AuthFilter
  |
  +--> RateLimitFilter
  |
  +--> GovernanceUpstreamClient
          |
          +--> LocalRpcUpstreamClient
                    |
                    +--> UserService
                    +--> FileMetaService
                    +--> TaskService