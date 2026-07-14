# TGW Gateway 性能测试报告

## 1. 项目背景

TGW Gateway 是一个基于 C++17 实现的微服务网关与统一 RPC 调度平台。项目参考 tRPC-Cpp 的分层思想，将系统拆分为：

- HTTP 接入层
- 路由匹配层
- 鉴权过滤器
- 限流过滤器
- 服务治理层
- Upstream 调用层
- 可观测性模块

本报告用于记录网关在本地环境或服务器环境下的基础压测结果。

## 2. 测试环境

| 项目 | 配置 |
|---|---|
| CPU | 待填写 |
| 内存 | 待填写 |
| 操作系统 | Ubuntu 22.04 / 待填写 |
| 编译器 | GCC / Clang |
| 构建类型 | Release |
| 网关端口 | 8080 |
| 压测工具 | hey / wrk |
| 运行方式 | local / docker compose |

## 3. 服务配置

| 配置项 | 值 |
|---|---|
| worker_threads | 4 |
| io_threads | 2 |
| auth.enabled | true |
| rate_limit.enabled | true |
| governance.enabled | true |
| tracing.enabled | true |
| user route timeout | 1000ms |
| file route timeout | 1500ms |
| task route timeout | 2000ms |

## 4. 压测命令

### 4.0 Release 一键压测

```bash
cd /root/code/trpc-gateway-platform
bash benchmark/run_release_benchmark.sh
```

脚本会执行 Release 构建、启动 `tgw_gateway`、登录获取 token、压测 `/health` 与 `/api/user/profile`，并将结果写入 `run_results/release_*`。

### 4.1 健康检查接口

```bash
BASE_URL=http://127.0.0.1:8080 N=10000 C=100 bash benchmark/hey_health.sh
