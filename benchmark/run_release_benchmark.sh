#!/usr/bin/env bash

set -euo pipefail

BASE_URL="${BASE_URL:-http://127.0.0.1:8080}"
HEY="${HEY:-/data/go-tools/bin/hey}"
RESULT_DIR="${RESULT_DIR:-run_results}"
BUILD_DIR="${BUILD_DIR:-build-release}"
CONFIG="${CONFIG:-configs/gateway.yaml}"

HEALTH_N="${HEALTH_N:-10000}"
HEALTH_C="${HEALTH_C:-100}"
PROFILE_N="${PROFILE_N:-1000}"
PROFILE_C="${PROFILE_C:-50}"

mkdir -p "${RESULT_DIR}"

echo "== TGW Gateway Release benchmark =="
echo "BASE_URL=${BASE_URL}"
echo "BUILD_DIR=${BUILD_DIR}"
echo "CONFIG=${CONFIG}"
echo "HEY=${HEY}"
echo

if [[ ! -x "${HEY}" ]]; then
  echo "hey executable not found: ${HEY}" >&2
  exit 1
fi

cmake -S . -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}" -j2

pkill -9 tgw_gateway 2>/dev/null || true
nohup "./${BUILD_DIR}/tgw_gateway" --config="${CONFIG}" > gateway-release.log 2>&1 &
GATEWAY_PID="$!"

cleanup() {
  kill "${GATEWAY_PID}" 2>/dev/null || true
}
trap cleanup EXIT

sleep 1
curl -fsS --max-time 3 "${BASE_URL}/health" | tee "${RESULT_DIR}/release_health_check.json"
echo

echo "[1/5] login"
TOKEN="$(
  curl -fsS -X POST "${BASE_URL}/api/user/login" \
    -H "Content-Type: application/json" \
    -d '{"username":"admin","password":"123456"}' \
    | python3 -c 'import sys,json; print(json.load(sys.stdin)["data"]["token"])'
)"

echo "token=<redacted>" > "${RESULT_DIR}/release_login.txt"

echo "[2/5] /health benchmark"
"${HEY}" -n "${HEALTH_N}" -c "${HEALTH_C}" "${BASE_URL}/health" \
  | tee "${RESULT_DIR}/release_hey_health_${HEALTH_N}_${HEALTH_C}.txt"

echo "[3/5] /api/user/profile benchmark"
"${HEY}" -n "${PROFILE_N}" -c "${PROFILE_C}" \
  -H "Authorization: Bearer ${TOKEN}" \
  "${BASE_URL}/api/user/profile?user_id=10001" \
  | tee "${RESULT_DIR}/release_hey_profile_${PROFILE_N}_${PROFILE_C}.txt"

echo "[4/5] metrics snapshot"
curl -fsS "${BASE_URL}/metrics" \
  | tee "${RESULT_DIR}/release_metrics_after_benchmark.txt"

echo "[5/5] traces snapshot"
curl -fsS "${BASE_URL}/debug/traces" \
  | tee "${RESULT_DIR}/release_traces_after_benchmark.json"

cat > "${RESULT_DIR}/release_summary.md" <<EOF
# Release Benchmark Summary

- build_type: Release
- build_dir: ${BUILD_DIR}
- base_url: ${BASE_URL}
- health: ${HEALTH_N} requests, ${HEALTH_C} concurrency
- profile: ${PROFILE_N} requests, ${PROFILE_C} concurrency
- generated_at: $(date -Is)

Result files:

- release_health_check.json
- release_hey_health_${HEALTH_N}_${HEALTH_C}.txt
- release_hey_profile_${PROFILE_N}_${PROFILE_C}.txt
- release_metrics_after_benchmark.txt
- release_traces_after_benchmark.json
EOF

echo
echo "Release benchmark finished. Results saved to ${RESULT_DIR}/release_*"
