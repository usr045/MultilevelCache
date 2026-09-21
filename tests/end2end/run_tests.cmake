cmake_minimum_required(VERSION 3.20)

# Скрипт ожидает четыре обязательных аргумента:
#
#   PROGRAM       — путь к end-to-end драйверу;
#   ALGORITHM     — имя алгоритма: arc, two_q и т. д.;
#   INPUT_FILE    — файл со входными данными;
#   EXPECTED_FILE — файл с ожидаемым выводом.
#
foreach(variable_name IN ITEMS
    PROGRAM
    ALGORITHM
    INPUT_FILE
    EXPECTED_FILE
)
    if(
        NOT DEFINED ${variable_name}
        OR "${${variable_name}}" STREQUAL ""
    )
        message(FATAL_ERROR
            "Required variable is not specified: ${variable_name}"
        )
    endif()
endforeach()


if(NOT EXISTS "${PROGRAM}")
    message(FATAL_ERROR
        "Test program does not exist:\n"
        "${PROGRAM}"
    )
endif()

if(NOT EXISTS "${INPUT_FILE}")
    message(FATAL_ERROR
        "Input file does not exist:\n"
        "${INPUT_FILE}"
    )
endif()

if(NOT EXISTS "${EXPECTED_FILE}")
    message(FATAL_ERROR
        "Expected output file does not exist:\n"
        "${EXPECTED_FILE}"
    )
endif()


# Запускаем:
#
#   cache_end2end_driver <algorithm>
#
# Содержимое INPUT_FILE передаётся программе через стандартный ввод.
execute_process(
    COMMAND
        "${PROGRAM}"
        "${ALGORITHM}"

    INPUT_FILE
        "${INPUT_FILE}"

    OUTPUT_VARIABLE
        actual_output

    ERROR_VARIABLE
        error_output

    RESULT_VARIABLE
        exit_code

    TIMEOUT
        10
)


# Ненулевой код возврата означает падение тестируемой программы.
if(NOT "${exit_code}" STREQUAL "0")
    message(FATAL_ERROR
        "Test program failed.\n"
        "\n"
        "Algorithm: ${ALGORITHM}\n"
        "Input file: ${INPUT_FILE}\n"
        "Exit code: ${exit_code}\n"
        "\n"
        "--- stderr ---\n"
        "${error_output}"
    )
endif()


file(
    READ
    "${EXPECTED_FILE}"
    expected_output
)


# Нормализация нужна, чтобы тест не зависел от:
#
#   - Windows/Linux-переносов строк;
#   - пробелов в конце строк;
#   - наличия последнего перевода строки.
#
function(normalize_output input_value output_variable)
    set(value "${input_value}")

    # CRLF -> LF
    string(REPLACE "\r\n" "\n" value "${value}")

    # Одиночный CR -> LF
    string(REPLACE "\r" "\n" value "${value}")

    # Удаляем пробелы и табуляцию в конце каждой строки.
    string(
        REGEX REPLACE
        "[ \t]+\n"
        "\n"
        value
        "${value}"
    )

    # Удаляем пробельные символы в начале и конце всего вывода.
    string(STRIP "${value}" value)

    set(
        "${output_variable}"
        "${value}"
        PARENT_SCOPE
    )
endfunction()


normalize_output(
    "${actual_output}"
    actual_output_normalized
)

normalize_output(
    "${expected_output}"
    expected_output_normalized
)


if(
    NOT "${actual_output_normalized}"
    STREQUAL "${expected_output_normalized}"
)
    message(FATAL_ERROR
        "Output mismatch.\n"
        "\n"
        "Algorithm: ${ALGORITHM}\n"
        "Input file: ${INPUT_FILE}\n"
        "Expected file: ${EXPECTED_FILE}\n"
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