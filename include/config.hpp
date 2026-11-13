#pragma once

#include <iostream>
#include <vector>

/* 
 * Config format:
 *      1 'policy A'
 *          or
 *      2 'policy A' 'policy B'
 *          or
 *      3 'policy A' 'policy B' 'policy C'
 *          etc.
 */

namespace nhwc_cache {

enum class CachePolicy { ARC, LFU, LIRS, TWO_Q };

class CacheConfig final {
public:
    struct LevelInfo {
        int capacity;
        CachePolicy policy;
    };

    void read_config(const char* config_path);

    std::vector<LevelInfo> config() const noexcept { return config_; }

private:
    std::vector<LevelInfo> config_;

    void parse(std::istream& ist);
    void fill_capacities(const int cache_capacity);
};

std::string_view policy_to_string(CachePolicy policy);

} // namespace nhwc_cache
