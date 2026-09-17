cmake_minimum_required(VERSION 4.4)
project(MultilevelCache)

include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

add_subdirectory(arc)
target_link_libraries(multilevelcache PRIVATE arc)
