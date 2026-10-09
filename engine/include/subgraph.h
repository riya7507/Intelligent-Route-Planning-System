#ifndef ENGINE_SUBGRAPH_H
#define ENGINE_SUBGRAPH_H

#include "graph.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Deterministically generates an induced subgraph using BFS from seed_node.
 * Produces a valid DIMACS9 .gr file with compact 1-indexed IDs and original weights.
 * Returns 0 on success, non-zero on failure.
 */
int generate_induced_subgraph(
    const CSRGraph *full_g,
    uint32_t target_nodes,
    uint32_t seed_node,
    const char *out_path,
    uint32_t *actual_nodes_out,
    uint64_t *actual_arcs_out
);

#ifdef __cplusplus
}
#endif

#endif /* ENGINE_SUBGRAPH_H */
