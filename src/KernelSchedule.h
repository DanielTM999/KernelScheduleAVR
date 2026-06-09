#ifndef KERNELSCHEDULE_H
#define KERNELSCHEDULE_H

#include <Arduino.h>

#define STACK_SIZE_SMALL 128
#define STACK_SIZE_MEDIUM 256
#define STACK_SIZE_LARGE 512

#define THREAD_UNUSED  0
#define THREAD_RUNNING 1
#define THREAD_READY   2
#define THREAD_SLEEP   3
#define THREAD_BLOCKED 4

#define MAX_THREADS 3

extern "C" void OS_yield_asm(); 
extern "C" void* OS_contextSwitch_Wrapper(void* oldSP, uint8_t timer_tick);

class OS;
class Thread;
class Mutex;

class Thread {
    friend class OS;
    friend class Mutex;
    friend void scheduler_isr();

private:
    uint8_t *stack_pointer;
    uint8_t *stack_base;
    uint32_t wake_time;
    uint8_t thread_state;
    void init(void (*func)(void), uint8_t *stack_mem, uint16_t size);

public:
    Thread();
    static void sleep(uint32_t ms);
    static bool isSleep();
    static void yield();
    static bool isCorrupted();
};

class Mutex {
private:
    bool locked = false;
    int8_t owner_index = -1;
    uint8_t waiting_mask = 0;

public:
    void lock();
    void unlock();
};

class OS {
    friend void scheduler_isr();
    friend class Thread;
    friend class Mutex;
    friend class AtomicGuard;

private:
    static const uint8_t CURRENT_INDEX_MASK = 0b01111111;
    static const uint8_t CONTEXT_SWITCH_LOCK_BIT = 0b10000000;
    static Thread threads[MAX_THREADS];
    // O bit 7 bloqueia a troca de thread; os demais bits guardam o indice.
    volatile static uint8_t current_index;
    volatile static uint32_t sys_ticks;

#if (F_CPU % 128000UL) == 0 && (F_CPU / 128000UL) <= UINT8_MAX
    typedef uint8_t TimerFraction;
    enum : uint32_t { TIMER_FRACTION_DIVISOR = 128000UL };
#elif (F_CPU % 64000UL) == 0 && (F_CPU / 64000UL) <= UINT8_MAX
    typedef uint8_t TimerFraction;
    enum : uint32_t { TIMER_FRACTION_DIVISOR = 64000UL };
#elif (F_CPU % 8000UL) == 0 && (F_CPU / 8000UL) <= UINT16_MAX
    typedef uint16_t TimerFraction;
    enum : uint32_t { TIMER_FRACTION_DIVISOR = 8000UL };
#else
    typedef uint32_t TimerFraction;
    enum : uint32_t { TIMER_FRACTION_DIVISOR = 1UL };
#endif
    static TimerFraction tick_fraction;

    static inline uint8_t currentThreadIndex() {
        return current_index & CURRENT_INDEX_MASK;
    }
    static Thread* newThreadInternal(void (*func)(void), uint8_t *stack_mem, uint16_t size);
    static void threadExit();

public:
    static uint32_t getTicks();
    static inline Thread* getCurrentThread();

    template <size_t N>
    static Thread* newThread(void (*func)(void), uint8_t (&stack_buffer)[N]) {return newThreadInternal(func, stack_buffer, N);}
    static uint8_t getActiveThreads();
    static void* contextSwitch(void* oldSP, bool timer_tick);
    static void init();
    static void enterCritical();
    static void exitCritical();
    static uint8_t getReadyThreadCount();
    static bool hasReadyThread();
};

class AtomicGuard {
public:
    AtomicGuard() { OS::enterCritical(); }
    ~AtomicGuard() { OS::exitCritical(); }
};

#endif
