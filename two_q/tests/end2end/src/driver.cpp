/*******************************************************************************
 * @file driver.cpp
 * @brief End-to-end testing driver for Two Q cache
 ******************************************************************************/

#include <iostream>

#include "two_q.hpp"

namespace cache_user {

using KeyT = int;
using DataT = int;

void slow_get_page(const KeyT&, DataT&)
{
    // cache user implementation
}

} // namespace cache_user

int main()
{
    using namespace cache;
    using namespace cache_user;

    std::size_t cache_size = 0;
    if(!(std::cin >> cache_size))   
        throw std::runtime_error("");

    TwoQ<int, int> two_q_cache{cache_size};

    std::size_t reqs_cnt = 0;
    if(!std::cin >> reqs_cnt)
        throw std::runtime_error("");

    std::size_t hits = 0;
    KeyT key = 0;
    
    for(std::size_t i = 0; i < reqs_cnt; ++i) {
        
        if(!(std::cin >> key))
            throw std::runtime_error("");

        auto result = two_q_cache.lookup_update(key, slow_get_page);
        if(result.hit_) {
            std::cout << "h"; // cache hit
            ++hits;
        }

        else
            std::cout << "m"; // cache miss
    }

    std::cout << "\n" << hits << std::endl;
}
