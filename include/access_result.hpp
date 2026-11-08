/*******************************************************************************
 * @file access_result.hpp
 * @brief Accessing result on each request class implementation.
 ******************************************************************************/

#pragma once

namespace cache {

enum HitStatus : bool {MISS=false, HIT=true};

template <typename DataT>
struct AccessResult {
    const DataT& data;
    HitStatus status;
};

} // namespace cache
