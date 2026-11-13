#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "config.hpp"

namespace nhwc_cache {

namespace {

CachePolicy string_to_policy(std::string& str)
{
    if (str == "ARC")
        return CachePolicy::ARC;
    else if (str == "LFU")
        return CachePolicy::LFU;
    else if (str == "LIRS")
        return CachePolicy::LIRS;
    else if (str == "TWO_Q")
        return CachePolicy::TWO_Q;

    throw std::runtime_error("Unknown cache policy");
}

} // anonymous namespace

std::string_view policy_to_string(CachePolicy policy)
{
    switch (policy) {
    case CachePolicy::ARC:
        return "ARC";
    case CachePolicy::LFU:
        return "LFU";
    case CachePolicy::LIRS:
        return "LIRS";
    case CachePolicy::TWO_Q:
        return "TWO_Q";

    default:
        throw std::runtime_error("Unknown policy");
    }
}

void CacheConfig::parse(std::istream& ist)
{
    int num_levels;
    if (!(ist >> num_levels) || num_levels <= 0)
        throw std::runtime_error("Levels must be a positive integer");

    for (auto i = 0; i < num_levels; ++i) {
        std::string token;

        if (!(ist >> token))
            throw std::runtime_error("Fail in reading policy name");
        config_.emplace_back(0, string_to_policy(token));
    }
}

/** @brief Almost uniform distribution. Remainder of the division is
    distributed over the last levels (each level +1 in size) */
void CacheConfig::fill_capacities(int cache_capacity)
{
    int num_levels = config_.size();

    int size = cache_capacity / num_levels;
    int remainder = cache_capacity % num_levels;

    for (auto&& level : config_)
        level.capacity = size;

    auto it = config_.rbegin();
    for (auto i = remainder; i != 0; --i, ++it)
        ++(it->capacity);
}

void CacheConfig::read_config(const char* config_path)
{
    std::ifstream ifst{config_path};
    if (!ifst)
        throw std::runtime_error("File cannot be opened");

    parse(ifst);

    int cache_capacity = 0;
    if (!(std::cin >> cache_capacity) || cache_capacity <= 0)
        throw std::runtime_error("Cache capacity must be a positive integer");

    fill_capacities(cache_capacity);
}

} // namespace nhwc_cache