#!/usr/bin/env bash
# Smoke test the four public legal page variants against a running Hatef server.
set -euo pipefail
base="${BASE_URL:-http://127.0.0.1:3000}"
tmp="$(mktemp)"
trap 'rm -f "$tmp"' EXIT
for entry in "privacy fa" "privacy?lang=en en" "terms fa" "terms?lang=en en"; do
  read -r url lang <<< "$entry"
  status="$(curl -sS -o "$tmp" -w '%{http_code}' "$base/$url")"
  [[ "$status" == "200" ]] || { echo "FAIL $url: HTTP $status"; exit 1; }
  grep -q "lang=\"$lang\"" "$tmp" || { echo "FAIL $url: wrong language"; exit 1; }
  grep -q 'href="/assets/legal.css"' "$tmp" || { echo "FAIL $url: missing CSS"; exit 1; }
  grep -q "Google" "$tmp" || { echo "FAIL $url: Google integration disclosure missing"; exit 1; }
  echo "PASS $url"
done
status="$(curl -sS -o /dev/null -w '%{http_code}' "$base/assets/legal.css")"
[[ "$status" == "200" ]] || { echo "FAIL /assets/legal.css: HTTP $status"; exit 1; }
echo "PASS legal stylesheet"
