set(AB_REFERENCE_ARM_LIBRARY "" CACHE FILEPATH
    "Optional pinned ARMv7 library for isolated target instruction comparisons")
set(AB_REFERENCE_ARM_MATH "" CACHE FILEPATH
    "Optional captured Android 5.1.1 math table for nonzero reference angles")

if(AB_REFERENCE_ARM_LIBRARY)
    find_program(AB_ORACLE_CLANG NAMES clang REQUIRED)
    find_program(AB_ORACLE_LLD NAMES lld
        HINTS /opt/local/libexec/llvm-19/bin REQUIRED)
    find_program(AB_ORACLE_QEMU NAMES qemu-system-arm REQUIRED)

    set(AB_ORACLE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/arm-oracle")
    set(AB_ORACLE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/tools/oracle")
    set(AB_ORACLE_FLAGS -target armv7a-none-eabi -mcpu=cortex-a15 -marm
        -mfpu=neon-vfpv4 -mfloat-abi=softfp -ffreestanding -fno-builtin
        -fno-stack-protector -ffp-contract=off -O2
        -Wall -Wextra -Wconversion -Wshadow -Werror)
    add_custom_command(OUTPUT "${AB_ORACLE_DIRECTORY}/probe.elf"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${AB_ORACLE_DIRECTORY}"
        COMMAND "${AB_ORACLE_CLANG}" ${AB_ORACLE_FLAGS}
            -c "${AB_ORACLE_SOURCE}/arm.c" -o "${AB_ORACLE_DIRECTORY}/arm.o"
        COMMAND "${AB_ORACLE_CLANG}" ${AB_ORACLE_FLAGS}
            -c "${AB_ORACLE_SOURCE}/arm_start.S" -o "${AB_ORACLE_DIRECTORY}/start.o"
        COMMAND "${AB_ORACLE_LLD}" -flavor gnu -T "${AB_ORACLE_SOURCE}/arm.ld"
            "${AB_ORACLE_DIRECTORY}/start.o" "${AB_ORACLE_DIRECTORY}/arm.o"
            -o "${AB_ORACLE_DIRECTORY}/probe.elf"
        DEPENDS "${AB_ORACLE_SOURCE}/arm.c" "${AB_ORACLE_SOURCE}/arm_start.S"
            "${AB_ORACLE_SOURCE}/arm.ld"
        VERBATIM)
    add_custom_target(ab_arm_oracle ALL DEPENDS "${AB_ORACLE_DIRECTORY}/probe.elf")
    function(add_arm_comparison name trace math_table)
        add_test(NAME "${name}"
            COMMAND "${CMAKE_COMMAND}"
                "-DQEMU=${AB_ORACLE_QEMU}"
                "-DPROBE=${AB_ORACLE_DIRECTORY}/probe.elf"
                "-DLIBRARY=${AB_REFERENCE_ARM_LIBRARY}"
                "-DMATH_TABLE=${math_table}"
                "-DTRACE=${AB_ORACLE_DIRECTORY}/${trace}"
                "-DCHECKER=$<TARGET_FILE:ab_physics_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_arm_oracle.cmake")
        set_tests_properties("${name}" PROPERTIES TIMEOUT 30)
    endfunction()
    add_arm_comparison(physics_native_arm_geometry geometry.txt "")
    if(AB_REFERENCE_ARM_MATH)
        add_arm_comparison(physics_native_arm_android_math geometry-android-math.txt
            "${AB_REFERENCE_ARM_MATH}")
    endif()
endif()
