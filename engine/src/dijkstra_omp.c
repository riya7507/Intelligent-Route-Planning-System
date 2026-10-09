#include "dijkstra.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>

typedef struct {
    uint64_t dist;
    uint32_t u;
    char pad[64 - sizeof(uint64_t) - sizeof(uint32_t)]; /* Cache-line padding to avoid false sharing */
} ThreadLocalMin;

static void reconstruct_path_omp(
    uint32_t src,
    uint32_t dst,
    const int32_t *pred,
    ShortestPathResult *res
) {
    if (!res->reachable) return;

    double p_start = get_time_ms();
    if (src == dst) {
        res->path_length = 1;
        res->path = (uint32_t *)malloc(sizeof(uint32_t));
        if (res->path) res->path[0] = src + 1;
        res->path_ms = get_time_ms() - p_start;
        return;
    }

    uint32_t count = 0;
    int32_t curr = (int32_t)dst;
    while (curr != -1) {
        count++;
        if ((uint32_t)curr == src) break;
        curr = pred[curr];
        if (count > 2000000) {
            res->reachable = false;
            return;
        }
    }

    if (curr != (int32_t)src) {
        res->reachable = false;
        return;
    }

    res->path_length = count;
    res->path = (uint32_t *)malloc(count * sizeof(uint32_t));
    if (!res->path) return;

    curr = (int32_t)dst;
    for (int32_t i = (int32_t)count - 1; i >= 0; i--) {
        res->path[i] = (uint32_t)curr + 1;
        curr = pred[curr];
    }
    res->path_ms = get_time_ms() - p_start;
}

ShortestPathResult dijkstra_omp_scan(const CSRGraph *g, uint32_t src, uint32_t dst, int num_threads) {
    ShortestPathResult res;
    memset(&res, 0, sizeof(ShortestPathResult));
    res.distance = INF_DIST;
    res.algorithm_name = "OpenMP Parallel Array-Scan";

    if (!g || src >= g->num_nodes || dst >= g->num_nodes) return res;

    int max_avail = omp_get_max_threads();
    if (num_threads <= 0 || num_threads > 128) {
        num_threads = max_avail;
    }
    res.threads_used = num_threads;

    uint32_t n = g->num_nodes;
    uint64_t *dist = (uint64_t *)malloc(n * sizeof(uint64_t));
    uint8_t *settled = (uint8_t *)calloc(n, sizeof(uint8_t));
    int32_t *pred = (int32_t *)malloc(n * sizeof(int32_t));
    ThreadLocalMin *local_mins = (ThreadLocalMin *)malloc(num_threads * sizeof(ThreadLocalMin));

    if (!dist || !settled || !pred || !local_mins) {
        if (dist) free(dist);
        if (settled) free(settled);
        if (pred) free(pred);
        if (local_mins) free(local_mins);
        return res;
    }

    /* Parallel initialization */
    #pragma omp parallel for num_threads(num_threads) schedule(static)
    for (uint32_t i = 0; i < n; i++) {
        dist[i] = INF_DIST;
        pred[i] = -1;
    }
    dist[src] = 0;

    double t_start = get_time_ms();
    volatile int stop_flag = 0;
    uint32_t settled_count = 0;

    /*
     * Persistent OpenMP Parallel Region:
     * Eliminates thread spawn/join overhead on every loop iteration.
     * Each thread computes a local minimum on its partition of vertices.
     * Master thread reduces local minima with deterministic tie-breaking and relaxes edges.
     */
    #pragma omp parallel num_threads(num_threads)
    {
        int tid = omp_get_thread_num();
        int nthreads = omp_get_num_threads();

        /* Partition vertex range [0, n) among threads */
        uint32_t chunk = (n + nthreads - 1) / nthreads;
        uint32_t my_start = (uint32_t)tid * chunk;
        if (my_start > n) my_start = n;
        uint32_t my_end = my_start + chunk;
        if (my_end > n) my_end = n;

        for (uint32_t iter = 0; iter < n; iter++) {
            /* 1. Local minimum scan */
            uint64_t loc_dist = INF_DIST;
            uint32_t loc_u = UINT32_MAX;

            for (uint32_t v = my_start; v < my_end; v++) {
                if (!settled[v] && dist[v] < loc_dist) {
                    loc_dist = dist[v];
                    loc_u = v;
                }
            }

            local_mins[tid].dist = loc_dist;
            local_mins[tid].u = loc_u;

            /* Synchronize all threads before reduction */
            #pragma omp barrier

            /* 2. Reduction and edge relaxation by master thread */
            #pragma omp master
            {
                uint64_t best_dist = INF_DIST;
                uint32_t best_u = UINT32_MAX;

                for (int t = 0; t < nthreads; t++) {
                    uint64_t td = local_mins[t].dist;
                    uint32_t tu = local_mins[t].u;
                    if (td < best_dist) {
                        best_dist = td;
                        best_u = tu;
                    } else if (td == best_dist && tu < best_u) {
                        /* Deterministic tie-breaking: lower node ID wins */
                        best_u = tu;
                    }
                }

                if (best_u == UINT32_MAX || best_dist == INF_DIST) {
                    stop_flag = 1;
                } else {
                    settled[best_u] = 1;
                    settled_count++;

                    if (best_u == dst) {
                        stop_flag = 1;
                    } else {
                        /* Relax outgoing edges */
                        uint64_t start = g->row_ptr[best_u];
                        uint64_t end = g->row_ptr[best_u + 1];
                        for (uint64_t e = start; e < end; e++) {
                            uint32_t v = g->col_idx[e];
                            uint32_t w = g->weights[e];
                            if (!settled[v]) {
                                uint64_t new_dist = best_dist + w;
                                if (new_dist < dist[v]) {
                                    dist[v] = new_dist;
                                    pred[v] = (int32_t)best_u;
                                }
                            }
                        }
                    }
                }
            }

            /* Synchronize before checking termination flag */
            #pragma omp barrier
            if (stop_flag) {
                break;
            }
        }
    }

    res.algorithm_ms = get_time_ms() - t_start;
    res.nodes_settled = settled_count;

    if (dist[dst] != INF_DIST) {
        res.reachable = true;
        res.distance = dist[dst];
        reconstruct_path_omp(src, dst, pred, &res);
    }

    free(dist);
    free(settled);
    free(pred);
    free(local_mins);
    return res;
}
