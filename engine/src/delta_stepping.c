#include "dijkstra.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>

#define MAX_BUCKETS 65536

typedef struct {
    uint32_t *nodes;
    size_t size;
    size_t capacity;
} Bucket;

static void bucket_push(Bucket *b, uint32_t u) {
    if (b->size >= b->capacity) {
        size_t new_cap = (b->capacity == 0) ? 64 : b->capacity * 2;
        uint32_t *new_nodes = (uint32_t *)realloc(b->nodes, new_cap * sizeof(uint32_t));
        if (!new_nodes) return;
        b->nodes = new_nodes;
        b->capacity = new_cap;
    }
    b->nodes[b->size++] = u;
}

ShortestPathResult delta_stepping_omp(
    const CSRGraph *g,
    uint32_t src,
    uint32_t dst,
    uint32_t delta,
    int num_threads
) {
    ShortestPathResult res;
    memset(&res, 0, sizeof(ShortestPathResult));
    res.distance = INF_DIST;
    res.algorithm_name = "OpenMP Delta-Stepping";

    if (!g || src >= g->num_nodes || dst >= g->num_nodes) return res;

    int max_avail = omp_get_max_threads();
    if (num_threads <= 0 || num_threads > 128) {
        num_threads = max_avail;
    }
    res.threads_used = num_threads;

    if (delta == 0) delta = 2000; /* Sensible default delta based on average edge weight */

    uint32_t n = g->num_nodes;
    uint64_t *dist = (uint64_t *)malloc(n * sizeof(uint64_t));
    int32_t *pred = (int32_t *)malloc(n * sizeof(int32_t));
    Bucket *buckets = (Bucket *)calloc(MAX_BUCKETS, sizeof(Bucket));

    if (!dist || !pred || !buckets) {
        if (dist) free(dist);
        if (pred) free(pred);
        if (buckets) free(buckets);
        return res;
    }

    #pragma omp parallel for num_threads(num_threads) schedule(static)
    for (uint32_t i = 0; i < n; i++) {
        dist[i] = INF_DIST;
        pred[i] = -1;
    }

    dist[src] = 0;
    bucket_push(&buckets[0], src);

    double t_start = get_time_ms();
    size_t current_bucket = 0;
    uint32_t settled_count = 0;

    while (current_bucket < MAX_BUCKETS) {
        /* Find next non-empty bucket */
        while (current_bucket < MAX_BUCKETS && buckets[current_bucket].size == 0) {
            current_bucket++;
        }
        if (current_bucket >= MAX_BUCKETS) break;

        Bucket *b = &buckets[current_bucket];
        if (b->size == 0) continue;

        /* Process vertices in current bucket */
        size_t bsize = b->size;
        uint32_t *b_nodes = b->nodes;

        #pragma omp parallel for num_threads(num_threads) schedule(guided) reduction(+:settled_count)
        for (size_t i = 0; i < bsize; i++) {
            uint32_t u = b_nodes[i];
            uint64_t d_u = dist[u];
            settled_count++;

            uint64_t start = g->row_ptr[u];
            uint64_t end = g->row_ptr[u + 1];

            for (uint64_t e = start; e < end; e++) {
                uint32_t v = g->col_idx[e];
                uint32_t w = g->weights[e];
                uint64_t new_d = d_u + w;

                if (new_d < dist[v]) {
                    #pragma omp critical
                    {
                        if (new_d < dist[v]) {
                            dist[v] = new_d;
                            pred[v] = (int32_t)u;
                            size_t target_b = (size_t)(new_d / delta);
                            if (target_b < MAX_BUCKETS) {
                                bucket_push(&buckets[target_b], v);
                            }
                        }
                    }
                }
            }
        }

        b->size = 0; /* Clear current bucket */
        if (dist[dst] != INF_DIST && current_bucket * (size_t)delta > dist[dst]) {
            break; /* Early termination: all remaining buckets have distances > dist[dst] */
        }
    }

    res.algorithm_ms = get_time_ms() - t_start;
    res.nodes_settled = settled_count;

    if (dist[dst] != INF_DIST) {
        res.reachable = true;
        res.distance = dist[dst];
        if (src == dst) {
            res.path_length = 1;
            res.path = (uint32_t *)malloc(sizeof(uint32_t));
            if (res.path) res.path[0] = src + 1;
        } else {
            uint32_t count = 0;
            int32_t curr = (int32_t)dst;
            while (curr != -1) {
                count++;
                if ((uint32_t)curr == src) break;
                curr = pred[curr];
                if (count > 2000000) break;
            }
            if (curr == (int32_t)src) {
                res.path_length = count;
                res.path = (uint32_t *)malloc(count * sizeof(uint32_t));
                if (res.path) {
                    curr = (int32_t)dst;
                    for (int32_t i = (int32_t)count - 1; i >= 0; i--) {
                        res.path[i] = (uint32_t)curr + 1;
                        curr = pred[curr];
                    }
                }
            }
        }
    }

    free(dist);
    free(pred);
    for (size_t i = 0; i < MAX_BUCKETS; i++) {
        if (buckets[i].nodes) free(buckets[i].nodes);
    }
    free(buckets);
    return res;
}
