;/*****************************************************************************/
; OSasm.s: low-level OS commands, written in assembly                       */
; Runs on LM4F120/TM4C123/MSP432
; Lab 3 starter file
; March 2, 2016

;


        AREA |.text|, CODE, READONLY, ALIGN=2
        THUMB
        REQUIRE8
        PRESERVE8

        EXTERN  RunPt            ; currently running thread
        EXPORT  StartOS
        EXPORT  SysTick_Handler
        IMPORT  Scheduler


SysTick_Handler                 ; 1) Saves R0-R3,R12,LR,PC,PSR
    CPSID   I                   ; 2) Prevent interrupt during switch
    PUSH    {R4-R11}            ; 3) save R4-R11 of outgoing thread
    LDR     R0, =RunPt          ; 4) R0 = &RunPt
    LDR     R1, [R0]            ;    R1 = pointer to outgoing thread
    STR     SP, [R1]            ; 5) save stack pointer outgoing thread
;    LDR     R1, [R1,#4]         ; 6) R1 = pointer to incoming thread
;    STR     R1, [R0]            ;    update RunPt with incoming thread
    PUSH    {R0,LR}
    BL      Scheduler
    POP     {R0,LR}
    LDR     R1, [R0]
    LDR     SP, [R1]            ; 7) restore stack pointer of incoming thread
    POP     {R4-R11}            ; 8) restore R4-R11 of incoming thread
    CPSIE   I                   ; 9) tasks run with interrupts enabled
    BX      LR                  ; 10) restore R0-R3,R12,LR,PC,PSR

StartOS
    LDR     R0, =RunPt          ; R0 = pointer to RunPT
    LDR     R1, [R0]            ; R1 = pointer to thread to run
    LDR     SP, [R1]            ; load stack pointer
    POP     {R4-R11}            ; restore registers
    POP     {R0-R3}
    POP     {R12}
    ADD     SP, SP, #4          ; discard link register from initial stack 
    POP     {LR}
    ADD     SP, SP, #4          ; discard program status register
    CPSIE   I                   ; Enable interrupts at processor level
    BX      LR                  ; start first thread

    ALIGN
    END
