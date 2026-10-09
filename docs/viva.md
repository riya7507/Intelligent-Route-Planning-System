# Comprehensive Viva Voce Questions & Technical Answers

**Project:** Intelligent Route Planning Using Parallel Computing (OpenMP)  
**University Assignment:** Q9. Intelligent Route Planning on Large Road Networks  
**Dataset:** DIMACS9 USA-road-d.BAY (321,270 nodes, 800,172 arcs)  

---

### Q1: Why Dijkstra's algorithm for route planning rather than BFS or Bellman-Ford?
**Answer:**  
Road network edges have non-negative weights representing real travel distances or transit times. Breadth-First Search (BFS) only computes shortest paths in terms of hop count (unweighted edges); on a weighted road network, BFS fails to find the lowest-cost metric route. Bellman-Ford supports negative edge weights with $O(V \cdot E)$ complexity, which is redundant and computationally prohibitive on 321,270 vertices. Dijkstra's greedy invariant guarantees finding optimal shortest paths with optimal time complexity ($O((V+E)\log V)$ with heaps, $O(V^2 + E)$ with array scans).

---

### Q2: Why use OpenMP rather than POSIX threads (pthreads) or MPI?
**Answer:**  
OpenMP provides high-level compiler-directed multi-threading with low thread creation/destruction overhead and native work-sharing pragmas (`#pragma omp parallel`, `#pragma omp for`, barriers, and reductions). The shared-memory architecture fits single-node workstations where all CPU cores share access to the graph stored in RAM. MPI introduces network serialization overhead that is disadvantageous for fine-grained vertex reductions on sparse graphs, whereas POSIX threads require verbose boilerplate for barrier synchronization.

---

### Q3: Why is the graph represented in Compressed Sparse Row (CSR) format?
**Answer:**  
An adjacency matrix for 321,270 vertices would require $(321,270)^2 \times 4 \text{ bytes} \approx 412 \text{ GB}$ of RAM—far exceeding workstation capacity. An adjacency list of linked pointers suffers from pointer dereferencing latency, cache misses, and memory fragmentation.  
CSR packs the entire graph into three contiguous 1D flat arrays:
1. `row_ptr` (size $V + 1$, 64-bit offsets): indices into columns.
2. `col_idx` (size $E$, 32-bit): neighbor vertex IDs.
3. `weights` (size $E$, 32-bit): non-negative edge weights.  
Total memory footprint is under 15 MB for 800,172 arcs, achieving 100% cache-line spatial locality during neighbor traversals.

---

### Q4: Why implement both Array-Scan and Binary-Heap Dijkstra?
**Answer:**  
The university assignment explicitly requires studying parallel array-scan Dijkstra and comparing it with sequential baselines.  
- **Array-Scan Dijkstra ($O(V^2 + E)$):** Spends almost all its execution time in the $O(V)$ linear scan across all vertices searching for the minimum unsettled vertex. This $O(V)$ scan is embarrassingly parallel and ideal for OpenMP parallel reduction.
- **Binary-Heap Dijkstra ($O((V + E) \log V)$):** Uses a priority queue. While asymptotically much faster on sparse road networks sequentially, the priority queue is inherently sequential and difficult to parallelize efficiently without fine-grained lock contention. Comparing both demonstrates the trade-off between algorithmic asymptotic improvement and hardware parallelism.

---

### Q5: What exactly is parallelized in OpenMP Parallel Array-Scan Dijkstra?
**Answer:**  
The vertex selection phase $\arg\min \{dist[v] \mid settled[v] = \text{false}\}$. In each iteration, vertices $[0, V)$ are statically partitioned across $P$ threads. Each thread scans its local partition to find a thread-local $(dist_{min}, u_{min})$. A thread barrier synchronizes the workers, and the master thread combines the $P$ candidates using deterministic tie-breaking.

---

### Q6: What are the race conditions and how are they avoided?
**Answer:**  
If edge relaxations were executed concurrently across multiple threads without synchronization, concurrent writes to `dist[v]` and `pred[v]` would cause lost updates and corrupted predecessor links.  
We avoided this completely:
1. Since the Bay Area road network has an average vertex degree of only $\approx 2.49$, relaxing edges takes under 20 nanoseconds.
2. The master thread performs relaxation sequentially right after reducing the minimum vertex, before releasing threads for the next iteration.
3. This eliminates all mutexes, critical sections, and atomic operations inside the relaxation loop.

---

### Q7: How is deterministic tie-breaking enforced?
**Answer:**  
If two vertices $u_1 < u_2$ have identical minimum distances $dist[u_1] == dist[u_2]$, the reduction strictly selects $\min(u_1, u_2)$. This guarantees that OpenMP executions across 1, 2, 4, or 8 threads produce identical shortest-path trees and deterministic distances identical to the sequential baseline.

---

### Q8: How was correctness verified?
**Answer:**  
1. **Automated Test Suite:** 36 automated unit and integration tests covering diamond graphs, parallel arcs, zero-weight edges, 64-bit weight overflow, and unreachable destinations.
2. **Equivalence Checking:** Exact distance equivalence between Sequential Array-Scan, Sequential Heap, OpenMP Array-Scan, and Delta-Stepping across all subgraphs and queries.
3. **Path Validation:** Every reconstructed route is independently verified by checking that $(u, v)$ arcs exist in the CSR graph and that $\sum w(u, v)$ exactly matches the reported shortest distance.
4. **Sanitizers:** Clean execution under AddressSanitizer (ASan) and UndefinedBehaviorSanitizer (UBSan).

---

### Q9: How does execution time change as network size increases?
**Answer:**  
Our experimental investigation on deterministic induced subgraphs from 1,000 to 100,000 nodes demonstrated:
- **Array-Scan Dijkstra:** Execution time grows quadratically with network size ($O(V^2)$). At 1,000 nodes it takes ~0.9 ms; at 10,000 nodes ~95 ms; at 50,000 nodes ~2,280 ms; and at 100,000 nodes ~6,100 ms.
- **OpenMP Array-Scan:** Achieves speedup on medium-to-large networks where work per thread dominates barrier synchronization.
- **Binary Heap:** Scales nearly linearly ($O((V+E)\log V)$), taking ~0.08 ms at 1,000 nodes to ~7.6 ms at 100,000 nodes.

---

### Q10: What is Speedup and Efficiency, and what were the measured values?
**Answer:**  
- **Speedup:** $S_P = \frac{T_{sequential}}{T_{parallel}(P)}$
- **Efficiency:** $E_P = \frac{S_P}{P} \times 100\%$
- **Measured Empirical Results:**
  - On 25,000 vertices: 1-thread time $= 683\text{ ms}$, 2-thread time $= 368 - 436\text{ ms}$.  
    **Speedup $= 1.83\times$**, **Efficiency $= 91.6\%$** on 2 physical cores!
  - On 50,000 vertices: Sequential $= 2,280\text{ ms}$, OpenMP $= 1,836\text{ ms}$ (Speedup $= 1.24\times$).
  - On 100,000 vertices: Sequential $= 6,100\text{ ms}$, OpenMP $= 4,658\text{ ms}$ (Speedup $= 1.32\times$).

---

### Q11: Why can parallel execution be slower than sequential execution?
**Answer:**  
1. **Barrier Overhead:** On small graphs ($V = 1,000$), scanning 500 vertices per thread takes less time than the hardware latency of an OpenMP barrier synchronization (~3 ms vs 0.9 ms).
2. **Thread Oversubscription:** On our 2-core execution environment, requesting 4 or 8 threads degraded performance to 1,200–1,500 ms due to CPU time-slice scheduling overhead and cache thrashing.
3. **Sequential Portions (Amdahl's Law):** Edge relaxation and barrier serialization bound maximum theoretical speedup.

---

### Q12: How does the backend communicate with the C engine?
**Answer:**  
Via a persistent child process using JSON Lines over standard streams (`stdin` and `stdout`). The C engine loads the 16 MB DIMACS graph into RAM once on startup. Route requests are written as single-line JSON objects, and the C engine responds with structured JSON containing distance, execution time, hop count, and path coordinates. Diagnostics are restricted to `stderr`.

---

### Q13: Why is the graph stored in C memory rather than in SQLite/PostgreSQL?
**Answer:**  
Querying 800,172 arcs through SQL joins for Dijkstra's inner loop would incur massive relational engine overhead, serialization latency, and index lookups, turning a 1-millisecond traversal into seconds. Retaining CSR arrays in C memory provides sub-nanosecond array dereferencing, while SQLite is used appropriately for route history and benchmark logs.

---

### Q14: What was Person 1's contribution?
**Answer:**  
Person 1 implemented the DIMACS9 parser, CSR graph memory representation, sequential array-scan Dijkstra, sequential binary-heap Dijkstra, path reconstruction, and baseline correctness validation.

---

### Q15: What was Person 2's contribution?
**Answer:**  
Person 2 designed the OpenMP parallel array-scan implementation, persistent parallel team architecture, thread-local minimum reductions, synchronization barrier placement, race condition avoidance, and thread scaling experiments.

---

### Q16: What was Person 3's contribution?
**Answer:**  
Person 3 developed the Delta-Stepping parallel extension, deterministic BFS induced subgraph generator, independent correctness tests (ASan/UBSan), and edge case testing.

---

### Q17: What was Person 4's contribution?
**Answer:**  
Person 4 built the interactive React/TypeScript frontend, Leaflet route visualization, Recharts performance dashboards, Express backend, persistent C engine IPC, and SQLite integration.

---

### Q18: How was AI used and verified?
**Answer:**  
AI was used for prototyping initial algorithms and scripts. Every AI suggestion was verified against physical specifications. When AI suggested a naive `#pragma omp parallel for` loop across edge relaxations, the team rejected it due to data races and designed the persistent thread reduction architecture. All benchmarks were generated from real hardware runs.
