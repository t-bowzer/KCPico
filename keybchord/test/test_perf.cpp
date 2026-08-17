#include <gtest/gtest.h>

#include "perf.h"


TEST(PerfStats, EmptyStatsReportZero) {
    PerfStats s;
    EXPECT_EQ(s.count(), 0u);
    EXPECT_EQ(s.minUs(), 0u);
    EXPECT_EQ(s.maxUs(), 0u);
    EXPECT_EQ(s.avgUs(), 0u);
}

TEST(PerfStats, SingleSample) {
    PerfStats s;
    s.record(1500);
    EXPECT_EQ(s.count(), 1u);
    EXPECT_EQ(s.minUs(), 1500u);
    EXPECT_EQ(s.maxUs(), 1500u);
    EXPECT_EQ(s.avgUs(), 1500u);
}

TEST(PerfStats, MinMaxAvg) {
    PerfStats s;
    s.record(100);
    s.record(300);
    s.record(200);
    EXPECT_EQ(s.count(), 3u);
    EXPECT_EQ(s.minUs(), 100u);
    EXPECT_EQ(s.maxUs(), 300u);
    EXPECT_EQ(s.avgUs(), 200u);
}

TEST(PerfStats, ResetClears) {
    PerfStats s;
    s.record(500);
    s.reset();
    EXPECT_EQ(s.count(), 0u);
    EXPECT_EQ(s.minUs(), 0u);
    EXPECT_EQ(s.maxUs(), 0u);

    s.record(42);
    EXPECT_EQ(s.minUs(), 42u);
    EXPECT_EQ(s.maxUs(), 42u);
}
