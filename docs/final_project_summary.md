# 最终项目完成度总结

## 项目定位

TGW Gateway Platform 是一个基于 tRPC-Cpp 架构思想实现的 C++ 后端简历项目，目标是展示微服务网关、统一 RPC 调度、服务治理和可观测性能力。

项目采用 C++17、CMake、Boost.Asio、Boost.Beast、Protobuf、yaml-cpp、spdlog、nlohmann_json 等技术栈，实现了从 HTTP 接入到本地 RPC 服务调用，再到鉴权、限流、治理、监控、调试、部署和压测验证的完整闭环。

## 已完成模块

| 模块 | 状态 | 说明 |
| --- | --- | --- |
| 项目初始化 | 已完成 | CMake、目录结构、配置、日志、启动入口 |
| HTTP 网关 | 已完成 | Boost.Beast 异步 HTTP Server、请求解析、响应封装，io_threads 负责网络 IO |
| HTTP 连接生命周期治理 | 已完成 | Keep-Alive、读写超时、Body Limit、SIGINT/SIGTERM 优雅退出 |
| 可取消 Deadline | 已完成 | Deadline 状态贯穿 Gateway、治理层、本地 RPC 和服务实现，可中断 debug sleep 与 retry backoff |
| 配置化路由 | 已完成 | YAML 路由、前缀匹配、路径裁剪 |
| RPC 服务模块 | 已完成 | UserService、FileMetaService、TaskService |
| 跨进程 UserService | 已完成 | 独立 `tgw_user_service` 进程，Gateway 可通过 tRPC-like TCP JSON RPC client 调用 |
| Gateway 与 Local RPC 打通 | 已完成 | HTTP JSON / Query 转 Protobuf 并调用本地服务 |
| Token 鉴权 | 已完成 | 登录签发 HS256 JWT，网关统一校验 |
| MySQL 持久化与 RBAC | 已完成 | 用户仓储抽象、MySQL 用户表、JWT role claim、路径级角色权限控制 |
| GoogleTest 与 CI | 已完成 | JWT/RBAC 单测，GitHub Actions 执行 CMake 构建和 `ctest` |
| OpenTelemetry / Jaeger | 已完成 | FinishedSpan 异步转换为 OTLP/HTTP JSON payload 并导出到 Jaeger |
| Release 压测 | 已完成 | 提供 Release 构建、启动、hey 压测、metrics/traces 采集脚本 |
| 限流 | 已完成 | 进程内固定窗口与 Redis 分布式固定窗口，默认规则与路径级规则，超限返回 42900 |
| Prometheus metrics | 已完成 | `/metrics` 暴露请求数、状态码、耗时指标 |
| Admin 管理接口 | 已完成 | `/admin/runtime`、`/admin/routes`、`/admin/features` |
| 服务治理 | 已完成 | 可取消 Deadline、重试、熔断、fallback 降级 |
| Debug tracing | 已完成 | `/debug/traces` 调试链路 |
| 生产安全收口 | 已完成 | Admin 接口强制鉴权，生产默认不暴露调试响应头 |
| Docker Compose 与可观测性配置 | 已完成 | Prometheus、Grafana 配置 |
| benchmark 与 run_results | 已完成 | hey/wrk 脚本、CentOS 实测结果 |
| 最终收尾 | 已完成 | README、结果说明、项目总结、面试材料 |

## 构建问题排查记录

Windows 本地环境缺少 `cmake`、`protoc`、C++ 编译器，因此 Windows 侧主要承担代码同步、patch 应用、脱敏检查和 GitHub 推送。

CentOS 虚拟机中完成了真实构建和运行验证：

```bash
cd /root/code/trpc-gateway-platform
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
```

CentOS 环境曾出现根分区空间不足，已通过第二块 20GB 磁盘 `/dev/sdb` 挂载到 `/data` 解决，并将 `/root/code` 迁移为 `/data/code` 的软链接。

Docker 已安装并验证 `docker run hello-world` 成功，`docker compose version v2.27.0` 可用。但由于 Docker Hub / registry 网络不稳定，最终验证采用 CentOS 原生 CMake 编译运行方式。

## 代码一致性检查

已完成以下检查：

- Windows 工作区 `main` 与 `origin/main` 同步。
- CentOS patch 对应提交已应用到当前 HEAD。
- `run_results/verify_governance.txt` 中 token 已脱敏为 `<redacted>`。
- `run_results/` 包含压测、限流、治理、metrics、traces 验证结果。
- `deploy/` 包含 Docker Compose、Prometheus、Grafana 配置。
- `benchmark/` 包含压测与验证脚本。

## 提交记录整理

关键提交如下：

- `项目整体架构设计与目录结构`
- `Gateway 网关模块`
- `Gateway 路由规则与请求转发设计`
- `模块 4：RPC 服务模块`
- `模块 5：Gateway 到本地 RPC 调用`
- `模块 6：统一鉴权模块`
- `模块 7：服务治理限流模块`
- `模块 8：Prometheus 指标模块`
- `管理接口模块`
- `服务治理增强模块`
- `可观测性增强`
- `部署与可观测性栈`
- `压测与性能报告`
- `补充 CentOS 运行验证与压测结果`
- `模块 14：最终项目收尾`
- `模块 15：JWT 与管理接口安全加固`
- `模块 16：异步 HTTP Server`
- `模块 17：HTTP 连接生命周期治理`
- `模块 18：可取消 Deadline`
- `模块 19：跨进程 UserService`
- `模块 20：Redis 分布式限流`
- `模块 21：MySQL 持久化与 RBAC`
- `模块 22：GoogleTest 与 GitHub Actions`
- `模块 23：OpenTelemetry 导出到 Jaeger`
- `模块 24：Release 环境压测`

## 验证结果摘要

- `/health` 1000 请求、20 并发：约 30658 req/s，全部 200。
- `/health` 5000 请求、50 并发：约 32681 req/s，全部 200。
- `/api/user/profile` 80 请求、10 并发：约 8590 req/s，全部 200。
- `/api/user/profile` 1000 请求、20 并发：20 个 200，980 个 429，用于验证限流生效。
- `/api/user/login` 限流验证：前 20 次 200，后 10 次 429。
- 服务治理验证：重试失败、熔断打开、fallback 降级均已出现并记录。

## 当前完成度

项目已达到简历项目可交付状态：核心功能完整、模块边界清晰、具备可观测性、具备部署配置、具备压测结果、具备运行验证记录。

后续如继续增强，可以考虑：

- 接入真实 Redis 存储 token 和限流计数。
- 接入真实 MySQL 存储用户、文件元数据和任务。
- 将 Local RPC 替换为跨进程 gRPC / tRPC Client。
- 补充自动化单元测试和 CI。

当前增强进度已继续补齐 HTTP Server 的连接生命周期治理：支持配置化 Keep-Alive、读超时、写超时、请求体大小限制，并在收到 SIGINT/SIGTERM 后关闭 acceptor、停止 io_context，实现更接近生产网关的退出路径。

服务治理超时也已升级为可取消 Deadline：每个 `ForwardContext` 创建共享 Deadline 状态，并继续传递到 `RpcContext`，治理层、等待点、本地 RPC 转发和服务实现都会检查取消状态，避免只在调用结束后被动比较耗时。

UserService 已从纯进程内实现扩展为可跨进程部署：新增 `tgw_user_service` 独立进程，Gateway 侧通过 `RemoteUserServiceClient` 发送 tRPC-like TCP JSON RPC 请求，并可用 `user_service_rpc.enabled` 在本地实现和远程实现之间切换。

限流模块已支持 Redis 分布式固定窗口：开启 `rate_limit.redis.enabled` 后，网关使用 Redis `INCR`、`EXPIRE` 和 `TTL` 维护跨实例计数；Redis 不可用时可按 `fail_open` 配置回退到本地限流或直接拒绝请求。

用户服务已引入 Repository 抽象，默认使用内存仓储，开启 `mysql.enabled` 后可通过 MySQL 用户表查询登录和用户资料；登录成功后 JWT 携带 role claim，网关在 AuthFilter 中根据 RBAC 路径规则执行角色权限控制。

测试体系已补齐基础自动化：新增 GoogleTest 单测覆盖 JWT 签发校验、token 篡改拒绝、RBAC admin/user 权限分支，并通过 GitHub Actions 在 Ubuntu 环境执行依赖安装、CMake 构建和 `ctest`。

Tracing 已增强为可导出：网关保留 `/debug/traces` 内存调试能力，同时支持开启 `tracing.otlp_exporter_enabled`，将完成的 span 异步转换为 OpenTelemetry OTLP/HTTP JSON payload 并发送到 Jaeger collector 的 `/v1/traces`。

Release 压测流程已脚本化：`benchmark/run_release_benchmark.sh` 会使用 `-DCMAKE_BUILD_TYPE=Release` 构建网关，启动服务后执行 hey 压测，并采集 health、profile、metrics 和 traces 结果到 `run_results/release_*`。
