/*******************************************************************************
 * @file belady.hpp
 * @brief Placeholder for the Belady cache.
 ******************************************************************************/

#pragma once

#include <cstddef>
#include <functional>
#include <stdexcept>

#include "access_result.hpp"
#include "cache_interface.hpp"

namespace nhwc_cache {

/**
 * @class Belady
 * @brief Placeholder for the Belady optimal replacement cache.
 * @tparam KeyT Cache key type.
 * @tparam DataT Cached data type.
 */
template <typename KeyT, typename DataT> class Belady final : public Cache<KeyT, DataT> {
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

    using LoaderT = std::function<void(const KeyT&, DataT&)>;

    AccessResult<DataT> lookup_update(const KeyT& key, const LoaderT& loader) override {}

private:
    std::size_t cache_capacity_;

}; // class Belady

} // namespace nhwc_cache
