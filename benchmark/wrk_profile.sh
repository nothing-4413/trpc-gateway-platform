#!/usr/bin/env bash

set -euo pipefail

BASE_URL="${BASE_URL:-http://127.0.0.1:8080}"
DURATION="${DURATION:-30s}"
THREADS="${THREADS:-4}"
CONNECTIONS="${CONNECTIONS:-100}"

echo "== TGW Gateway wrk profile benchmark =="
echo "BASE_URL=${BASE_URL}"
echo "DURATION=${DURATION}"
echo "THREADS=${THREADS}"
echo "CONNECTIONS=${CONNECTIONS}"
echo

if ! command -v wrk >/dev/null 2>&1; then
  echo "wrk not found. Install it first:"
  echo "  sudo apt update && sudo apt install -y wrk"
  exit 1
fi

echo "[1/2] login and fetch token"

TOKEN="$(
  curl -fsS -X POST "${BASE_URL}/api/user/login" \
    -H "Content-Type: application/json" \
    -H "X-Trace-Id: wrk-login-trace" \
    -d '{"username":"admin","password":"123456"}' \
    | python3 -c 'import sys,json; print(json.load(sys.stdin)["data"]["token"])'
)"

if [[ -z "${TOKEN}" ]]; then
  echo "failed to fetch token"
  exit 1
fi

echo "[2/2] run wrk benchmark"

TGW_TOKEN="${TOKEN}" wrk \
  -t"${THREADS}" \
  -c"${CONNECTIONS}" \
  -d"${DURATION}" \
  -s benchmark/wrk_profile.lua \
  "${BASE_URL}"