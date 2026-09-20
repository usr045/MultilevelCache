find_package(GTest)
include(GoogleTest)

add_executable(arc_unit_tests
    src/tests.cpp
)
target_link_libraries(arc_unit_tests PRIVATE
    arc
    GTest::gtest_main
)

enable_warnings(arc_unit_tests)

gtest_discover_tests(arc_unit_tests
    TEST_PREFIX "arc.unit."
)
