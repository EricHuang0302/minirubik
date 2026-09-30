#include <stdint.h>
#if !defined(RV32_C_NO_MAIN) && !defined(RV32_FREESTANDING)
#include <stdio.h>
#endif

#include "rv32_tables.h"

/* 以一個角落為基準，只記錄其餘 7 個小方塊。
 * 允許的 9 種轉動如下；轉 90、180、270 度都算一步，最多需要 11 步。
 */
enum { CUBIES = 7, MAX_DEPTH = 11, MOVES = 9 };

/* 0 = 右面 R、1 = 後面 B、2 = 底面 D。用來跳過連續轉同一面的情況。 */
static const uint8_t move_face[MOVES] = {0, 0, 0, 1, 1, 1, 2, 2, 2};
#if !defined(RV32_C_NO_MAIN) && !defined(RV32_FREESTANDING)
static const char *const move_name[MOVES] = {"R", "R2", "R'", "B", "B2",
                                            "B'", "D", "D2", "D'"};
#endif

/* 輸入共 14 個字元，例如 "12345671111111" 是完成狀態。
 * 前 7 碼：每個位置放哪個小方塊（1～7，各出現一次）。
 * 後 7 碼：每個位置的小方塊朝向（1～3，轉成程式中的 0～2）。
 * 驗證後，把排列與朝向各換成一個編號，供搜尋時查表。
 */
static int rv32_parse(const char *input, uint16_t *perm, uint16_t *ori)
{
    uint8_t pieces[CUBIES], twists[CUBIES];
    uint8_t seen = 0, twist_sum = 0;

    /* seen 的每個位元記錄一個小方塊是否已出現。 */
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
        twist_sum = (uint8_t) (twist_sum + twists[i]);
    }
    if (input[2 * CUBIES] != '\0')
        return 0;
    /* 合法方塊的朝向總和必須是 3 的倍數；用減法求餘數。 */
    while (twist_sum >= 3)
        twist_sum -= 3;
    if (twist_sum)
        return 0;

    /* 排列編號 0～5039：先數出每個位置右邊有幾個較小的編號。 */
    uint8_t smaller[CUBIES];
    for (uint8_t i = 0; i < CUBIES; ++i) {
        smaller[i] = 0;
        for (uint8_t j = (uint8_t) (i + 1); j < CUBIES; ++j)
            smaller[i] += pieces[j] < pieces[i];
    }
    /* 每往右一格，可選的小方塊就少一個，因此乘數依序是 6、5、4、3、2。
     * 保留固定乘數，讓 RV32I 編譯器用位移與加法實作。
     */
    *perm = smaller[0];
    *perm = (uint16_t) (*perm * 6 + smaller[1]);
    *perm = (uint16_t) (*perm * 5 + smaller[2]);
    *perm = (uint16_t) (*perm * 4 + smaller[3]);
    *perm = (uint16_t) (*perm * 3 + smaller[4]);
    *perm = (uint16_t) (*perm * 2 + smaller[5]);

    /* 朝向編號 0～728：把前 6 個朝向視為三進位數字。
     * 第 7 個朝向由「總和是 3 的倍數」決定，不必再存進編號。
     */
    *ori = 0;
    for (uint8_t i = 0; i < CUBIES - 1; ++i)
        *ori = (uint16_t) (*ori * 3 + twists[i]);
    return 1;
}

/* 只看排列、只看朝向，各自至少還要幾步？取兩者較大值。
 * 不能相加，因為同一次轉動可能同時改善排列和朝向。
 * 這個估計不會超過真正需要的步數，因此可用來排除不可能的路線。
 */
static uint8_t min_remaining_moves(uint16_t perm, uint16_t ori)
{
    uint8_t perm_steps = perm_distance[perm];
    uint8_t ori_steps = ori_distance[ori];
    return perm_steps > ori_steps ? perm_steps : ori_steps;
}

/* IDA* 搜尋：逐次增加允許的步數，每次從起點重新嘗試。
 * 較小的步數都找不到解，才會增加上限，所以第一次找到的解就是最短解。
 * 用固定陣列記錄每一步的狀態，回頭時只要減少 depth，不需要遞迴。
 */
static int rv32_solve(uint16_t start_perm, uint16_t start_ori,
                      uint8_t path[MAX_DEPTH], uint32_t *expanded)
{
    /* 索引 depth 表示已走幾步；0 是起點，path[depth] 是接下來的轉動。 */
    uint16_t perm_stack[MAX_DEPTH + 1], ori_stack[MAX_DEPTH + 1];
    uint8_t next_move[MAX_DEPTH + 1];
    uint8_t limit = min_remaining_moves(start_perm, start_ori);
    *expanded = 0;

    for (; limit <= MAX_DEPTH; ++limit) {
        uint8_t depth = 0;
        perm_stack[0] = start_perm;
        ori_stack[0] = start_ori;
        next_move[0] = 0;
        for (;;) {
            /* 第一次走到這個狀態時，先判斷剩餘步數是否足夠。 */
            if (next_move[depth] == 0) {
                uint8_t remaining =
                    min_remaining_moves(perm_stack[depth], ori_stack[depth]);
                if (depth + remaining > limit)
                    goto backtrack;
                if (perm_stack[depth] == 0 && ori_stack[depth] == 0)
                    return depth;
                ++*expanded; /* 記錄展開的狀態數，供效能測量使用。 */
            }
            if (depth == limit || next_move[depth] == MOVES)
                goto backtrack;

            /* 依序試 9 種轉動。先記住下次要試哪一種，再往下一步走。 */
            uint8_t move = next_move[depth]++;
            /* 同一面連轉兩次可合併成一步或抵消，不會出現在最短解中。 */
            if (depth && move_face[move] == move_face[path[depth - 1]])
                continue;
            path[depth] = move;
            perm_stack[depth + 1] = perm_next[move][perm_stack[depth]];
            ori_stack[depth + 1] = ori_next[move][ori_stack[depth]];
            ++depth;
            next_move[depth] = 0;
            continue;

        backtrack:
            /* 這條路走不通：回到上一步，接著試尚未試過的轉動。 */
            if (depth == 0)
                break;
            --depth;
        }
    }
    return -1;
}

/* 共用入口：回傳解答步數；-2 表示輸入不合法，-1 表示 11 步內找不到解。 */
int rv32_solve_input(const char *input, uint8_t path[MAX_DEPTH],
                     uint32_t *expanded)
{
    uint16_t perm, ori;
    if (!rv32_parse(input, &perm, &ori))
        return -2;
    return rv32_solve(perm, ori, path, expanded);
}

/* 本機執行：讀取命令列輸入 → 求解 → 印出轉動。
 * 測試或 Ripes 編譯時會略過這個 main，使用各自的入口。
 */
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
