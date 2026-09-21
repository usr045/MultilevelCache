#pragma once

#include <stdexcept>

namespace cache {

template <typename KeyT, typename DataT>
class LfuCache {
public:
    LfuCache(std::size_t cache_size) : cache_size_(cache_size)
    {
        if(cache_size <= 1) 
            throw std::invalid_argument("");
    }

    struct LookupResult {
        const DataT& data_;
        bool hit_;
        
        LookupResult(const DataT& data, bool hit) :data_(data), hit_(hit) {}
    };

    template <typename FuncT>
    LookupResult lookup_update(const KeyT& key, FuncT& loader)
    {
        
    }

private:
    std::size_t cache_size_;

}; // class Lfu

} // namespace cache