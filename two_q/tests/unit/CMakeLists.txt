find_package(GTest)
include(GoogleTest)

add_executable(two_q_unit_tests
    src/tests.cpp
)
target_link_libraries(two_q_unit_tests PRIVATE
    two_q
    GTest::gtest_main
)

enable_warnings(two_q_unit_tests)

gtest_discover_tests(two_q_unit_tests
    TEST_PREFIX "two_q.unit."
)
