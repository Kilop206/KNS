file(MAKE_DIRECTORY "${KNS_OUTPUT_DIR}")
foreach(run RANGE 1 2)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env KNS_AUTO_START=1
            "${KNS_EXECUTABLE}" --headless --topology "${KNS_TOPOLOGY}"
            --seed 12345 --packet-size 512 --output "${KNS_OUTPUT_DIR}/${run}.csv"
        RESULT_VARIABLE result OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Configured run failed: ${stdout}${stderr}")
    endif()
endforeach()
file(READ "${KNS_OUTPUT_DIR}/1.csv" first)
file(READ "${KNS_OUTPUT_DIR}/2.csv" second)
if(NOT first STREQUAL second)
    message(FATAL_ERROR "Identical configurations produced different statistics")
endif()

foreach(option seed packet-size)
    foreach(value -1 invalid 18446744073709551616)
        execute_process(
            COMMAND "${KNS_EXECUTABLE}" --headless --topology "${KNS_TOPOLOGY}" "--${option}" "${value}"
            RESULT_VARIABLE result OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr
        )
        if(result EQUAL 0 OR NOT stderr MATCHES "Invalid value for --${option}")
            message(FATAL_ERROR "Invalid ${option} was not rejected: ${stdout}${stderr}")
        endif()
    endforeach()
endforeach()
execute_process(
    COMMAND "${KNS_EXECUTABLE}" --headless --topology "${KNS_TOPOLOGY}" --packet-size 0
    RESULT_VARIABLE result ERROR_VARIABLE stderr
)
if(result EQUAL 0 OR NOT stderr MATCHES "Invalid value for --packet-size")
    message(FATAL_ERROR "Zero packet size was not rejected")
endif()
