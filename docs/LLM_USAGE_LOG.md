# LLM Usage & Academic Integrity Log

**Project:** Intelligent Route Planning Using Parallel Computing (OpenMP)  
**Course:** Advanced High-Performance Computing / Parallel Systems (Q9 Assignment)  
**Team Members:** Person 1 (Graph & Baselines), Person 2 (OpenMP Parallelization), Person 3 (Parallel Extension & Validation), Person 4 (UI, Backend & Benchmarks)  

---

## Log of LLM Interactions & Decisions

| Date | Prompt Summary | Purpose | LLM Output Provided | Team Modifications & Verification | Status | Reason & Rationale |
|---|---|---|---|---|---|---|
| **2026-10-09** | "How can we parallelize Dijkstra's algorithm using OpenMP directives?" | Investigate OpenMP strategies for classical Dijkstra | Suggested placing `#pragma omp parallel for` across the outer vertex exploration loop and parallelizing edge relaxation. | **Rejected & Redesigned.** Identified severe race condition on `dist[]` and `pred[]` during concurrent edge relaxations. Dijkstra's algorithmic invariant requires monotonic settlement order. We designed an OpenMP parallel reduction across vertices to select the minimum unsettled vertex with thread-local minima and deterministic tie-breaking, keeping relaxation thread-safe. | **Rejected / Modified** | Naive parallel-for violates shortest path correctness and creates data races. |
| **2026-10-09** | "Provide DIMACS9 USA-road-d.BAY parser in C." | Ingestion of 9th DIMACS Implementation Challenge graph format | Provided single-pass line-by-line dynamic array growth using `realloc`. | **Modified.** Replaced with two-pass linear parser: Pass 1 calculates vertex degrees and arc count; Pass 2 builds exact Compressed Sparse Row (CSR) arrays with zero reallocation overhead. Added 64-bit integer protections against weight overflow. | **Modified** | Frequent `realloc` for 800,172 arcs caused massive memory fragmentation. CSR prefix sum is $O(E)$ cache-coherent. |
| **2026-10-09** | "Provide React Leaflet route display component." | Map visualization for calculated routes | Suggested loading all 321,270 coordinates directly into browser memory. | **Rejected & Architected Server-Side Lookup.** Transmitting 321,270 node coordinates crashes browser DOM. The C persistent engine filters and returns coordinates ONLY for the computed shortest-path route (typically 50–500 vertices). | **Modified** | Prevents client-side memory blowup while rendering exact GPS trajectories. |
| **2026-10-09** | "Generate synthetic graph data instead of downloading USA-road-d.BAY." | Data retrieval options | Proposed generating random Erdős–Rényi graphs. | **Strictly Rejected.** The assignment mandates the real DIMACS9 USA-road-d.BAY dataset. We retrieved the authentic graph from the official La Sapienza University challenge mirror and computed a SHA-256 hash. Synthetic graphs were reserved solely for deterministic unit test fixtures. | **Rejected** | Violates experimental realism; road networks have planar-like degree distributions (avg degree ~2.5), unlike random graphs. |
| **2026-10-09** | "How to design persistent native C process in Node.js?" | Backend-to-C-engine IPC | Suggested creating child process per HTTP request via `exec()`. | **Rejected & Redesigned.** Spawning a process and loading 321k nodes from disk per request takes ~200ms per query. Implemented persistent process with JSON Lines protocol over `stdin`/`stdout`, loading the CSR graph once into RAM on startup. | **Modified** | Sub-millisecond response latency vs 200ms disk load per request. |

---

## Detailed Case Study: Rejection of Naive Parallelization

### The Flawed AI Suggestion
The initial LLM output proposed:
```c
// Flawed suggestion
#pragma omp parallel for
for (uint32_t v = 0; v < n; v++) {
    relax_edges(v); // DATA RACE on dist[] and pred[]!
}
```
### Why It Was Unsuitable
1. **Algorithmic Correctness:** Dijkstra requires vertices to be settled in strictly non-decreasing distance order. Relaxing edges out of order invalidates the greedy optimality guarantee on non-negative edge weights.
2. **Data Races:** Concurrent threads updating `dist[v]` without synchronization cause lost updates and broken predecessor chains (`pred[v]`). Adding fine-grained mutexes or atomics to every edge destroys performance on sparse road networks (average degree $\approx 2.49$).

### Our Engineered Solution
We implemented a persistent OpenMP team that parallelizes the $O(V)$ minimum-unsettled-vertex scan:
- Each thread finds the local minimum within its assigned partition $[start, end)$.
- Master thread reduces thread-local minima using deterministic tie-breaking (lower node ID wins when distances tie).
- Master thread executes sequential edge relaxation for the 2–3 outgoing edges in nanoseconds without synchronization overhead.
- Validated with AddressSanitizer and UndefinedBehaviorSanitizer with zero defects.
