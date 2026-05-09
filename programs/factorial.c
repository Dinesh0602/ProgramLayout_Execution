/*
 * factorial.c — the C reference for programs/factorial.asm
 *
 * The assignment for this project is:
 *
 *     Create a simple C function that has recursion (a factorial),
 *     a driver / main program that calls it, and then show — running
 *     on the TinyCPU you built in the earlier project — how the
 *     executable is laid out in memory, how function calls are
 *     handled, and how recursion is carried out.
 *
 * This file is the *source* version of that program. TinyCPU
 * consumes assembly, not C, so we hand-translate it into
 * programs/factorial.asm. The two files sit side by side so a
 * reviewer can read the C, scan the asm, and see the same
 * algorithm in both.
 *
 * For the TinyCPU port:
 *   - The "host stdout" of the running C program is replaced by
 *     the MMIO TTY at 0xC000. Instead of putchar, the asm version
 *     stores the byte to that address.
 *   - The C compiler's stack-frame / linkage convention is
 *     replaced by a small TinyCPU calling convention, documented
 *     at the top of factorial.asm.
 *
 * The walkthrough of how factorial(3) actually unfolds on TinyCPU
 * — including stack snapshots between recursive calls — is in
 * docs/PROGRAM_LAYOUT.md.
 */

#include <stdio.h>

/*
 * factorial(n)
 *
 * Classic recursive definition:
 *      n == 0 || n == 1   →  1
 *      otherwise          →  n * factorial(n - 1)
 *
 * The recursion bottoms out at n <= 1, which is the base case.
 * Every other invocation pushes a new frame onto the call stack
 * with its own copy of `n` and waits for the recursive call to
 * return before multiplying.
 */
int factorial(int n) {
    if (n <= 1) {
        return 1;                  /* base case */
    }
    return n * factorial(n - 1);   /* recursive case */
}

/*
 * main — the driver the assignment asks for.
 *
 * It picks a small input (3), calls factorial, and prints the
 * result in the form `3! = 6` followed by a newline. The asm
 * version prints each character through the MMIO TTY at 0xC000.
 */
int main(void) {
    int n      = 3;
    int result = factorial(n);

    /* Mirrors the asm output: prints "3! = 6\n". */
    printf("%d! = %d\n", n, result);
    return 0;
}
