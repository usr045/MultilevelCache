#include "config.hpp"
#include "system.hpp"

int main(int argc, char** argv)
{
    using namespace nhwc_cache;

    CacheConfig info{};
    info.read_config(argv[1]);

    using KeyT = int;
    using DataT = int;

    CacheSystem<KeyT, DataT> cache_system(info.config());
}
