# Release Benchmark Summary

- build_type: Release
- build_dir: build-release
- base_url: http://127.0.0.1:8080
- health: 10000 requests, 100 concurrency
- profile: 1000 requests, 50 concurrency
- generated_at: 2026-07-14T11:41:48+08:00

Environment:

- OS: CentOS Linux 8 on VMware
- CPU: 8 vCPU, Intel(R) Core(TM) i9-14900HX
- Memory: 2.7 GiB
- Compiler: GCC 8.5.0
- CMake: 3.20.2
- Benchmark tool: hey

Results:

- `/health`: 10000 requests, 100 concurrency, 34590.2847 req/s, avg 2.8 ms, p99 5.2 ms, 10000 HTTP 200.
- `/api/user/profile`: 1000 requests, 50 concurrency, 22868.2743 req/s, avg 2.0 ms, p99 4.8 ms, 100 HTTP 200 and 900 HTTP 429.
- Profile benchmark result is rate-limit oriented. The 429 responses are expected and prove the limiter is active under pressure.

Result files:

- release_health_check.json
- release_hey_health_10000_100.txt
- release_hey_profile_1000_50.txt
- release_metrics_after_benchmark.txt
- release_traces_after_benchmark.json
