#include <gtest/gtest.h>

#include "access_result.hpp"
#include "cache/lirs.hpp"
#include "trace.hpp"

namespace {

using namespace cache;

TEST(ConstructorTest, RightSize)
{
    using LIRS = LIRS<int, int>;

    EXPECT_NO_THROW(LIRS{2});
}

TEST(ConstructorTest, WrongSize)
{
    using LIRS = LIRS<int, int>;

    EXPECT_THROW(LIRS{0}, std::invalid_argument);
    EXPECT_THROW(LIRS{1}, std::invalid_argument);
}

} // anonymous namespace
