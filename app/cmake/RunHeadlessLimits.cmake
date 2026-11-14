file(MAKE_DIRECTORY "${KNS_OUTPUT_DIR}/path with spaces")
foreach(option until max-events)
    foreach(value -1 invalid nan inf 184467440737095516160)
        # Very large finite time is valid; integer overflow is not.
        if(option STREQUAL "until" AND value STREQUAL "184467440737095516160")
            continue()
        endif()
        execute_process(COMMAND "${KNS_EXECUTABLE}" --headless --topology "${KNS_TOPOLOGY}"
            "--${option}" "${value}" RESULT_VARIABLE code ERROR_VARIABLE error TIMEOUT 60)
        if(NOT code EQUAL 1 OR NOT error MATCHES "Invalid value for --${option}")
            message(FATAL_ERROR "Invalid ${option}=${value} accepted: ${code} ${error}")
        endif()
    endforeach()
    execute_process(COMMAND "${KNS_EXECUTABLE}" "--${option}" RESULT_VARIABLE code ERROR_VARIABLE error TIMEOUT 60)
    if(NOT code EQUAL 1 OR NOT error MATCHES "Missing value")
        message(FATAL_ERROR "Missing ${option} value accepted")
    endif()
    execute_process(COMMAND "${KNS_EXECUTABLE}" "--${option}" 0 RESULT_VARIABLE code ERROR_VARIABLE error TIMEOUT 60)
    if(NOT code EQUAL 1 OR NOT error MATCHES "require --headless")
        message(FATAL_ERROR "GUI accepted ${option}")
    endif()
    set(csv "${KNS_OUTPUT_DIR}/path with spaces/${option}.csv")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env KNS_AUTO_START=1
        "${KNS_EXECUTABLE}" --headless --topology "${KNS_TOPOLOGY}"
        "--${option}" 0 --output "${csv}"
        RESULT_VARIABLE code ERROR_VARIABLE error TIMEOUT 60)
    if(NOT code EQUAL 3 OR NOT error MATCHES "\"status\":\"incomplete\"" OR NOT EXISTS "${csv}")
        message(FATAL_ERROR "Bounded ${option} failed to preserve an incomplete snapshot: ${code} ${error}")
    endif()
endforeach()
execute_process(COMMAND "${CMAKE_COMMAND}" -E env KNS_AUTO_START=1
    "${KNS_EXECUTABLE}" --headless --topology "${KNS_TOPOLOGY}"
    --until 1000000 --max-events 1000000 --output "${KNS_OUTPUT_DIR}/complete.csv"
    RESULT_VARIABLE code ERROR_VARIABLE error TIMEOUT 60)
if(NOT code EQUAL 0)
    message(FATAL_ERROR "Generous execution limits did not complete: ${code} ${error}")
endif()
