#ifndef ENGINE_GRAPH_H
#define ENGINE_GRAPH_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define INF_DIST UINT64_MAX

typedef struct {
    uint32_t num_nodes;
    uint64_t num_arcs;
    uint64_t *row_ptr;   /* Size: num_nodes + 1 */
    uint32_t *col_idx;   /* Size: num_arcs */
    uint32_t *weights;   /* Size: num_arcs */
} CSRGraph;

typedef struct {
    uint32_t num_nodes;
    double *lat;         /* Latitude in degrees */
    double *lon;         /* Longitude in degrees */
    bool loaded;
} Coordinates;

/* Graph lifecycle and operations */
int load_dimacs_graph(const char *path, CSRGraph *graph);
int load_dimacs_coordinates(const char *path, Coordinates *coords, uint32_t expected_nodes);
void free_graph(CSRGraph *graph);
void free_coordinates(Coordinates *coords);
int validate_graph(const CSRGraph *graph);

typedef struct {
    uint32_t num_nodes;
    uint64_t num_arcs;
    uint32_t min_weight;
    uint32_t max_weight;
    double avg_degree;
    uint32_t max_degree;
    uint32_t zero_degree_nodes;
} GraphStats;

void get_graph_stats(const CSRGraph *graph, GraphStats *stats);

#ifdef __cplusplus
}
#endif

#endif /* ENGINE_GRAPH_H */
