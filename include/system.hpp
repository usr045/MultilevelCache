#pragma once

#include <memory>
#include <stdexcept>

#include "config.hpp"

#include "cache/arc.hpp"
#include "cache/lfu.hpp"
#include "cache/lirs.hpp"
#include "cache/two_q.hpp"

namespace nhwc_cache {

template <typename KeyT, typename DataT> class CacheSystem {
public:
    CacheSystem(const std::vector<CacheConfig::LevelInfo>& config)
    {

        std::size_t num_levels = config.size();
        levels_.reserve(num_levels);

        for (auto&& info : config) {
            levels_.emplace_back(make_level(info.capacity, info.policy));
        }
    }

private:
    std::vector<std::unique_ptr<Cache<KeyT, DataT>>> levels_;

    std::unique_ptr<Cache<KeyT, DataT>> make_level(int capacity, CachePolicy policy)
    {
        switch (policy) {
        case CachePolicy::ARC: {
            return std::make_unique<ARC<KeyT, DataT>>(capacity);
        }
        case CachePolicy::LFU: {
            return std::make_unique<LFU<KeyT, DataT>>(capacity);
        }
        case CachePolicy::LIRS: {
            return std::make_unique<LIRS<KeyT, DataT>>(capacity);
        }
        case CachePolicy::TWO_Q: {
            return std::make_unique<TwoQ<KeyT, DataT>>(capacity);
        }

        default:
            throw std::runtime_error("Unknown cache policy");
        }
    }

}; // class CacheSystem

} // namespace nhwc_cache