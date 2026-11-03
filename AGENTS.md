# ACME Trading Platform — AI Agent Rules & Guidelines

## 1. User Profile & Teaching Style
- The user is an experienced software engineer from other languages who is learning **C++20 for quantitative finance and low-latency trading**.
- **Plain-English First:** Whenever introducing or explaining C++ or trading concepts (e.g., cache lines, intrusive lists, move semantics, atomics, memory pools), ALWAYS explain the concept in simple, everyday English with a clear analogy before showing low-level C++ syntax.
- Avoid unexplained walls of jargon. Reference [`docs/BEGINNER_GUIDE.md`](docs/BEGINNER_GUIDE.md) and the visual diagrams in [`docs/diagrams/`](docs/diagrams/) when explaining how the project works.

## 2. Low-Latency C++20 Coding Standards
- **Zero Hot-Path Heap Allocations:** Never use `new`, `delete`, `malloc`, `std::map`, `std::unordered_map`, or `std::function` on the order-processing hot path. Use pre-allocated arenas ([`ObjectPool`](include/acme/object_pool.hpp)), flat open-addressing tables ([`FlatHashMap`](include/acme/flat_hash_map.hpp)), and contiguous vectors.
- **Fixed-Point Prices:** Never use `float` or `double` for prices. Always use `acme::Price` (`std::int64_t` scaled by `PRICE_SCALE = 10'000`).
- **Compiler Warnings & Sanitizers:** All code must compile cleanly with zero warnings under `-std=c++20 -O3 -march=native -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast` and pass `make test` and `make sanitize` (ASan + UBSan).
- **Diagrams:** Keep PlantUML diagrams in `docs/diagrams/*.puml` and re-export PNGs via `plantuml docs/diagrams/*.puml` whenever architecture changes.
