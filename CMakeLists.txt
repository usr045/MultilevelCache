cmake_minimum_required(VERSION 4.4)
project(MultilevelCache)

include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SANITIZE "Enable compiler sanitizers" OFF)

if(SANITIZE)
    if(MSVC)
        add_compile_options()
    else()
        add_compile_options(-fsanitize=address,undefined)
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()

function(enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4 /WX /permissive-
        )
    else()
    target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Weffc++ # -Werror
        )
    endif()
endfunction()

add_executable(multilevelcache
    src/driver.cpp
)

enable_warnings(multilevelcache)
target_include_directories(multilevelcache PRIVATE include)

function(add_cache_lib target)
    add_library(${target} INTERFACE "include/cache/${target}.hpp")
    target_include_directories(${target} INTERFACE include)
endfunction()

add_cache_lib(arc)
# add_cache_lib(lirs)
add_cache_lib(lfu)
add_cache_lib(two_q)

target_link_libraries(multilevelcache PRIVATE arc)
target_link_libraries(multilevelcache PRIVATE two_q)

add_subdirectory(tests/unit)
add_subdirectory(tests/end2end)
