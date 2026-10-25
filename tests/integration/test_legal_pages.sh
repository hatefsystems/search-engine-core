#!/usr/bin/env bash
# Public route regression checks against a running C++ Hatef server.
set -euo pipefail
cd "$(dirname "$0")/../.."
python3 tests/legal/http-check.py
