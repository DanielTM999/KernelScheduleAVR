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
extern "C" void* OS_contextSwitch_Wrapper(void* oldSP);

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
    uint16_t stack_size;
    uint32_t wake_time;
    uint8_t thread_state;
    void init(uint8_t _id, void (*func)(void), uint8_t *stack_mem, uint16_t size);

public:
    Thread();
    static void sleep(uint32_t ms);
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
    static Thread threads[MAX_THREADS];
    volatile static uint8_t current_index;
    volatile static uint32_t sys_ticks;

    static Thread* newThreadInternal(void (*func)(void), uint8_t *stack_mem, uint16_t size);
    static void threadExit();

public:
    static uint32_t getTicks();
    static inline Thread* getCurrentThread();

    template <size_t N>
    static Thread* newThread(void (*func)(void), uint8_t (&stack_buffer)[N]) {return newThreadInternal(func, stack_buffer, N);}
    static uint8_t getActiveThreads();
    static void* contextSwitch(void* oldSP);
    static void init();
    static void enterCritical();
    static void exitCritical();
};

class AtomicGuard {
public:
    AtomicGuard() { OS::enterCritical(); }
    ~AtomicGuard() { OS::exitCritical(); }
};

#endif

