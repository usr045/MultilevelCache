#include <iostream>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string_view>

#include "cache/arc.hpp"
// #include "cache/lirs.hpp"
#include "cache/lfu.hpp"
#include "cache/two_q.hpp"

using namespace cache;

using KeyT = int;
using DataT = int;

namespace cache_user {

void slow_get_page(const KeyT&, DataT&)
{ 
    // cache user implementation
}

} // namespace cache_user

template <typename CacheT, typename FuncT>
int run_tests(FuncT& loader)
{    
    std::size_t cache_size = 0;
    if(!(std::cin >> cache_size))
        throw std::runtime_error("Failed to read cache size");

    using KeyT = typename CacheT::key_t;

    CacheT cache(cache_size);

    std::size_t reqs_cnt;
    if(!(std::cin >> reqs_cnt))
        throw std::runtime_error("Failed to read request count");

    std::size_t hits = 0;
    KeyT key = 0;

    for(std::size_t i = 0; i < reqs_cnt; ++i) {    

        if(!(std::cin >> key))
            throw std::runtime_error("Failed to read key");

        auto result = cache.lookup_update(key, loader);
        if(result.hit_) {
            std::cout << "h"; // cache hit 
            ++hits;

            // DEBUG
            
        }
        else
            std::cout << "m"; // cache miss
    }

    std::cout << "\n" << hits << std::endl;
    return 0;
}

int main(int argc, char** argv)
{

    if(argc != 2)
        throw std::runtime_error("Wrong usage: arc_cache_driver <algorithm's name>");

    const std::string_view algorithm = argv[1];

    if(algorithm == "arc")
        return run_tests<ARC<KeyT, DataT>>(cache_user::slow_get_page);

    // else if(algorithm == "lirs")
        // return run_tests<LirsCache<KeyT, DataT>>(cache_user::slow_get_page);   

    else if(algorithm == "lfu")
        return run_tests<LFU<KeyT, DataT>>(cache_user::slow_get_page);

    else if(algorithm == "two_q")
        return run_tests<TwoQ<KeyT, DataT>>(cache_user::slow_get_page);

    else throw std::runtime_error("Unknown cache algorithm");
}