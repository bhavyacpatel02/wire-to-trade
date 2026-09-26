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

    // Counter frequency (NOT CPU GHz). This is how we interpret virtual counter
    static uint64_t frequency() {
        uint64_t value;
        asm volatile("mrs %0, cntfrq_el0" : "=r"(value));
        return value;
    }

    static double ticks_to_ns(uint64_t ticks) {
        static const double ns_per_tick =
            1e9 / static_cast<double>(frequency());
        return ns_per_tick * ticks;
    }
};

#endif
