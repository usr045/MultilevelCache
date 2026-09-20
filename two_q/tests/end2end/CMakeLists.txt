add_executable(two_q_end2end_tests
    src/driver.cpp
)

target_link_libraries(two_q_end2end_tests PRIVATE two_q)

enable_warnings(two_q_end2end_tests)
