#include "graph.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

int load_dimacs_graph(const char *path, CSRGraph *graph) {
    if (!path || !graph) return -1;
    memset(graph, 0, sizeof(CSRGraph));

    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "[ERROR] Unable to open graph file: %s\n", path);
        return -1;
    }

    char line[512];
    uint32_t num_nodes = 0;
    uint64_t num_arcs = 0;
    bool found_header = false;

    /* Pass 1: find header and count degrees */
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == 'c') continue;
        if (line[0] == 'p') {
            char ptype[16];
            if (sscanf(line, "p %15s %" SCNu32 " %" SCNu64, ptype, &num_nodes, &num_arcs) >= 3) {
                if (strcmp(ptype, "sp") == 0) {
                    found_header = true;
                    break;
                }
            }
        }
    }

    if (!found_header || num_nodes == 0 || num_arcs == 0) {
        fprintf(stderr, "[ERROR] Invalid or missing DIMACS 'p sp' header in %s\n", path);
        fclose(f);
        return -2;
    }

    /* Allocate degree count array */
    uint32_t *degrees = (uint32_t *)calloc(num_nodes, sizeof(uint32_t));
    if (!degrees) {
        fprintf(stderr, "[ERROR] Memory allocation failed for vertex degrees\n");
        fclose(f);
        return -3;
    }

    uint64_t parsed_arcs = 0;
    while (fgets(line, sizeof(line), f)) {
        if (line[0] != 'a') continue;
        uint32_t u, v, w;
        if (sscanf(line, "a %" SCNu32 " %" SCNu32 " %" SCNu32, &u, &v, &w) == 3) {
            if (u >= 1 && u <= num_nodes && v >= 1 && v <= num_nodes) {
                degrees[u - 1]++;
                parsed_arcs++;
            }
        }
    }

    if (parsed_arcs == 0) {
        fprintf(stderr, "[ERROR] No valid arc records found in %s\n", path);
        free(degrees);
        fclose(f);
        return -4;
    }

    /* Allocate CSR arrays */
    graph->num_nodes = num_nodes;
    graph->num_arcs = parsed_arcs;
    graph->row_ptr = (uint64_t *)malloc((num_nodes + 1) * sizeof(uint64_t));
    graph->col_idx = (uint32_t *)malloc(parsed_arcs * sizeof(uint32_t));
    graph->weights = (uint32_t *)malloc(parsed_arcs * sizeof(uint32_t));

    if (!graph->row_ptr || !graph->col_idx || !graph->weights) {
        fprintf(stderr, "[ERROR] Memory allocation failed for CSR graph\n");
        free(degrees);
        free_graph(graph);
        fclose(f);
        return -5;
    }

    /* Prefix sum to compute row_ptr */
    graph->row_ptr[0] = 0;
    for (uint32_t i = 0; i < num_nodes; i++) {
        graph->row_ptr[i + 1] = graph->row_ptr[i] + degrees[i];
    }

    /* Temporary cursor array initialized to row_ptr */
    uint64_t *cursor = (uint64_t *)malloc(num_nodes * sizeof(uint64_t));
    if (!cursor) {
        free(degrees);
        free_graph(graph);
        fclose(f);
        return -6;
    }
    for (uint32_t i = 0; i < num_nodes; i++) {
        cursor[i] = graph->row_ptr[i];
    }
    free(degrees);

    /* Pass 2: populate CSR arrays */
    rewind(f);
    while (fgets(line, sizeof(line), f)) {
        if (line[0] != 'a') continue;
        uint32_t u, v, w;
        if (sscanf(line, "a %" SCNu32 " %" SCNu32 " %" SCNu32, &u, &v, &w) == 3) {
            if (u >= 1 && u <= num_nodes && v >= 1 && v <= num_nodes) {
                uint32_t u_idx = u - 1;
                uint32_t v_idx = v - 1;
                uint64_t pos = cursor[u_idx]++;
                graph->col_idx[pos] = v_idx;
                graph->weights[pos] = w;
            }
        }
    }

    free(cursor);
    fclose(f);

    fprintf(stderr, "[INFO] Loaded CSR graph: %" PRIu32 " nodes, %" PRIu64 " arcs\n",
            graph->num_nodes, graph->num_arcs);
    return 0;
}

int load_dimacs_coordinates(const char *path, Coordinates *coords, uint32_t expected_nodes) {
    if (!path || !coords) return -1;
    memset(coords, 0, sizeof(Coordinates));

    FILE *f = fopen(path, "r");
    if (!f) return -1;

    coords->num_nodes = expected_nodes;
    coords->lat = (double *)malloc(expected_nodes * sizeof(double));
    coords->lon = (double *)malloc(expected_nodes * sizeof(double));

    if (!coords->lat || !coords->lon) {
        free_coordinates(coords);
        fclose(f);
        return -2;
    }

    char line[256];
    uint32_t loaded_count = 0;

    while (fgets(line, sizeof(line), f)) {
        if (line[0] != 'v') continue;
        uint32_t id;
        long long lon_raw, lat_raw;
        if (sscanf(line, "v %" SCNu32 " %lld %lld", &id, &lon_raw, &lat_raw) == 3) {
            if (id >= 1 && id <= expected_nodes) {
                uint32_t idx = id - 1;
                /* DIMACS9 coords are integer microdegrees (1e-6) */
                coords->lon[idx] = (double)lon_raw / 1000000.0;
                coords->lat[idx] = (double)lat_raw / 1000000.0;
                loaded_count++;
            }
        }
    }

    fclose(f);
    if (loaded_count > 0) {
        coords->loaded = true;
        fprintf(stderr, "[INFO] Loaded %" PRIu32 " coordinates from %s\n", loaded_count, path);
        return 0;
    }

    free_coordinates(coords);
    return -3;
}

void free_graph(CSRGraph *graph) {
    if (!graph) return;
    if (graph->row_ptr) free(graph->row_ptr);
    if (graph->col_idx) free(graph->col_idx);
    if (graph->weights) free(graph->weights);
    memset(graph, 0, sizeof(CSRGraph));
}

void free_coordinates(Coordinates *coords) {
    if (!coords) return;
    if (coords->lat) free(coords->lat);
    if (coords->lon) free(coords->lon);
    memset(coords, 0, sizeof(Coordinates));
}

int validate_graph(const CSRGraph *graph) {
    if (!graph || !graph->row_ptr || !graph->col_idx || !graph->weights) return -1;
    if (graph->num_nodes == 0) return -2;

    if (graph->row_ptr[0] != 0) return -3;
    if (graph->row_ptr[graph->num_nodes] != graph->num_arcs) return -4;

    for (uint32_t u = 0; u < graph->num_nodes; u++) {
        uint64_t start = graph->row_ptr[u];
        uint64_t end = graph->row_ptr[u + 1];
        if (start > end || end > graph->num_arcs) return -5;

        for (uint64_t e = start; e < end; e++) {
            if (graph->col_idx[e] >= graph->num_nodes) {
                return -6; /* Out-of-bounds target vertex */
            }
        }
    }
    return 0;
}

void get_graph_stats(const CSRGraph *graph, GraphStats *stats) {
    if (!graph || !stats) return;
    memset(stats, 0, sizeof(GraphStats));

    stats->num_nodes = graph->num_nodes;
    stats->num_arcs = graph->num_arcs;
    stats->min_weight = UINT32_MAX;
    stats->max_weight = 0;
    stats->max_degree = 0;
    stats->zero_degree_nodes = 0;

    for (uint32_t u = 0; u < graph->num_nodes; u++) {
        uint64_t deg = graph->row_ptr[u + 1] - graph->row_ptr[u];
        if (deg == 0) stats->zero_degree_nodes++;
        if (deg > stats->max_degree) stats->max_degree = (uint32_t)deg;

        for (uint64_t e = graph->row_ptr[u]; e < graph->row_ptr[u + 1]; e++) {
            uint32_t w = graph->weights[e];
            if (w < stats->min_weight) stats->min_weight = w;
            if (w > stats->max_weight) stats->max_weight = w;
        }
    }

    if (stats->num_nodes > 0) {
        stats->avg_degree = (double)stats->num_arcs / (double)stats->num_nodes;
    }
}
