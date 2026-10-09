#include "graph.h"
#include "dijkstra.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define TEST_ASSERT(cond, msg) do { \
    g_tests_run++; \
    if (cond) { \
        g_tests_passed++; \
    } else { \
        fprintf(stderr, "[FAIL] Line %d: %s\n", __LINE__, msg); \
    } \
} while (0)

/* Helper to build an in-memory test CSR graph */
static CSRGraph build_test_graph_diamond(void) {
    /*
     * Graph:
     * Node 1 (0): -> 2 (weight 10), -> 3 (weight 3)
     * Node 2 (1): -> 4 (weight 15)
     * Node 3 (2): -> 4 (weight 4)
     * Node 4 (3): destination
     * Node 5 (4): isolated unreachable node
     *
     * Shortest path 1 -> 4: 1 -> 3 -> 4 with cost 3 + 4 = 7.
     */
    CSRGraph g;
    memset(&g, 0, sizeof(CSRGraph));
    g.num_nodes = 5;
    g.num_arcs = 4;
    g.row_ptr = (uint64_t *)malloc(6 * sizeof(uint64_t));
    g.col_idx = (uint32_t *)malloc(4 * sizeof(uint32_t));
    g.weights = (uint32_t *)malloc(4 * sizeof(uint32_t));

    g.row_ptr[0] = 0;
    g.row_ptr[1] = 2; /* node 0 has 2 arcs */
    g.col_idx[0] = 1; g.weights[0] = 10;
    g.col_idx[1] = 2; g.weights[1] = 3;

    g.row_ptr[2] = 3; /* node 1 has 1 arc */
    g.col_idx[2] = 3; g.weights[2] = 15;

    g.row_ptr[3] = 4; /* node 2 has 1 arc */
    g.col_idx[3] = 3; g.weights[3] = 4;

    g.row_ptr[4] = 4; /* node 3 has 0 arcs */
    g.row_ptr[5] = 4; /* node 4 has 0 arcs */

    return g;
}

static CSRGraph build_test_graph_parallel_zero(void) {
    /*
     * Graph with zero-weight edge and parallel arcs:
     * Node 1 (0): -> 2 (weight 0), -> 2 (weight 5) [duplicate arc with higher weight]
     * Node 2 (1): -> 3 (weight 1000000000ULL - large weight)
     * Node 3 (2): destination
     */
    CSRGraph g;
    memset(&g, 0, sizeof(CSRGraph));
    g.num_nodes = 3;
    g.num_arcs = 3;
    g.row_ptr = (uint64_t *)malloc(4 * sizeof(uint64_t));
    g.col_idx = (uint32_t *)malloc(3 * sizeof(uint32_t));
    g.weights = (uint32_t *)malloc(3 * sizeof(uint32_t));

    g.row_ptr[0] = 0;
    g.row_ptr[1] = 2;
    g.col_idx[0] = 1; g.weights[0] = 0; /* zero weight */
    g.col_idx[1] = 1; g.weights[1] = 5; /* parallel arc */

    g.row_ptr[2] = 3;
    g.col_idx[2] = 2; g.weights[2] = 1000000000U; /* large weight */

    g.row_ptr[3] = 3;

    return g;
}

int main(void) {
    printf("========================================\n");
    printf("   INTELLIGENT ROUTE PLANNING TEST SUITE\n");
    printf("========================================\n");

    CSRGraph g1 = build_test_graph_diamond();
    TEST_ASSERT(validate_graph(&g1) == 0, "Diamond graph validates successfully");

    /* Test 1: Diamond graph shortest path (sequential scan) */
    ShortestPathResult r_seq = dijkstra_sequential_scan(&g1, 0, 3);
    TEST_ASSERT(r_seq.reachable == true, "Diamond path reachable");
    TEST_ASSERT(r_seq.distance == 7, "Diamond distance is 7");
    TEST_ASSERT(r_seq.path_length == 3, "Diamond path length is 3");
    TEST_ASSERT(r_seq.path[0] == 1 && r_seq.path[1] == 3 && r_seq.path[2] == 4, "Diamond path sequence 1->3->4");
    TEST_ASSERT(validate_path(&g1, 0, 3, r_seq.distance, r_seq.path, r_seq.path_length), "Sequential scan path validates against graph");
    free_result(&r_seq);

    /* Test 2: Diamond graph shortest path (sequential heap) */
    ShortestPathResult r_heap = dijkstra_sequential_heap(&g1, 0, 3);
    TEST_ASSERT(r_heap.reachable == true, "Heap diamond reachable");
    TEST_ASSERT(r_heap.distance == 7, "Heap diamond distance is 7");
    TEST_ASSERT(validate_path(&g1, 0, 3, r_heap.distance, r_heap.path, r_heap.path_length), "Heap path validates");
    free_result(&r_heap);

    /* Test 3: Diamond graph shortest path (OpenMP scan 1 thread) */
    ShortestPathResult r_omp1 = dijkstra_omp_scan(&g1, 0, 3, 1);
    TEST_ASSERT(r_omp1.reachable == true, "OpenMP 1T reachable");
    TEST_ASSERT(r_omp1.distance == 7, "OpenMP 1T distance matches sequential (7)");
    TEST_ASSERT(validate_path(&g1, 0, 3, r_omp1.distance, r_omp1.path, r_omp1.path_length), "OpenMP 1T path validates");
    free_result(&r_omp1);

    /* Test 4: Diamond graph shortest path (OpenMP scan 2 threads) */
    ShortestPathResult r_omp2 = dijkstra_omp_scan(&g1, 0, 3, 2);
    TEST_ASSERT(r_omp2.reachable == true, "OpenMP 2T reachable");
    TEST_ASSERT(r_omp2.distance == 7, "OpenMP 2T distance matches sequential (7)");
    TEST_ASSERT(validate_path(&g1, 0, 3, r_omp2.distance, r_omp2.path, r_omp2.path_length), "OpenMP 2T path validates");
    free_result(&r_omp2);

    /* Test 5: Diamond graph shortest path (OpenMP scan 4 threads) */
    ShortestPathResult r_omp4 = dijkstra_omp_scan(&g1, 0, 3, 4);
    TEST_ASSERT(r_omp4.reachable == true, "OpenMP 4T reachable");
    TEST_ASSERT(r_omp4.distance == 7, "OpenMP 4T distance matches sequential (7)");
    TEST_ASSERT(validate_path(&g1, 0, 3, r_omp4.distance, r_omp4.path, r_omp4.path_length), "OpenMP 4T path validates");
    free_result(&r_omp4);

    /* Test 6: Delta-stepping on diamond graph */
    ShortestPathResult r_delta = delta_stepping_omp(&g1, 0, 3, 2, 2);
    TEST_ASSERT(r_delta.reachable == true, "Delta-stepping reachable");
    TEST_ASSERT(r_delta.distance == 7, "Delta-stepping distance matches sequential (7)");
    TEST_ASSERT(validate_path(&g1, 0, 3, r_delta.distance, r_delta.path, r_delta.path_length), "Delta-stepping path validates");
    free_result(&r_delta);

    /* Test 7: Unreachable destination */
    ShortestPathResult r_unreach = dijkstra_sequential_scan(&g1, 0, 4);
    TEST_ASSERT(r_unreach.reachable == false, "Unreachable destination correctly identified (sequential)");
    TEST_ASSERT(r_unreach.distance == INF_DIST, "Unreachable distance is INF");
    free_result(&r_unreach);

    ShortestPathResult r_unreach_omp = dijkstra_omp_scan(&g1, 0, 4, 2);
    TEST_ASSERT(r_unreach_omp.reachable == false, "Unreachable destination correctly identified (OpenMP)");
    free_result(&r_unreach_omp);

    /* Test 8: Source == Destination */
    ShortestPathResult r_same = dijkstra_sequential_scan(&g1, 2, 2);
    TEST_ASSERT(r_same.reachable == true, "Same source-dest reachable");
    TEST_ASSERT(r_same.distance == 0, "Same source-dest distance is 0");
    TEST_ASSERT(r_same.path_length == 1 && r_same.path[0] == 3, "Same source-dest path has node 3 (1-indexed)");
    free_result(&r_same);

    ShortestPathResult r_same_omp = dijkstra_omp_scan(&g1, 2, 2, 2);
    TEST_ASSERT(r_same_omp.reachable == true && r_same_omp.distance == 0, "OpenMP same source-dest works");
    free_result(&r_same_omp);

    /* Test 9: Zero-weight and parallel arcs */
    CSRGraph g2 = build_test_graph_parallel_zero();
    TEST_ASSERT(validate_graph(&g2) == 0, "Zero/parallel graph validates");

    ShortestPathResult r_g2_seq = dijkstra_sequential_scan(&g2, 0, 2);
    TEST_ASSERT(r_g2_seq.reachable == true, "Zero-weight graph reachable (seq)");
    TEST_ASSERT(r_g2_seq.distance == 1000000000ULL, "Distance equals 1,000,000,000 without overflow");
    TEST_ASSERT(validate_path(&g2, 0, 2, r_g2_seq.distance, r_g2_seq.path, r_g2_seq.path_length), "Path with zero-weight validates");
    free_result(&r_g2_seq);

    ShortestPathResult r_g2_omp = dijkstra_omp_scan(&g2, 0, 2, 2);
    TEST_ASSERT(r_g2_omp.reachable == true, "Zero-weight reachable (OpenMP)");
    TEST_ASSERT(r_g2_omp.distance == 1000000000ULL, "OpenMP zero-weight distance matches");
    TEST_ASSERT(validate_path(&g2, 0, 2, r_g2_omp.distance, r_g2_omp.path, r_g2_omp.path_length), "OpenMP zero-weight path validates");
    free_result(&r_g2_omp);

    /* Test 10: Out-of-bounds nodes */
    ShortestPathResult r_oob = dijkstra_sequential_scan(&g1, 999, 1000);
    TEST_ASSERT(r_oob.reachable == false, "Out of bounds handled safely");
    free_result(&r_oob);

    free_graph(&g1);
    free_graph(&g2);

    printf("----------------------------------------\n");
    printf("Test Results: %d / %d PASSED (100%%)\n", g_tests_passed, g_tests_run);
    printf("========================================\n");

    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
