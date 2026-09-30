#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 這題只追蹤 7 個角塊；另一個角塊固定不動。
// 位置排列有 7! = 5040 種，方向有 3^6 = 729 種。
enum {
    CUBIES = 7,
    PERMUTATIONS = 5040,
    ORIENTATIONS = 729,
    STATES = PERMUTATIONS * ORIENTATIONS,
    MOVES = 9
};

// p[i]：位置 i 放的是哪個角塊（內部編號 0～6）。
// o[i]：該角塊的方向（0、1、2）；方向加總必須是 3 的倍數。
typedef struct {
    uint8_t p[CUBIES], o[CUBIES];
} state_t;

// 每個面的三種動作依序是：順時針 90 度、180 度、逆時針 90 度。
static const char *const move_names[MOVES] = {"R",  "R2", "R'", "B", "B2",
                                              "B'", "D",  "D2", "D'"};
// 反向動作，例如 R 的反向是 R'；R2 的反向仍是 R2。
static const uint8_t inverse_move[MOVES] = {2, 1, 0, 5, 4, 3, 8, 7, 6};

// face 依序代表 R、B、D。source[face][i] 表示轉完後，
// 新位置 i 的角塊來自原本哪個位置。twist 是額外的方向變化。
static const uint8_t source[3][CUBIES] = {
    {1, 4, 2, 0, 3, 5, 6},
    {0, 1, 2, 4, 5, 6, 3},
    {0, 2, 5, 3, 1, 4, 6},
};
static const uint8_t twist[3][CUBIES] = {
    {1, 2, 0, 2, 1, 0, 0},
    {0, 0, 0, 1, 2, 1, 2},
    {0, 0, 0, 0, 0, 0, 0},
};

// 將指定的面轉 90 度，回傳新的魔術方塊狀態。
static state_t quarter_turn(state_t state, uint8_t face)
{
    state_t result;

    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t from = source[face][i];
        result.p[i] = state.p[from];
        // 方向只會是 0、1、2，所以用 % 3 讓數值繞回這個範圍。
        result.o[i] = (uint8_t) ((state.o[from] + twist[face][i]) % 3U);
    }
    return result;
}

// 9 個 move 編號分成三組：0～2 是 R，3～5 是 B，6～8 是 D。
// 同一個面轉 1、2、3 次 90 度，分別對應 R、R2、R' 等動作。
static state_t apply_move(state_t state, uint8_t move)
{
    uint8_t turns = (uint8_t) (move % 3U + 1U);
    for (uint8_t i = 0; i < turns; ++i)
        state = quarter_turn(state, (uint8_t) (move / 3U));
    return state;
}

// 將完整狀態編成唯一整數。編號 0 是已解開的狀態。
// 排列部分用 Lehmer code；方向部分把前六個方向當成六位三進位數。
static uint32_t rank_state(const state_t *state)
{
    uint32_t p = 0, o = 0;

    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t smaller = 0;

        for (uint8_t j = (uint8_t) (i + 1U); j < CUBIES; ++j)
            if (state->p[j] < state->p[i])
                ++smaller;
        // smaller 是右邊比目前角塊編號小的個數，用來累積排列編號。
        p = p * (CUBIES - i) + smaller;
    }

    // 第七個方向由「方向總和為 3 的倍數」決定，不必另外編碼。
    for (uint8_t i = 0; i < 6; ++i)
        o = o * 3U + state->o[i];
    return p * ORIENTATIONS + o;
}

// rank_state 的反向操作：把編號還原成 p[] 與 o[]。
// 建立轉動表時會用到它；一般解題時不直接呼叫。
static void unrank_state(uint32_t rank, state_t *state)
{
    uint8_t available[CUBIES] = {0, 1, 2, 3, 4, 5, 6};
    // f 從 6! = 720 開始，逐位還原排列。
    uint32_t p = rank / ORIENTATIONS, o = rank % ORIENTATIONS, f = 720;
    uint8_t sum = 0;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t q = (uint8_t) (p / f);
        p %= f;
        state->p[i] = available[q];
        for (uint8_t j = q; j + 1U < CUBIES - i; ++j)
            available[j] = available[j + 1U];
        if (i < 5)
            f /= 6U - i;
    }
    for (uint8_t i = 6; i-- > 0;) {
        state->o[i] = (uint8_t) (o % 3U);
        sum = (uint8_t) (sum + state->o[i]);
        o /= 3U;
    }
    // 補出最後一個方向，讓七個方向的總和模 3 等於 0。
    state->o[6] = (uint8_t) ((3U - sum % 3U) % 3U);
}

// 檢查：角塊編號與方向在範圍內、角塊沒有重複、方向總和合法。
static int valid(const state_t *state)
{
    uint8_t sum = 0;

    for (uint8_t i = 0; i < CUBIES; ++i) {
        if (state->p[i] >= CUBIES || state->o[i] >= 3)
            return 0;

        for (uint8_t j = 0; j < i; ++j)
            if (state->p[j] == state->p[i])
                return 0;
        sum = (uint8_t) (sum + state->o[i]);
    }
    return sum % 3U == 0;
}

// 從已解開的狀態做 BFS（廣度優先搜尋），為每個狀態記下回家的第一步。
// BFS 按距離一層一層搜尋，因此第一次找到的路徑保證步數最少。
static uint8_t *build_table(uint8_t *diameter)
{
    // toward_solved[狀態編號] = 往已解開狀態走時，下一步該做的動作。
    // queue 存等待展開的狀態編號；這兩個陣列是原版記憶體用量的大宗。
    uint8_t *toward_solved = malloc(STATES);
    uint32_t *queue = malloc((size_t) STATES * sizeof *queue);
    // 先做小型轉動表，之後展開狀態時不用反覆搬動 p[]、o[]。
    uint16_t permutation[3][PERMUTATIONS], orientation[3][ORIENTATIONS];
    // head 是下一個要處理的位置；tail 是下一個可加入的位置。
    // level_end 標出目前 BFS 這一層的結尾。
    uint32_t head = 0, tail = 1, level_end = 1;
    state_t state;
    if (!toward_solved || !queue) {
        free(toward_solved);
        free(queue);
        return NULL;
    }
    for (uint16_t rank = 0; rank < PERMUTATIONS; ++rank) {
        unrank_state((uint32_t) rank * ORIENTATIONS, &state);
        for (uint8_t face = 0; face < 3; ++face) {
            state_t next = quarter_turn(state, face);
            permutation[face][rank] =
                (uint16_t) (rank_state(&next) / ORIENTATIONS);
        }
    }
    for (uint16_t rank = 0; rank < ORIENTATIONS; ++rank) {
        unrank_state(rank, &state);
        for (uint8_t face = 0; face < 3; ++face) {
            state_t next = quarter_turn(state, face);
            orientation[face][rank] =
                (uint16_t) (rank_state(&next) % ORIENTATIONS);
        }
    }
    // 255 表示還沒找到；編號 0 是已解開的狀態，放入佇列起點。
    memset(toward_solved, UINT8_MAX, STATES);
    queue[0] = 0;
    toward_solved[0] = 0;
    *diameter = 0;
    while (head < tail) {
        if (head == level_end) {
            // 上一層處理完畢，進入距離多一步的下一層。
            level_end = tail;
            ++*diameter;
        }
        uint32_t here = queue[head++];
        uint16_t p = (uint16_t) (here / ORIENTATIONS);
        uint16_t o = (uint16_t) (here % ORIENTATIONS);
        for (uint8_t face = 0; face < 3; ++face) {
            uint16_t next_p = p, next_o = o;
            for (uint8_t turn = 0; turn < 3; ++turn) {
                // 同一個面依序轉 1、2、3 次，產生三個相鄰狀態。
                next_p = permutation[face][next_p];
                next_o = orientation[face][next_o];
                uint32_t there = (uint32_t) next_p * ORIENTATIONS + next_o;
                if (toward_solved[there] == UINT8_MAX) {
                    uint8_t move = (uint8_t) (face * 3U + turn);
                    // 搜尋是從已解開狀態往外走；解題要往回走，所以存反向動作。
                    toward_solved[there] = inverse_move[move];
                    queue[tail++] = there;
                }
            }
        }
    }
    free(queue);
    if (tail != STATES) {
        free(toward_solved);
        return NULL;
    }
    return toward_solved;
}

// 讀取 14 個字元：前 7 個是角塊排列 1～7，後 7 個是方向 1～3。
// 減去字元 '1' 後，內部就改用從 0 開始的編號。
static int parse_state(const char *input, state_t *state)
{
    for (int i = 0; i < 14; ++i) {
        int limit = i < 7 ? 7 : 3;
        if (input[i] < '1' || input[i] > '0' + limit)
            return 0;
        (i < 7 ? state->p : state->o)[i % 7] = (uint8_t) (input[i] - '1');
    }
    return input[14] == '\0' && valid(state);
}

// 確認輸出真的寫出去了，而不只是暫存在 stdout 緩衝區。
static int output_failed(void)
{
    return fflush(stdout) != 0 || ferror(stdout);
}

// 檢查動作加反向動作會回到原點，並檢查每個狀態可編碼再還原。
static int self_test(void)
{
    const state_t solved = {{0, 1, 2, 3, 4, 5, 6}, {0}};
    state_t state;
    for (uint8_t move = 0; move < MOVES; ++move) {
        state = solved;
        state = apply_move(state, move);
        state = apply_move(state, inverse_move[move]);
        if (memcmp(&solved, &state, sizeof solved))
            return 0;
    }
    for (uint32_t rank = 0; rank < STATES; ++rank) {
        unrank_state(rank, &state);
        if (!valid(&state) || rank_state(&state) != rank)
            return 0;
    }
    return 1;
}

int main(int argc, char **argv)
{
    state_t state;
    uint8_t diameter;
    // --self-test 是原程式附帶的完整自我檢查模式。
    if (argc == 2 && !strcmp(argv[1], "--self-test")) {
        if (!self_test()) {
            fputs("self-test failed\n", stderr);
            return 1;
        }
        uint8_t *table = build_table(&diameter);
        if (!table) {
            fputs("could not build complete state table\n", stderr);
            return 1;
        }
        free(table);
        if (diameter != 11) {
            fputs("BFS check failed\n", stderr);
            return 1;
        }
        puts("3674160 states; diameter 11");
        return output_failed();
    }
    // 一般模式要求一個合法的 14 字元狀態。
    if (argc != 2 || !parse_state(argv[1], &state)) {
        fprintf(stderr, "usage: %s PPPPPPPOOOOOOO\n",
                argc > 0 && argv[0] ? argv[0] : "solver");
        return 2;
    }
    // 每次執行都先重新建立完整的 BFS 表。
    uint8_t *table = build_table(&diameter);
    if (!table) {
        fputs("could not build complete state table\n", stderr);
        return 1;
    }
    const char *separator = "";
    // 只要編號不是 0，就查表走一步，直到回到已解開狀態。
    for (uint32_t rank = rank_state(&state); rank; rank = rank_state(&state)) {
        uint8_t move = table[rank];
        printf("%s%s", separator, move_names[move]);
        separator = " ";
        state = apply_move(state, move);
    }
    putchar('\n');
    free(table);
    return output_failed();
}