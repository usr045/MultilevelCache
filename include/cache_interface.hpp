#pragma once

#include <functional>

#include "access_result.hpp"

namespace nhwc_cache {

template <typename KeyT, typename DataT> class Cache {
public:
    using LoaderT = std::function<void(const KeyT&, DataT&)>;

    virtual AccessResult<DataT> lookup_update(const KeyT&, const LoaderT& loader) = 0;

    virtual ~Cache() = default;

}; // class Cache

} //namespace nhwc_cache
