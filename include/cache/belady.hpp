/*******************************************************************************
 * @file belady.hpp
 * @brief Placeholder for the Belady cache.
 ******************************************************************************/

#pragma once

#include <cstddef>
#include <stdexcept>

#include "access_result.hpp"

namespace cache {

/**
 * @class Belady
 * @brief Placeholder for the Belady optimal replacement cache.
 * @tparam KeyT Cache key type.
 * @tparam DataT Cached data type.
 */
template <typename KeyT, typename DataT> class Belady {
public:
    /**
     * @brief Creates a cache with the given resident capacity.
     * @param capacity Maximum number of resident entries.
     * @throws std::invalid_argument If capacity <= 2.
     */
    explicit Belady(std::size_t capacity) : cache_capacity_(capacity)
    {
        if (capacity <= 2)
            throw std::invalid_argument("Belady cache capacity must be "
                                        "greater than 1");
    }

    using key_t = KeyT;
    using data_t = DataT;

    /** @brief Unimplemented request placeholder; performs no lookup or loading. */
    template <typename FuncT> void lookup_update(KeyT key) {}

private:
    std::size_t cache_capacity_;

}; // class Belady

} // namespace cache
