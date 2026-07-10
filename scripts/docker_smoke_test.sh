#!/usr/bin/env bash

set -euo pipefail

BASE_URL="${BASE_URL:-http://127.0.0.1:8080}"

echo "[1/6] health check"
curl -fsS "${BASE_URL}/health"
echo

echo "[2/6] login"
TOKEN="$(
  curl -fsS -X POST "${BASE_URL}/api/user/login" \
    -H "Content-Type: application/json" \
    -H "X-Trace-Id: smoke-login-trace" \
    -d '{"username":"admin","password":"123456"}' \
    | python3 -c 'import sys,json; print(json.load(sys.stdin)["data"]["token"])'
)"

echo "token=${TOKEN}"

echo "[3/6] profile"
curl -fsS "${BASE_URL}/api/user/profile?user_id=10001" \
  -H "Authorization: Bearer ${TOKEN}" \
  -H "X-Trace-Id: smoke-profile-trace"
echo

echo "[4/6] metrics"
curl -fsS "${BASE_URL}/metrics" | head -n 20
echo

echo "[5/6] debug traces"
curl -fsS "${BASE_URL}/debug/traces" | python3 -m json.tool | head -n 80
echo

echo "[6/6] admin runtime"
curl -fsS "${BASE_URL}/admin/runtime" | python3 -m json.tool | head -n 120
echo

echo "smoke test passed"