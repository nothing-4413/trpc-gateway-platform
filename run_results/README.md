# TGW Gateway CentOS Run Results

本目录保存 TGW Gateway Platform 在 VMware CentOS 环境下的运行验证与压测结果。

## 已验证内容

- `/health` 健康检查。
- `/api/user/login` 登录接口。
- `/api/user/profile` Token 鉴权访问。
- `/metrics` Prometheus 指标。
- `/debug/traces` 链路追踪结果。
- `/admin/runtime`、`/admin/routes`、`/admin/features` 管理接口。
- hey 压测 `/health`。
- hey 压测 `/api/user/profile`。
- 固定窗口限流验证。
- 服务治理验证：超时、重试、熔断、fallback 降级。

## 压测摘要

- `/health`，1000 请求，20 并发：Requests/sec 约 30658，全部 200。
- `/health`，5000 请求，50 并发：Requests/sec 约 32681，全部 200。
- `/api/user/profile`，80 请求，10 并发：Requests/sec 约 8590，全部 200。
- `/api/user/profile`，1000 请求，20 并发：20 个 200，980 个 429，用于验证限流生效。
- `/api/user/login` 限流验证：前 20 次返回 200，后 10 次返回 429。
- 服务治理验证：出现 `all retry attempts failed`、`circuit breaker open`、`X-Governance-Fallback: true`、`degraded=true`，验证通过。
- Release 压测可通过 `benchmark/run_release_benchmark.sh` 生成 `release_*` 结果文件。
- Release `/health`，10000 请求，100 并发：Requests/sec 约 34590，全部 200。
- Release `/api/user/profile`，1000 请求，50 并发：Requests/sec 约 22868，其中 100 个 200、900 个 429，用于验证限流在高并发下生效。

## 文件说明

- `hey_health.txt`：基础 `/health` 压测结果。
- `hey_health_1000_20.txt`：`/health` 1000 请求、20 并发结果。
- `hey_health_5000_50.txt`：`/health` 5000 请求、50 并发结果。
- `hey_profile_80_10.txt`：鉴权接口正常压测结果。
- `hey_profile_with_rate_limit.txt`：鉴权接口限流压测结果。
- `verify_rate_limit.txt`：限流验证结果。
- `verify_governance.txt`：服务治理验证结果，token 已脱敏。
- `collect_metrics_snapshot.txt`：Prometheus 指标快照。
- `metrics_after_benchmark.txt`：压测后的 metrics。
- `metrics_after_verify.txt`：验证后的 metrics。
- `traces_after_benchmark.json`：压测后的 traces。
- `traces_after_verify.json`：验证后的 traces。
- `release_*`：Release 构建压测结果，由 `benchmark/run_release_benchmark.sh` 生成。

## 构建运行命令

```bash
cd /root/code/trpc-gateway-platform
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
pkill -9 tgw_gateway 2>/dev/null || true
nohup ./build/tgw_gateway --config=configs/gateway.yaml > gateway.log 2>&1 &
sleep 1
curl -v --max-time 3 http://127.0.0.1:8080/health
```

Release 压测命令：

```bash
cd /root/code/trpc-gateway-platform
bash benchmark/run_release_benchmark.sh
```
