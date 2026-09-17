cmake_minimum_required(VERSION 3.20)

foreach(variable_name IN ITEMS
    PROGRAM
    INPUT_FILE
    EXPECTED_FILE
)
    if(NOT DEFINED ${variable_name}
        OR "${${variable_name}}" STREQUAL "")
        message(FATAL_ERROR
            "Required variable is not specified: ${variable_name}"
        )
    endif()
endforeach()

if(NOT EXISTS "${PROGRAM}")
    message(FATAL_ERROR
        "Test program does not exist: ${PROGRAM}"
    )
endif()

if(NOT EXISTS "${INPUT_FILE}")
    message(FATAL_ERROR
        "Input file does not exist: ${INPUT_FILE}"
    )
endif()

if(NOT EXISTS "${EXPECTED_FILE}")
    message(FATAL_ERROR
        "Expected output file does not exist: ${EXPECTED_FILE}"
    )
endif()

execute_process(
    COMMAND "${PROGRAM}"

    INPUT_FILE "${INPUT_FILE}"

    OUTPUT_VARIABLE actual_output
    ERROR_VARIABLE error_output
    RESULT_VARIABLE exit_code

    TIMEOUT 10
)

if(NOT "${exit_code}" STREQUAL "0")
    message(FATAL_ERROR
        "Test program failed\n"
        "Input: ${INPUT_FILE}\n"
        "Exit code: ${exit_code}\n"
        "stderr:\n"
        "${error_output}"
    )
endif()

file(
    READ
    "${EXPECTED_FILE}"
    expected_output
)

function(normalize_output input_value output_variable)
    set(value "${input_value}")

    string(REPLACE "\r\n" "\n" value "${value}")
    string(REPLACE "\r" "\n" value "${value}")
    string(REGEX REPLACE "[ \t]+\n" "\n" value "${value}")
    string(STRIP "${value}" value)

    set("${output_variable}" "${value}" PARENT_SCOPE)
endfunction()

normalize_output("${actual_output}" actual_output_normalized)
normalize_output("${expected_output}" expected_output_normalized)

if(NOT actual_output_normalized
    STREQUAL expected_output_normalized)
    message(FATAL_ERROR
        "Output mismatch\n"
        "Input file: ${INPUT_FILE}\n"
        "\n"
        "--- Expected ---\n"
        "${expected_output_normalized}\n"
        "\n"
        "--- Actual ---\n"
        "${actual_output_normalized}\n"
        "\n"
        "--- stderr ---\n"
        "${error_output}"
    )
endif()
