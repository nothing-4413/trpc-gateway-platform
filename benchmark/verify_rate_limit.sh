#!/usr/bin/env bash

set -euo pipefail

BASE_URL="${BASE_URL:-http://127.0.0.1:8080}"
COUNT="${COUNT:-30}"

echo "== TGW Gateway rate limit verification =="
echo "BASE_URL=${BASE_URL}"
echo "COUNT=${COUNT}"
echo

echo "This script hits /api/user/login."
echo "Default config limits /api/user/login to 20 requests per 60s."
echo

ok_count=0
limited_count=0

for i in $(seq 1 "${COUNT}"); do
  status="$(
    curl -s -o /tmp/tgw_rate_limit_body.txt -w "%{http_code}" \
      -X POST "${BASE_URL}/api/user/login" \
      -H "Content-Type: application/json" \
      -H "X-Trace-Id: rate-limit-test-${i}" \
      -d '{"username":"admin","password":"123456"}'
  )"

  if [[ "${status}" == "429" ]]; then
    limited_count=$((limited_count + 1))
  elif [[ "${status}" == "200" ]]; then
    ok_count=$((ok_count + 1))
  fi

  echo "request=${i}, status=${status}"
done

echo
echo "ok_count=${ok_count}"
echo "limited_count=${limited_count}"

if [[ "${limited_count}" -le 0 ]]; then
  echo "rate limit did not trigger"
  exit 1
fi

echo "rate limit verification passed"