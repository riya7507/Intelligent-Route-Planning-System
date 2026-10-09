#ifndef ENGINE_DIJKSTRA_H
#define ENGINE_DIJKSTRA_H

#include "graph.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint64_t distance;
    uint32_t *path;       /* 1-indexed node IDs for user presentation */
    uint32_t path_length;
    bool reachable;
    double algorithm_ms;  /* Core execution time (excluding graph load & path reconstruction) */
    double path_ms;       /* Path reconstruction time */
    uint32_t nodes_settled;
    int threads_used;
    const char *algorithm_name;
} ShortestPathResult;

/* Algorithm implementations (src and dst are 0-indexed internally) */
ShortestPathResult dijkstra_sequential_scan(const CSRGraph *g, uint32_t src, uint32_t dst);
ShortestPathResult dijkstra_sequential_heap(const CSRGraph *g, uint32_t src, uint32_t dst);
ShortestPathResult dijkstra_omp_scan(const CSRGraph *g, uint32_t src, uint32_t dst, int num_threads);
ShortestPathResult delta_stepping_omp(const CSRGraph *g, uint32_t src, uint32_t dst, uint32_t delta, int num_threads);

/* Path and correctness verification */
bool validate_path(const CSRGraph *g, uint32_t src, uint32_t dst, uint64_t reported_dist, const uint32_t *path, uint32_t path_len);
void free_result(ShortestPathResult *res);

/* Helper timing function returning milliseconds */
double get_time_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* ENGINE_DIJKSTRA_H */
