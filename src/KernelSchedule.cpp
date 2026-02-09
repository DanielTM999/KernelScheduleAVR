#include "KernelSchedule.h"

/* Configurações do Timer para o Time Slice do Scheduler */
#define TIMER_OCRNA 255
#define TIMER_PRESCALER 1024
#define TIMER_TICK_US ((TIMER_PRESCALER * 1000000UL) / F_CPU)
#define TIME_SLICE_MS (((TIMER_OCRNA + 1) * TIMER_TICK_US) / 1000)

/* Alocação estática das threads e variáveis de controle do sistema */
Thread OS::threads[MAX_THREADS];
volatile uint8_t OS::current_index = 0;
volatile uint32_t OS::sys_ticks = 0;

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
 * @param _id ID numérico da thread(não faz nada pois o sistema não usa IDs devido a quantidade me memoria limitada a 2kb).
 * @param func Ponteiro para a função que a thread executará.
 * @param stack_mem Ponteiro para o array de memória da pilha.
 * @param size Tamanho da pilha em bytes.
 */
void Thread::init(uint8_t _id, void (*func)(void), uint8_t *stack_mem, uint16_t size) {
    stack_base = stack_mem;
    stack_size = size;
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
    cli();
    Thread* t = OS::getCurrentThread();
    t->wake_time = OS::getTicks() + ms;
    t->thread_state = THREAD_SLEEP;
    sei();
    Thread::yield();
}

/**
 * Cede voluntariamente o processador para a próxima thread (Yield).
 * Aciona a troca de contexto via Assembly.
 */
void Thread::yield() {
    OS_yield_asm();
}

/**
 * Tenta adquirir o Mutex.
 * Se o Mutex já estiver bloqueado, a thread atual entra em estado de bloqueio
 * e aguarda até que ele seja liberado.
 */
void Mutex::lock() {
    cli();
    if (!locked) {
        locked = true;
        owner_index = OS::current_index;
        sei();
    } else {
        OS::getCurrentThread()->thread_state = THREAD_BLOCKED;
        waiting_mask |= (1 << OS::current_index);
        sei();
        Thread::yield();
    }
}

/**
 * Libera o Mutex.
 * Se houver threads aguardando por este recurso, a de maior prioridade (ou ordem definida)
 * é acordada e colocada em estado READY.
 */
void Mutex::unlock() {
    cli();
    if (owner_index == OS::current_index) {
        locked = false;
        owner_index = -1;
        if (waiting_mask != 0) {
            uint8_t next_thread = __builtin_ctz(waiting_mask);
            waiting_mask &= ~(1 << next_thread);
            OS::threads[next_thread].thread_state = THREAD_READY;
        }
    }
    sei();
}

/**
 * Retorna o tempo total de execução do sistema.
 *
 * @return Número de ticks (em ms) desde a inicialização do OS.
 */
uint32_t OS::getTicks() {
    return sys_ticks;
}

/**
 * Retorna o ponteiro para a thread que está sendo executada no momento.
 *
 * @return Ponteiro para o objeto Thread atual.
 */
Thread* OS::getCurrentThread() {
    return &threads[current_index];
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
            threads[i].init(i, func, stack_mem, size);
            return &threads[i];
        }
    }
    return nullptr;
}

/**
 * Inicializa o Kernel e o Timer1.
 * Configura a Thread 0 como running e prepara a interrupção de tempo (tick).
 */
void OS::init() {
    sys_ticks = 0;
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
 * Entra em uma seção crítica do código.
 * Desabilita interrupções globais.
 */
void OS::enterCritical() {
    cli();
}

/**
 * Sai de uma seção crítica do código.
 * Habilita interrupções globais.
 */
void OS::exitCritical() {
    sei();
}

/**
 * Função chamada quando uma thread termina sua execução.
 * Marca a thread como não utilizada e aciona o escalonador para escolher a próxima thread a ser executada.
 * Esta função é colocada como endereço de retorno no frame de execução das threads, garantindo que quando uma thread terminar, ela chame esta função para limpar seu estado e permitir que o sistema não chame uma thread morta.
 */
void OS::threadExit() {
    OS::enterCritical();
    threads[current_index].thread_state = THREAD_UNUSED;
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
 * @return Novo ponteiro de pilha para a CPU carregar.
 */
void* OS::contextSwitch(void* oldSP) {
    int contagem = 0;
    threads[current_index].stack_pointer = (uint8_t*)oldSP;
    
    sys_ticks += TIME_SLICE_MS;

    for (uint8_t i = 0; i < MAX_THREADS; i++) {
        if (threads[i].thread_state == THREAD_SLEEP) {
            if (sys_ticks >= threads[i].wake_time) {
                threads[i].thread_state = THREAD_READY;
            }
        }
    }

    uint8_t next = current_index;
    for (uint8_t i = 0; i < MAX_THREADS; i++) {
        next = (next + 1) % MAX_THREADS;

        if (threads[next].thread_state == THREAD_READY || (next == current_index && threads[next].thread_state == THREAD_RUNNING)) {
            
            if (next != current_index) {
                if (threads[current_index].thread_state == THREAD_RUNNING) {
                    threads[current_index].thread_state = THREAD_READY;
                }
                threads[next].thread_state = THREAD_RUNNING;
                current_index = next;
            }
            break;
        }
    }

    return threads[current_index].stack_pointer;
}

/**
 * Wrapper com linkage "C" para ser chamado pelo código Assembly (ISR).
 * Redireciona a chamada para o método estático C++ do Kernel.
 */
extern "C" void* OS_contextSwitch_Wrapper(void* oldSP) {
    return OS::contextSwitch(oldSP);
}