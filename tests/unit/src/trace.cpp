#include <string>

#include "trace.hpp"

namespace nhwc_cache::unit_tests {

void load_data(int key, int load_count, int& data)
{
    data = key * 100 + load_count;
}

void load_data(const std::string& key, int load_count, std::string& data)
{
    data = key + std::to_string(load_count);
}

} // namespace nhwc_cache::unit_tests
