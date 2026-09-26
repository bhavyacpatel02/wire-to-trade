#ifndef HISTOGRAM_H
#define HISTOGRAM_H

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

/**
 * First 16 indices directly map to that number of ticks
 * Then, there are 28 "buckets"
 * Within each bucket, there are 16 slices
 */
class Histogram {
   public:
    static constexpr size_t BASE_INDICES = 16;
    static constexpr size_t BUCKETS = 28;
    static constexpr size_t BUCKET_SLICES = 16;
    static constexpr size_t ARRAY_SIZE = BASE_INDICES +
                                         (BUCKETS * BUCKET_SLICES) +
                                         1;  // Additional overflow slot
    static constexpr size_t REPR_DIGITS =
        std::countr_zero(BASE_INDICES) + BUCKETS;
    static constexpr uint64_t MAX_TICK_SIZE = (uint64_t{1} << REPR_DIGITS) - 1;

    // This is how callers add a data point to the histogram
    void add_data_point(uint64_t ticks) {
        ++m_buckets[get_index(ticks)];
        ++m_count;
        m_min = std::min(m_min, ticks);
        m_max = std::max(m_max, ticks);
        m_sum += ticks;
    }

    // Given p in [0, 1], return the number of ticks at that percentile: the
    // upper edge of the bucket holding the sample at rank ceil(p * count),
    // capped at the true max. Never below the true value, at most ~6.25% above.
    //
    // Returns 0 if p is outside [0, 1] or NaN, or if the histogram is empty.
    // 0 is also a legitimate latency, so callers must check count() first.
    uint64_t percentile(double p) const {
        // Written as !(in range) rather than (out of range) so NaN is rejected:
        // every comparison with NaN is false.
        if (!(p >= 0.0 && p <= 1.0)) {
            return 0;
        }
        if (m_count == 0) {
            return 0;
        }

        // Rank of the sample we want, 1-based. p = 0 means the first sample.
        uint64_t target =
            static_cast<uint64_t>(std::ceil(p * static_cast<double>(m_count)));
        target = std::max<uint64_t>(1, target);

        uint64_t seen{};
        for (std::size_t i = 0; i < m_buckets.size(); ++i) {
            seen += m_buckets[i];
            if (seen >= target) {
                // Cap: a bucket's upper edge can exceed every recorded value,
                // and the overflow slot's edge is UINT64_MAX.
                return std::min(get_ticks(i), m_max);
            }
        }

        return m_max;  // Unreachable while bucket counts sum to m_count
    }

    // Get the actual histogram
    const std::array<uint64_t, ARRAY_SIZE>& buckets() const {
        return m_buckets;
    }

    // Getters for sample statistics
    uint64_t count() const { return m_count; }
    uint64_t min() const { return m_min; }
    uint64_t max() const { return m_max; }
    double mean() const {
        if (m_count == 0) {
            return 0;
        }
        return static_cast<double>(m_sum) / m_count;
    }

    // Given some number of ticks, map it into a bucket index
    static constexpr size_t get_index(uint64_t ticks) {
        if (ticks < BASE_INDICES) {
            return ticks;
        }
        if (ticks > MAX_TICK_SIZE) {
            return ARRAY_SIZE - 1;
        }
        auto width = std::bit_width(ticks);
        auto bucket_width = std::bit_width(BUCKET_SLICES);
        auto base_index = (width - bucket_width) * BUCKET_SLICES;
        auto offset =
            ticks >>
            (width - bucket_width);  // keep just enough of the significant high
                                     // bits to index into a slice
        return base_index + offset;
    }

    // Given a bucket index, return the ceiling of the tick range it holds
    static constexpr uint64_t get_ticks(size_t index) {
        if (index < BASE_INDICES) {
            return index;
        }
        if (index >= ARRAY_SIZE - 1) {
            // Overflow slot: no real upper edge. percentile() caps at m_max.
            return std::numeric_limits<uint64_t>::max();
        }
        auto base_index = (index - BASE_INDICES) / BUCKET_SLICES;
        auto offset = (index - BASE_INDICES) % BUCKET_SLICES;

        // Range number base_index covers [2^(base_index + 4), 2^(base_index + 5)),
        // where 4 is the bit count of the exact region (16 = 2^4). Splitting it
        // into 16 slices makes each slice 2^base_index ticks wide.
        const uint64_t range_start =
            uint64_t{1} << (base_index + std::countr_zero(BASE_INDICES));
        const uint64_t slice_width = uint64_t{1} << base_index;

        // Top of slice `offset`: the start of the next slice, minus one.
        return range_start + (offset + 1) * slice_width - 1;
    }

   private:
    // Histogram data structure
    std::array<uint64_t, ARRAY_SIZE> m_buckets{};

    // Sample statistics
    uint64_t m_count{};
    uint64_t m_min{std::numeric_limits<uint64_t>::max()};
    uint64_t m_max{};
    uint64_t m_sum{};
};

#endif
