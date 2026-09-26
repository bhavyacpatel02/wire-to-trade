#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <numeric>

#include "histogram.h"

// ---------- Bucket mapping ----------

TEST(HistogramIndex, ExactRegionMapsToItself) {
    for (uint64_t v = 0; v < Histogram::BASE_INDICES; ++v) {
        EXPECT_EQ(Histogram::get_index(v), v) << "value " << v;
    }
}

TEST(HistogramIndex, BoundaryValues) {
    EXPECT_EQ(Histogram::get_index(16), 16u);  // first log range, width 1
    EXPECT_EQ(Histogram::get_index(31), 31u);
    EXPECT_EQ(Histogram::get_index(32), 32u);  // width 2 starts here
    EXPECT_EQ(Histogram::get_index(33), 32u);
    EXPECT_EQ(Histogram::get_index(34), 33u);
    EXPECT_EQ(Histogram::get_index(63), 47u);
    EXPECT_EQ(Histogram::get_index(64), 48u);
    EXPECT_EQ(Histogram::get_index(1000), 111u);  // bucket 992..1023
    EXPECT_EQ(Histogram::get_index(Histogram::MAX_TICK_SIZE), 463u);
}

TEST(HistogramIndex, TooLargeGoesToOverflowSlot) {
    const std::size_t overflow = Histogram::ARRAY_SIZE - 1;
    EXPECT_EQ(Histogram::get_index(Histogram::MAX_TICK_SIZE + 1), overflow);
    EXPECT_EQ(Histogram::get_index(std::numeric_limits<uint64_t>::max()),
              overflow);
}

// Every bucket's upper edge maps back to that bucket, and one past the edge
// maps to the next one. Together: the buckets tile the range with no gaps
// and no overlaps.
TEST(HistogramIndex, UpperEdgesRoundTrip) {
    for (std::size_t i = 0; i + 1 < Histogram::ARRAY_SIZE; ++i) {
        const uint64_t edge = Histogram::get_ticks(i);
        ASSERT_EQ(Histogram::get_index(edge), i) << "edge of bucket " << i;
        ASSERT_EQ(Histogram::get_index(edge + 1), i + 1)
            << "edge + 1 of bucket " << i;
    }
    EXPECT_EQ(Histogram::get_ticks(463), Histogram::MAX_TICK_SIZE);
}

// ---------- Recording ----------

TEST(HistogramRecord, CountsAddUpIncludingOverflow) {
    Histogram h;
    const uint64_t huge = Histogram::MAX_TICK_SIZE + 1;
    for (uint64_t v : {0ull, 5ull, 100ull, 1000ull, 1000ull}) {
        h.add_data_point(v);
    }
    h.add_data_point(huge);

    const auto& b = h.buckets();
    EXPECT_EQ(std::accumulate(b.begin(), b.end(), uint64_t{0}), h.count());
    EXPECT_EQ(h.count(), 6u);
    EXPECT_EQ(b[Histogram::ARRAY_SIZE - 1], 1u);  // the overflow sample
    EXPECT_EQ(h.min(), 0u);
    EXPECT_EQ(h.max(), huge);
}

TEST(HistogramRecord, MeanIsExact) {
    Histogram h;
    EXPECT_EQ(h.mean(), 0.0);  // empty
    h.add_data_point(1);
    h.add_data_point(2);
    EXPECT_DOUBLE_EQ(h.mean(), 1.5);
}

// ---------- Percentiles ----------

// True p50 of 1..1000 is 500 and p90 is 900. The histogram may overstate by
// up to 1/16 of the value, and never understate.
TEST(HistogramPercentile, WithinErrorBoundOnKnownData) {
    Histogram h;
    for (uint64_t v = 1; v <= 1000; ++v) {
        h.add_data_point(v);
    }
    const uint64_t p50 = h.percentile(0.5);
    const uint64_t p90 = h.percentile(0.9);
    EXPECT_GE(p50, 500u);
    EXPECT_LE(p50, 500u + 500u / 16);
    EXPECT_GE(p90, 900u);
    EXPECT_LE(p90, 900u + 900u / 16);
    EXPECT_EQ(h.percentile(1.0), 1000u);  // capped at the true max
}

TEST(HistogramPercentile, SingleSampleIsExact) {
    Histogram h;
    h.add_data_point(40);  // bucket 40..41: the cap must pull it back to 40
    EXPECT_EQ(h.percentile(0.0), 40u);
    EXPECT_EQ(h.percentile(0.5), 40u);
    EXPECT_EQ(h.percentile(1.0), 40u);
}

TEST(HistogramPercentile, OverflowReportsTrueMax) {
    Histogram h;
    const uint64_t huge = uint64_t{1} << 33;
    h.add_data_point(10);
    h.add_data_point(huge);
    EXPECT_EQ(h.percentile(1.0), huge);
}

TEST(HistogramPercentile, InvalidInputReturnsZero) {
    Histogram empty;
    EXPECT_EQ(empty.percentile(0.5), 0u);

    Histogram h;
    h.add_data_point(100);
    EXPECT_EQ(h.percentile(-0.1), 0u);
    EXPECT_EQ(h.percentile(1.1), 0u);
    EXPECT_EQ(h.percentile(std::nan("")), 0u);
}
