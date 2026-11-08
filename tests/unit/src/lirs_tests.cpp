#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "access_result.hpp"
#include "cache/lirs.hpp"
#include "trace.hpp"

namespace {

using namespace cache;
using IntTrace = unit_tests::Trace<LIRS<int, int>>;

TEST(ConstructorTest, AcceptsCacheSizeGreaterThanOne)
{
    using Cache = LIRS<int, int>;
    EXPECT_NO_THROW(Cache{2});
    EXPECT_NO_THROW(Cache{200});
}

TEST(ConstructorTest, RejectsCacheSizeZeroAndOne)
{
    using Cache = LIRS<int, int>;
    EXPECT_THROW(Cache{0}, std::invalid_argument);
    EXPECT_THROW(Cache{1}, std::invalid_argument);
}

TEST(ConstructorTest, RejectsStackCapacityOverflow)
{
    using Cache = LIRS<int, int>;
    constexpr auto maximum = std::numeric_limits<std::size_t>::max();
    EXPECT_THROW(Cache{maximum / 2 + 1}, std::invalid_argument);
    EXPECT_THROW(Cache{maximum}, std::invalid_argument);
}

TEST(LookupUpdateTest, MissLoadsDataOnceAndHitsDoNotReload)
{
    IntTrace trace{2};
    EXPECT_TRUE(trace.loaded_keys().empty());
    trace.request(1, 101, HitStatus::MISS);
    trace.request(1, 101, HitStatus::HIT);
    trace.request(1, 101, HitStatus::HIT);
    EXPECT_EQ(trace.loaded_keys(), (std::vector<int>{1}));
}

TEST(LookupUpdateTest, FillsAllResidentSlots)
{
    IntTrace trace{3};
    trace.request(1, 101, HitStatus::MISS);
    trace.request(2, 201, HitStatus::MISS);
    trace.request(3, 301, HitStatus::MISS);
    trace.request(3, 301, HitStatus::HIT);
    trace.request(2, 201, HitStatus::HIT);
    trace.request(1, 101, HitStatus::HIT);
    EXPECT_EQ(trace.loaded_keys(), (std::vector<int>{1, 2, 3}));
}

TEST(LookupUpdateTest, HitReturnsReferenceToStoredData)
{
    IntTrace trace{3};
    const auto first = trace.request(1, 101, HitStatus::MISS);
    const auto second = trace.request(1, 101, HitStatus::HIT);
    trace.request(2, 201, HitStatus::MISS);
    const auto third = trace.request(1, 101, HitStatus::HIT);
    EXPECT_EQ(&first.data, &second.data);
    EXPECT_EQ(&second.data, &third.data);
    EXPECT_EQ(trace.loaded_keys(), (std::vector<int>{1, 2}));
}

TEST(EvictionTest, ScanPreservesLirEntriesAndBoundsGhostHistory)
{
    IntTrace trace{3};
    std::vector<int> expected;
    for(int key = 1; key <= 32; ++key) {
        trace.request(key, key * 100 + 1, HitStatus::MISS);
        expected.push_back(key);
    }
    trace.request(1, 101, HitStatus::HIT);
    trace.request(2, 201, HitStatus::HIT);
    trace.request(3, 302, HitStatus::MISS);
    expected.push_back(3);
    trace.request(33, 3301, HitStatus::MISS);
    trace.request(3, 303, HitStatus::MISS); // Forgotten 3 stayed HIR and was evicted again.
    trace.request(1, 101, HitStatus::HIT);
    trace.request(2, 201, HitStatus::HIT);
    expected.push_back(33);
    expected.push_back(3);
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(TransitionTest, ResidentStackHirHitPromotesAndDemotesOldestLir)
{
    IntTrace trace{3};
    trace.request(1, 101, HitStatus::MISS);
    trace.request(2, 201, HitStatus::MISS);
    trace.request(3, 301, HitStatus::MISS);
    trace.request(3, 301, HitStatus::HIT); // 3 becomes LIR; 1 becomes HIR.
    trace.request(4, 401, HitStatus::MISS); // Evicts 1.
    trace.request(2, 201, HitStatus::HIT);
    trace.request(3, 301, HitStatus::HIT);
    trace.request(1, 102, HitStatus::MISS);
    EXPECT_EQ(trace.loaded_keys(), (std::vector<int>{1, 2, 3, 4, 1}));
}

TEST(TransitionTest, GhostHitReloadsDataAndPromotesToLir)
{
    IntTrace trace{3};
    trace.request(1, 101, HitStatus::MISS);
    trace.request(2, 201, HitStatus::MISS);
    trace.request(3, 301, HitStatus::MISS);
    trace.request(4, 401, HitStatus::MISS); // 3 becomes a ghost.
    trace.request(3, 302, HitStatus::MISS); // Reloads 3 as LIR; demotes 1.
    trace.request(5, 501, HitStatus::MISS); // Evicts 1, preserving 3.
    trace.request(3, 302, HitStatus::HIT);
    trace.request(2, 201, HitStatus::HIT);
    trace.request(1, 102, HitStatus::MISS);
    EXPECT_EQ(trace.loaded_keys(), (std::vector<int>{1, 2, 3, 4, 3, 5, 1}));
}

TEST(TransitionTest, NonStackHirRequiresAnotherHitBeforePromotion)
{
    IntTrace trace{3};
    trace.request(1, 101, HitStatus::MISS);
    trace.request(2, 201, HitStatus::MISS);
    trace.request(3, 301, HitStatus::MISS);
    trace.request(1, 101, HitStatus::HIT);
    trace.request(2, 201, HitStatus::HIT); // Prunes resident HIR 3 from the stack.
    trace.request(3, 301, HitStatus::HIT); // Restores stack membership, still HIR.
    trace.request(4, 401, HitStatus::MISS); // Evicts 3.
    trace.request(3, 302, HitStatus::MISS);
    EXPECT_EQ(trace.loaded_keys(), (std::vector<int>{1, 2, 3, 4, 3}));
}

TEST(TransitionTest, SecondNonStackHirHitPromotesToLir)
{
    IntTrace trace{3};
    trace.request(1, 101, HitStatus::MISS);
    trace.request(2, 201, HitStatus::MISS);
    trace.request(3, 301, HitStatus::MISS);
    trace.request(1, 101, HitStatus::HIT);
    trace.request(2, 201, HitStatus::HIT);
    trace.request(3, 301, HitStatus::HIT);
    trace.request(3, 301, HitStatus::HIT); // Now 3 is LIR and 1 is HIR.
    trace.request(4, 401, HitStatus::MISS);
    trace.request(3, 301, HitStatus::HIT);
    trace.request(1, 102, HitStatus::MISS);
    EXPECT_EQ(trace.loaded_keys(), (std::vector<int>{1, 2, 3, 4, 1}));
}

TEST(TransitionTest, PrunedGhostReturnsAsNewHir)
{
    IntTrace trace{3};
    trace.request(1, 101, HitStatus::MISS);
    trace.request(2, 201, HitStatus::MISS);
    trace.request(3, 301, HitStatus::MISS);
    trace.request(4, 401, HitStatus::MISS);
    trace.request(1, 101, HitStatus::HIT);
    trace.request(2, 201, HitStatus::HIT); // Forgets ghost 3 during stack pruning.
    trace.request(3, 302, HitStatus::MISS); // New HIR, not a ghost promotion.
    trace.request(5, 501, HitStatus::MISS); // Evicts 3 again.
    trace.request(3, 303, HitStatus::MISS);
    EXPECT_EQ(trace.loaded_keys(), (std::vector<int>{1, 2, 3, 4, 3, 5, 3}));
}

TEST(EvictionTest, NonStackHirHitRefreshesQueueRecency)
{
    IntTrace trace{200}; // Two HIR slots.
    std::vector<int> expected;
    for(int key = 1; key <= 200; ++key) {
        trace.request(key, key * 100 + 1, HitStatus::MISS);
        expected.push_back(key);
    }
    for(int key = 1; key <= 198; ++key)
        trace.request(key, key * 100 + 1, HitStatus::HIT);
    trace.request(199, 19901, HitStatus::HIT);
    trace.request(201, 20101, HitStatus::MISS); // Evicts 200 rather than 199.
    trace.request(199, 19901, HitStatus::HIT);
    trace.request(200, 20002, HitStatus::MISS);
    expected.push_back(201);
    expected.push_back(200);
    EXPECT_EQ(trace.loaded_keys(), expected);
}

TEST(TypesTest, WorksWithStringKeysAndValues)
{
    unit_tests::Trace<LIRS<std::string, std::string>> trace{2};
    trace.request("a", "a1", HitStatus::MISS);
    trace.request("b", "b1", HitStatus::MISS);
    trace.request("c", "c1", HitStatus::MISS);
    trace.request("b", "b2", HitStatus::MISS);
    trace.request("b", "b2", HitStatus::HIT);
    trace.request("d", "d1", HitStatus::MISS); // Evicts demoted HIR a.
    trace.request("a", "a2", HitStatus::MISS);
    EXPECT_EQ(trace.loaded_keys(), (std::vector<std::string>{"a", "b", "c", "b", "d", "a"}));
}

} // anonymous namespace
