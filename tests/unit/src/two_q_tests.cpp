#include "gtest/gtest.h"
#include <stdexcept>

#include <string>
#include <vector>

#include "access_result.hpp"
#include "cache/two_q.hpp"
#include "trace.hpp"

namespace {

using namespace cache;

void fill_am(unit_tests::Trace<TwoQ<int, int>>& trace)
{
    trace.request(1, 101, HitStatus::MISS);
    trace.request(2, 201, HitStatus::MISS);
    trace.request(3, 301, HitStatus::MISS);
    trace.request(4, 401, HitStatus::MISS);
    trace.request(5, 501, HitStatus::MISS); // 1 -> A1out
    trace.request(6, 601, HitStatus::MISS); // 2 -> A1out
    trace.request(1, 102, HitStatus::MISS); // ghost hit: 1 -> Am
    trace.request(2, 202, HitStatus::MISS); // ghost hit: 2 -> Am
    trace.request(1, 102, HitStatus::HIT);  // Am = [1, 2]
    trace.request(7, 701, HitStatus::MISS);
    trace.request(8, 801, HitStatus::MISS);
    trace.request(5, 502, HitStatus::MISS); // ghost hit: Am = [5, 1, 2]
}

TEST(ConstructorTest, AcceptsCacheSizeGreaterThanOne)
{
    using TwoQ = TwoQ<int, int>;

    EXPECT_NO_THROW(TwoQ{2});
    EXPECT_NO_THROW(TwoQ{15});
}
TEST(ConstructorTest, RejectsCacheSizeZeroAndOne)
{
    using TwoQ = TwoQ<int, int>;

    EXPECT_THROW(TwoQ{0}, std::invalid_argument);
    EXPECT_THROW(TwoQ{1}, std::invalid_argument);
}

TEST(LookupUpdateTest, MissLoadsDataOnce)
{
    unit_tests::Trace<TwoQ<int, int>> trace{2};

    trace.request(1, 101, HitStatus::MISS);

    const std::vector<int> expected{1};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(LookupUpdateTest, FillsFreeSlotsWithoutEviction)
{
    unit_tests::Trace<TwoQ<int, int>> trace{4};

    trace.request(1, 101, HitStatus::MISS);
    trace.request(2, 201, HitStatus::MISS);
    trace.request(3, 301, HitStatus::MISS);
    trace.request(4, 401, HitStatus::MISS);
    trace.request(4, 401, HitStatus::HIT);
    trace.request(3, 301, HitStatus::HIT);
    trace.request(2, 201, HitStatus::HIT);
    trace.request(1, 101, HitStatus::HIT);

    const std::vector<int> expected{1, 2, 3, 4};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(A1inTest, EvictsInFifoOrder)
{
    unit_tests::Trace<TwoQ<int, int>> trace{4};

    trace.request(1, 101, HitStatus::MISS);
    trace.request(2, 201, HitStatus::MISS);
    trace.request(3, 301, HitStatus::MISS);
    trace.request(4, 401, HitStatus::MISS);
    trace.request(1, 101, HitStatus::HIT);
    trace.request(2, 201, HitStatus::HIT);
    trace.request(5, 501, HitStatus::MISS); // 1 -> A1out
    trace.request(3, 301, HitStatus::HIT);
    trace.request(1, 102, HitStatus::MISS); // ghost hit

    const std::vector<int> expected{1, 2, 3, 4, 5, 1};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(A1inTest, HitInA1inDoesNotPromoteToAm)
{
    unit_tests::Trace<TwoQ<int, int>> trace{4};

    trace.request(1, 101, HitStatus::MISS);
    trace.request(1, 101, HitStatus::HIT);
    trace.request(2, 201, HitStatus::MISS);
    trace.request(3, 301, HitStatus::MISS);
    trace.request(4, 401, HitStatus::MISS);
    trace.request(5, 501, HitStatus::MISS); // 1 -> A1out
    trace.request(6, 601, HitStatus::MISS); // 2 -> A1out
    trace.request(1, 102, HitStatus::MISS); // ghost hit

    const std::vector<int> expected{1, 2, 3, 4, 5, 6, 1};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(A1outTest, GhostHitReloadsDataAndCountsAsMiss)
{
    unit_tests::Trace<TwoQ<int, int>> trace{4};

    trace.request(1, 101, HitStatus::MISS);
    trace.request(2, 201, HitStatus::MISS);
    trace.request(3, 301, HitStatus::MISS);
    trace.request(4, 401, HitStatus::MISS);
    trace.request(5, 501, HitStatus::MISS); // 1 -> A1out
    trace.request(1, 102, HitStatus::MISS); // ghost hit: 1 -> Am
    trace.request(1, 102, HitStatus::HIT);

    const std::vector<int> expected{1, 2, 3, 4, 5, 1};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(A1outTest, KeyReturnedFromGhostSurvivesScan)
{
    unit_tests::Trace<TwoQ<int, int>> trace{4};

    trace.request(1, 101, HitStatus::MISS);
    trace.request(2, 201, HitStatus::MISS);
    trace.request(3, 301, HitStatus::MISS);
    trace.request(4, 401, HitStatus::MISS);
    trace.request(5, 501, HitStatus::MISS); // 1 -> A1out
    trace.request(1, 102, HitStatus::MISS); // ghost hit: 1 -> Am
    trace.request(6, 601, HitStatus::MISS);
    trace.request(7, 701, HitStatus::MISS);
    trace.request(8, 801, HitStatus::MISS);
    trace.request(9, 901, HitStatus::MISS);
    trace.request(10, 1001, HitStatus::MISS);
    trace.request(1, 102, HitStatus::HIT);

    const std::vector<int> expected{1, 2, 3, 4, 5, 1, 6, 7, 8, 9, 10};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(A1outTest, GhostListIsBoundedByKout)
{
    unit_tests::Trace<TwoQ<int, int>> trace{4};

    trace.request(1, 101, HitStatus::MISS);
    trace.request(2, 201, HitStatus::MISS);
    trace.request(3, 301, HitStatus::MISS);
    trace.request(4, 401, HitStatus::MISS);
    trace.request(5, 501, HitStatus::MISS); // A1out = [1]
    trace.request(6, 601, HitStatus::MISS); // A1out = [2, 1]
    trace.request(7, 701, HitStatus::MISS); // A1out = [3, 2], 1 is forgotten
    trace.request(1, 102, HitStatus::MISS); // plain miss: 1 -> A1in
    trace.request(8, 801, HitStatus::MISS);
    trace.request(9, 901, HitStatus::MISS);
    trace.request(10, 1001, HitStatus::MISS);
    trace.request(11, 1101, HitStatus::MISS); // 1 -> A1out
    trace.request(1, 103, HitStatus::MISS);   // ghost hit

    const std::vector<int> expected{1, 2, 3, 4, 5, 6, 7, 1, 8, 9, 10, 11, 1};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(AmTest, HitInAmMovesKeyToMru)
{
    unit_tests::Trace<TwoQ<int, int>> trace{4};
    fill_am(trace);

    trace.request(9, 901, HitStatus::MISS); // evicts 2 from Am
    trace.request(1, 102, HitStatus::HIT);
    trace.request(2, 203, HitStatus::MISS);

    const std::vector<int> expected{1, 2, 3, 4, 5, 6, 1, 2, 7, 8, 5, 9, 2};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(AmTest, AmVictimIsNotRememberedAsGhost)
{
    unit_tests::Trace<TwoQ<int, int>> trace{4};
    fill_am(trace);

    trace.request(9, 901, HitStatus::MISS); // evicts 2 from Am
    trace.request(2, 203, HitStatus::MISS); // plain miss: 2 -> A1in
    trace.request(1, 102, HitStatus::HIT);
    trace.request(10, 1001, HitStatus::MISS);
    trace.request(11, 1101, HitStatus::MISS); // 2 -> A1out
    trace.request(2, 204, HitStatus::MISS);   // ghost hit

    const std::vector<int> expected{1, 2, 3, 4, 5, 6, 1, 2, 7, 8, 5, 9, 2, 10, 11, 2};
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(TypesTest, WorksWithStringKeysAndValues)
{
    unit_tests::Trace<TwoQ<std::string, std::string>> trace{2};

    trace.request("a", "a1", HitStatus::MISS);
    trace.request("a", "a1", HitStatus::HIT);
    trace.request("b", "b1", HitStatus::MISS);
    trace.request("c", "c1", HitStatus::MISS);
    trace.request("b", "b1", HitStatus::HIT);
    trace.request("a", "a2", HitStatus::MISS);

    EXPECT_EQ(trace.loaded_keys(), (std::vector<std::string>{"a", "b", "c", "a"}));
}

} // anonymous namespace

#ifdef SANITIZE
int a = 4;
#endif
