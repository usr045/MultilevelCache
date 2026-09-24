#include "cache/lfu.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

using namespace cache;

enum Result { MISS=0, HIT=1 };

template <typename KeyT, typename DataT>
class CacheTrace {
public:
    explicit CacheTrace(std::size_t size) :
        cache_{size}, loaded_keys_{}, loads_per_key_{}
    {}

    const std::vector<KeyT>& loaded_keys() const { return loaded_keys_; }

    void request(KeyT key, const DataT& expected_data, bool expected_hit)
    {
        auto loader = [this](const KeyT& loaded_key, DataT& dest) {
            loaded_keys_.push_back(loaded_key);
            dest = loaded_key * 100 + (++loads_per_key_[loaded_key]);
        };

        auto result = cache_.lookup_update(key, loader);

        EXPECT_EQ(result.hit_, expected_hit) << "key " << key;
        EXPECT_EQ(result.data_, expected_data) << "key " << key;
    }

private:
    LFU<KeyT, DataT> cache_;

    std::vector<KeyT> loaded_keys_;
    std::unordered_map<KeyT, int> loads_per_key_;

}; // class CacheTrace

TEST(ConstructorTest, AcceptsCacheSizeGreaterThanOne)
{
    using Cache = LFU<int, int>;
    EXPECT_NO_THROW(Cache{2});
    EXPECT_NO_THROW(Cache{15});
}

TEST(ConstructorTest, RejectsCacheSizeZeroAndOne)
{
    using Cache = LFU<int, int>;
    EXPECT_THROW(Cache{0}, std::invalid_argument);
    EXPECT_THROW(Cache{1}, std::invalid_argument);
}

TEST(LookupUpdateTest, MissLoadsDataOnce)
{
    CacheTrace<int, int> trace{2};

    trace.request(1, 101, Result::MISS);

    const std::vector<int> expected{1};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(LookupUpdateTest, RepeatedRequestsAreHitsWithoutReload)
{
    CacheTrace<int, int> trace{2};

    trace.request(1, 101, Result::MISS);
    trace.request(1, 101, Result::HIT);
    trace.request(1, 101, Result::HIT);
    trace.request(1, 101, Result::HIT);

    const std::vector<int> expected{1};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(LookupUpdateTest, FillsFreeSlotsWithoutEviction)
{
    CacheTrace<int, int> trace{3};

    trace.request(1, 101, Result::MISS);
    trace.request(2, 201, Result::MISS);
    trace.request(3, 301, Result::MISS);
    trace.request(3, 301, Result::HIT);
    trace.request(2, 201, Result::HIT);
    trace.request(1, 101, Result::HIT);

    const std::vector<int> expected{1, 2, 3};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(LookupUpdateTest, HitReturnsReferenceToStoredData)
{
    LFU<int, int> cache{2};
    auto loader = [](const int& key, int& dest) { dest = key; };

    const auto first = cache.lookup_update(1, loader);
    const auto second = cache.lookup_update(1, loader);
    cache.lookup_update(2, loader);
    const auto third = cache.lookup_update(1, loader);

    EXPECT_EQ(&first.data_, &second.data_);
    EXPECT_EQ(&second.data_, &third.data_);
}

TEST(EvictionTest, EvictsLeastFrequentlyUsedKey)
{
    CacheTrace<int, int> trace{3};

    trace.request(1, 101, Result::MISS);
    trace.request(2, 201, Result::MISS);
    trace.request(3, 301, Result::MISS);
    trace.request(1, 101, Result::HIT);
    trace.request(3, 301, Result::HIT);
    trace.request(4, 401, Result::MISS); // evicts 2
    trace.request(1, 101, Result::HIT);
    trace.request(3, 301, Result::HIT);
    trace.request(2, 202, Result::MISS); // evicts 4
    trace.request(4, 402, Result::MISS); // evicts 2

    const std::vector<int> expected{1, 2, 3, 4, 2, 4};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(EvictionTest, FrequencyBeatsRecency)
{
    CacheTrace<int, int> trace{2};

    trace.request(1, 101, Result::MISS);
    trace.request(1, 101, Result::HIT);
    trace.request(1, 101, Result::HIT);
    trace.request(2, 201, Result::MISS);
    trace.request(3, 301, Result::MISS); // evicts 2, not 1
    trace.request(1, 101, Result::HIT);
    trace.request(2, 202, Result::MISS); // evicts 3

    const std::vector<int> expected{1, 2, 3, 2};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(EvictionTest, TieIsBrokenByLeastRecentlyUsed)
{
    CacheTrace<int, int> trace{3};

    trace.request(1, 101, Result::MISS);
    trace.request(2, 201, Result::MISS);
    trace.request(3, 301, Result::MISS);
    trace.request(2, 201, Result::HIT);
    trace.request(1, 101, Result::HIT);
    trace.request(3, 301, Result::HIT);
    trace.request(4, 401, Result::MISS); // evicts 2
    trace.request(1, 101, Result::HIT);
    trace.request(3, 301, Result::HIT);
    trace.request(2, 202, Result::MISS); // evicts 4

    const std::vector<int> expected{1, 2, 3, 4, 2};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(EvictionTest, NewKeysEvictEachOther)
{
    CacheTrace<int, int> trace{2};

    trace.request(1, 101, Result::MISS);
    trace.request(1, 101, Result::HIT);
    trace.request(2, 201, Result::MISS);
    trace.request(3, 301, Result::MISS); // evicts 2
    trace.request(2, 202, Result::MISS); // evicts 3
    trace.request(3, 302, Result::MISS); // evicts 2
    trace.request(1, 101, Result::HIT);

    const std::vector<int> expected{1, 2, 3, 2, 3};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(FrequencyTest, PromotionDoesNotSkipFrequency)
{
    CacheTrace<int, int> trace{2};

    trace.request(1, 101, Result::MISS);
    trace.request(1, 101, Result::HIT);
    trace.request(1, 101, Result::HIT);
    trace.request(2, 201, Result::MISS);
    trace.request(2, 201, Result::HIT);
    trace.request(3, 301, Result::MISS); // evicts 2 (freq 2 < 3)
    trace.request(1, 101, Result::HIT);
    trace.request(2, 202, Result::MISS); // evicts 3

    const std::vector<int> expected{1, 2, 3, 2};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(FrequencyTest, EvictedKeyStartsFromFrequencyOne)
{
    CacheTrace<int, int> trace{2};

    trace.request(1, 101, Result::MISS);
    trace.request(2, 201, Result::MISS);
    trace.request(2, 201, Result::HIT);
    trace.request(2, 201, Result::HIT);
    trace.request(3, 301, Result::MISS); // evicts 1
    trace.request(1, 102, Result::MISS); // evicts 3
    trace.request(1, 102, Result::HIT);
    trace.request(3, 302, Result::MISS); // evicts 1 (freq 2 < 3)
    trace.request(1, 103, Result::MISS); // evicts 3

    const std::vector<int> expected{1, 2, 3, 1, 3, 1};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(TypesTest, WorksWithStringKeysAndValues)
{
    LFU<std::string, std::string> cache{2};
    std::vector<std::string> loaded_keys;
    auto loader = [&loaded_keys](const std::string& key, std::string& dest) {
        loaded_keys.push_back(key);
        dest = "page:" + key;
    };

    EXPECT_FALSE(cache.lookup_update("a", loader).hit_);
    EXPECT_EQ(cache.lookup_update("a", loader).data_, "page:a");
    EXPECT_FALSE(cache.lookup_update("b", loader).hit_);
    EXPECT_FALSE(cache.lookup_update("c", loader).hit_); // evicts "b"

    const auto result = cache.lookup_update("a", loader);
    EXPECT_TRUE(result.hit_);
    EXPECT_EQ(result.data_, "page:a");

    const std::vector<std::string> expected{"a", "b", "c"};
    EXPECT_EQ(loaded_keys, expected);
}

} // anonymous namespace
