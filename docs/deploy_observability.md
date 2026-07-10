# TGW Gateway 部署与可观测性栈

## 1. 模块目标

本模块使用 Docker Compose 启动：

- tgw-gateway
- Prometheus
- Grafana
- Jaeger

其中：

- Gateway 暴露业务端口 `8080`
- Prometheus 暴露 `9090`
- Grafana 暴露 `3000`
- Jaeger UI 暴露 `16686`

## 2. 启动

在项目根目录执行：

```bash
docker compose -f deploy/docker-compose.yml up --build