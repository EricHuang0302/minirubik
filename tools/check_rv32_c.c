/* Host-only exhaustive oracle for the small-table C solver. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RV32_C_NO_MAIN
#include "../rv32_c.c"

enum { STATES = 5040 * 729 };

int main(void)
{
    uint16_t parsed_p, parsed_o;
    const char *invalid[] = {"1234567111111",  "123456711111111",
                             "02345671111111", "82345671111111",
                             "12345671111110", "12345671111114",
                             "1234567111111a", "11345671111111",
                             "12345671111112"};
    if (!rv32_parse("12345671111111", &parsed_p, &parsed_o) || parsed_p ||
        parsed_o)
        return 1;
    for (size_t i = 0; i < sizeof invalid / sizeof invalid[0]; ++i)
        if (rv32_parse(invalid[i], &parsed_p, &parsed_o))
            return 1;

    size_t table_bytes = sizeof perm_next + sizeof ori_next +
                         sizeof perm_distance + sizeof ori_distance;
    if (table_bytes >= 128 * 1024)
        return 1;
    uint8_t *distance = malloc(STATES);
    uint32_t *queue = malloc((size_t) STATES * sizeof *queue);
    if (!distance || !queue)
        return 2;
    memset(distance, UINT8_MAX, STATES);
    distance[0] = 0;
    queue[0] = 0;
    uint32_t head = 0, tail = 1;
    while (head < tail) {
        uint32_t rank = queue[head++];
        uint16_t p = (uint16_t) (rank / 729), o = (uint16_t) (rank % 729);
        for (uint8_t move = 0; move < MOVES; ++move) {
            uint32_t there = (uint32_t) perm_next[move][p] * 729 +
                             ori_next[move][o];
            if (distance[there] == UINT8_MAX) {
                distance[there] = (uint8_t) (distance[rank] + 1);
                queue[tail++] = there;
            }
        }
    }
    if (tail != STATES) {
        fprintf(stderr, "BFS reached %u / %u states\n", tail, STATES);
        return 1;
    }

    uint8_t max_p = 0, max_o = 0, max_d = 0;
    for (uint16_t p = 0; p < 5040; ++p) {
        if (perm_distance[p] == UINT8_MAX)
            return 1;
        if (perm_distance[p] > max_p)
            max_p = perm_distance[p];
        for (uint8_t move = 0; move < MOVES; ++move)
            if (perm_next[move][p] >= 5040)
                return 1;
    }
    for (uint16_t o = 0; o < 729; ++o) {
        if (ori_distance[o] == UINT8_MAX)
            return 1;
        if (ori_distance[o] > max_o)
            max_o = ori_distance[o];
        for (uint8_t move = 0; move < MOVES; ++move)
            if (ori_next[move][o] >= 729)
                return 1;
    }
    if (perm_distance[0] || ori_distance[0])
        return 1;

    uint8_t path[MAX_DEPTH];
    uint32_t worst_nodes = 0, hardest = 0, depth11 = 0;
    for (uint32_t rank = 0; rank < STATES; ++rank) {
        uint16_t p = (uint16_t) (rank / 729), o = (uint16_t) (rank % 729);
        uint8_t h = perm_distance[p] > ori_distance[o] ? perm_distance[p]
                                                         : ori_distance[o];
        if (h > distance[rank]) {
            fprintf(stderr, "inadmissible heuristic at rank %u\n", rank);
            return 1;
        }
        if (distance[rank] > max_d)
            max_d = distance[rank];
        if (distance[rank] == 11)
            ++depth11;

        uint32_t nodes;
        int length = rv32_solve(p, o, path, &nodes);
        if (length != distance[rank]) {
            fprintf(stderr, "wrong distance at rank %u: got %d, want %u\n",
                    rank, length, distance[rank]);
            return 1;
        }
        for (int i = 0; i < length; ++i) {
            p = perm_next[path[i]][p];
            o = ori_next[path[i]][o];
        }
        if (p || o) {
            fprintf(stderr, "path does not solve rank %u\n", rank);
            return 1;
        }
        if (distance[rank] == 11 && nodes > worst_nodes) {
            worst_nodes = nodes;
            hardest = rank;
        }
    }

    printf("H1-H3/T5 passed: %u states, diameter %u, depth-11 states %u\n",
           tail, max_d, depth11);
    printf("H2 passed: permutation max %u, orientation max %u\n", max_p,
           max_o);
    printf("static table bytes: %zu / %u\n", table_bytes, 128 * 1024);
    printf("depth-11 worst expanded nodes: %u (rank %u)\n", worst_nodes,
           hardest);
    free(queue);
    free(distance);
    return max_d == 11 && depth11 == 2644 ? 0 : 1;
}
