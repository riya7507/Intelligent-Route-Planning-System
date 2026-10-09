#include "dijkstra.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

double get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}

static void reconstruct_path(
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

    /* Count path length */
    uint32_t count = 0;
    int32_t curr = (int32_t)dst;
    while (curr != -1) {
        count++;
        if ((uint32_t)curr == src) break;
        curr = pred[curr];
        if (count > 2000000) { /* Cycle guard */
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
        res->path[i] = (uint32_t)curr + 1; /* 1-indexed for DIMACS presentation */
        curr = pred[curr];
    }
    res->path_ms = get_time_ms() - p_start;
}

ShortestPathResult dijkstra_sequential_scan(const CSRGraph *g, uint32_t src, uint32_t dst) {
    ShortestPathResult res;
    memset(&res, 0, sizeof(ShortestPathResult));
    res.distance = INF_DIST;
    res.algorithm_name = "Sequential Array-Scan";
    res.threads_used = 1;

    if (!g || src >= g->num_nodes || dst >= g->num_nodes) return res;

    uint32_t n = g->num_nodes;
    uint64_t *dist = (uint64_t *)malloc(n * sizeof(uint64_t));
    uint8_t *settled = (uint8_t *)calloc(n, sizeof(uint8_t));
    int32_t *pred = (int32_t *)malloc(n * sizeof(int32_t));

    if (!dist || !settled || !pred) {
        if (dist) free(dist);
        if (settled) free(settled);
        if (pred) free(pred);
        return res;
    }

    for (uint32_t i = 0; i < n; i++) {
        dist[i] = INF_DIST;
        pred[i] = -1;
    }
    dist[src] = 0;

    double t_start = get_time_ms();
    uint32_t settled_count = 0;

    for (uint32_t iter = 0; iter < n; iter++) {
        uint64_t min_dist = INF_DIST;
        uint32_t min_u = UINT32_MAX;

        /* Linear scan over all vertices to find minimum unsettled vertex */
        for (uint32_t v = 0; v < n; v++) {
            if (!settled[v] && dist[v] < min_dist) {
                min_dist = dist[v];
                min_u = v;
            }
        }

        if (min_u == UINT32_MAX || min_dist == INF_DIST) {
            break; /* No more reachable vertices */
        }

        settled[min_u] = 1;
        settled_count++;

        if (min_u == dst) {
            break; /* Early termination for point-to-point query */
        }

        /* Relax outgoing edges */
        uint64_t start = g->row_ptr[min_u];
        uint64_t end = g->row_ptr[min_u + 1];
        for (uint64_t e = start; e < end; e++) {
            uint32_t v = g->col_idx[e];
            uint32_t w = g->weights[e];
            if (!settled[v]) {
                uint64_t new_dist = min_dist + w;
                if (new_dist < dist[v]) {
                    dist[v] = new_dist;
                    pred[v] = (int32_t)min_u;
                }
            }
        }
    }

    res.algorithm_ms = get_time_ms() - t_start;
    res.nodes_settled = settled_count;

    if (dist[dst] != INF_DIST) {
        res.reachable = true;
        res.distance = dist[dst];
        reconstruct_path(src, dst, pred, &res);
    }

    free(dist);
    free(settled);
    free(pred);
    return res;
}

/* Binary Min-Heap Implementation for Heap Dijkstra */
typedef struct {
    uint64_t dist;
    uint32_t u;
} HeapNode;

typedef struct {
    HeapNode *data;
    size_t size;
    size_t capacity;
} MinHeap;

static MinHeap *create_heap(size_t capacity) {
    MinHeap *h = (MinHeap *)malloc(sizeof(MinHeap));
    if (!h) return NULL;
    h->data = (HeapNode *)malloc(capacity * sizeof(HeapNode));
    if (!h->data) {
        free(h);
        return NULL;
    }
    h->size = 0;
    h->capacity = capacity;
    return h;
}

static void free_heap(MinHeap *h) {
    if (h) {
        if (h->data) free(h->data);
        free(h);
    }
}

static void heap_push(MinHeap *h, uint32_t u, uint64_t dist) {
    if (h->size >= h->capacity) {
        size_t new_cap = h->capacity * 2;
        HeapNode *new_data = (HeapNode *)realloc(h->data, new_cap * sizeof(HeapNode));
        if (!new_data) return;
        h->data = new_data;
        h->capacity = new_cap;
    }

    size_t i = h->size++;
    while (i > 0) {
        size_t p = (i - 1) / 2;
        if (h->data[p].dist <= dist) break;
        h->data[i] = h->data[p];
        i = p;
    }
    h->data[i].u = u;
    h->data[i].dist = dist;
}

static bool heap_pop(MinHeap *h, uint32_t *out_u, uint64_t *out_dist) {
    if (h->size == 0) return false;
    *out_u = h->data[0].u;
    *out_dist = h->data[0].dist;

    HeapNode last = h->data[--h->size];
    if (h->size == 0) return true;

    size_t i = 0;
    while (i * 2 + 1 < h->size) {
        size_t left = i * 2 + 1;
        size_t right = left + 1;
        size_t smallest = left;
        if (right < h->size && h->data[right].dist < h->data[left].dist) {
            smallest = right;
        }
        if (last.dist <= h->data[smallest].dist) break;
        h->data[i] = h->data[smallest];
        i = smallest;
    }
    h->data[i] = last;
    return true;
}

ShortestPathResult dijkstra_sequential_heap(const CSRGraph *g, uint32_t src, uint32_t dst) {
    ShortestPathResult res;
    memset(&res, 0, sizeof(ShortestPathResult));
    res.distance = INF_DIST;
    res.algorithm_name = "Sequential Binary-Heap";
    res.threads_used = 1;

    if (!g || src >= g->num_nodes || dst >= g->num_nodes) return res;

    uint32_t n = g->num_nodes;
    uint64_t *dist = (uint64_t *)malloc(n * sizeof(uint64_t));
    uint8_t *settled = (uint8_t *)calloc(n, sizeof(uint8_t));
    int32_t *pred = (int32_t *)malloc(n * sizeof(int32_t));
    MinHeap *heap = create_heap(4096);

    if (!dist || !settled || !pred || !heap) {
        if (dist) free(dist);
        if (settled) free(settled);
        if (pred) free(pred);
        if (heap) free_heap(heap);
        return res;
    }

    for (uint32_t i = 0; i < n; i++) {
        dist[i] = INF_DIST;
        pred[i] = -1;
    }
    dist[src] = 0;
    heap_push(heap, src, 0);

    double t_start = get_time_ms();
    uint32_t settled_count = 0;

    uint32_t u;
    uint64_t d;
    while (heap_pop(heap, &u, &d)) {
        if (settled[u]) continue;
        settled[u] = 1;
        settled_count++;

        if (u == dst) break;

        uint64_t start = g->row_ptr[u];
        uint64_t end = g->row_ptr[u + 1];
        for (uint64_t e = start; e < end; e++) {
            uint32_t v = g->col_idx[e];
            uint32_t w = g->weights[e];
            if (!settled[v]) {
                uint64_t new_dist = d + w;
                if (new_dist < dist[v]) {
                    dist[v] = new_dist;
                    pred[v] = (int32_t)u;
                    heap_push(heap, v, new_dist);
                }
            }
        }
    }

    res.algorithm_ms = get_time_ms() - t_start;
    res.nodes_settled = settled_count;

    if (dist[dst] != INF_DIST) {
        res.reachable = true;
        res.distance = dist[dst];
        reconstruct_path(src, dst, pred, &res);
    }

    free(dist);
    free(settled);
    free(pred);
    free_heap(heap);
    return res;
}

bool validate_path(
    const CSRGraph *g,
    uint32_t src,
    uint32_t dst,
    uint64_t reported_dist,
    const uint32_t *path,
    uint32_t path_len
) {
    if (!g) return false;
    if (src == dst) {
        return (path_len == 1 && path[0] == src + 1 && reported_dist == 0);
    }
    if (path_len < 2 || !path) return false;
    if (path[0] != src + 1 || path[path_len - 1] != dst + 1) return false;

    uint64_t total_weight = 0;
    for (uint32_t i = 0; i < path_len - 1; i++) {
        uint32_t u = path[i] - 1;
        uint32_t v = path[i + 1] - 1;
        bool arc_found = false;
        uint32_t arc_weight = 0;

        uint64_t start = g->row_ptr[u];
        uint64_t end = g->row_ptr[u + 1];
        for (uint64_t e = start; e < end; e++) {
            if (g->col_idx[e] == v) {
                arc_found = true;
                arc_weight = g->weights[e];
                break;
            }
        }

        if (!arc_found) return false;
        total_weight += arc_weight;
    }

    return (total_weight == reported_dist);
}

void free_result(ShortestPathResult *res) {
    if (res && res->path) {
        free(res->path);
        res->path = NULL;
        res->path_length = 0;
    }
}
