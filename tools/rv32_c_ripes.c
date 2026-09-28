/* Freestanding Ripes entry point for the -O2 C comparison build. */
#include <stdint.h>

int rv32_solve_input(const char *input, uint8_t path[11], uint32_t *expanded);

static const char input_state[] = "21345671111111";
static const char move_first[] = "RRRBBBDDD";
static const char move_second[] = "\0" "2'\0" "2'\0" "2'";

static void print_char(int value)
{
    register int a0 __asm__("a0") = value;
    register int a7 __asm__("a7") = 11;
    __asm__ volatile("ecall" : : "r"(a0), "r"(a7) : "memory");
}

__attribute__((noreturn)) static void finish(int status)
{
    register int a0 __asm__("a0") = status;
    register int a7 __asm__("a7") = 93;
    __asm__ volatile("ecall" : : "r"(a0), "r"(a7) : "memory");
    __builtin_unreachable();
}

void _start(void)
{
    uint8_t path[11];
    uint32_t expanded;
    int length = rv32_solve_input(input_state, path, &expanded);
    if (length < 0)
        finish(length == -2 ? 2 : 1);
    for (int i = 0; i < length; ++i) {
        if (i)
            print_char(' ');
        print_char(move_first[path[i]]);
        if (move_second[path[i]])
            print_char(move_second[path[i]]);
    }
    print_char('\n');
    finish(0);
}
