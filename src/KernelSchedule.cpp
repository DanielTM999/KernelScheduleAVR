#include "KernelSchedule.h"

/* Configurações do Timer para o Time Slice do Scheduler */
#define TIMER_OCRNA 155
#define TIMER_PRESCALER 1024UL
#define TIMER_CYCLES (((uint32_t)TIMER_OCRNA + 1UL) * TIMER_PRESCALER)
#define TIMER_MILLISECOND_UNITS (TIMER_CYCLES * 1000UL)
#define TIMER_WHOLE_MILLISECONDS (TIMER_MILLISECOND_UNITS / F_CPU)
#define TIMER_FRACTION_STEP ((TIMER_MILLISECOND_UNITS % F_CPU) / TIMER_FRACTION_DIVISOR)
#define TIMER_FRACTION_SCALE (F_CPU / TIMER_FRACTION_DIVISOR)

/* Alocação estática das threads e variáveis de controle do sistema */
Thread OS::threads[MAX_THREADS];
volatile uint8_t OS::current_index = 0;
volatile uint32_t OS::sys_ticks = 0;
OS::TimerFraction OS::tick_fraction = 0;

/**
 * Construtor padrão da classe Thread.
 * Define o estado inicial como não utilizado.
 */
Thread::Thread() {
    thread_state = THREAD_UNUSED;
}

/**
 * Inicializa o contexto de uma nova thread.
 * Esta função prepara a memória da pilha simulando manualmente o estado que o
 * processador teria se uma interrupção tivesse acabado de ocorrer. Isso permite
 * que o escalonador execute um RETI para iniciar a thread.
 * 
 * para quem ver isso aqui a função faz o seguinte(ate para eu não me perder depois):
 * A sequência de inicialização da pilha é:
 * 1. Define base e tamanho da pilha para monitoramento.
 * 2. Insere um byte sentinela (0xAA) no início da pilha para detectar Stack Overflow (como não tem MMU, é necessário detectar estouro manualmente).
 * 3. Prepara o frame de execução no topo da memória alocada.
 * 4. Empilha o endereço de retorno (threadExit) no fundo, garantindo que se a thread der 'return', ela pule para threadExit e limpe seu estado.
 * 5. Empilha o endereço da função da thread (PC). Quando o RETI final acontecer, o processador usará este endereço para começar a execução.
 * 6. Empilha o valor inicial de R0 (0x00). No assembly de troca de contexto, o R0 é o último registrador a ser restaurado (pop r0) antes do RETI.
 * 7. Empilha o valor do SREG (0x80). O assembly usa um registrador temporário para ler isso da pilha e jogar no SREG, reabilitando as interrupções globais.
 * 8. Preenche os registradores R1 até R31 com zeros (31 bytes). Isso limpa o contexto geral para evitar lixo de memória nos cálculos iniciais da thread.
 * 9. Salva o ponteiro de pilha (SP) resultante na thread.
 * 10. Define o estado da thread como READY, indicando que ela está pronta para o escalonador.
 * 
 * essa função é chamada internamente pelo método newThreadInternal da classe OS, que é responsável por encontrar um slot livre para a nova thread e chamar essa função de inicialização.
 * alem de que esse é o principal metodo de configuração do contexto de execução da thread, garantindo que quando a thread for escalonada pela primeira vez, ela comece a executar a função correta e tenha um ambiente de execução limpo e controlado 
 * ou seja se alguem for dar um fork tome cuidado com essa função aqui, se tiver erro aqui o controlador pode n fazer nada ou pior, corromper a pilha e causar comportamentos imprevisíveis.
 * 
 * @param func Ponteiro para a função que a thread executará.
 * @param stack_mem Ponteiro para o array de memória da pilha.
 * @param size Tamanho da pilha em bytes.
 */
void Thread::init(void (*func)(void), uint8_t *stack_mem, uint16_t size) {
    stack_base = stack_mem;
    stack_mem[0] = 0xAA;
    uint8_t *sp = &stack_mem[size - 1];

    uint16_t exitAddress = (uint16_t)&OS::threadExit;
    *sp-- = exitAddress & 0xFF;
    *sp-- = (exitAddress >> 8) & 0xFF;


    uint16_t funcAddress = (uint16_t)func;
    *sp-- = funcAddress & 0xFF;
    *sp-- = (funcAddress >> 8) & 0xFF;

    *sp-- = 0;
    *sp-- = 0x80;
    for (int i = 1; i < 32; i++) *sp-- = 0;
    stack_pointer = sp;
    thread_state = THREAD_READY;
}

/**
 * Verifica se houve estouro de pilha (Stack Overflow).
 * Confere se o byte sentinela (0xAA) na base da pilha foi alterado.
 *
 * @return true se a pilha estiver corrompida, false caso contrário.
 */
bool Thread::isCorrupted() {
    Thread* t = OS::getCurrentThread();
    if (t == nullptr) return true;
    if (t->stack_base == nullptr) {
        return false; 
    }
    return (t->stack_base[0] != 0xAA);
}

/**
 * Coloca a thread atual em estado de suspensão (Sleep).
 * A thread ficará inativa até que o tempo do sistema atinja o wake_time calculado.
 *
 * @param ms Tempo em milissegundos para dormir.
 */
void Thread::sleep(uint32_t ms) {
    uint8_t previous_sreg = SREG;
    cli();
    Thread* t = OS::getCurrentThread();
    t->wake_time = OS::sys_ticks + ms;
    t->thread_state = THREAD_SLEEP;
    SREG = previous_sreg;
    Thread::yield();
}

/**
 * Verifica se a thread atual está marcada como THREAD_SLEEP.
 *
 * Esta função NÃO desabilita interrupções e NÃO coloca a thread para dormir.
 * Ela apenas consulta o estado lógico da thread atual.
 *
 * Quando todas as threads estão dormindo, o escalonador pode manter
 * o contexto da thread anterior executando até o próximo tick/interrupção
 * do timer, mesmo que essa thread ainda esteja marcada como THREAD_SLEEP.
 *
 * Nesse cenário, o código da thread pode usar isSleep() para decidir
 * se deve executar seu fluxo normal ou apenas aguardar a próxima preempção.
 *
 * @return true se a thread atual estiver em THREAD_SLEEP, false caso contrário.
 */
bool Thread::isSleep(){
    Thread* t = OS::getCurrentThread();
    return t->thread_state == THREAD_SLEEP;
}

/**
 * Cede voluntariamente o processador para a próxima thread (Yield).
 * Aciona a troca de contexto via Assembly.
 */
void Thread::yield() {
    OS_yield_asm();
}

/**
 * Retorna o número de threads em estado READY.
 *
 * Conta apenas threads prontas para serem escalonadas.
 * Não conta THREAD_RUNNING, THREAD_SLEEP, THREAD_BLOCKED ou THREAD_UNUSED.
 *
 * @return Quantidade de threads em THREAD_READY.
 */
uint8_t OS::getReadyThreadCount() {
    AtomicGuard guard;

    uint8_t count = 0;

    for (uint8_t i = 0; i < MAX_THREADS; i++) {
        if (threads[i].thread_state == THREAD_READY) {
            count++;
        }
    }

    return count;
}

/**
 * Verifica se existe pelo menos uma thread em estado READY.
 *
 * Útil para saber se a thread atual pode dormir sem deixar o kernel
 * sem nenhuma outra thread pronta para executar.
 *
 * @return true se existir pelo menos uma thread READY, false caso contrário.
 */
bool OS::hasReadyThread() {
    AtomicGuard guard;

    for (uint8_t i = 0; i < MAX_THREADS; i++) {
        if (threads[i].thread_state == THREAD_READY) {
            return true;
        }
    }

    return false;
}

/**
 * Tenta adquirir o Mutex.
 * Se o Mutex já estiver bloqueado, a thread atual entra em estado de bloqueio
 * e aguarda até que ele seja liberado.
 */
void Mutex::lock() {
    while (true) {
        uint8_t previous_sreg = SREG;
        cli();

        uint8_t current = OS::currentThreadIndex();
        if (!locked) {
            locked = true;
            owner_index = current;
            SREG = previous_sreg;
            return;
        }

        OS::threads[current].thread_state = THREAD_BLOCKED;
        waiting_mask |= (uint8_t)(1U << current);
        SREG = previous_sreg;
        Thread::yield();
    }
}

/**
 * Libera o Mutex.
 * Se houver threads aguardando por este recurso, a de maior prioridade (ou ordem definida)
 * é acordada e colocada em estado READY.
 */
void Mutex::unlock() {
    uint8_t previous_sreg = SREG;
    cli();
    if (owner_index == OS::currentThreadIndex()) {
        locked = false;
        owner_index = -1;
        if (waiting_mask != 0) {
            uint8_t next_thread = __builtin_ctz(waiting_mask);
            waiting_mask &= (uint8_t)~(1U << next_thread);
            OS::threads[next_thread].thread_state = THREAD_READY;
        }
    }
    SREG = previous_sreg;
}

/**
 * Retorna o tempo total de execução do sistema.
 *
 * @return Número de ticks (em ms) desde a inicialização do OS.
 */
uint32_t OS::getTicks() {
    uint8_t previous_sreg = SREG;
    cli();
    uint32_t ticks = sys_ticks;
    SREG = previous_sreg;
    return ticks;
}

/**
 * Retorna o ponteiro para a thread que está sendo executada no momento.
 *
 * @return Ponteiro para o objeto Thread atual.
 */
Thread* OS::getCurrentThread() {
    return &threads[currentThreadIndex()];
}

/**
 * Busca um slot livre na lista de threads e inicializa uma nova tarefa.
 *
 * @param func Função da thread.
 * @param stack_mem Memória da pilha.
 * @param size Tamanho da pilha.
 * @return Ponteiro para a thread criada ou nullptr se não houver espaço.
 */
Thread* OS::newThreadInternal(void (*func)(void), uint8_t *stack_mem, uint16_t size) {
    for (uint8_t i = 1; i < MAX_THREADS; i++) {
        if (threads[i].thread_state == THREAD_UNUSED) {
            threads[i].init(func, stack_mem, size);
            return &threads[i];
        }
    }
    return nullptr;
}

/**
 * Inicializa o Kernel e o Timer2.
 * Configura a Thread 0 como running e prepara a interrupção de tempo (tick).
 */
void OS::init() {
    sys_ticks = 0;
    tick_fraction = 0;
    current_index = 0;
    threads[0].thread_state = THREAD_RUNNING;
    threads[0].stack_base = nullptr;
    for (uint8_t i = 1; i < MAX_THREADS; i++) threads[i].thread_state = THREAD_UNUSED;
    cli();
    TCCR2A = 0;
    TCCR2B = 0;
    TCNT2  = 0;
    OCR2A = TIMER_OCRNA;
    TCCR2A |= (1 << WGM21);
    TCCR2B |= (1 << CS22) | (1 << CS21) | (1 << CS20);
    TIMSK2 |= (1 << OCIE2A);
    sei();
}

/**
 * Entra em uma seção crítica do escalonador.
 * Bloqueia a troca de thread, mas mantém as interrupções habilitadas.
 */
void OS::enterCritical() {
    cli();
    current_index |= CONTEXT_SWITCH_LOCK_BIT;
    sei();
}

/**
 * Sai de uma seção crítica do escalonador e libera a troca de thread.
 */
void OS::exitCritical() {
    cli();
    current_index &= CURRENT_INDEX_MASK;
    sei();
}

/**
 * Função chamada quando uma thread termina sua execução.
 * Marca a thread como não utilizada e aciona o escalonador para escolher a próxima thread a ser executada.
 * Esta função é colocada como endereço de retorno no frame de execução das threads, garantindo que quando uma thread terminar, ela chame esta função para limpar seu estado e permitir que o sistema não chame uma thread morta.
 */
void OS::threadExit() {
    OS::enterCritical();
    threads[currentThreadIndex()].thread_state = THREAD_UNUSED;
    OS::exitCritical();
    Thread::yield();
    while(1);
}

/**
 * Retorna o número de threads ativas (não UNUSED) no sistema.
 */
uint8_t OS::getActiveThreads() {
    AtomicGuard guard;
    uint8_t count = 0;
    for (uint8_t i = 0; i < MAX_THREADS; i++) {
        if (threads[i].thread_state != THREAD_UNUSED) {
            count++;
        }
    }
    return count;
}

/**
 * Realiza a troca de contexto (Scheduler).
 * 1. Salva o Stack Pointer da thread anterior.
 * 2. Atualiza o relógio do sistema.
 * 3. Acorda threads que finalizaram o tempo de sleep.
 * 4. Executa o algoritmo Round Robin para escolher a próxima thread.
 * 5. Retorna o Stack Pointer da nova thread para o Assembly.
 *
 * @param oldSP Ponteiro de pilha da thread que estava rodando antes da interrupção.
 * @param timer_tick true quando a troca foi iniciada pela interrupção do Timer2.
 * @return Novo ponteiro de pilha para a CPU carregar.
 */
void* OS::contextSwitch(void* oldSP, bool timer_tick) {
    uint8_t current = currentThreadIndex();
    threads[current].stack_pointer = (uint8_t*)oldSP;

    if (timer_tick) {
        sys_ticks += TIMER_WHOLE_MILLISECONDS;
        if (TIMER_FRACTION_STEP != 0) {
            const TimerFraction carry_threshold = TIMER_FRACTION_SCALE - TIMER_FRACTION_STEP;
            if (tick_fraction >= carry_threshold) {
                tick_fraction -= carry_threshold;
                sys_ticks++;
            } else {
                tick_fraction += TIMER_FRACTION_STEP;
            }
        }
    }

    for (uint8_t i = 0; i < MAX_THREADS; i++) {
        if (threads[i].thread_state == THREAD_SLEEP) {
            if ((int32_t)(sys_ticks - threads[i].wake_time) >= 0) {
                threads[i].thread_state = THREAD_READY;
            }
        }
    }

    if (current_index & CONTEXT_SWITCH_LOCK_BIT) {
        return threads[current].stack_pointer;
    }

    uint8_t next = current;
    bool found = false;
    for (uint8_t i = 0; i < MAX_THREADS; i++) {
        next = (next + 1) % MAX_THREADS;

        if (threads[next].thread_state == THREAD_READY || (next == current && threads[next].thread_state == THREAD_RUNNING)) {
            
            found = true;
            
            if (next != current && threads[current].thread_state == THREAD_RUNNING) {
                threads[current].thread_state = THREAD_READY;
            }

            threads[next].thread_state = THREAD_RUNNING;
            current_index = next;
            break;
        }
        
    }
    
    
    if (!found) {
        current_index = 0;
    }

    return threads[currentThreadIndex()].stack_pointer;
}

/**
 * Wrapper com linkage "C" para ser chamado pelo código Assembly (ISR).
 * Redireciona a chamada para o método estático C++ do Kernel.
 */
extern "C" void* OS_contextSwitch_Wrapper(void* oldSP, uint8_t timer_tick) {
    return OS::contextSwitch(oldSP, timer_tick != 0);
}
