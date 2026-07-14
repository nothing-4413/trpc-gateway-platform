# 简历描述与面试讲解稿

## 简历项目描述

**基于 tRPC-Cpp 架构思想的高性能微服务网关与统一 RPC 调度平台**

基于 C++17、CMake、Boost.Asio、Boost.Beast、Protobuf、yaml-cpp、spdlog 实现微服务网关与统一 RPC 调度平台，参考 tRPC-Cpp 的 Runtime / Transport / RPC / Plugin 分层思想，完成 HTTP 接入、配置化路由、统一响应封装、Token 鉴权、固定窗口限流、服务治理、Prometheus 指标、Debug tracing、Admin 管理接口、Docker Compose 可观测性配置和 hey/wrk 压测验证。项目在 CentOS 环境原生 CMake 编译运行，`/health` 在 5000 请求、50 并发压测下达到约 32681 req/s，限流、熔断、降级、metrics、traces 均有运行结果留档。

## 简历要点

- 设计并实现基于 Boost.Beast 的异步 HTTP Gateway，封装统一 `HttpRequest` / `HttpResponse`，支持 request id 透传和统一 JSON 响应，并让 `io_threads` 承担网络 IO 事件循环。
- 补齐 HTTP 连接生命周期治理，支持 Keep-Alive、读写超时、Body Limit 和 SIGINT/SIGTERM 优雅退出。
- 实现 YAML 配置化路由系统，支持前缀匹配、最长路径优先、strip prefix 和 upstream 抽象。
- 使用 Protobuf 定义 User / FileMeta / Task 服务接口，实现 Gateway 到本地 RPC 服务的协议转换与调度，并将 UserService 扩展为可独立部署的跨进程 RPC 服务。
- 实现 Token 鉴权与 RBAC，登录接口签发携带 role claim 的 HS256 JWT，业务接口统一校验 Bearer token，并按路径规则做角色权限控制。
- 引入 UserRepository 抽象，支持内存用户仓储与 MySQL 用户表仓储切换。
- 实现进程内固定窗口和 Redis 分布式固定窗口限流，支持默认规则和路径级规则，超限统一返回 42900。
- 实现服务治理能力，包括可取消 Deadline、重试、熔断和 fallback 降级，降低下游异常对网关的影响。
- 接入 Prometheus metrics、Debug traces 和 Admin 管理接口，提升运行时可观测性。
- 编写 benchmark 脚本并在 CentOS 环境完成压测，保存完整 run_results。

## 面试讲解稿

这个项目是我参考 tRPC-Cpp 架构思想做的一个 C++ 微服务网关项目。它不是简单的 HTTP demo，而是围绕真实微服务网关的核心链路做了分层设计。

整体链路是：客户端请求先进入基于 Boost.Beast 实现的 HTTP Server，然后由 Router 做精确路由或进入 GatewayHandler。GatewayHandler 内部按顺序执行鉴权、限流、服务治理，最后通过 UpstreamClient 调用本地 RPC 服务。服务层用 Protobuf 定义接口，包括 UserService、FileMetaService 和 TaskService。

HTTP Server 采用异步 accept、异步 read 和异步 write，网络事件由 `io_threads` 承担，业务逻辑再投递到 worker 线程池。连接层支持 Keep-Alive 复用，同时配置读超时、写超时和请求体大小限制，避免慢请求、慢写出或超大 body 长时间占用资源；进程收到 SIGINT/SIGTERM 后会关闭 acceptor 并停止 io_context，实现可控退出。

UserService 后续被拆成了可独立运行的 `tgw_user_service` 进程。Gateway 可以通过配置选择进程内 `UserServiceImpl`，也可以使用 `RemoteUserServiceClient` 走 tRPC-like TCP JSON RPC 调用远程 UserService，从而把网关和用户服务的进程边界拆开。

路由层是配置化的，配置文件里可以定义 `/api/user`、`/api/file`、`/api/task` 这样的前缀路由，每条路由可以配置 upstream、timeout 和是否 strip prefix。路由匹配时采用最长前缀优先，避免 `/api/user/admin` 被 `/api/user` 提前匹配。

鉴权模块采用过滤器设计。登录接口是 public path，登录成功后生成 HS256 JWT，token 中包含 user_id、username 和 role。其他业务接口和 Admin 接口要求携带 `Authorization: Bearer <token>`。鉴权成功后，网关会把用户信息和角色注入 `ForwardContext`，并根据 RBAC 路径规则判断当前角色是否允许访问，比如 `/admin` 和 `/api/task` 默认只允许 admin。

用户数据层引入了 `IUserRepository` 抽象。默认环境用内存仓储保证项目开箱即跑；开启 `mysql.enabled` 后，UserService 会从 MySQL `users` 表读取用户、密码、邮箱和角色，用同一套登录和资料查询逻辑完成持久化切换。

限流模块先实现了进程内固定窗口算法，支持默认额度和路径级规则。例如登录接口单独限制，业务接口也可以按路径配置额度。后续增强为 Redis 分布式固定窗口：开启配置后使用 Redis `INCR`、`EXPIRE` 和 `TTL` 维护跨实例计数，限流 key 可以基于用户 ID 或客户端 IP，超限会返回统一错误码 42900。

服务治理模块放在 UpstreamClient 外层，通过组合方式包装 LocalRpcUpstreamClient。它支持可取消 Deadline、重试、熔断和 fallback。每次请求会创建共享 Deadline 状态，并随 `ForwardContext` 传到 `RpcContext`，治理层等待点、本地 RPC 转发和服务实现都会检查取消状态，而不是只在调用结束后比较耗时。当下游连续失败时，熔断器会打开，后续请求直接走降级逻辑，避免继续打爆下游。

可观测性方面，项目暴露了 `/metrics`，Prometheus 可以抓取请求数、状态码、耗时等指标；`/debug/traces` 可以查看调试链路；`/admin/runtime`、`/admin/routes`、`/admin/features` 可以查看运行时配置和路由状态。生产配置默认不向响应头暴露 `X-Trace-Id` / `X-Span-Id` 这类调试信息。

项目最后在 CentOS 虚拟机里用 CMake 原生编译运行，并用 hey 做压测。`/health` 在 5000 请求、50 并发下达到约 32681 req/s；限流验证中登录接口前 20 次返回 200，后 10 次返回 429；服务治理验证中观察到了重试失败、熔断打开和 fallback 降级。

如果继续扩展，我会把当前进程内 token 缓存和限流计数替换成 Redis，把本地内存数据替换成 MySQL，并把更多服务从 LocalRpcUpstreamClient 拆成独立 gRPC 或 tRPC 服务。

## 面试高频问题回答

**为什么要抽象 UpstreamClient？**

网关本身不应该绑定某一种下游协议。通过 `IUpstreamClient` 抽象，GatewayHandler 只关心转发上下文和响应，后续可以替换为 Local RPC、HTTP、gRPC 或 tRPC client。

**为什么路由使用最长前缀优先？**

因为真实网关里会出现嵌套路由，比如 `/api/user` 和 `/api/user/admin`。最长前缀优先可以保证更具体的规则先匹配。

**限流为什么选择固定窗口？**

固定窗口实现简单、性能开销低，适合作为学习版网关的第一版限流策略。单机版本用内存 map，分布式版本用 Redis 原子自增保证多个网关实例共享计数，后续还可以替换为滑动窗口或令牌桶。

**服务治理怎么防止下游故障扩散？**

通过可取消 Deadline 避免请求长期占用资源，通过重试处理短暂抖动，通过熔断避免持续打到故障下游，通过 fallback 返回可控降级响应。

**这个项目的亮点是什么？**

亮点是它覆盖了完整网关链路：HTTP 接入、路由、鉴权、限流、RPC 调度、服务治理、可观测、部署和压测结果，不只是单点 demo，而是可以讲清楚架构分层和工程闭环的项目。
