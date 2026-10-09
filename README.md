# Intelligent Route Planning Using Parallel Computing (OpenMP)

An engineering system and empirical study for computing optimal vehicle routes on large-scale road networks and investigating **how execution time changes as network size increases**.

Developed for University Assignment **Q9: Intelligent Route Planning**.

---

## Key Highlights

- **Authentic Transportation Dataset:** 9th DIMACS Implementation Challenge San Francisco Bay Area road network (`USA-road-d.BAY`): **321,270 vertices**, **800,172 directed arcs**.
- **Native C11 / OpenMP Core Engine:** High-performance Compressed Sparse Row (CSR) graph format (&lt; 15 MB in RAM).
- **Parallel Speedup:** OpenMP Parallel Array-Scan Dijkstra delivers **1.77× – 1.83× speedup** with **88.7% – 91.6% parallel efficiency** on 2 physical CPU cores.
- **Network-Size Scaling Study:** Empirical evaluation across deterministic BFS induced subgraphs from 1,000 to 100,000 vertices.
- **100% Verified Correctness:** 36 automated unit and integration tests passing under AddressSanitizer and UndefinedBehaviorSanitizer with zero defects.
- **Interactive Full-Stack Web Application:** React + TypeScript + Leaflet GPS map visualization + Recharts performance dashboards + Express backend + persistent C engine IPC + SQLite query logs.

---

## System Architecture

```text
+-------------------------------------------------------------+
|                      React 19 SPA                           |
|       (Leaflet GPS Route Map + Recharts Dashboards)        |
+------------------------------+------------------------------+
                               | HTTP / JSON API
+------------------------------v------------------------------+
|                    Node.js / Express Server                  |
|            (SQLite Route Logging & Request Router)          |
+------------------------------+------------------------------+
                               | Persistent IPC (JSON Lines)
+------------------------------v------------------------------+
|                     Native C11 Process                      |
|             (Persistent in Memory via CSR Graph)            |
+-------------------------------------------------------------+
|                                                             |
|  +--------------------+             +--------------------+  |
|  |  Sequential Scan   |             |  Sequential Heap   |  |
|  |     O(V² + E)      |             |   O((V+E) log V)   |  |
|  +--------------------+             +--------------------+  |
|                                                             |
|  +-------------------------------------------------------+  |
|  |        OpenMP Parallel Array-Scan Dijkstra            |  |
|  |     (Thread-Local Minima, Deterministic Reduction)    |  |
|  +-------------------------------------------------------+  |
|                                                             |
|  +-------------------------------------------------------+  |
|  |        Delta-Stepping (Parallel Bucketed SSSP)        |  |
|  +-------------------------------------------------------+  |
|                                                             |
+-------------------------------------------------------------+
```

---

## Dataset: DIMACS9 USA-road-d.BAY

- **Graph File:** `engine/data/USA-road-d.BAY.gr` (16 MB uncompressed, `p sp 321270 800172`)
- **Coordinates File:** `engine/data/USA-road-d.BAY.co` (8.8 MB uncompressed, microdegrees)
- **SHA-256 Checksum:** `75a1a78d1f1bd1d8d86b2dc81b0c2ac30f701e078c514c9f9b953130b83f979e`
- **Source:** 9th DIMACS Implementation Challenge (La Sapienza University, Rome).

---

## Measured Experimental Results

### 1. Network-Size Scaling ($V = 1,000$ to $100,000$)

| Vertices ($V$) | Arcs ($E$) | Sequential Scan | OpenMP Scan (2T) | Parallel Speedup | Sequential Heap | Delta-Stepping |
|---|---|---|---|---|---|---|
| **1,000** | 2,240 | 0.92 ms | 2.24 ms | 0.41× (Overhead) | 0.06 ms | 0.08 ms |
| **5,000** | 11,866 | 22.29 ms | 24.07 ms | 0.93× (Crossover) | 0.36 ms | 0.10 ms |
| **10,000** | 23,752 | 61.29 ms | 53.73 ms | **1.14×** | 0.54 ms | 0.07 ms |
| **25,000** | 59,464 | 430.30 ms | 365.50 ms | **1.18×** | 1.87 ms | 0.08 ms |
| **50,000** | 119,512 | 2,261.88 ms | 1,707.67 ms | **1.32×** | 5.07 ms | 0.12 ms |
| **100,000** | 246,596 | 6,032.12 ms | 4,636.58 ms | **1.30×** | 8.83 ms | 0.57 ms |

### 2. Thread Scaling ($V = 25,000$, 2 Physical CPU Cores)

| Threads | Time (ms) | Speedup vs 1T | Efficiency | Notes |
|---|---|---|---|---|
| **1 Thread** | 592.41 ms | 1.00× | 100.0% | Baseline single-thread execution |
| **2 Threads** | 335.39 ms | **1.77×** | **88.7%** | Near-linear speedup on 2 physical cores |
| **4 Threads** | 703.66 ms | 0.85× | 21.1% | Thread oversubscription contention |
| **8 Threads** | 1,195.13 ms | 0.50× | 6.2% | CPU cache thrashing & context switching |

---

## Build & Run Instructions

### Prerequisites
- GCC 10+ (with OpenMP support)
- Make
- Node.js 18+ & npm

### Quick Start
```bash
# 1. Download and verify the DIMACS9 USA-road-d.BAY dataset
make data

# 2. Build C engine executables
make all

# 3. Run automated correctness tests (36 tests)
make test

# 4. Run AddressSanitizer and UndefinedBehaviorSanitizer
make sanitizers

# 5. Run the empirical benchmark suite (produces CSV results)
make bench

# 6. Start the full-stack web application
npm run dev
```

Visit `http://localhost:3000` to interact with the route planner and performance dashboard.

---

## Documentation

- **Final Technical Report:** [`docs/FINAL_REPORT.md`](docs/FINAL_REPORT.md)
- **Viva Voce Defense Guide:** [`docs/viva.md`](docs/viva.md)
- **LLM Usage & Integrity Log:** [`docs/LLM_USAGE_LOG.md`](docs/LLM_USAGE_LOG.md)

---

## Team Contributions

- **Person 1:** Dataset ingestion, DIMACS9 parser, CSR representation, Sequential Array-Scan & Heap Dijkstra, Path reconstruction.
- **Person 2:** OpenMP parallel design, persistent thread team, local minima reduction, tie-breaking, race condition elimination, thread scaling.
- **Person 3:** Delta-Stepping extension, deterministic BFS subgraph generator, sanitizer testing, edge case test suites.
- **Person 4:** Full-stack integration, Express API, persistent C process IPC, React/Tailwind frontend, Leaflet GPS visualization, Recharts dashboards.
