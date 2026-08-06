#include <gtest/gtest.h>

TEST(ScaffoldingTest, Trivial) {
    EXPECT_EQ(1 + 1, 2);
}

TEST(ScaffoldingTest, AdaptersExist) {
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
