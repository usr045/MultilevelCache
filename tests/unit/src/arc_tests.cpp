#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "../tests/unit/include/trace.hpp"
#include "access_result.hpp"
#include "cache/arc.hpp"

namespace {

using namespace cache;

TEST(ConstructorTest, AcceptsPositiveCacheSize)
{
    using ARC = ARC<int, int>;

    EXPECT_NO_THROW(ARC{2});
    EXPECT_NO_THROW(ARC{15});
}

TEST(ConstructorTest, RejectZeroCacheSize)
{    
    using ARC = ARC<int, int>;

    EXPECT_THROW(ARC{0}, std::invalid_argument);
    EXPECT_THROW(ARC{1}, std::invalid_argument);
}

TEST(LookupUpdateTest, AllMisses)
{
    unit_tests::Trace<ARC<int, int>> trace{2};

    trace.request(1, 101, HitStatus::MISS);
    trace.request(2, 201, HitStatus::MISS);
    trace.request(3, 301, HitStatus::MISS);

    std::vector<int> expected_loaded_keys{1, 2, 3};

    EXPECT_EQ(trace.loaded_keys(), expected_loaded_keys);
}


TEST(TransitionTest, FullT1RemovesOldestKey) {
    unit_tests::Trace<ARC<int, int>> trace{2};

    trace.request(1, 101, HitStatus::MISS);
    trace.request(2, 201, HitStatus::MISS);
    trace.request(3, 301, HitStatus::MISS);
    trace.request(2, 201, HitStatus::HIT);
    trace.request(1, 102, HitStatus::MISS);
    trace.request(2, 201, HitStatus::HIT);

    const std::vector<int> expected{1, 2, 3, 1};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(TransitionTest, ReturnFromB1ReloadsAndReplacesFromT2) {
    unit_tests::Trace<ARC<int, int>> trace{2};

    trace.request(1, 101, HitStatus::MISS);
    trace.request(1, 101, HitStatus::HIT);
    trace.request(2, 201, HitStatus::MISS);
    trace.request(3, 301, HitStatus::MISS);
    trace.request(2, 202, HitStatus::MISS);
    trace.request(1, 102, HitStatus::MISS);
    trace.request(2, 202, HitStatus::HIT);

    const std::vector<int> expected{1, 2, 3, 2, 1};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(TransitionTest, ReturnFromB2ReloadsAndAffectsNextVictim) {
    unit_tests::Trace<ARC<int, int>> trace{2};

    trace.request(1, 101, HitStatus::MISS);
    trace.request(1, 101, HitStatus::HIT);
    trace.request(2, 201, HitStatus::MISS);
    trace.request(2, 201, HitStatus::HIT);
    trace.request(3, 301, HitStatus::MISS);
    trace.request(1, 102, HitStatus::MISS);
    trace.request(4, 401, HitStatus::MISS);
    trace.request(2, 202, HitStatus::MISS);

    const std::vector<int> expected{1, 2, 3, 1, 4, 2};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(TransitionTest, HitInT2UpdatesRecency) {
    unit_tests::Trace<ARC<int, int>> trace{3};

    trace.request(1, 101, HitStatus::MISS);
    trace.request(1, 101, HitStatus::HIT);
    trace.request(2, 201, HitStatus::MISS);
    trace.request(2, 201, HitStatus::HIT);
    trace.request(3, 301, HitStatus::MISS);
    trace.request(3, 301, HitStatus::HIT);
    trace.request(1, 101, HitStatus::HIT);
    trace.request(4, 401, HitStatus::MISS);
    trace.request(1, 101, HitStatus::HIT);
    trace.request(2, 202, HitStatus::MISS);

    const std::vector<int> expected{1, 2, 3, 4, 2};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(TransitionTest, B2ReturnUsesEqualityCaseInReplace) {
    unit_tests::Trace<ARC<int, int>> trace{3};

    trace.request(1, 101, HitStatus::MISS);
    trace.request(1, 101, HitStatus::HIT);
    trace.request(2, 201, HitStatus::MISS);
    trace.request(3, 301, HitStatus::MISS);
    trace.request(4, 401, HitStatus::MISS);
    trace.request(2, 202, HitStatus::MISS);
    trace.request(3, 302, HitStatus::MISS);
    trace.request(1, 102, HitStatus::MISS);
    trace.request(2, 202, HitStatus::HIT);

    const std::vector<int> expected{1, 2, 3, 4, 2, 3, 1};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(TransitionTest, RemovesOldestB1Ghost) {
    unit_tests::Trace<ARC<int, int>> trace{2};

    trace.request(1, 101, HitStatus::MISS);
    trace.request(1, 101, HitStatus::HIT);
    trace.request(2, 201, HitStatus::MISS);
    trace.request(3, 301, HitStatus::MISS);
    trace.request(4, 401, HitStatus::MISS);
    trace.request(2, 202, HitStatus::MISS);
    trace.request(1, 101, HitStatus::HIT);

    const std::vector<int> expected{1, 2, 3, 4, 2};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(TransitionTest, RemovesOldestB2Ghost) {
    unit_tests::Trace<ARC<int, int>> trace{2};

    trace.request(1, 101, HitStatus::MISS);
    trace.request(1, 101, HitStatus::HIT);
    trace.request(2, 201, HitStatus::MISS);
    trace.request(2, 201, HitStatus::HIT);
    trace.request(3, 301, HitStatus::MISS);
    trace.request(3, 301, HitStatus::HIT);
    trace.request(4, 401, HitStatus::MISS);
    trace.request(5, 501, HitStatus::MISS);
    trace.request(1, 102, HitStatus::MISS);
    trace.request(6, 601, HitStatus::MISS);
    trace.request(3, 301, HitStatus::HIT);

    const std::vector<int> expected{1, 2, 3, 4, 5, 1, 6};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

} // anonymous namespace
