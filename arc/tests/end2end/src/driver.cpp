/*******************************************************************************
 * @file driver.cpp
 * @brief End-to-end testing driver for ARC cache
 *
 * @author usr045
 * @date 2026
 ******************************************************************************/

#include <iostream>

#include "arc.hpp"

namespace cache_user {

using KeyT = int;
using DataT = double;

void slow_get_page(const KeyT& key, DataT& dest)
{ 
    /* cache user implementation */    
    dest = static_cast<DataT>(key); // just example
}

} // namespace cache_user

int main()
{
    using namespace cache;
    using namespace cache_user;
    
    std::size_t cache_size = 0;
    if(!(std::cin >> cache_size))
        throw std::runtime_error("Failed to read cache size");

    ArcCache<KeyT, DataT> arc_cache(cache_size);

    std::size_t reqs_cnt;
    if(!(std::cin >> reqs_cnt))
        throw std::runtime_error("Failed to read request count");

    std::size_t hits = 0;
    KeyT key = 0;

    for(std::size_t i = 0; i < reqs_cnt; ++i) {    

        if(!(std::cin >> key))
            throw std::runtime_error("Failed to read key");

        auto result = arc_cache.lookup_update(key, slow_get_page);
        if(result.hit_) {
            std::cout << "h"; // means cache hit 
            ++hits;
        }
        else
            std::cout << "m"; // means cache miss
    }

    std::cout << "\n" << hits << std::endl;

}
