if(NOT DEFINED AB_BUILD_DIRECTORY)
    message(FATAL_ERROR "Set AB_BUILD_DIRECTORY to the configured CI build directory.")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${AB_BUILD_DIRECTORY}"
        --config Release --parallel 4
    RESULT_VARIABLE build_result
    OUTPUT_VARIABLE build_output
    ERROR_VARIABLE build_error
    ECHO_OUTPUT_VARIABLE
    ECHO_ERROR_VARIABLE)

file(WRITE "${AB_BUILD_DIRECTORY}/ci-build.log" "${build_output}${build_error}")
if(NOT build_result STREQUAL "0")
    # Keep compiler diagnostics available in Actions annotations as well as the
    # full job log. Escape command data before publishing an error message.
    string(REGEX MATCHALL "[^\r\n]*([Ee]rror(:| [A-Z]+[0-9]+| at )|[Ww]arning C[0-9]+|[Uu]ndefined (symbols|reference))[^\r\n]*"
        diagnostics "${build_output}\n${build_error}")
    list(REMOVE_DUPLICATES diagnostics)
    set(count 0)
    foreach(diagnostic IN LISTS diagnostics)
        if(diagnostic MATCHES "error C2220:")
            continue()
        endif()
        if(count GREATER_EQUAL 20)
            break()
        endif()
        string(SUBSTRING "${diagnostic}" 0 3000 diagnostic)
        string(REPLACE "%" "%25" diagnostic "${diagnostic}")
        string(REPLACE "\r" "%0D" diagnostic "${diagnostic}")
        string(REPLACE "\n" "%0A" diagnostic "${diagnostic}")
        message("::error::${diagnostic}")
        math(EXPR count "${count} + 1")
    endforeach()
    message(FATAL_ERROR "CI build failed: ${build_result}")
endif()
