#include <cstdint>
#include <iostream>

// This is the virtual timer counter, we're gonna use it as our timestamp
uint64_t read_cntvct() {
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
uint64_t read_cntfrq() {
    uint64_t value;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(value));
    return value;
}

int main() {
    auto freq = read_cntfrq();
    auto cnt1 = read_cntvct();
    auto cnt2 = read_cntvct();

    std::cout << "Freq: " << freq << "\nCount1: " << cnt1 << "\nCount2: " << cnt2 << "\n";
}
