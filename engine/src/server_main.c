#include "graph.h"
#include "dijkstra.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
#include <inttypes.h>

#define BUFFER_SIZE 65536

static CSRGraph g_graph;
static Coordinates g_coords;
static bool g_loaded = false;

static void send_json_error(const char *msg) {
    printf("{\"status\":\"error\",\"error\":\"%s\"}\n", msg);
    fflush(stdout);
}

static void handle_info_cmd(void) {
    if (!g_loaded) {
        send_json_error("Graph not loaded");
        return;
    }
    GraphStats stats;
    get_graph_stats(&g_graph, &stats);
    int max_t = omp_get_max_threads();

    printf("{\"status\":\"ok\",\"nodes\":%" PRIu32 ",\"arcs\":%" PRIu64
           ",\"avg_degree\":%.2f,\"max_degree\":%" PRIu32 ",\"coords_loaded\":%s,\"max_threads\":%d}\n",
           stats.num_nodes, stats.num_arcs, stats.avg_degree, stats.max_degree,
           g_coords.loaded ? "true" : "false", max_t);
    fflush(stdout);
}

static void handle_route_cmd(const char *line) {
    if (!g_loaded) {
        send_json_error("Graph not loaded");
        return;
    }

    uint32_t src = 1, dst = 1;
    char algo[32] = "omp-scan";
    int threads = 2;

    /* Basic JSON parsing from line */
    const char *p_src = strstr(line, "\"source\":");
    if (p_src) sscanf(p_src + 9, "%" SCNu32, &src);

    const char *p_dst = strstr(line, "\"destination\":");
    if (p_dst) sscanf(p_dst + 14, "%" SCNu32, &dst);

    const char *p_thr = strstr(line, "\"threads\":");
    if (p_thr) sscanf(p_thr + 10, "%d", &threads);

    const char *p_alg = strstr(line, "\"algorithm\":");
    if (p_alg) {
        sscanf(p_alg + 12, "\"%31[^\"]\"", algo);
    }

    if (src < 1 || src > g_graph.num_nodes || dst < 1 || dst > g_graph.num_nodes) {
        send_json_error("Node ID out of bounds (1 to 321270)");
        return;
    }

    uint32_t internal_src = src - 1;
    uint32_t internal_dst = dst - 1;

    ShortestPathResult res;
    if (strcmp(algo, "seq-scan") == 0) {
        res = dijkstra_sequential_scan(&g_graph, internal_src, internal_dst);
    } else if (strcmp(algo, "seq-heap") == 0) {
        res = dijkstra_sequential_heap(&g_graph, internal_src, internal_dst);
    } else if (strcmp(algo, "omp-delta") == 0) {
        res = delta_stepping_omp(&g_graph, internal_src, internal_dst, 2000, threads);
    } else {
        res = dijkstra_omp_scan(&g_graph, internal_src, internal_dst, threads);
    }

    bool valid = false;
    if (res.reachable) {
        valid = validate_path(&g_graph, internal_src, internal_dst, res.distance, res.path, res.path_length);
    }

    /* Format response */
    printf("{\"status\":\"ok\",\"source\":%" PRIu32 ",\"destination\":%" PRIu32
           ",\"algorithm\":\"%s\",\"threads\":%d,\"reachable\":%s,\"distance\":%" PRIu64
           ",\"algorithmMs\":%.3f,\"pathMs\":%.3f,\"nodesSettled\":%" PRIu32 ",\"pathLength\":%" PRIu32
           ",\"validated\":%s",
           src, dst, res.algorithm_name, res.threads_used,
           res.reachable ? "true" : "false",
           res.distance, res.algorithm_ms, res.path_ms, res.nodes_settled,
           res.path_length, valid ? "true" : "false");

    /* Include path node IDs (up to 5000 nodes) */
    printf(",\"path\":[");
    uint32_t output_len = res.path_length > 5000 ? 5000 : res.path_length;
    for (uint32_t i = 0; i < output_len; i++) {
        printf("%" PRIu32 "%s", res.path[i], (i + 1 < output_len) ? "," : "");
    }
    printf("]");

    /* Include coordinates if available */
    if (g_coords.loaded && res.reachable && res.path) {
        printf(",\"coordinates\":[");
        for (uint32_t i = 0; i < output_len; i++) {
            uint32_t node_idx = res.path[i] - 1;
            printf("[%.6f,%.6f]%s", g_coords.lat[node_idx], g_coords.lon[node_idx],
                   (i + 1 < output_len) ? "," : "");
        }
        printf("]");
    }

    printf("}\n");
    fflush(stdout);

    free_result(&res);
}

int main(int argc, char **argv) {
    const char *graph_path = "engine/data/USA-road-d.BAY.gr";
    const char *co_path = "engine/data/USA-road-d.BAY.co";

    if (argc > 1) graph_path = argv[1];
    if (argc > 2) co_path = argv[2];

    fprintf(stderr, "[SERVER] Initializing C/OpenMP Engine with graph: %s\n", graph_path);

    if (load_dimacs_graph(graph_path, &g_graph) == 0) {
        g_loaded = true;
        load_dimacs_coordinates(co_path, &g_coords, g_graph.num_nodes);
        fprintf(stderr, "[SERVER] Engine ready for queries.\n");
    } else {
        fprintf(stderr, "[ERROR] Engine failed to load graph %s\n", graph_path);
    }

    char *line = (char *)malloc(BUFFER_SIZE);
    if (!line) return 1;

    while (fgets(line, BUFFER_SIZE, stdin)) {
        /* Strip newline */
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        if (len == 0) continue;

        if (strstr(line, "\"ping\"") != NULL) {
            printf("{\"status\":\"ok\",\"pong\":true}\n");
            fflush(stdout);
        } else if (strstr(line, "\"info\"") != NULL) {
            handle_info_cmd();
        } else if (strstr(line, "\"route\"") != NULL) {
            handle_route_cmd(line);
        } else {
            send_json_error("Unknown command");
        }
    }

    free(line);
    if (g_loaded) {
        free_graph(&g_graph);
        free_coordinates(&g_coords);
    }
    return 0;
}
