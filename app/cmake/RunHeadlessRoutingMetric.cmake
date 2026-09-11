set(candidate_direct "${KNS_APP_DIR}/KNS${KNS_EXECUTABLE_SUFFIX}")
set(candidate_debug "${KNS_APP_DIR}/Debug/KNS${KNS_EXECUTABLE_SUFFIX}")
set(candidate_release "${KNS_APP_DIR}/Release/KNS${KNS_EXECUTABLE_SUFFIX}")
set(candidate_relwithdebinfo
    "${KNS_APP_DIR}/RelWithDebInfo/KNS${KNS_EXECUTABLE_SUFFIX}"
)
set(candidate_minsizerel
    "${KNS_APP_DIR}/MinSizeRel/KNS${KNS_EXECUTABLE_SUFFIX}"
)

foreach(candidate IN ITEMS
    "${candidate_direct}"
    "${candidate_debug}"
    "${candidate_release}"
    "${candidate_relwithdebinfo}"
    "${candidate_minsizerel}"
)
    if(EXISTS "${candidate}")
        set(kns_executable "${candidate}")
        break()
    endif()
endforeach()

if(NOT DEFINED kns_executable)
    message(FATAL_ERROR "KNS executable was not found under ${KNS_APP_DIR}")
endif()

set(command
    "${CMAKE_COMMAND}" -E env KNS_AUTO_START=1
    "${kns_executable}"
    --headless
    --topology "${KNS_TOPOLOGY}"
    --routing-metric "${KNS_METRIC}"
)

if(DEFINED KNS_OUTPUT)
    list(APPEND command --output "${KNS_OUTPUT}")
endif()

execute_process(
    COMMAND ${command}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
)

set(process_output "${stdout}${stderr}")

if(KNS_EXPECT_FAILURE)
    if(result EQUAL 0)
        message(FATAL_ERROR "Invalid routing metric unexpectedly succeeded")
    endif()
    if(NOT process_output MATCHES
        "Invalid value for --routing-metric: ${KNS_METRIC}"
    )
        message(FATAL_ERROR
            "Invalid metric error was not reported:\n${process_output}"
        )
    endif()
elseif(NOT result EQUAL 0)
    message(FATAL_ERROR
        "Headless run failed for metric ${KNS_METRIC}:\n${process_output}"
    )
endif()
