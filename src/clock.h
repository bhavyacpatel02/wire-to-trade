#ifndef CLOCK_H
#define CLOCK_H

#include <cstdint>

class Clock {
   public:
    // This is the virtual timer counter, we're gonna use it as our timestamp
    static uint64_t now() {
        uint64_t value;
        asm volatile(
            "isb\n"
            "mrs %0, cntvct_el0"
            : "=r"(value)
            :
            : "memory");
        return value;
    }

    // This is the counter frequency (NOT CPU GHz). This is how we can actually
    // interpret the virtual timer counter
    static uint64_t frequency() {
        uint64_t value;
        asm volatile("mrs %0, cntfrq_el0" : "=r"(value));
        return value;
    }
};

#endif
