#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <thread>

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
    auto tim1 = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto cnt2 = read_cntvct();
    auto tim2 = std::chrono::steady_clock::now();

    std::chrono::duration<double> elapsed = tim2 - tim1;
    const double ns_per_tick = 1e9 / static_cast<double>(freq);
    std::cout << std::fixed;  // Sticky: no scientific notation from here on
    std::cout << "Freq: " << freq << " ticks/s ("
              << std::setprecision(2) << ns_per_tick << " ns per tick)"
              << "\nCount1: " << cnt1 << "\nCount2: " << cnt2 << "\n";
    auto adjustedFreq = (cnt2 - cnt1) / elapsed.count();
    std::cout << "Adjusted Freq: " << std::setprecision(0) << adjustedFreq
              << " ticks/s\n";

    // Batch cost
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 1000000; ++i) {
        read_cntvct();
    }
    auto end = std::chrono::steady_clock::now();

    elapsed = end - start;
    const double ns_per_read = elapsed.count() * 1e9 / 1000000;
    std::cout << "Batch cost per read: " << std::setprecision(2) << ns_per_read
              << " ns (" << ns_per_read / ns_per_tick << " ticks)\n";

    // Check difference across many pairs of back-to-back reads
    struct DiffCnts {
        uint32_t zero{};
        uint32_t one{};
        uint32_t two{};
    };

    auto diffCnts = DiffCnts{};
    auto prevCnt = read_cntvct();
    for (int i = 1; i < 1000000; ++i) {
        auto cnt = read_cntvct();
        auto diff = cnt - prevCnt;
        if (diff == 0) {
            ++diffCnts.zero;
        } else if (diff == 1) {
            ++diffCnts.one;
        } else if (diff == 2) {
            ++diffCnts.two;
        }

        prevCnt = cnt;
    }

    std::cout << "Diff of 0: " << diffCnts.zero
              << "\nDiff of 1: " << diffCnts.one
              << "\nDiff of 2: " << diffCnts.two << "\n";
}
