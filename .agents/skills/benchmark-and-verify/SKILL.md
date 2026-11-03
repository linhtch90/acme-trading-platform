---
name: benchmark-and-verify
description: >-
  Runs the full verification pipeline for ACME Trading Platform: strict C++20 compilation,
  zero-heap-allocation unit tests, AddressSanitizer/UBSan checks, __rdtsc cycle benchmarks,
  and PlantUML PNG diagram generation. Use this skill after modifying code or diagrams.
---

# Benchmark & Verify Pipeline

Use this skill to validate any code or documentation changes in the workspace.

## Steps

1. Run the verification script:
   [verify_all.sh](./scripts/verify_all.sh)
2. Check that:
   - `test_zero_heap_allocations_on_hot_path` reports `0 bytes allocated on heap`.
   - AddressSanitizer + UndefinedBehaviorSanitizer (`make sanitize`) passes with zero errors.
   - `make bench` reports `p50` and `p99` CPU cycle latencies and overall throughput.
   - All `.puml` files in `docs/diagrams/` are exported to `.png`.
