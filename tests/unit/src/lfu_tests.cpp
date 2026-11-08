#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "access_result.hpp"
#include "cache/lfu.hpp"
#include "trace.hpp"

namespace {

using namespace cache;

TEST(ConstructorTest, AcceptsCacheSizeGreaterThanOne)
{
    using LFU = LFU<int, int>;

    EXPECT_NO_THROW(LFU{2});
    EXPECT_NO_THROW(LFU{15});
}

TEST(ConstructorTest, RejectsCacheSizeZeroAndOne)
{
    using LFU = LFU<int, int>;

    EXPECT_THROW(LFU{0}, std::invalid_argument);
    EXPECT_THROW(LFU{1}, std::invalid_argument);
}

TEST(LookupUpdateTest, MissLoadsDataOnce)
{
    unit_tests::Trace<LFU<int, int>> trace{2};

    trace.request(1, 101, HitStatus::MISS);

    const std::vector<int> expected{1};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(LookupUpdateTest, RepeatedRequestsAreHitsWithoutReload)
{
    unit_tests::Trace<LFU<int, int>> trace{2};

    trace.request(1, 101, HitStatus::MISS);
    trace.request(1, 101, HitStatus::HIT);
    trace.request(1, 101, HitStatus::HIT);
    trace.request(1, 101, HitStatus::HIT);

    const std::vector<int> expected{1};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(LookupUpdateTest, FillsFreeSlotsWithoutEviction)
{
    unit_tests::Trace<LFU<int, int>> trace{3};

    trace.request(1, 101, HitStatus::MISS);
    trace.request(2, 201, HitStatus::MISS);
    trace.request(3, 301, HitStatus::MISS);
    trace.request(3, 301, HitStatus::HIT);
    trace.request(2, 201, HitStatus::HIT);
    trace.request(1, 101, HitStatus::HIT);

    const std::vector<int> expected{1, 2, 3};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(LookupUpdateTest, HitReturnsReferenceToStoredData)
{
    unit_tests::Trace<LFU<int, int>> trace{2};
    const auto first = trace.request(1, 101, HitStatus::MISS);
    const auto second = trace.request(1, 101, HitStatus::HIT);
    trace.request(2, 201, HitStatus::MISS);
    const auto third = trace.request(1, 101, HitStatus::HIT);

    EXPECT_EQ(&first.data, &second.data);
    EXPECT_EQ(&second.data, &third.data);
    EXPECT_EQ(trace.loaded_keys(), (std::vector<int>{1, 2}));
}

TEST(EvictionTest, EvictsLeastFrequentlyUsedKey)
{
    unit_tests::Trace<LFU<int, int>> trace{3};

    trace.request(1, 101, HitStatus::MISS);
    trace.request(2, 201, HitStatus::MISS);
    trace.request(3, 301, HitStatus::MISS);
    trace.request(1, 101, HitStatus::HIT);
    trace.request(3, 301, HitStatus::HIT);
    trace.request(4, 401, HitStatus::MISS); // evicts 2
    trace.request(1, 101, HitStatus::HIT);
    trace.request(3, 301, HitStatus::HIT);
    trace.request(2, 202, HitStatus::MISS); // evicts 4
    trace.request(4, 402, HitStatus::MISS); // evicts 2

    const std::vector<int> expected{1, 2, 3, 4, 2, 4};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(EvictionTest, FrequencyBeatsRecency)
{
    unit_tests::Trace<LFU<int, int>> trace{2};

    trace.request(1, 101, HitStatus::MISS);
    trace.request(1, 101, HitStatus::HIT);
    trace.request(1, 101, HitStatus::HIT);
    trace.request(2, 201, HitStatus::MISS);
    trace.request(3, 301, HitStatus::MISS); // evicts 2, not 1
    trace.request(1, 101, HitStatus::HIT);
    trace.request(2, 202, HitStatus::MISS); // evicts 3

    const std::vector<int> expected{1, 2, 3, 2};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(EvictionTest, TieIsBrokenByLeastRecentlyUsed)
{
    unit_tests::Trace<LFU<int, int>> trace{3};

    trace.request(1, 101, HitStatus::MISS);
    trace.request(2, 201, HitStatus::MISS);
    trace.request(3, 301, HitStatus::MISS);
    trace.request(2, 201, HitStatus::HIT);
    trace.request(1, 101, HitStatus::HIT);
    trace.request(3, 301, HitStatus::HIT);
    trace.request(4, 401, HitStatus::MISS); // evicts 2
    trace.request(1, 101, HitStatus::HIT);
    trace.request(3, 301, HitStatus::HIT);
    trace.request(2, 202, HitStatus::MISS); // evicts 4

    const std::vector<int> expected{1, 2, 3, 4, 2};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(EvictionTest, NewKeysEvictEachOther)
{
    unit_tests::Trace<LFU<int, int>> trace{2};

    trace.request(1, 101, HitStatus::MISS);
    trace.request(1, 101, HitStatus::HIT);
    trace.request(2, 201, HitStatus::MISS);
    trace.request(3, 301, HitStatus::MISS); // evicts 2
    trace.request(2, 202, HitStatus::MISS); // evicts 3
    trace.request(3, 302, HitStatus::MISS); // evicts 2
    trace.request(1, 101, HitStatus::HIT);

    const std::vector<int> expected{1, 2, 3, 2, 3};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(FrequencyTest, PromotionDoesNotSkipFrequency)
{
    unit_tests::Trace<LFU<int, int>> trace{2};

    trace.request(1, 101, HitStatus::MISS);
    trace.request(1, 101, HitStatus::HIT);
    trace.request(1, 101, HitStatus::HIT);
    trace.request(2, 201, HitStatus::MISS);
    trace.request(2, 201, HitStatus::HIT);
    trace.request(3, 301, HitStatus::MISS); // evicts 2 (freq 2 < 3)
    trace.request(1, 101, HitStatus::HIT);
    trace.request(2, 202, HitStatus::MISS); // evicts 3

    const std::vector<int> expected{1, 2, 3, 2};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(FrequencyTest, EvictedKeyStartsFromFrequencyOne)
{
    unit_tests::Trace<LFU<int, int>> trace{2};

    trace.request(1, 101, HitStatus::MISS);
    trace.request(2, 201, HitStatus::MISS);
    trace.request(2, 201, HitStatus::HIT);
    trace.request(2, 201, HitStatus::HIT);
    trace.request(3, 301, HitStatus::MISS); // evicts 1
    trace.request(1, 102, HitStatus::MISS); // evicts 3
    trace.request(1, 102, HitStatus::HIT);
    trace.request(3, 302, HitStatus::MISS); // evicts 1 (freq 2 < 3)
    trace.request(1, 103, HitStatus::MISS); // evicts 3

    const std::vector<int> expected{1, 2, 3, 1, 3, 1};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(TypesTest, WorksWithStringKeysAndValues)
{
    unit_tests::Trace<LFU<std::string, std::string>> trace{2};

    trace.request("a", "a1", HitStatus::MISS);
    trace.request("a", "a1", HitStatus::HIT);
    trace.request("b", "b1", HitStatus::MISS);
    trace.request("c", "c1", HitStatus::MISS);
    trace.request("a", "a1", HitStatus::HIT);
    trace.request("b", "b2", HitStatus::MISS);

    EXPECT_EQ(trace.loaded_keys(), (std::vector<std::string>{"a", "b", "c", "b"}));
}

} // anonymous namespace
