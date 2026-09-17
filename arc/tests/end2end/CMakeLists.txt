add_executable(arc_end2end_driver
    src/driver.cpp
)

target_link_libraries(arc_end2end_driver PRIVATE arc)
enable_warnings(arc_end2end_driver)

function(add_arc_test_group group_name data_dir answer_dir)
    file(
        GLOB input_files
        CONFIGURE_DEPENDS
        "${data_dir}/*.dat"
    )

    foreach(input_file IN LISTS input_files)
        get_filename_component(case_name "${input_file}" NAME_WE)

        set(expected_file "${answer_dir}/${case_name}.answ")

        if(NOT EXISTS "${expected_file}")
            message(FATAL_ERROR
                "Answer for ${input_file} is missing: ${expected_file}"
            )
        endif()

        add_test(
            NAME arc.end2end.${group_name}.${case_name}
            COMMAND "${CMAKE_COMMAND}"
                "-DPROGRAM=$<TARGET_FILE:arc_end2end_driver>"
                "-DINPUT_FILE=${input_file}"
                "-DEXPECTED_FILE=${expected_file}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_tests.cmake"
        )
    endforeach()
endfunction()

add_arc_test_group(
    golden
    "${CMAKE_CURRENT_SOURCE_DIR}/data/golden"
    "${CMAKE_CURRENT_SOURCE_DIR}/answ/golden"
)

add_arc_test_group(
    generated
    "${CMAKE_CURRENT_SOURCE_DIR}/data/generated"
    "${CMAKE_CURRENT_SOURCE_DIR}/answ/generated"
)
