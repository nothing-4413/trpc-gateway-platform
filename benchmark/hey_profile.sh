#!/usr/bin/env bash

set -euo pipefail

BASE_URL="${BASE_URL:-http://127.0.0.1:8080}"
N="${N:-10000}"
C="${C:-100}"

echo "== TGW Gateway authenticated profile benchmark =="
echo "BASE_URL=${BASE_URL}"
echo "N=${N}"
echo "C=${C}"
echo

if ! command -v hey >/dev/null 2>&1; then
  echo "hey not found. Install it first:"
  echo "  sudo apt update && sudo apt install -y hey"
  exit 1
fi

echo "[1/2] login and fetch token"

TOKEN="$(
  curl -fsS -X POST "${BASE_URL}/api/user/login" \
    -H "Content-Type: application/json" \
    -H "X-Trace-Id: bench-login-trace" \
    -d '{"username":"admin","password":"123456"}' \
    | python3 -c 'import sys,json; print(json.load(sys.stdin)["data"]["token"])'
)"

if [[ -z "${TOKEN}" ]]; then
  echo "failed to fetch token"
  exit 1
fi

echo "[2/2] run hey benchmark"

hey \
  -n "${N}" \
  -c "${C}" \
  -H "Authorization: Bearer ${TOKEN}" \
  -H "X-Trace-Id: bench-profile-trace" \
  "${BASE_URL}/api/user/profile?user_id=10001"