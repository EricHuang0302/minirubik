#include <stdint.h>
#if !defined(RV32_C_NO_MAIN) && !defined(RV32_FREESTANDING)
#include <stdio.h>
#endif

#include "rv32_tables.h"

enum { CUBIES = 7, MAX_DEPTH = 11, MOVES = 9 };

static const uint8_t move_face[MOVES] = {0, 0, 0, 1, 1, 1, 2, 2, 2};
#if !defined(RV32_C_NO_MAIN) && !defined(RV32_FREESTANDING)
static const char *const move_name[MOVES] = {"R", "R2", "R'", "B", "B2",
                                            "B'", "D", "D2", "D'"};
#endif

/* Rank once at input; the search itself uses only table lookups. */
static int rv32_parse(const char *input, uint16_t *perm, uint16_t *ori)
{
    uint8_t pieces[CUBIES], twists[CUBIES], seen = 0, sum = 0;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        char c = input[i];
        if (c < '1' || c > '7' || (seen & (1U << (c - '1'))))
            return 0;
        seen = (uint8_t) (seen | (1U << (c - '1')));
        pieces[i] = (uint8_t) (c - '1');
    }
    for (uint8_t i = 0; i < CUBIES; ++i) {
        char c = input[CUBIES + i];
        if (c < '1' || c > '3')
            return 0;
        twists[i] = (uint8_t) (c - '1');
        sum = (uint8_t) (sum + twists[i]);
    }
    if (input[2 * CUBIES] != '\0')
        return 0;
    while (sum >= 3)
        sum -= 3;
    if (sum)
        return 0;

    uint8_t digit[CUBIES];
    for (uint8_t i = 0; i < CUBIES; ++i) {
        digit[i] = 0;
        for (uint8_t j = (uint8_t) (i + 1); j < CUBIES; ++j)
            digit[i] += pieces[j] < pieces[i];
    }
    *perm = (uint16_t) (((((digit[0] * 6 + digit[1]) * 5 + digit[2]) * 4 +
                          digit[3]) * 3 + digit[4]) * 2 + digit[5]);
    *ori = 0;
    for (uint8_t i = 0; i < CUBIES - 1; ++i)
        *ori = (uint16_t) (*ori * 3 + twists[i]);
    return 1;
}

/* Each projected distance is a lower bound on the real distance. Their maximum
 * is admissible, so increasing IDA* bounds returns a shortest path. The
 * explicit 12-frame stack needs no heap, recursion, or hot division.
 */
static int rv32_solve(uint16_t start_p, uint16_t start_o,
                      uint8_t path[MAX_DEPTH], uint32_t *expanded)
{
    uint16_t p[MAX_DEPTH + 1], o[MAX_DEPTH + 1];
    uint8_t next_move[MAX_DEPTH + 1];
    uint8_t bound = perm_distance[start_p] > ori_distance[start_o]
                        ? perm_distance[start_p]
                        : ori_distance[start_o];
    *expanded = 0;

    for (; bound <= MAX_DEPTH; ++bound) {
        uint8_t depth = 0;
        p[0] = start_p;
        o[0] = start_o;
        next_move[0] = 0;
        for (;;) {
            if (next_move[depth] == 0) {
                uint8_t h = perm_distance[p[depth]] > ori_distance[o[depth]]
                                ? perm_distance[p[depth]]
                                : ori_distance[o[depth]];
                if (depth + h > bound)
                    goto backtrack;
                if (p[depth] == 0 && o[depth] == 0)
                    return depth;
                ++*expanded;
            }
            if (depth == bound || next_move[depth] == MOVES)
                goto backtrack;

            uint8_t move = next_move[depth]++;
            if (depth && move_face[move] == move_face[path[depth - 1]])
                continue;
            path[depth] = move;
            p[depth + 1] = perm_next[move][p[depth]];
            o[depth + 1] = ori_next[move][o[depth]];
            ++depth;
            next_move[depth] = 0;
            continue;

        backtrack:
            if (depth == 0)
                break;
            --depth;
        }
    }
    return -1;
}

/* Freestanding entry point: -2 means invalid input; -1 means no solution. */
int rv32_solve_input(const char *input, uint8_t path[MAX_DEPTH],
                     uint32_t *expanded)
{
    uint16_t p, o;
    if (!rv32_parse(input, &p, &o))
        return -2;
    return rv32_solve(p, o, path, expanded);
}

#if !defined(RV32_C_NO_MAIN) && !defined(RV32_FREESTANDING)
int main(int argc, char **argv)
{
    uint8_t path[MAX_DEPTH];
    uint32_t expanded;
    if (argc != 2) {
        fputs("usage: rv32_c PPPPPPPOOOOOOO\n", stderr);
        return 2;
    }
    int length = rv32_solve_input(argv[1], path, &expanded);
    if (length == -2) {
        fputs("usage: rv32_c PPPPPPPOOOOOOO\n", stderr);
        return 2;
    }
    if (length < 0) {
        fputs("no solution within 11 moves\n", stderr);
        return 1;
    }
    for (int i = 0; i < length; ++i)
        printf("%s%s", i ? " " : "", move_name[path[i]]);
    putchar('\n');
    return fflush(stdout) == 0 && !ferror(stdout) ? 0 : 1;
}
#endif
