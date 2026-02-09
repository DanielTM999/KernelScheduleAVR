#define __SFR_OFFSET 0
#include <avr/io.h>

.global OS_yield_asm
.global __vector_7  ; TIMER2 COMPA no UNO/NANO

; Função chamada manualmente (Yield)
OS_yield_asm:
    rjmp SAVE_CONTEXT

; Interrupção do Timer (Preempção)
__vector_7:
SAVE_CONTEXT:
    ; 1. Salvar Registradores
    push r0
    in   r0, SREG
    cli
    push r0
    push r1
    clr  r1
    push r2
    push r3
    push r4
    push r5
    push r6
    push r7
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    push r16
    push r17
    push r18
    push r19
    push r20
    push r21
    push r22
    push r23
    push r24
    push r25
    push r26
    push r27
    push r28
    push r29
    push r30
    push r31

    ; 2. Passar SP atual para o C++
    in   r24, SPL
    in   r25, SPH

    ; 3. Chamar a função C++ que decide a troca
    call OS_contextSwitch_Wrapper

    ; 4. Receber novo SP do C++ 
    out  SPL, r24
    out  SPH, r25

    ; 5. Restaurar Registradores
    pop  r31
    pop  r30
    pop  r29
    pop  r28
    pop  r27
    pop  r26
    pop  r25
    pop  r24
    pop  r23
    pop  r22
    pop  r21
    pop  r20
    pop  r19
    pop  r18
    pop  r17
    pop  r16
    pop  r15
    pop  r14
    pop  r13
    pop  r12
    pop  r11
    pop  r10
    pop  r9
    pop  r8
    pop  r7
    pop  r6
    pop  r5
    pop  r4
    pop  r3
    pop  r2
    pop  r1
    pop  r0
    out  SREG, r0
    pop  r0
    reti