#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/../../../.."

echo "=== [1/4] Running Unit Tests (with Zero-Heap-Allocation Check) ==="
make test

echo "=== [2/4] Running AddressSanitizer & UndefinedBehaviorSanitizer ==="
make sanitize

echo "=== [3/4] Running Hardware Cycle (__rdtsc) & Throughput Benchmark ==="
make bench

echo "=== [4/4] Rendering PlantUML Diagrams to PNG ==="
if command -v plantuml >/dev/null 2>&1; then
    plantuml docs/diagrams/*.puml
    ls -lh docs/diagrams/*.png
fi

echo "=== All Checks Passed Successfully! ==="
