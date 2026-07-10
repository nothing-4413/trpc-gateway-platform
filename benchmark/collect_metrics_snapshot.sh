#!/usr/bin/env bash

set -euo pipefail

BASE_URL="${BASE_URL:-http://127.0.0.1:8080}"
OUT="${OUT:-benchmark/metrics_snapshot.txt}"

echo "== collect TGW metrics snapshot =="
echo "BASE_URL=${BASE_URL}"
echo "OUT=${OUT}"
echo

curl -fsS "${BASE_URL}/metrics" > "${OUT}"

echo "metrics snapshot saved to ${OUT}"
head -n 40 "${OUT}"