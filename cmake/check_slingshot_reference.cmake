file(SHA256 "${REFERENCE}" reference_hash)
if(KIND STREQUAL "routing")
    set(expected_hash
        "46e4182c06d5184f83f3946219b704d3e5034896514595409b46dcb9de89f0d0")
elseif(KIND STREQUAL "impulse")
    set(expected_hash
        "1b172a35f25d094a28ad2d88c1778a739c63a12f7c06a5f8367144e810bfb630")
elseif(KIND STREQUAL "drag")
    set(expected_hash
        "7e1cc832960b121e5a02bc874e20862acf6703e1fce96c15f66b851db1021e1c")
elseif(KIND STREQUAL "start")
    set(expected_hash
        "d345da9a720939fb1a4746b70c93d9da7717c73ff35ed3197329b1e3ff1c7ff4")
elseif(KIND STREQUAL "transfer")
    set(expected_hash
        "f8a15e9b5ade1c5905303f9bc76aca45b1db07b74e9dde895a107298fbbd8775")
else()
    set(expected_hash
        "15507639665ac5964d10fa7132768e45f7a34b921c7d98170cc72399532d1a4d")
endif()
if(NOT reference_hash STREQUAL expected_hash)
    message(FATAL_ERROR "Expected the recorded original-Lua slingshot corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Slingshot comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
