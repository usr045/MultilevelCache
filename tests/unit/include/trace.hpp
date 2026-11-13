/*******************************************************************************
 * @file trace.hpp
 * @brief Shared cache request assertions and loader history.
 ******************************************************************************/

#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include <gtest/gtest.h>

#include "access_result.hpp"

namespace nhwc_cache::unit_tests {

void load_data(int key, int load_count, int& data);
void load_data(const std::string& key, int load_count, std::string& data);

template <typename CacheT> class Trace {
public:
    using KeyT = typename CacheT::key_t;
    using DataT = typename CacheT::data_t;

    explicit Trace(std::size_t size) : cache_{size} {}

    const std::vector<KeyT>& loaded_keys() const { return loaded_keys_; }

    AccessResult<DataT> request(const KeyT& key, const DataT& expected_data, HitStatus expected_hit)
    {
        Loader loader{*this};

        const auto result = cache_.lookup_update(key, loader);
        EXPECT_EQ(result.status, expected_hit);
        EXPECT_EQ(result.data, expected_data);
        return result;
    }

private:
    struct Loader {
        Trace& trace;

        void operator()(const KeyT& key, DataT& data)
        {
            trace.loaded_keys_.push_back(key);
            load_data(key, ++trace.loads_per_key_[key], data);
        }
    };

    CacheT cache_;
    std::vector<KeyT> loaded_keys_{};
    std::unordered_map<KeyT, int> loads_per_key_{};
};

} // namespace nhwc_cache::unit_tests
