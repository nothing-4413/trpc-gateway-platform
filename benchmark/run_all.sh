#!/usr/bin/env bash

set -euo pipefail

BASE_URL="${BASE_URL:-http://127.0.0.1:8080}"

echo "== TGW Gateway benchmark suite =="
echo "BASE_URL=${BASE_URL}"
echo

bash benchmark/hey_health.sh
echo

bash benchmark/hey_profile.sh
echo

bash benchmark/verify_governance.sh
echo

bash benchmark/collect_metrics_snapshot.sh
echo

echo "all benchmark tasks finished"