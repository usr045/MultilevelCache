/*******************************************************************************
 * @file access_result.hpp
 * @brief Shared cache access result and hit status.
 ******************************************************************************/

#pragma once

namespace cache {

/** @brief Whether the requested data was resident before the lookup. */
enum HitStatus : bool { MISS = false, HIT = true };

/**
 * @brief Non-owning result of a cache lookup.
 * @tparam DataT Type of data owned by the cache.
 * @note data remains valid until its entry is evicted or the cache is destroyed.
 *       Copying this result does not copy data or extend its lifetime.
 */
template <typename DataT> struct AccessResult {
    const DataT& data; // Read-only reference to cache-owned data.
    HitStatus status;  // HIT for resident data; MISS when loading was required.
};

} // namespace cache
