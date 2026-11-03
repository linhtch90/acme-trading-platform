# Prompt Library for ACME Trading Platform

Copy and paste any of these prompts into the chat to guide your learning!

---

## Prompt 1: Explain Any File Line-by-Line in Plain English
> "Using the `cpp-hft-tutor` skill, please walk me through `include/acme/types.hpp` (or another file) line by line in plain English. Compare each C++ keyword to how Python/Java works, and give me a tiny 5-line change I can make and test with `make run`."

---

## Prompt 2: Guided Hands-On Coding Exercise
> "Give me a beginner-friendly hands-on C++ exercise in this project (for example, adding a `modify_order_price` function or tracking total traded volume in `OrderBook`). Explain the goal simply, show me where to start, and review my code when I'm done."

---

## Prompt 3: Visual Diagram for a Concept
> "I want to visualize how [Order Cancellation / SPSC Ring Buffer / FlatHashMap] works step-by-step. Please create a PlantUML diagram in `docs/diagrams/`, export it to `.png`, and explain each step in simple English."

---

## Prompt 4: HFT Code Review & Benchmark
> "Please use the `benchmark-and-verify` skill to test my changes, check for any hidden heap allocations or memory bugs with AddressSanitizer, and compare the `__rdtsc` CPU cycle benchmark."
