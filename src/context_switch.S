; Faz os enderecos dos registradores de I/O comecarem em zero.
; Isso permite acessar SREG, SPL e SPH diretamente com IN e OUT.
#define __SFR_OFFSET 0

; Importa os nomes dos registradores e bits do microcontrolador AVR.
#include <avr/io.h>

; Exporta a funcao para que o codigo C++ possa chamar Thread::yield().
.global OS_yield_asm

; Exporta o vetor TIMER2_COMPA do ATmega328P (Arduino Uno/Nano).
.global __vector_7

; Salva todo o contexto da CPU na pilha da thread atual.
; A ordem dos PUSH deve ser exatamente o inverso da ordem dos POP abaixo.
.macro SAVE_CONTEXT
    push r0             ; Preserva r0 antes de usa-lo como registrador temporario.
    in   r0, SREG       ; Copia o registrador de status SREG para r0.
    cli                 ; Desabilita interrupcoes durante a troca de contexto.
    push r0             ; Salva o SREG original, inclusive o estado do bit I.
    push r1             ; Salva r1 antes de restaura-lo ao valor exigido pela ABI.
    clr  r1             ; A ABI AVR-GCC exige que r1 permaneca igual a zero.
    push r2             ; Salva o registrador de uso geral r2.
    push r3             ; Salva o registrador de uso geral r3.
    push r4             ; Salva o registrador de uso geral r4.
    push r5             ; Salva o registrador de uso geral r5.
    push r6             ; Salva o registrador de uso geral r6.
    push r7             ; Salva o registrador de uso geral r7.
    push r8             ; Salva o registrador de uso geral r8.
    push r9             ; Salva o registrador de uso geral r9.
    push r10            ; Salva o registrador de uso geral r10.
    push r11            ; Salva o registrador de uso geral r11.
    push r12            ; Salva o registrador de uso geral r12.
    push r13            ; Salva o registrador de uso geral r13.
    push r14            ; Salva o registrador de uso geral r14.
    push r15            ; Salva o registrador de uso geral r15.
    push r16            ; Salva o registrador de uso geral r16.
    push r17            ; Salva o registrador de uso geral r17.
    push r18            ; Salva o registrador de uso geral r18.
    push r19            ; Salva o registrador de uso geral r19.
    push r20            ; Salva o registrador de uso geral r20.
    push r21            ; Salva o registrador de uso geral r21.
    push r22            ; Salva r22; depois ele transportara timer_tick.
    push r23            ; Salva r23, par de alinhamento de r22 na ABI AVR-GCC.
    push r24            ; Salva r24; depois recebera o byte baixo de oldSP.
    push r25            ; Salva r25; depois recebera o byte alto de oldSP.
    push r26            ; Salva r26, byte baixo do registrador ponteiro X.
    push r27            ; Salva r27, byte alto do registrador ponteiro X.
    push r28            ; Salva r28, byte baixo do registrador ponteiro Y.
    push r29            ; Salva r29, byte alto do registrador ponteiro Y.
    push r30            ; Salva r30, byte baixo do registrador ponteiro Z.
    push r31            ; Salva r31, byte alto do registrador ponteiro Z.
.endm

; Entrada usada por Thread::yield(). O CALL feito pelo C++ ja colocou o
; endereco de retorno na pilha, assim como a CPU faz ao entrar em uma ISR.
OS_yield_asm:
    SAVE_CONTEXT        ; Salva a execucao da thread que chamou yield().
    clr  r22            ; Segundo argumento: timer_tick = 0 (nao avanca tempo).
    rjmp SWITCH_CONTEXT ; Salta para o trecho comum sem empilhar outro retorno.

; Entrada do vetor de comparacao A do Timer2.
; O hardware ja salvou o endereco interrompido na pilha antes de chegar aqui.
__vector_7:
    SAVE_CONTEXT        ; Salva a execucao interrompida pelo Timer2.
    ldi  r22, 1         ; Segundo argumento: timer_tick = 1 (avanca o relogio).

; Trecho comum para selecionar a proxima thread.
SWITCH_CONTEXT:
    ; ABI AVR-GCC da chamada abaixo:
    ;   r24:r25 = primeiro argumento, oldSP
    ;   r22     = segundo argumento, timer_tick
    ;   r24:r25 = valor de retorno, novo SP
    in   r24, SPL       ; Le o byte baixo do Stack Pointer da thread atual.
    in   r25, SPH       ; Le o byte alto do Stack Pointer da thread atual.

    call OS_contextSwitch_Wrapper
                        ; Salva o retorno desta chamada, executa o scheduler C++
                        ; e recebe em r24:r25 o SP da thread selecionada.

    out  SPL, r24       ; Instala o byte baixo do novo Stack Pointer.
    out  SPH, r25       ; Instala o byte alto do novo Stack Pointer.

    ; A partir daqui, cada POP le a pilha da thread selecionada.
    ; A ordem inversa recompoe exatamente o frame criado por SAVE_CONTEXT.
    pop  r31            ; Restaura o byte alto do ponteiro Z.
    pop  r30            ; Restaura o byte baixo do ponteiro Z.
    pop  r29            ; Restaura o byte alto do ponteiro Y.
    pop  r28            ; Restaura o byte baixo do ponteiro Y.
    pop  r27            ; Restaura o byte alto do ponteiro X.
    pop  r26            ; Restaura o byte baixo do ponteiro X.
    pop  r25            ; Restaura o registrador de uso geral r25.
    pop  r24            ; Restaura o registrador de uso geral r24.
    pop  r23            ; Restaura o registrador de uso geral r23.
    pop  r22            ; Restaura r22; descarta o timer_tick temporario.
    pop  r21            ; Restaura o registrador de uso geral r21.
    pop  r20            ; Restaura o registrador de uso geral r20.
    pop  r19            ; Restaura o registrador de uso geral r19.
    pop  r18            ; Restaura o registrador de uso geral r18.
    pop  r17            ; Restaura o registrador de uso geral r17.
    pop  r16            ; Restaura o registrador de uso geral r16.
    pop  r15            ; Restaura o registrador de uso geral r15.
    pop  r14            ; Restaura o registrador de uso geral r14.
    pop  r13            ; Restaura o registrador de uso geral r13.
    pop  r12            ; Restaura o registrador de uso geral r12.
    pop  r11            ; Restaura o registrador de uso geral r11.
    pop  r10            ; Restaura o registrador de uso geral r10.
    pop  r9             ; Restaura o registrador de uso geral r9.
    pop  r8             ; Restaura o registrador de uso geral r8.
    pop  r7             ; Restaura o registrador de uso geral r7.
    pop  r6             ; Restaura o registrador de uso geral r6.
    pop  r5             ; Restaura o registrador de uso geral r5.
    pop  r4             ; Restaura o registrador de uso geral r4.
    pop  r3             ; Restaura o registrador de uso geral r3.
    pop  r2             ; Restaura o registrador de uso geral r2.
    pop  r1             ; Restaura o valor original de r1.
    pop  r0             ; Recupera em r0 o SREG salvo pela thread.
    out  SREG, r0       ; Restaura flags e o estado original das interrupcoes.
    pop  r0             ; Restaura o valor original de r0.
    reti                ; Retira o PC da pilha e continua a thread selecionada.
