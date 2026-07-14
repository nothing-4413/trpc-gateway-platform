# TGW Gateway Platform

基于 tRPC-Cpp 架构思想实现的高性能微服务网关与统一 RPC 调度平台。

本项目面向 C++ 后端简历展示，参考 tRPC-Cpp 的 Runtime / Transport / RPC / Plugin 分层思想，围绕微服务网关核心场景实现 HTTP 接入、配置化路由、统一鉴权、限流、服务治理、可观测性、管理接口、部署配置和压测验证。

## 核心能力

- HTTP Gateway：基于 Boost.Asio / Boost.Beast 实现异步 HTTP 接入，`io_threads` 负责网络 IO，`worker_threads` 负责业务分发，支持 Keep-Alive、读写超时、Body Limit 和优雅退出。
- 配置化路由：通过 YAML 配置前缀路由、上游服务、超时时间和路径裁剪。
- RPC 服务抽象：使用 Protobuf 定义 UserService、FileMetaService、TaskService，UserService 支持本地实现与跨进程 tRPC-like TCP RPC 实现切换。
- Gateway 到 Local RPC：将 HTTP JSON / Query 转换为 Protobuf 请求并调用本地服务实现。
- Token 鉴权与 RBAC：登录签发携带 role claim 的 HS256 JWT，网关统一校验 Authorization Bearer token，并按路径规则做角色权限控制。
- MySQL 持久化：UserService 支持内存仓储与 MySQL 用户表仓储切换。
- 限流：支持进程内固定窗口和 Redis 分布式固定窗口，支持默认规则与路径级规则，超限返回 42900。
- 服务治理：支持可取消 Deadline、重试、熔断和 fallback 降级。
- Prometheus metrics：暴露 `/metrics`，记录请求数、状态码、耗时等指标。
- Debug tracing / OpenTelemetry：暴露 `/debug/traces`，用于查看请求链路，支持 OTLP/HTTP 导出到 Jaeger，生产默认不回传调试响应头。
- Admin 接口：暴露 `/admin/runtime`、`/admin/routes`、`/admin/features`，需要 Bearer token 访问。
- Docker Compose 配置：提供 Prometheus / Grafana 可观测性部署文件。
- Benchmark：提供 hey / wrk 压测脚本与 CentOS 实测结果。
- Testing / CI：接入 GoogleTest 单元测试和 GitHub Actions 自动构建验证。

## 架构图

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
```

## 模块进度

1. 项目初始化
2. HTTP 网关
3. 配置化路由
4. RPC 服务模块
5. Gateway 与 Local RPC 打通
6. Token 鉴权
7. 固定窗口限流
8. Prometheus metrics
9. Admin 管理接口
10. 服务治理：超时、重试、熔断、fallback 降级
11. Debug tracing
12. Docker Compose 部署文件和可观测性配置
13. benchmark 压测脚本和 run_results 运行结果
14. 最终收尾：构建问题排查、代码一致性检查、提交记录整理、项目说明
15. 标准 JWT、生产 Debug Header 收口、Admin 接口保护
16. 异步 HTTP Server：`io_threads` 真正承担网络 IO
17. HTTP 连接生命周期治理：Keep-Alive、读写超时、Body Limit、优雅退出
18. 可取消 Deadline：Deadline 状态贯穿 Gateway、治理层、本地 RPC 和服务实现
19. 跨进程 UserService：独立 `tgw_user_service` 进程 + Gateway 远程 RPC client
20. Redis 分布式限流：基于 Redis `INCR` / `EXPIRE` / `TTL` 实现跨实例固定窗口限流
21. MySQL 持久化与 RBAC：用户仓储抽象、MySQL 用户表、JWT role claim、路径级权限控制
22. GoogleTest 与 GitHub Actions：JWT/RBAC 单测、CMake 构建、`ctest` 自动验证
23. OpenTelemetry 导出到 Jaeger：完成 span 后异步 OTLP/HTTP 导出到 Jaeger `/v1/traces`

## 构建运行

CentOS 验证路径：

```bash
cd /root/code/trpc-gateway-platform
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
```

运行网关：

```bash
pkill -9 tgw_gateway 2>/dev/null || true
nohup ./build/tgw_gateway --config=configs/gateway.yaml > gateway.log 2>&1 &
sleep 1
curl -v --max-time 3 http://127.0.0.1:8080/health
```

跨进程 UserService 验证：

```bash
./build/tgw_user_service --host=0.0.0.0 --port=9001
# 将 configs/gateway.yaml 中 user_service_rpc.enabled 改为 true 后启动网关
./build/tgw_gateway --config=configs/gateway.yaml
```

MySQL / RBAC 验证：

```bash
# 启动 MySQL 后，将 configs/gateway.yaml 中 mysql.enabled 改为 true
# admin / 123456 具备 admin 角色，alice / 123456 为普通 user 角色
curl -s -X POST http://127.0.0.1:8080/api/user/login \
  -H "Content-Type: application/json" \
  -d '{"username":"alice","password":"123456"}'
```

单元测试：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

健康检查成功返回：

```json
{"code":0,"message":"ok","data":{"status":"ok","service":"tgw-gateway"}}
```

## 常用接口

- `GET /health`
- `POST /api/user/login`
- `GET /api/user/profile?user_id=10001`
- `GET /metrics`
- `GET /debug/traces`
- `GET /admin/runtime`，需要 Bearer token
- `GET /admin/routes`，需要 Bearer token
- `GET /admin/features`，需要 Bearer token

登录示例：

```bash
curl -s -X POST http://127.0.0.1:8080/api/user/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"123456"}'
```

携带 token 访问：

```bash
curl -s "http://127.0.0.1:8080/api/user/profile?user_id=10001" \
  -H "Authorization: Bearer <token>"
```

## 压测结果摘要

CentOS 原生 CMake 编译运行验证结果保存在 `run_results/`。

- `/health`，1000 请求，20 并发：约 30658 req/s，全部 200。
- `/health`，5000 请求，50 并发：约 32681 req/s，全部 200。
- `/api/user/profile`，80 请求，10 并发：约 8590 req/s，全部 200。
- `/api/user/profile`，1000 请求，20 并发：20 个 200，980 个 429，用于验证限流生效。
- `/api/user/login` 限流验证：前 20 次返回 200，后 10 次返回 429。
- 服务治理验证：出现 retry failed、circuit breaker open、fallback degraded 等关键日志，验证通过。

## 目录说明

- `include/tgw/`：公共头文件与核心模块接口。
- `src/`：网关、服务、鉴权、治理、可观测性、管理接口实现。
- `proto/`：Protobuf IDL。
- `configs/`：网关 YAML 配置。
- `deploy/`：Docker Compose、Prometheus、Grafana 配置。
- `benchmark/`：压测和验证脚本。
- `run_results/`：CentOS 实测结果。
- `docs/`：部署、性能、最终总结和面试材料。

## 说明

Docker 与 Docker Compose 已在 CentOS 中验证可用，但 Docker Hub / registry 镜像拉取受网络影响较大。因此最终项目验证采用 CentOS 原生 CMake 编译运行方式，Docker Compose 文件作为部署与可观测性栈配置保留。
