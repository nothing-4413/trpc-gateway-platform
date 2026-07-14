# TGW Gateway 性能测试报告

## 1. 项目背景

TGW Gateway 是一个基于 C++17 实现的微服务网关与统一 RPC 调度平台。项目参考 tRPC-Cpp 的分层思想，将系统拆分为 HTTP 接入、路由匹配、鉴权、限流、服务治理、Upstream 调用与可观测性模块。

本报告记录 VMware CentOS 环境下的 Release 构建压测结果。结果文件保存在 `run_results/release_*`。

## 2. 测试环境

| 项目 | 配置 |
|---|---|
| 操作系统 | CentOS Linux 8 |
| 运行环境 | VMware NAT，本机到虚拟机内回环压测 |
| CPU | 8 vCPU，Intel(R) Core(TM) i9-14900HX |
| 内存 | 2.7 GiB |
| 编译器 | GCC 8.5.0 |
| CMake | 3.20.2 |
| 构建类型 | Release |
| 网关端口 | 8080 |
| 压测工具 | hey |
| 运行方式 | CentOS 原生 CMake 构建运行 |

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

```bash
cd /root/code/trpc-gateway-platform
bash benchmark/run_release_benchmark.sh
```

脚本会执行 Release 构建、启动 `tgw_gateway`、登录获取 token、压测 `/health` 与 `/api/user/profile`，并将结果写入 `run_results/release_*`。

## 5. Release 压测结果

| 接口 | 请求数 | 并发 | Requests/sec | 平均延迟 | P99 | 状态码 |
|---|---:|---:|---:|---:|---:|---|
| `/health` | 10000 | 100 | 34590.2847 | 2.8ms | 5.2ms | 10000 个 200 |
| `/api/user/profile` | 1000 | 50 | 22868.2743 | 2.0ms | 4.8ms | 100 个 200，900 个 429 |

`/api/user/profile` 的 429 是预期结果：本轮压测保留了限流配置，用于验证高并发下固定窗口限流仍然生效，不代表鉴权接口的无锁性能上限。

## 6. 结果文件

- `run_results/release_health_check.json`：Release 服务健康检查结果。
- `run_results/release_login.txt`：登录 token 脱敏记录。
- `run_results/release_hey_health_10000_100.txt`：`/health` Release 压测详情。
- `run_results/release_hey_profile_1000_50.txt`：`/api/user/profile` Release 压测详情。
- `run_results/release_metrics_after_benchmark.txt`：压测后的 Prometheus 指标快照。
- `run_results/release_traces_after_benchmark.json`：压测后的 debug traces 快照。
- `run_results/release_summary.md`：Release 压测摘要。
