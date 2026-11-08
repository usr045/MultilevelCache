/*******************************************************************************
 * @file belady.hpp
 * @brief Declaration and implementation of the Belady cache algorithm
 ******************************************************************************/

#pragma once

#include <cstddef>
#include <stdexcept>

#include "access_result.hpp"

namespace cache {

template <typename KeyT, typename DataT>
class Belady {
public:
    explicit Belady(std::size_t capacity) :
        cache_capacity_(capacity)
    {
        if(capacity <= 2)
            throw std::invalid_argument("Belady cache capacity must be "
                                        "greater than 1");
    }

    using key_t = KeyT;
    using data_t = DataT;

    template<typename FuncT>
    void lookup_update(KeyT key) {
        
    }

private:
    std::size_t cache_capacity_;


}; // class Belady

} // namespace cache
