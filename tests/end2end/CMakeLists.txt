set(CACHE_ALGORITHMS
    arc
    two_q
)

set(END2END_TEST_GROUPS
    golden
    generated
)

add_executable(cache_end2end_driver
    driver.cpp
)

target_link_libraries(cache_end2end_driver PRIVATE
    ${CACHE_ALGORITHMS}
)

enable_warnings(cache_end2end_driver)


function(add_cache_end2end_tests cache_name)
    foreach(group_name IN LISTS END2END_TEST_GROUPS)
        set(data_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/data/${cache_name}/${group_name}"
        )

        set(answer_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/answers/${cache_name}/${group_name}"
        )

        # CONFIGURE_DEPENDS заставляет CMake следить за изменением
        # набора файлов, подходящих под шаблон.
        file(
            GLOB input_files
            CONFIGURE_DEPENDS
            "${data_directory}/*.dat"
        )

        foreach(input_file IN LISTS input_files)
            get_filename_component(
                case_name
                "${input_file}"
                NAME_WE
            )

            set(expected_file
                "${answer_directory}/${case_name}.answ"
            )

            # Ошибка обнаруживается на этапе конфигурации CMake,
            # а не во время запуска CTest.
            if(NOT EXISTS "${expected_file}")
                message(FATAL_ERROR
                    "Expected answer is missing.\n"
                    "Algorithm: ${cache_name}\n"
                    "Input file: ${input_file}\n"
                    "Expected file: ${expected_file}"
                )
            endif()

            set(test_name
                "${cache_name}.end2end.${group_name}.${case_name}"
            )

            add_test(
                NAME "${test_name}"

                COMMAND
                    "${CMAKE_COMMAND}"
                    "-DPROGRAM=$<TARGET_FILE:cache_end2end_driver>"
                    "-DALGORITHM=${cache_name}"
                    "-DINPUT_FILE=${input_file}"
                    "-DEXPECTED_FILE=${expected_file}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/run_tests.cmake"
            )

            # Метки позволяют запускать отдельные группы тестов.
            set_tests_properties(
                "${test_name}"
                PROPERTIES
                    LABELS "${cache_name};end2end;${group_name}"
            )
        endforeach()
    endforeach()
endfunction()


foreach(cache_name IN LISTS CACHE_ALGORITHMS)
    add_cache_end2end_tests("${cache_name}")
endforeach()
