; ============================================================
; factorial.asm  —  TinyCPU port of programs/factorial.c
;
; The C source is:
;
;     int factorial(int n) {
;         if (n <= 1) return 1;
;         return n * factorial(n - 1);
;     }
;
;     int main(void) {
;         int n      = 3;
;         int result = factorial(n);
;         printf("%d! = %d\n", n, result);
;         return 0;
;     }
;
; ----------------------------------------------------------------
; TinyCPU has eight 8-bit GPRs (R0..R7), a 16-bit PC and SP, no
; multiply instruction, and no immediate forms of arithmetic. So:
;
;   * `n * factorial(n-1)` is implemented as repeated addition.
;   * Comparisons against constants are done by loading a scratch
;     register first (e.g. MOVI R1, #1 then CMP R0, R1).
;   * Stdio is replaced by stores to MMIO TTY at 0xC000.
;
; ----------------------------------------------------------------
; Calling convention used in this file:
;
;   argument         passed in R0
;   return value     returned in R0
;   caller-saved     R0 (caller must save it before CALL if needed)
;   callee-saved     R1, R2, R3 (saved on entry, restored on exit)
;   linkage          CALL pushes a 16-bit return address; RET pops
;                    it. SP grows downward from 0x03FF.
;
; A walkthrough of how factorial(3) unfolds on this CPU — with a
; stack snapshot at every interesting moment — lives in
; docs/PROGRAM_LAYOUT.md.
; ============================================================


.org 0x0000


; ------------------------------------------------------------
; main — the driver
; ------------------------------------------------------------
START:
    ; --- printf("%d", 3) ---
    MOVI R0, #'3'             ; ASCII '3' = 51
    ST   R0, [0xC000]         ; → MMIO TTY

    ; --- printf("! = ") ---
    MOVI R0, #'!'
    ST   R0, [0xC000]
    MOVI R0, #' '
    ST   R0, [0xC000]
    MOVI R0, #'='
    ST   R0, [0xC000]
    MOVI R0, #' '
    ST   R0, [0xC000]

    ; --- result = factorial(3) ---
    MOVI R0, #3               ; argument: n = 3
    CALL FACTORIAL            ; on return, R0 = 6

    ; --- printf("%d", result) ---
    MOVI R1, #'0'             ; 48
    ADD  R0, R1               ; convert single digit (0..9) to ASCII
    ST   R0, [0xC000]

    ; --- printf("\n") ---
    MOVI R0, #10
    ST   R0, [0xC000]

    HLT                       ; return 0


; ------------------------------------------------------------
; FACTORIAL — recursive integer factorial (8-bit)
;
;   in:   R0 = n
;   out:  R0 = n!
;
; Saves callee-saved registers R1, R2, R3 in the prologue and
; restores them in the epilogue, so callers (including the
; recursive call to itself) see them unchanged.
; ------------------------------------------------------------
FACTORIAL:
    ; ---- prologue: save callee-saved registers ----
    PUSH R1
    PUSH R2
    PUSH R3

    ; ---- if (n == 0) return 1; ----
    MOVI R1, #0
    CMP  R0, R1               ; sets Z if R0 == 0
    BEQ  FACT_BASE

    ; ---- if (n == 1) return 1; ----
    MOVI R1, #1
    CMP  R0, R1               ; sets Z if R0 == 1
    BEQ  FACT_BASE

    ; ---- recursive case ----
    ; tmp = factorial(n - 1)
    ; return n * tmp

    PUSH R0                   ; save n on the stack so we can multiply
                              ; by it after the recursive call. R0 is
                              ; caller-saved by our convention, so we
                              ; do this ourselves before recursing.

    MOVI R1, #1
    SUB  R0, R1               ; R0 = n - 1     (the new argument)

    CALL FACTORIAL            ; recursive call; on return R0 = (n-1)!

    POP  R1                   ; R1 = saved n
                              ; (we PUSHed R0 above)

    ; ---- multiply: R0 = R0 * R1   (repeated addition) ----
    ; R0 currently holds (n-1)!; R1 holds n. We want R0 = (n-1)! * n.
    MOV  R2, R0               ; R2 = (n-1)!   (multiplicand)
    MOVI R0, #0               ; R0 = 0        (accumulator)

FACT_MUL:
    ADD  R0, R2               ; R0 += R2
    DEC  R1                   ; R1--          (also sets Z if R1 == 0)
    BNE  FACT_MUL             ; loop while R1 != 0

    ; ---- epilogue: restore registers and return ----
    POP  R3
    POP  R2
    POP  R1
    RET                       ; pop return address, jump to caller


FACT_BASE:
    ; factorial(0) == factorial(1) == 1
    MOVI R0, #1
    POP  R3                   ; restore in reverse order of PUSH
    POP  R2
    POP  R1
    RET
