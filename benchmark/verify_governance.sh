#!/usr/bin/env bash

set -euo pipefail

BASE_URL="${BASE_URL:-http://127.0.0.1:8080}"

echo "== TGW Gateway governance verification =="
echo "BASE_URL=${BASE_URL}"
echo

echo "[1/4] login"

TOKEN="$(
  curl -fsS -X POST "${BASE_URL}/api/user/login" \
    -H "Content-Type: application/json" \
    -H "X-Trace-Id: governance-login-trace" \
    -d '{"username":"admin","password":"123456"}' \
    | python3 -c 'import sys,json; print(json.load(sys.stdin)["data"]["token"])'
)"

echo "token=${TOKEN}"
echo

echo "[2/4] timeout verification"
curl -i "${BASE_URL}/api/user/profile?user_id=10001" \
  -H "Authorization: Bearer ${TOKEN}" \
  -H "X-Trace-Id: governance-timeout-trace" \
  -H "X-Debug-Sleep-Ms: 1500" || true

echo
echo "[3/4] retry/fallback verification"
curl -i "${BASE_URL}/api/user/profile?user_id=10001" \
  -H "Authorization: Bearer ${TOKEN}" \
  -H "X-Trace-Id: governance-fallback-trace" \
  -H "X-Debug-Force-Status: 500" || true

echo
echo "[4/4] circuit breaker verification"

for i in {1..5}; do
  echo "force failure ${i}"
  curl -s "${BASE_URL}/api/user/profile?user_id=10001" \
    -H "Authorization: Bearer ${TOKEN}" \
    -H "X-Trace-Id: governance-circuit-${i}" \
    -H "X-Debug-Force-Status: 500" | python3 -m json.tool || true
done

echo
echo "request after repeated failures"
curl -i "${BASE_URL}/api/user/profile?user_id=10001" \
  -H "Authorization: Bearer ${TOKEN}" \
  -H "X-Trace-Id: governance-circuit-open-check" || true

echo
echo "governance verification finished"