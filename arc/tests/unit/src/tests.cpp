#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "arc.hpp"

namespace {

using namespace cache;

enum Result { MISS=0, HIT=1 };

template <typename KeyT, typename DataT>
class CacheTrace {
public:
    CacheTrace(std::size_t size) :
        cache_{size}, loaded_keys_{}, loads_per_key_{}, size_(size)
    {}

    const std::vector<KeyT>& loaded_keys() const { return loaded_keys_; }

    void request(KeyT key, const DataT& expected_data, bool expected_hit)
    {
        auto loader = [this](const KeyT& loaded_key, DataT& dest){
            loaded_keys_.push_back(loaded_key);
            dest = loaded_key * 100 + (++loads_per_key_[loaded_key]);
        };

        auto result = cache_.lookup_update(key,  loader);

        EXPECT_EQ(result.hit_, expected_hit);
        // NOTE in DataT must be implemented operator==
        EXPECT_EQ(result.data_, expected_data);
    }

private:
    ArcCache<KeyT, DataT> cache_;

    std::vector<KeyT> loaded_keys_;
    std::unordered_map<KeyT, DataT> loads_per_key_;
    std::size_t size_;

}; // class CacheTrace

TEST(ConstructorTest, AcceptsPositiveCacheSize)
{
    using Cache = ArcCache<int, int>;
    EXPECT_NO_THROW(Cache{2});
    EXPECT_NO_THROW(Cache{15});
}

TEST(ConstructorTest, RejectZeroCacheSize)
{    
    using Cache = ArcCache<int, int>;
    EXPECT_THROW(Cache{0}, std::invalid_argument);
    EXPECT_THROW(Cache{1}, std::invalid_argument);
}

TEST(LookupUpdateTest, AllMisses)
{
    CacheTrace<int, int> trace{2};

    trace.request(1, 101, Result::MISS);
    trace.request(2, 201, Result::MISS);
    trace.request(3, 301, Result::MISS);

    std::vector<int> expected_loaded_keys{1, 2, 3};

    EXPECT_EQ(trace.loaded_keys(), expected_loaded_keys);
}


TEST(TransitionTest, FullT1RemovesOldestKey) {
    CacheTrace<int, int> trace{2};

    trace.request(1, 101, Result::MISS);
    trace.request(2, 201, Result::MISS);
    trace.request(3, 301, Result::MISS);
    trace.request(2, 201, Result::HIT);
    trace.request(1, 102, Result::MISS);
    trace.request(2, 201, Result::HIT);

    const std::vector<int> expected{1, 2, 3, 1};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(TransitionTest, ReturnFromB1ReloadsAndReplacesFromT2) {
    CacheTrace<int, int> trace{2};

    trace.request(1, 101, Result::MISS);
    trace.request(1, 101, Result::HIT);
    trace.request(2, 201, Result::MISS);
    trace.request(3, 301, Result::MISS);
    trace.request(2, 202, Result::MISS);
    trace.request(1, 102, Result::MISS);
    trace.request(2, 202, Result::HIT);

    const std::vector<int> expected{1, 2, 3, 2, 1};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(TransitionTest, ReturnFromB2ReloadsAndAffectsNextVictim) {
    CacheTrace<int, int> trace{2};

    trace.request(1, 101, Result::MISS);
    trace.request(1, 101, Result::HIT);
    trace.request(2, 201, Result::MISS);
    trace.request(2, 201, Result::HIT);
    trace.request(3, 301, Result::MISS);
    trace.request(1, 102, Result::MISS);
    trace.request(4, 401, Result::MISS);
    trace.request(2, 202, Result::MISS);

    const std::vector<int> expected{1, 2, 3, 1, 4, 2};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(TransitionTest, HitInT2UpdatesRecency) {
    CacheTrace<int, int> trace{3};

    trace.request(1, 101, Result::MISS);
    trace.request(1, 101, Result::HIT);
    trace.request(2, 201, Result::MISS);
    trace.request(2, 201, Result::HIT);
    trace.request(3, 301, Result::MISS);
    trace.request(3, 301, Result::HIT);
    trace.request(1, 101, Result::HIT);
    trace.request(4, 401, Result::MISS);
    trace.request(1, 101, Result::HIT);
    trace.request(2, 202, Result::MISS);

    const std::vector<int> expected{1, 2, 3, 4, 2};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(TransitionTest, B2ReturnUsesEqualityCaseInReplace) {
    CacheTrace<int, int> trace{3};

    trace.request(1, 101, Result::MISS);
    trace.request(1, 101, Result::HIT);
    trace.request(2, 201, Result::MISS);
    trace.request(3, 301, Result::MISS);
    trace.request(4, 401, Result::MISS);
    trace.request(2, 202, Result::MISS);
    trace.request(3, 302, Result::MISS);
    trace.request(1, 102, Result::MISS);
    trace.request(2, 202, Result::HIT);

    const std::vector<int> expected{1, 2, 3, 4, 2, 3, 1};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(TransitionTest, RemovesOldestB1Ghost) {
    CacheTrace<int, int> trace{2};

    trace.request(1, 101, Result::MISS);
    trace.request(1, 101, Result::HIT);
    trace.request(2, 201, Result::MISS);
    trace.request(3, 301, Result::MISS);
    trace.request(4, 401, Result::MISS);
    trace.request(2, 202, Result::MISS);
    trace.request(1, 101, Result::HIT);

    const std::vector<int> expected{1, 2, 3, 4, 2};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(TransitionTest, RemovesOldestB2Ghost) {
    CacheTrace<int, int> trace{2};

    trace.request(1, 101, Result::MISS);
    trace.request(1, 101, Result::HIT);
    trace.request(2, 201, Result::MISS);
    trace.request(2, 201, Result::HIT);
    trace.request(3, 301, Result::MISS);
    trace.request(3, 301, Result::HIT);
    trace.request(4, 401, Result::MISS);
    trace.request(5, 501, Result::MISS);
    trace.request(1, 102, Result::MISS);
    trace.request(6, 601, Result::MISS);
    trace.request(3, 301, Result::HIT);

    const std::vector<int> expected{1, 2, 3, 4, 5, 1, 6};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

} // anonymous namespace
