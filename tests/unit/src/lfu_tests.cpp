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

TEST(LookupUpdateTest, HitReturnsReferenceToStoredData) //FIXME implement using Trace class
{
    LFU<int, int> cache{2};
    auto loader = [](const int& key, int& dest) { dest = key; };

    const auto first = cache.lookup_update(1, loader);
    const auto second = cache.lookup_update(1, loader);
    cache.lookup_update(2, loader);
    const auto third = cache.lookup_update(1, loader);

    EXPECT_EQ(&first.data, &second.data);
    EXPECT_EQ(&second.data, &third.data);
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

TEST(TypesTest, WorksWithStringKeysAndValues) //FIXME implement using Trace class
{
    LFU<std::string, std::string> cache{2};
    std::vector<std::string> loaded_keys;
    auto loader = [&loaded_keys](const std::string& key, std::string& dest) {
        loaded_keys.push_back(key);
        dest = "page:" + key;
    };

    EXPECT_FALSE(cache.lookup_update("a", loader).status);
    EXPECT_EQ(cache.lookup_update("a", loader).data, "page:a");
    EXPECT_FALSE(cache.lookup_update("b", loader).status);
    EXPECT_FALSE(cache.lookup_update("c", loader).status); // evicts "b"

    const auto result = cache.lookup_update("a", loader);
    EXPECT_TRUE(result.status);
    EXPECT_EQ(result.data, "page:a");

    const std::vector<std::string> expected{"a", "b", "c"};
    EXPECT_EQ(loaded_keys, expected);
}

} // anonymous namespace
