#!/usr/bin/env bash

set -euo pipefail

BASE_URL="${BASE_URL:-http://127.0.0.1:8080}"
N="${N:-10000}"
C="${C:-100}"

echo "== TGW Gateway health benchmark =="
echo "BASE_URL=${BASE_URL}"
echo "N=${N}"
echo "C=${C}"
echo

if ! command -v hey >/dev/null 2>&1; then
  echo "hey not found. Install it first:"
  echo "  sudo apt update && sudo apt install -y hey"
  exit 1
fi

hey -n "${N}" -c "${C}" "${BASE_URL}/health"