find_package(GTest)
include(GoogleTest)

function(add_unit_tests target lib_name)
    add_executable(${target}
        "src/${lib_name}_tests.cpp"
    )
    target_link_libraries(${target} PRIVATE
        ${lib_name}
        GTest::gtest_main
    )
    enable_warnings(${target})
    
    gtest_discover_tests(${target}
        TEST_PREFIX "${lib_name}.unit."
    )
endfunction()

add_unit_tests(arc_unit_tests  arc)
# add_unit_tests(lirs_unit_tests lirs)
# add_unit_tests(lfu_unit_tests lfu)
add_unit_tests(two_q_unit_tests two_q)
