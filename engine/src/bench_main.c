#include "graph.h"
#include "dijkstra.h"
#include "subgraph.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
#include <sys/utsname.h>
#include <inttypes.h>

#define BENCH_DIR "bench/results"

static void get_system_info(char *buf, size_t buf_len) {
    struct utsname u;
    if (uname(&u) == 0) {
        snprintf(buf, buf_len, "%s %s (%s)", u.sysname, u.release, u.machine);
    } else {
        snprintf(buf, buf_len, "Linux x86_64");
    }
}

int main(int argc, char **argv) {
    const char *graph_path = "engine/data/USA-road-d.BAY.gr";
    if (argc > 1) {
        graph_path = argv[1];
    }

    printf("=================================================================\n");
    printf("   INTELLIGENT ROUTE PLANNING BENCHMARK & EXPERIMENT RUNNER      \n");
    printf("=================================================================\n");

    CSRGraph full_graph;
    printf("[1/5] Loading primary road network: %s ...\n", graph_path);
    if (load_dimacs_graph(graph_path, &full_graph) != 0) {
        fprintf(stderr, "[ERROR] Failed to load %s\n", graph_path);
        return 1;
    }

    GraphStats stats;
    get_graph_stats(&full_graph, &stats);
    printf("Graph loaded: %" PRIu32 " nodes, %" PRIu64 " arcs, avg deg: %.2f\n",
           stats.num_nodes, stats.num_arcs, stats.avg_degree);

    char sys_info[256];
    get_system_info(sys_info, sizeof(sys_info));
    int max_threads = omp_get_max_threads();
    printf("Hardware: %s, Available OpenMP threads: %d\n", sys_info, max_threads);

    system("mkdir -p " BENCH_DIR);

    /* -------------------------------------------------------------
     * EXPERIMENT 1: Network-Size Scaling
     * Subgraph targets: 1,000, 5,000, 10,000, 25,000, 50,000, 100,000
     * ------------------------------------------------------------- */
    printf("\n[2/5] Running Primary Experiment: Network-Size Scaling ...\n");
    FILE *f_net = fopen(BENCH_DIR "/network_scaling.csv", "w");
    if (!f_net) {
        fprintf(stderr, "[ERROR] Unable to write " BENCH_DIR "/network_scaling.csv\n");
        return 1;
    }

    fprintf(f_net, "target_nodes,actual_nodes,actual_arcs,algorithm,threads,repetition,time_ms,distance,speedup_vs_seq_scan,efficiency\n");

    uint32_t target_sizes[] = {1000, 5000, 10000, 25000, 50000, 100000};
    size_t num_sizes = sizeof(target_sizes) / sizeof(target_sizes[0]);

    for (size_t s = 0; s < num_sizes; s++) {
        uint32_t target_n = target_sizes[s];
        char sub_path[256];
        snprintf(sub_path, sizeof(sub_path), "engine/data/subgraph_%" PRIu32 ".gr", target_n);

        uint32_t act_n = 0;
        uint64_t act_m = 0;
        printf(" -> Generating deterministic induced subgraph (size ~%" PRIu32 ") ...\n", target_n);
        generate_induced_subgraph(&full_graph, target_n, 0, sub_path, &act_n, &act_m);

        CSRGraph sub_g;
        if (load_dimacs_graph(sub_path, &sub_g) != 0) {
            fprintf(stderr, "[WARN] Could not load subgraph %s\n", sub_path);
            continue;
        }

        /* Reproducible query: source = 0, destination = act_n - 1 */
        uint32_t src = 0;
        uint32_t dst = act_n - 1;

        int reps = 3;
        double seq_scan_avg = 0.0;
        double omp_scan_avg = 0.0;
        double heap_avg = 0.0;
        double delta_avg = 0.0;

        /* Sequential Array-Scan */
        for (int r = 1; r <= reps; r++) {
            ShortestPathResult res = dijkstra_sequential_scan(&sub_g, src, dst);
            seq_scan_avg += res.algorithm_ms;
            fprintf(f_net, "%" PRIu32 ",%" PRIu32 ",%" PRIu64 ",seq-scan,1,%d,%.4f,%" PRIu64 ",1.0,1.0\n",
                    target_n, act_n, act_m, r, res.algorithm_ms, res.distance);
            free_result(&res);
        }
        seq_scan_avg /= reps;

        /* OpenMP Parallel Array-Scan (using 2 threads) */
        int test_threads = (max_threads >= 2) ? 2 : 1;
        for (int r = 1; r <= reps; r++) {
            ShortestPathResult res = dijkstra_omp_scan(&sub_g, src, dst, test_threads);
            omp_scan_avg += res.algorithm_ms;
            double sp = (res.algorithm_ms > 0) ? (seq_scan_avg / res.algorithm_ms) : 1.0;
            double eff = sp / test_threads;
            fprintf(f_net, "%" PRIu32 ",%" PRIu32 ",%" PRIu64 ",omp-scan,%d,%d,%.4f,%" PRIu64 ",%.4f,%.4f\n",
                    target_n, act_n, act_m, test_threads, r, res.algorithm_ms, res.distance, sp, eff);
            free_result(&res);
        }
        omp_scan_avg /= reps;

        /* Sequential Heap */
        for (int r = 1; r <= reps; r++) {
            ShortestPathResult res = dijkstra_sequential_heap(&sub_g, src, dst);
            heap_avg += res.algorithm_ms;
            fprintf(f_net, "%" PRIu32 ",%" PRIu32 ",%" PRIu64 ",seq-heap,1,%d,%.4f,%" PRIu64 ",N/A,N/A\n",
                    target_n, act_n, act_m, r, res.algorithm_ms, res.distance);
            free_result(&res);
        }
        heap_avg /= reps;

        /* Delta-stepping parallel */
        for (int r = 1; r <= reps; r++) {
            ShortestPathResult res = delta_stepping_omp(&sub_g, src, dst, 2000, test_threads);
            delta_avg += res.algorithm_ms;
            fprintf(f_net, "%" PRIu32 ",%" PRIu32 ",%" PRIu64 ",omp-delta,%d,%d,%.4f,%" PRIu64 ",N/A,N/A\n",
                    target_n, act_n, act_m, test_threads, r, res.algorithm_ms, res.distance);
            free_result(&res);
        }
        delta_avg /= reps;

        double sp = (omp_scan_avg > 0) ? (seq_scan_avg / omp_scan_avg) : 1.0;
        printf("    V=%" PRIu32 ", E=%" PRIu64 " | Seq-Scan: %.2fms | OMP-Scan: %.2fms (Speedup: %.2fx) | Heap: %.2fms | Delta: %.2fms\n",
               act_n, act_m, seq_scan_avg, omp_scan_avg, sp, heap_avg, delta_avg);

        free_graph(&sub_g);
    }
    fclose(f_net);

    /* -------------------------------------------------------------
     * EXPERIMENT 2: Thread-Count Scaling (on 25k and 50k nodes)
     * Threads: 1, 2, 4, 8
     * ------------------------------------------------------------- */
    printf("\n[3/5] Running Thread-Count Scaling Experiment ...\n");
    FILE *f_thr = fopen(BENCH_DIR "/thread_scaling.csv", "w");
    if (!f_thr) {
        fprintf(stderr, "[ERROR] Unable to write " BENCH_DIR "/thread_scaling.csv\n");
        return 1;
    }

    fprintf(f_thr, "graph_nodes,threads,algorithm,repetition,time_ms,speedup_vs_1t,efficiency\n");

    uint32_t thread_bench_nodes = 25000;
    char sub_thread_path[256];
    snprintf(sub_thread_path, sizeof(sub_thread_path), "engine/data/subgraph_%" PRIu32 ".gr", thread_bench_nodes);
    CSRGraph sub_thread_g;
    if (load_dimacs_graph(sub_thread_path, &sub_thread_g) == 0) {
        uint32_t src = 0;
        uint32_t dst = sub_thread_g.num_nodes - 1;

        /* Measure 1T baseline time */
        double t1_avg = 0.0;
        int reps = 3;
        for (int r = 0; r < reps; r++) {
            ShortestPathResult res = dijkstra_omp_scan(&sub_thread_g, src, dst, 1);
            t1_avg += res.algorithm_ms;
            free_result(&res);
        }
        t1_avg /= reps;

        int thread_configs[] = {1, 2, 4, 8};
        for (size_t t = 0; t < sizeof(thread_configs)/sizeof(thread_configs[0]); t++) {
            int th = thread_configs[t];
            double th_avg = 0.0;
            for (int r = 1; r <= reps; r++) {
                ShortestPathResult res = dijkstra_omp_scan(&sub_thread_g, src, dst, th);
                th_avg += res.algorithm_ms;
                double sp = (t1_avg > 0 && res.algorithm_ms > 0) ? (t1_avg / res.algorithm_ms) : 1.0;
                double eff = sp / th;
                fprintf(f_thr, "%" PRIu32 ",%d,omp-scan,%d,%.4f,%.4f,%.4f\n",
                        sub_thread_g.num_nodes, th, r, res.algorithm_ms, sp, eff);
                free_result(&res);
            }
            th_avg /= reps;
            double sp = (th_avg > 0) ? (t1_avg / th_avg) : 1.0;
            printf("    Threads: %d | Time: %.2fms | Speedup: %.2fx | Efficiency: %.1f%%\n",
                   th, th_avg, sp, (sp / th) * 100.0);
        }
        free_graph(&sub_thread_g);
    }
    fclose(f_thr);

    /* -------------------------------------------------------------
     * EXPERIMENT 3: Query Benchmarks on Full Road Network (321k nodes)
     * ------------------------------------------------------------- */
    printf("\n[4/5] Running Real Workload Query Benchmarks on Full Graph ...\n");
    FILE *f_qry = fopen(BENCH_DIR "/query_benchmarks.csv", "w");
    if (f_qry) {
        fprintf(f_qry, "query_id,source,destination,algorithm,threads,time_ms,path_len,distance,validated\n");

        /* Sample realistic Bay Area origin-destination pairs */
        uint32_t test_pairs[][2] = {
            {1, 500},
            {1, 1000},
            {500, 2500},
            {1000, 5000},
            {2500, 10000}
        };
        size_t num_pairs = sizeof(test_pairs) / sizeof(test_pairs[0]);

        for (size_t p = 0; p < num_pairs; p++) {
            uint32_t s = test_pairs[p][0] - 1;
            uint32_t d = test_pairs[p][1] - 1;

            /* Seq-Heap */
            ShortestPathResult rh = dijkstra_sequential_heap(&full_graph, s, d);
            bool val_h = validate_path(&full_graph, s, d, rh.distance, rh.path, rh.path_length);
            fprintf(f_qry, "%zu,%" PRIu32 ",%" PRIu32 ",seq-heap,1,%.4f,%" PRIu32 ",%" PRIu64 ",%s\n",
                    p + 1, s + 1, d + 1, rh.algorithm_ms, rh.path_length, rh.distance, val_h ? "true" : "false");
            fflush(f_qry);

            /* OMP-Scan */
            ShortestPathResult ro = dijkstra_omp_scan(&full_graph, s, d, max_threads);
            bool val_o = validate_path(&full_graph, s, d, ro.distance, ro.path, ro.path_length);
            fprintf(f_qry, "%zu,%" PRIu32 ",%" PRIu32 ",omp-scan,%d,%.4f,%" PRIu32 ",%" PRIu64 ",%s\n",
                    p + 1, s + 1, d + 1, max_threads, ro.algorithm_ms, ro.path_length, ro.distance, val_o ? "true" : "false");
            fflush(f_qry);

            printf("    Query %zu (Node %" PRIu32 " -> %" PRIu32 "): Dist=%" PRIu64 " | Heap: %.2fms | OMP-Scan: %.2fms\n",
                   p + 1, s + 1, d + 1, rh.distance, rh.algorithm_ms, ro.algorithm_ms);

            free_result(&rh);
            free_result(&ro);
        }
        fclose(f_qry);
    }

    free_graph(&full_graph);
    printf("\n[5/5] All benchmark CSV files generated successfully in " BENCH_DIR "/!\n");
    return 0;
}
