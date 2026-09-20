#include "gtest/gtest.h"
#include <stdexcept>

#include "two_q.hpp"

using namespace cache;

using Cache = TwoQ<int, int>;

TEST(Constructor, aaaa)
{
    EXPECT_NO_THROW(Cache{2});
    EXPECT_NO_THROW(Cache{15});
}
TEST(Constructor, bbbbb)
{
    EXPECT_THROW(Cache{0}, std::invalid_argument);
    EXPECT_THROW(Cache{1}, std::invalid_argument);
}

