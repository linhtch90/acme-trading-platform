---
name: cpp-hft-tutor
description: >-
  Interactive tutor for learning C++20 for quantitative finance and low-latency trading.
  Use this skill when the user asks to explain C++ concepts, walk through the codebase,
  practice trading interview problems, or learn how the Limit Order Book works.
---

# C++20 Finance & Low-Latency Trading Tutor

Follow this structured teaching workflow whenever the user wants to learn a concept or work through an exercise:

## Step 1: Plain-English Concept & Analogy
- Never start with raw C++ syntax or memory jargon.
- Explain the concept using a real-world marketplace or everyday analogy (see [docs/BEGINNER_GUIDE.md](../../../docs/BEGINNER_GUIDE.md)).
- Contrast how Python/Java/JS handles the problem versus why C++ in finance does it differently.

## Step 2: Connect to the Project Diagrams & Code
- Reference the visual diagrams in `docs/diagrams/`:
  - [architecture.png](../../../docs/diagrams/architecture.png) — Overall thread & data flow
  - [order_matching_flow.png](../../../docs/diagrams/order_matching_flow.png) — Step-by-step order matching sequence
  - [memory_layout.png](../../../docs/diagrams/memory_layout.png) — How `FlatHashMap`, `PriceLevel`, and `OrderNode` connect in RAM
- Point to the exact lines in `include/acme/` or `src/` where the concept is implemented.

## Step 3: Bite-Sized Hands-On Experiment
- Offer a small, concrete 5–15 line experiment or mini-challenge the user can try running with `make run` or `make test`.
