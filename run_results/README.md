# TGW Gateway CentOS Run Results

本目录保存 tRPC-Cpp 微服务网关在 VMware CentOS 环境下的运行与压测结果。

## 已验证内容

- /health 健康检查
- /api/user/login 登录
- /api/user/profile Token 鉴权访问
- /metrics Prometheus 指标
- /debug/traces 链路追踪
- /admin/runtime、/admin/routes、/admin/features 管理接口
- hey 压测 /health
- hey 压测 /api/user/profile
- 限流验证
- 服务治理验证：超时、重试、熔断、fallback 降级

## 压测摘要

- /health，1000 请求，20 并发：约 30658 req/s，全部 200
- /health，5000 请求，50 并发：约 32681 req/s，全部 200
- /api/user/profile，80 请求，10 并发：约 8590 req/s，全部 200
- /api/user/profile，1000 请求，20 并发：20 个 200，980 个 429，用于验证限流生效
- /api/user/login 限流验证：前 20 次 200，后 10 次 429
- 服务治理验证：出现 all retry attempts failed、circuit breaker open、fallback response returned，验证通过

## 运行方式

cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
./build/tgw_gateway --config=configs/gateway.yaml

## 结果文件说明

- hey_health_1000_20.txt：/health 轻量压测结果
- hey_health_5000_50.txt：/health 较高并发压测结果
- hey_profile_80_10.txt：鉴权接口正常压测结果
- hey_profile_with_rate_limit.txt：鉴权接口限流压测结果
- verify_rate_limit.txt：限流验证结果
- verify_governance.txt：服务治理验证结果
- collect_metrics_snapshot.txt：Prometheus 指标快照
- metrics_after_benchmark.txt：压测后 metrics
- traces_after_benchmark.json：压测后 traces
