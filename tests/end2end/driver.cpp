#include <iostream>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string_view>

#include "cache/arc.hpp"
// #include "cache/lirs.hpp"
// #include "cache/lfu.hpp"
#include "cache/two_q.hpp"

using namespace cache;

namespace cache_user {


void slow_get_page(const KeyT&, DataT&)
{ 
    // cache user implementation
}

} // namespace cache_user


using KeyT = int;
using DataT = double;

template <template <typename, typename>typename CacheT>
int run_tests()
{    


    std::size_t cache_size = 0;
    if(!(std::cin >> cache_size))
        throw std::runtime_error("Failed to read cache size");

    CacheT<KeyT, DataT> arc_cache(cache_size);

    std::size_t reqs_cnt;
    if(!(std::cin >> reqs_cnt))
        throw std::runtime_error("Failed to read request count");

    std::size_t hits = 0;
    KeyT key = 0;

    for(std::size_t i = 0; i < reqs_cnt; ++i) {    

        if(!(std::cin >> key))
            throw std::runtime_error("Failed to read key");

        auto result = arc_cache.lookup_update(key,  cache_user::slow_get_page);
        if(result.hit_) {
            std::cout << "h"; // cache hit 
            ++hits;
        }
        else
            std::cout << "m"; // cache miss
    }

    std::cout << "\n" << hits << std::endl;
    return 0;
}

int main(int argc, char** argv)
{
    const std::string_view algorithm = argv[1];

    if(algorithm == "arc")
        return run_tests<ArcCache>();

    // else if(algorithm == "lirs")
    //     return run_tests<LirsCache<KeyT, DataT>>();

    // else if(algorithm == "lfu")
    //     return run_tests<LfuCache<KeyT, DataT>>();

    else if(algorithm == "two_q")
        return run_tests<TwoQ>();

    else throw std::runtime_error("Unknown cache algorithm");
}