/* Host-only generator. Reuse the original cube model instead of copying it. */
#define main original_solver_main
#include "../solver.c"
#undef main

static uint16_t perm_next[MOVES][PERMUTATIONS];
static uint16_t ori_next[MOVES][ORIENTATIONS];
static uint8_t perm_distance[PERMUTATIONS];
static uint8_t ori_distance[ORIENTATIONS];

static void print_u16(const char *name, uint16_t *values, size_t count,
                      size_t width)
{
    printf("static const uint16_t %s[%zu][%zu] = {\n", name, count / width,
           width);
    for (size_t row = 0; row < count / width; ++row) {
        puts("    {");
        for (size_t col = 0; col < width; ++col) {
            if (col % 12 == 0)
                fputs("        ", stdout);
            printf("%u,", values[row * width + col]);
            putchar(col % 12 == 11 || col + 1 == width ? '\n' : ' ');
        }
        puts("    },");
    }
    puts("};\n");
}

static void print_u8(const char *name, const uint8_t *values, size_t count)
{
    printf("static const uint8_t %s[%zu] = {\n", name, count);
    for (size_t i = 0; i < count; ++i) {
        if (i % 24 == 0)
            fputs("    ", stdout);
        printf("%u,", values[i]);
        putchar(i % 24 == 23 || i + 1 == count ? '\n' : ' ');
    }
    puts("};\n");
}

static int fill_distances(uint8_t *distance, uint16_t *queue, size_t count,
                          uint16_t *next)
{
    memset(distance, UINT8_MAX, count);
    distance[0] = 0;
    queue[0] = 0;
    size_t head = 0, tail = 1;
    while (head < tail) {
        uint16_t here = queue[head++];
        for (uint8_t move = 0; move < MOVES; ++move) {
            uint16_t there = next[(size_t) move * count + here];
            if (distance[there] == UINT8_MAX) {
                distance[there] = (uint8_t) (distance[here] + 1);
                queue[tail++] = there;
            }
        }
    }
    return tail == count;
}

int main(void)
{
    state_t state;
    for (uint8_t move = 0; move < MOVES; ++move) {
        for (uint16_t rank = 0; rank < PERMUTATIONS; ++rank) {
            unrank_state((uint32_t) rank * ORIENTATIONS, &state);
            state_t next = apply_move(state, move);
            perm_next[move][rank] =
                (uint16_t) (rank_state(&next) / ORIENTATIONS);
        }
        for (uint16_t rank = 0; rank < ORIENTATIONS; ++rank) {
            unrank_state(rank, &state);
            state_t next = apply_move(state, move);
            ori_next[move][rank] =
                (uint16_t) (rank_state(&next) % ORIENTATIONS);
        }
    }

    uint16_t queue[PERMUTATIONS];
    if (!fill_distances(perm_distance, queue, PERMUTATIONS, &perm_next[0][0]) ||
        !fill_distances(ori_distance, queue, ORIENTATIONS, &ori_next[0][0])) {
        fputs("incomplete projection table\n", stderr);
        return 1;
    }

    puts("/* Generated from solver.c by tools/gen_rv32_tables.c. */");
    puts("#ifndef RV32_TABLES_H");
    puts("#define RV32_TABLES_H");
    puts("#include <stdint.h>\n");
    print_u16("perm_next", &perm_next[0][0], MOVES * PERMUTATIONS,
              PERMUTATIONS);
    print_u16("ori_next", &ori_next[0][0], MOVES * ORIENTATIONS,
              ORIENTATIONS);
    print_u8("perm_distance", perm_distance, PERMUTATIONS);
    print_u8("ori_distance", ori_distance, ORIENTATIONS);
    puts("#endif");
    return ferror(stdout) ? 1 : 0;
}
