#include "KernelSchedule.h"

Thread OS::threads[MAX_THREADS];
uint8_t OS::current_index = 0;
uint32_t OS::sys_ticks = 0;
bool OS::atomic_lock = false;

Thread::Thread() {
    thread_state = THREAD_UNUSED;
}

void Thread::init(uint8_t _id, void (*func)(void), uint8_t *stack_mem, uint16_t size) {
    stack_base = stack_mem;
    stack_size = size;
    stack_mem[0] = 0xAA;
    uint8_t *sp = &stack_mem[size - 1];
    uint16_t address = (uint16_t)func;
    *sp-- = address & 0xFF;
    *sp-- = (address >> 8) & 0xFF;
    for (int i = 0; i < 33; i++) *sp-- = 0;
    stack_pointer = sp;
    thread_state = THREAD_READY;
}

bool Thread::isCorrupted() {
  Thread* t = OS::getCurrentThread();
  if (t == nullptr) return true;
  if (t->stack_base == nullptr) {
    return false; 
  }
  return (t->stack_base[0] != 0xAA);
}

void Thread::sleep(uint32_t ms) {
    cli();
    Thread* t = OS::getCurrentThread();
    t->wake_time = OS::getTicks() + ms;
    t->thread_state = THREAD_SLEEP;
    sei();
    Thread::yield();
}

void Thread::yield() {
    TIFR1 |= (1 << OCF1A);
}

void Mutex::lock() {
    cli();
    if (!locked) {
        locked = true;
        owner_index = OS::current_index;
        sei();
    } else {
        OS::getCurrentThread()->thread_state = THREAD_BLOCKED;
        waiting_threads[OS::current_index] = true;
        sei();
        Thread::yield();
    }
}

void Mutex::unlock() {
    cli();
    if (owner_index == OS::current_index) {
        locked = false;
        owner_index = -1;
        for (uint8_t i = 0; i < MAX_THREADS; i++) {
            if (waiting_threads[i]) {
                waiting_threads[i] = false;
                OS::threads[i].thread_state = THREAD_READY;
                break;
            }
        }
    }
    sei();
}

uint32_t OS::getTicks() {
    return sys_ticks;
}

Thread* OS::getCurrentThread() {
    return &threads[current_index];
}

Thread* OS::newThread(void (*func)(void), uint16_t stack_size) {
    for (uint8_t i = 1; i < MAX_THREADS; i++) {
        if (threads[i].thread_state == THREAD_UNUSED) {
            uint8_t *stack_mem = new uint8_t[stack_size];
            if (!stack_mem) return nullptr;
            threads[i].init(i, func, stack_mem, stack_size);
            return &threads[i];
        }
    }
    return nullptr;
}

void OS::scheduler() {
    sys_ticks += TIME_SLICE;

    for (uint8_t i = 0; i < MAX_THREADS; i++) {
        if (threads[i].thread_state == THREAD_SLEEP) {
            if (sys_ticks >= threads[i].wake_time) {
                threads[i].thread_state = THREAD_READY;
            }
        }
    }

    if (atomic_lock) return;
    uint8_t next = current_index;
    for (uint8_t i = 0; i < MAX_THREADS; i++) {
        next = (next + 1) % MAX_THREADS;
        if (threads[next].thread_state == THREAD_READY) {
            if (next != current_index) {
                if (threads[current_index].thread_state == THREAD_RUNNING) {
                    threads[current_index].thread_state = THREAD_READY;
                }
                threads[next].thread_state = THREAD_RUNNING;
                current_index = next;
            }
            return;
        }
    }
}

void OS::saveStackPointer() {
    threads[current_index].stack_pointer = (uint8_t*)SP;
}

void OS::loadStackPointer() {
    SP = (uint16_t)threads[current_index].stack_pointer;
}

void OS::init() {
    sys_ticks = 0;
    current_index = 0;
    threads[0].thread_state = THREAD_RUNNING;
    threads[0].stack_base = nullptr;
    for (uint8_t i = 1; i < MAX_THREADS; i++) threads[i].thread_state = THREAD_UNUSED;
    cli();
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1 = 0;
    OCR1A = 4999;
    TCCR1B |= (1 << WGM12);
    TCCR1B |= (1 << CS11) | (1 << CS10);
    TIMSK1 |= (1 << OCIE1A);
    sei();
}

void OS::enterCritical() {
    atomic_lock = true;
}

void OS::exitCritical() {
    atomic_lock = false;
}

extern "C" {
    void OS_saveStackPointer() {
        OS::saveStackPointer();
    }
    void OS_scheduler() {
        OS::scheduler();
    }
    void OS_loadStackPointer() {
        OS::loadStackPointer();
    }
}

