add_library(two_q INTERFACE include/two_q.hpp)

target_include_directories(two_q INTERFACE include)

if(BUILD_TESTING)
    add_subdirectory(tests/end2end)
    add_subdirectory(tests/unit)
endif()
