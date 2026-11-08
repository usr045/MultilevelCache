/*******************************************************************************
 * @file cache_trace.hpp
 * @brief Declaration and implementation of the ARC cache algorithm.
 ******************************************************************************/
#pragma once

#include <cstddef>
#include <vector>
#include <unordered_map>

namespace cache::unit_tests {

template <typename CacheT>
class Trace {
public:
    Trace(std::size_t size) :
        cache_{size}, loaded_keys_{}, loads_per_key_{}
    {}

    using KeyT  = CacheT::key_t;
    using DataT = CacheT::data_t;

    const std::vector<KeyT>& loaded_keys() const { return loaded_keys_; }

    void request(KeyT key, const DataT& expected_data, bool expected_hit)
    {
        auto loader = [this](const KeyT& loaded_key, DataT& dest){
            loaded_keys_.push_back(loaded_key);
            dest = loaded_key * 100 + (++loads_per_key_[loaded_key]);
        };

        auto result = cache_.lookup_update(key,  loader);

        EXPECT_EQ(result.status, expected_hit);
        
        // NOTE in DataT must be implemented operator==
        EXPECT_EQ(result.data, expected_data);
    }

private:
    CacheT cache_;

    std::vector<KeyT> loaded_keys_;
    std::unordered_map<KeyT, DataT> loads_per_key_;

}; // class CacheTrace

} // namespace cache
