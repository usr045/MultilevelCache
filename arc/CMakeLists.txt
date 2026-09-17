add_library(arc INTERFACE include/arc.hpp)
target_include_directories(arc INTERFACE include)

if(BUILD_TESTING)
    add_subdirectory(tests/end2end)
    # add_subdirectory(tests/unit) - for future
endif()
