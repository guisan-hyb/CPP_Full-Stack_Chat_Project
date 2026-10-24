if(NOT DEFINED SOURCE_FILE OR NOT DEFINED DESTINATION_DIR)
    message(FATAL_ERROR "SOURCE_FILE and DESTINATION_DIR are required")
endif()

if(NOT EXISTS "${SOURCE_FILE}")
    message(WARNING
        "Config file not found: ${SOURCE_FILE}\n"
        "Copy the matching config.example.ini to config.ini and rebuild.")
    return()
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${DESTINATION_DIR}"
    RESULT_VARIABLE make_result
)
if(NOT make_result EQUAL 0)
    message(FATAL_ERROR "Failed to create config destination: ${DESTINATION_DIR}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${SOURCE_FILE}"
        "${DESTINATION_DIR}"
    RESULT_VARIABLE copy_result
)
if(NOT copy_result EQUAL 0)
    message(FATAL_ERROR "Failed to copy ${SOURCE_FILE} to ${DESTINATION_DIR}")
endif()

message(STATUS "Copied ${SOURCE_FILE} to ${DESTINATION_DIR}")
