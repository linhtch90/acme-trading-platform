# ACME Low-Latency Trading Platform (C++20 Reference Project)

A hands-on C++20 reference project built for learning **quantitative trading & low-latency systems** from the ground up.

> **New to C++ or Trading?** Start with **[`docs/BEGINNER_GUIDE.md`](docs/BEGINNER_GUIDE.md)**! It explains every concept in plain English using simple analogies and visual diagrams.

---

## Visual Diagrams (PlantUML + PNG)

All diagrams are located in `docs/diagrams/` (both `.puml` source and exported `.png` images):

1. **System Architecture & Data Flow:** [`docs/diagrams/architecture.png`](docs/diagrams/architecture.png) ([source](docs/diagrams/architecture.puml))
2. **Step-by-Step Order Matching Sequence:** [`docs/diagrams/order_matching_flow.png`](docs/diagrams/order_matching_flow.png) ([source](docs/diagrams/order_matching_flow.puml))
3. **Inside the Limit Order Book (RAM Layout):** [`docs/diagrams/memory_layout.png`](docs/diagrams/memory_layout.png) ([source](docs/diagrams/memory_layout.puml))

---

## Quick Start Commands

```bash
# 1. Build and run the interactive Matching Engine + SPSC Queue demo
make run

# 2. Run the unit test suite (verifies 0 bytes allocated on the heap)
make test

# 3. Run the CPU cycle (__rdtsc) & throughput benchmark (~14+ million ops/sec)
make bench

# 4. Run tests with AddressSanitizer (ASan) + UndefinedBehaviorSanitizer (UBSan)
make sanitize
```

---

## AI Assistant Support (`AGENTS.md` & `.agents/`)

This repository is pre-configured with AI rules, skills, agents, and prompts so your AI pair programmer acts as a beginner-friendly C++ finance tutor:

* **[`AGENTS.md`](AGENTS.md)** — Root instructions telling the AI to explain concepts in plain English first while enforcing strict C++20 trading best practices.
* **Rules (`.agents/rules/`)**:
  * [`tutor-communication.md`](.agents/rules/tutor-communication.md) — Enforces everyday analogies and comparisons to Python/Java/JS.
  * [`low-latency-cpp.md`](.agents/rules/low-latency-cpp.md) — Enforces zero hot-path heap allocations, fixed-point prices, and cache-line alignment.
* **Skills (`.agents/skills/`)**:
  * [`cpp-hft-tutor/SKILL.md`](.agents/skills/cpp-hft-tutor/SKILL.md) — Step-by-step interactive C++ tutoring skill.
  * [`benchmark-and-verify/SKILL.md`](.agents/skills/benchmark-and-verify/SKILL.md) & [`verify_all.sh`](.agents/skills/benchmark-and-verify/scripts/verify_all.sh) — Automated test, sanitizer, benchmark, and PlantUML rendering pipeline.
* **Agents & Prompts (`.agents/agents/` & `.agents/prompts/`)**:
  * [`cpp-finance-tutor.md`](.agents/agents/cpp-finance-tutor.md) & [`hft-performance-auditor.md`](.agents/agents/hft-performance-auditor.md) — Specialized agent personas.
  * [`PROMPTS.md`](.agents/prompts/PROMPTS.md) — Copy-paste prompts to learn line-by-line, request visual diagrams, or get guided exercises.
