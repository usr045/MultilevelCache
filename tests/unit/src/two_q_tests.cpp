#include "gtest/gtest.h"
#include <stdexcept>

#include "cache/two_q.hpp"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

using namespace cache;

using Cache = TwoQ<int, int>;

namespace {

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
    TwoQ<KeyT, DataT> cache_;

    std::vector<KeyT> loaded_keys_;
    std::unordered_map<KeyT, int> loads_per_key_;

}; // class CacheTrace


void fill_am(CacheTrace<int, int>& trace)
{
    trace.request(1, 101, Result::MISS);
    trace.request(2, 201, Result::MISS);
    trace.request(3, 301, Result::MISS);
    trace.request(4, 401, Result::MISS);
    trace.request(5, 501, Result::MISS); // 1 -> A1out
    trace.request(6, 601, Result::MISS); // 2 -> A1out
    trace.request(1, 102, Result::MISS); // ghost hit: 1 -> Am
    trace.request(2, 202, Result::MISS); // ghost hit: 2 -> Am
    trace.request(1, 102, Result::HIT);  // Am = [1, 2]
    trace.request(7, 701, Result::MISS);
    trace.request(8, 801, Result::MISS);
    trace.request(5, 502, Result::MISS); // ghost hit: Am = [5, 1, 2]
}

TEST(ConstructorTest, AcceptsCacheSizeGreaterThanOne)
{
    EXPECT_NO_THROW(Cache{2});
    EXPECT_NO_THROW(Cache{15});
}
TEST(ConstructorTest, RejectsCacheSizeZeroAndOne)
{
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

TEST(LookupUpdateTest, FillsFreeSlotsWithoutEviction)
{
    CacheTrace<int, int> trace{4};

    trace.request(1, 101, Result::MISS);
    trace.request(2, 201, Result::MISS);
    trace.request(3, 301, Result::MISS);
    trace.request(4, 401, Result::MISS);
    trace.request(4, 401, Result::HIT);
    trace.request(3, 301, Result::HIT);
    trace.request(2, 201, Result::HIT);
    trace.request(1, 101, Result::HIT);

    const std::vector<int> expected{1, 2, 3, 4};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(A1inTest, EvictsInFifoOrder)
{
    CacheTrace<int, int> trace{4};

    trace.request(1, 101, Result::MISS);
    trace.request(2, 201, Result::MISS);
    trace.request(3, 301, Result::MISS);
    trace.request(4, 401, Result::MISS);
    trace.request(1, 101, Result::HIT);
    trace.request(2, 201, Result::HIT);
    trace.request(5, 501, Result::MISS); // 1 -> A1out
    trace.request(3, 301, Result::HIT);
    trace.request(1, 102, Result::MISS); // ghost hit

    const std::vector<int> expected{1, 2, 3, 4, 5, 1};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(A1inTest, HitInA1inDoesNotPromoteToAm)
{
    CacheTrace<int, int> trace{4};

    trace.request(1, 101, Result::MISS);
    trace.request(1, 101, Result::HIT);
    trace.request(2, 201, Result::MISS);
    trace.request(3, 301, Result::MISS);
    trace.request(4, 401, Result::MISS);
    trace.request(5, 501, Result::MISS); // 1 -> A1out
    trace.request(6, 601, Result::MISS); // 2 -> A1out
    trace.request(1, 102, Result::MISS); // ghost hit

    const std::vector<int> expected{1, 2, 3, 4, 5, 6, 1};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(A1outTest, GhostHitReloadsDataAndCountsAsMiss)
{
    CacheTrace<int, int> trace{4};

    trace.request(1, 101, Result::MISS);
    trace.request(2, 201, Result::MISS);
    trace.request(3, 301, Result::MISS);
    trace.request(4, 401, Result::MISS);
    trace.request(5, 501, Result::MISS); // 1 -> A1out
    trace.request(1, 102, Result::MISS); // ghost hit: 1 -> Am
    trace.request(1, 102, Result::HIT);

    const std::vector<int> expected{1, 2, 3, 4, 5, 1};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(A1outTest, KeyReturnedFromGhostSurvivesScan)
{
    CacheTrace<int, int> trace{4};

    trace.request(1, 101, Result::MISS);
    trace.request(2, 201, Result::MISS);
    trace.request(3, 301, Result::MISS);
    trace.request(4, 401, Result::MISS);
    trace.request(5, 501, Result::MISS); // 1 -> A1out
    trace.request(1, 102, Result::MISS); // ghost hit: 1 -> Am
    trace.request(6, 601, Result::MISS);
    trace.request(7, 701, Result::MISS);
    trace.request(8, 801, Result::MISS);
    trace.request(9, 901, Result::MISS);
    trace.request(10, 1001, Result::MISS);
    trace.request(1, 102, Result::HIT);

    const std::vector<int> expected{1, 2, 3, 4, 5, 1, 6, 7, 8, 9, 10};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(A1outTest, GhostListIsBoundedByKout)
{
    CacheTrace<int, int> trace{4};

    trace.request(1, 101, Result::MISS);
    trace.request(2, 201, Result::MISS);
    trace.request(3, 301, Result::MISS);
    trace.request(4, 401, Result::MISS);
    trace.request(5, 501, Result::MISS); // A1out = [1]
    trace.request(6, 601, Result::MISS); // A1out = [2, 1]
    trace.request(7, 701, Result::MISS); // A1out = [3, 2], 1 is forgotten
    trace.request(1, 102, Result::MISS); // plain miss: 1 -> A1in
    trace.request(8, 801, Result::MISS);
    trace.request(9, 901, Result::MISS);
    trace.request(10, 1001, Result::MISS);
    trace.request(11, 1101, Result::MISS); // 1 -> A1out
    trace.request(1, 103, Result::MISS);   // ghost hit

    const std::vector<int> expected{1, 2, 3, 4, 5, 6, 7, 1, 8, 9, 10, 11, 1};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(AmTest, HitInAmMovesKeyToMru)
{
    CacheTrace<int, int> trace{4};
    fill_am(trace);

    trace.request(9, 901, Result::MISS); // evicts 2 from Am
    trace.request(1, 102, Result::HIT);
    trace.request(2, 203, Result::MISS);

    const std::vector<int> expected{1, 2, 3, 4, 5, 6, 1, 2, 7, 8, 5, 9, 2};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(AmTest, AmVictimIsNotRememberedAsGhost)
{
    CacheTrace<int, int> trace{4};
    fill_am(trace);

    trace.request(9, 901, Result::MISS);   // evicts 2 from Am
    trace.request(2, 203, Result::MISS);   // plain miss: 2 -> A1in
    trace.request(1, 102, Result::HIT);
    trace.request(10, 1001, Result::MISS);
    trace.request(11, 1101, Result::MISS); // 2 -> A1out
    trace.request(2, 204, Result::MISS);   // ghost hit

    const std::vector<int> expected{1, 2, 3, 4, 5, 6, 1, 2, 7, 8, 5, 9, 2, 10, 11, 2};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(TypesTest, WorksWithStringKeysAndValues)
{
    TwoQ<std::string, std::string> cache{2};
    std::vector<std::string> loaded_keys;
    auto loader = [&loaded_keys](const std::string& key, std::string& dest) {
        loaded_keys.push_back(key);
        dest = "page:" + key;
    };

    EXPECT_FALSE(cache.lookup_update("a", loader).hit_);
    EXPECT_EQ(cache.lookup_update("a", loader).data_, "page:a");
    EXPECT_FALSE(cache.lookup_update("b", loader).hit_);
    EXPECT_FALSE(cache.lookup_update("c", loader).hit_); // "a" -> A1out
    EXPECT_TRUE(cache.lookup_update("b", loader).hit_);

    const auto result = cache.lookup_update("a", loader);
    EXPECT_FALSE(result.hit_);
    EXPECT_EQ(result.data_, "page:a");

    const std::vector<std::string> expected{"a", "b", "c", "a"};
    EXPECT_EQ(loaded_keys, expected);
}

} // anonymous namespace
