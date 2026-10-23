cmake_minimum_required(VERSION 3.22)

foreach(required AB_ANDROID_LIBM AB_ANDROID_MATH_OUTPUT_DIR)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Supply ${required} explicitly; this script never downloads tools")
    endif()
endforeach()

find_program(AB_ANDROID_CLANG NAMES clang REQUIRED)
find_program(AB_ANDROID_LLD NAMES ld.lld lld
    HINTS /opt/local/libexec/llvm-19/bin REQUIRED)
file(SHA256 "${AB_ANDROID_LIBM}" library_hash)
if(NOT library_hash STREQUAL
    "213211c76e1ee56c5281dcbf94af0318babd120a01cd4600106cb4d6c08463f5")
    message(FATAL_ERROR "Expected the recorded Android 5.1.1 ARM reference libm")
endif()
get_filename_component(platform_directory "${AB_ANDROID_LIBM}" DIRECTORY)
set(android_libdl "${platform_directory}/libdl.so")
file(SHA256 "${android_libdl}" loader_hash)
if(NOT loader_hash STREQUAL
    "1aab4607fb7d2463c32a188e69ea786b0acdcacc04eb3660ad6d973ea56d8b45")
    message(FATAL_ERROR "Expected the recorded Android 5.1.1 ARM dynamic loader interface")
endif()

get_filename_component(source_directory "${CMAKE_CURRENT_LIST_DIR}/../tools/oracle" ABSOLUTE)
get_filename_component(private_directory "${CMAKE_CURRENT_LIST_DIR}/../Files" REALPATH)
get_filename_component(output_directory "${AB_ANDROID_MATH_OUTPUT_DIR}" ABSOLUTE)
cmake_path(IS_PREFIX private_directory "${output_directory}" NORMALIZE private_output)
if(NOT private_output)
    message(FATAL_ERROR "Keep Android reference outputs under the repository Files directory")
endif()
file(MAKE_DIRECTORY "${output_directory}")

function(run_checked)
    execute_process(COMMAND ${ARGV} RESULT_VARIABLE status
        OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "Android reference build failed: ${output}${diagnostics}")
    endif()
endfunction()

set(arm_options -target armv7a-linux-androideabi22 -marm -fPIC)
set(c_options -mcpu=cortex-a15
    -mfpu=neon-vfpv4 -mfloat-abi=softfp -ffreestanding -fno-builtin
    -fno-stack-protector -fomit-frame-pointer -ffp-contract=off -std=c11 -O2
    -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror)

set(android_probe_built FALSE)
function(build_android_probe name source entry)
    if(DEFINED AB_ANDROID_PROBE AND NOT name STREQUAL AB_ANDROID_PROBE)
        return()
    endif()
    run_checked("${AB_ANDROID_CLANG}" ${arm_options} ${c_options} ${ARGN}
        -c "${source_directory}/${source}" -o "${output_directory}/${name}.o")
    run_checked("${AB_ANDROID_CLANG}" ${arm_options} "-DAB_ANDROID_ENTRY=${entry}"
        -c "${source_directory}/android_start.S"
        -o "${output_directory}/${name}_start.o")
    run_checked("${AB_ANDROID_LLD}" -flavor gnu -pie -e _start --hash-style=sysv
        --dynamic-linker=/system/bin/linker --allow-shlib-undefined
        -o "${output_directory}/${name}" "${output_directory}/${name}_start.o"
        "${output_directory}/${name}.o" "${AB_ANDROID_LIBM}" "${android_libdl}")
    set(android_probe_built TRUE PARENT_SCOPE)
endfunction()

build_android_probe(ab_android_math android_math.c android_math_main)
build_android_probe(ab_android_game_hud_queries arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_GAME_HUD_DRAW_ONLY=1 -DAB_ORACLE_GAME_HUD_QUERIES_ONLY=1)
build_android_probe(ab_android_native_events arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_NATIVE_EVENTS_ONLY=1)
build_android_probe(ab_android_root arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_ROOT_ONLY=1)
build_android_probe(ab_android_audio_ramp arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_AUDIO_RAMP_ONLY=1)
build_android_probe(ab_android_url_requests arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_URL_REQUESTS_ONLY=1)
build_android_probe(ab_android_calendar_time arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_CALENDAR_TIME_ONLY=1)
build_android_probe(ab_android_calendar_parts arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_CALENDAR_TIME_ONLY=1
    -DAB_ORACLE_CALENDAR_PARTS_ONLY=1)
build_android_probe(ab_android_ui_slider arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_UI_SLIDER_ONLY=1)
build_android_probe(ab_android_session_timer arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_SESSION_TIMER_ONLY=1)
build_android_probe(ab_android_anger arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_ANGER_ONLY=1)
build_android_probe(ab_android_root_queries arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_ROOT_QUERIES_ONLY=1)
build_android_probe(ab_android_gui_queries arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_GUI_QUERIES_ONLY=1)
build_android_probe(ab_android_level_start_flow arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_LEVEL_START_FLOW_ONLY=1)
build_android_probe(ab_android_museum_pause arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_MUSEUM_PAUSE_ONLY=1)
build_android_probe(ab_android_game_mode arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_GAME_MODE_ONLY=1)
build_android_probe(ab_android_reload_geometry arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_RELOAD_GEOMETRY_ONLY=1)
build_android_probe(ab_android_create_circle arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_CREATE_CIRCLE_ONLY=1)
build_android_probe(ab_android_blues_child arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_BLUES_CHILD_ONLY=1)
build_android_probe(ab_android_blues_split arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_BLUES_SPLIT_ONLY=1)
build_android_probe(ab_android_world_iteration arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_WORLD_ITERATION_ONLY=1)
build_android_probe(ab_android_game_draw arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_GAME_DRAW_ONLY=1)
build_android_probe(ab_android_process_manager arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_PROCESS_MANAGER_ONLY=1)
build_android_probe(ab_android_loader arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_LOADER_ONLY=1)
build_android_probe(ab_android_mp3 arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_MP3_ONLY=1)
build_android_probe(ab_android_mp3_gain arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_MP3_GAIN_ONLY=1)
build_android_probe(ab_android_mp3_dct36 arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_MP3_DCT36_ONLY=1)
build_android_probe(ab_android_mp3_dct12 arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_MP3_DCT12_ONLY=1)
build_android_probe(ab_android_mp3_windows arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_MP3_WINDOWS_ONLY=1)
build_android_probe(ab_android_joint_geometry arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_JOINT_GEOMETRY_ONLY=1)
build_android_probe(ab_android_matrix_inverse arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_MATRIX_INVERSE_ONLY=1)
build_android_probe(ab_android_matrix_solve arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_MATRIX_SOLVE_ONLY=1)
build_android_probe(ab_android_weld_initialize arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_WELD_INITIALIZE_ONLY=1)
build_android_probe(ab_android_weld_velocity arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_WELD_INITIALIZE_ONLY=1
    -DAB_ORACLE_WELD_VELOCITY=1)
build_android_probe(ab_android_weld_position arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_WELD_INITIALIZE_ONLY=1
    -DAB_ORACLE_WELD_POSITION=1)
build_android_probe(ab_android_distance_joint arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_DISTANCE_JOINT_ONLY=1)
build_android_probe(ab_android_revolute_initialize arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_REVOLUTE_INITIALIZE_ONLY=1)
build_android_probe(ab_android_revolute_velocity arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_REVOLUTE_INITIALIZE_ONLY=1
    -DAB_ORACLE_REVOLUTE_VELOCITY=1)
build_android_probe(ab_android_revolute_position arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_REVOLUTE_INITIALIZE_ONLY=1
    -DAB_ORACLE_REVOLUTE_POSITION=1)
build_android_probe(ab_android_prismatic_initialize arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_PRISMATIC_INITIALIZE_ONLY=1)
build_android_probe(ab_android_prismatic_velocity arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_PRISMATIC_INITIALIZE_ONLY=1
    -DAB_ORACLE_PRISMATIC_VELOCITY=1)
build_android_probe(ab_android_prismatic_position arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_PRISMATIC_INITIALIZE_ONLY=1
    -DAB_ORACLE_PRISMATIC_POSITION=1)
build_android_probe(ab_android_joint_island arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_JOINT_ISLAND_ONLY=1)
build_android_probe(ab_android_world_joint arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_WORLD_JOINT_ONLY=1)
build_android_probe(ab_android_mp3_side arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_MP3_SIDE_ONLY=1)
build_android_probe(ab_android_mp3_pristine arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_MP3_SIDE_ONLY=1
    -DAB_ORACLE_MP3_SCALEFACTORS=1 -DAB_ORACLE_MP3_PRISTINE=1)
build_android_probe(ab_android_mp3_reservoir arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_MP3_SIDE_ONLY=1 -DAB_ORACLE_MP3_RESERVOIR=1)
build_android_probe(ab_android_mp3_scalefactors arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_MP3_SIDE_ONLY=1 -DAB_ORACLE_MP3_SCALEFACTORS=1)
build_android_probe(ab_android_mp3_huffman arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_MP3_SIDE_ONLY=1 -DAB_ORACLE_MP3_HUFFMAN=1)
build_android_probe(ab_android_mp3_spectrum arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_MP3_SIDE_ONLY=1 -DAB_ORACLE_MP3_SPECTRUM=1)
build_android_probe(ab_android_mp3_requantize arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_MP3_SIDE_ONLY=1 -DAB_ORACLE_MP3_REQUANTIZE=1)
build_android_probe(ab_android_audio_mixer arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_AUDIO_MIXER_ONLY=1)
build_android_probe(ab_android_audio_mixer_extended arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_AUDIO_MIXER_ONLY=1
    -DAB_ORACLE_AUDIO_MIXER_EXTENDED=1)
build_android_probe(ab_android_audio_mixer_multiple arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_AUDIO_MIXER_ONLY=1
    -DAB_ORACLE_AUDIO_MIXER_EXTENDED=1 -DAB_ORACLE_AUDIO_MIXER_MULTIPLE=1)
build_android_probe(ab_android_audio_mixer_state arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_AUDIO_MIXER_STATE_ONLY=1)
build_android_probe(ab_android_audio_mixer_state_extended arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_AUDIO_MIXER_STATE_ONLY=1
    -DAB_ORACLE_AUDIO_MIXER_STATE_EXTENDED=1)
build_android_probe(ab_android_audio_mixer_clips arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_AUDIO_MIXER_STATE_ONLY=1
    -DAB_ORACLE_AUDIO_MIXER_CLIPS=1)
build_android_probe(ab_android_math_extended android_math.c android_math_main
    -DAB_MATH_EXTENDED=1)
build_android_probe(ab_android_angle android_math.c android_math_main
    -DAB_MATH_COMPONENTS=1)
build_android_probe(ab_android_script_angle android_math.c android_math_main
    -DAB_MATH_COMPONENTS=1 -DAB_MATH_SCRIPT_ANGLE=1)
build_android_probe(ab_android_oracle arm.c oracle_main -DAB_ORACLE_ANDROID=1)
build_android_probe(ab_android_lua_data android_lua_data.c android_lua_data_main)
build_android_probe(ab_android_lua_write android_lua_write.c android_lua_write_main)
build_android_probe(ab_android_sprite_registry android_sprite_registry.c
    android_sprite_registry_main)
build_android_probe(ab_android_resource_owners android_sprite_registry.c
    android_sprite_registry_main -DAB_NATIVE_RESOURCE_OWNERSHIP=1)
build_android_probe(ab_android_resource_cache android_sprite_registry.c
    android_sprite_registry_main -DAB_NATIVE_RESOURCE_OWNERSHIP=1
    -DAB_NATIVE_RESOURCE_CACHE=1)
build_android_probe(ab_android_resource_path android_resource_path.c
    android_resource_path_main)
build_android_probe(ab_android_composite_part android_composite_part.c
    android_composite_part_main)
build_android_probe(ab_android_webp_alpha android_webp_alpha.c android_webp_alpha_main)
build_android_probe(ab_android_webp_images android_webp_images.c android_webp_images_main)
build_android_probe(ab_android_webp_yuv android_webp_images.c android_webp_images_main
    -DAB_WEBP_YUV=1)
build_android_probe(ab_android_webp_upsample android_webp_upsample.c
    android_webp_upsample_main)
build_android_probe(ab_android_webp_bits android_webp_bits.c android_webp_bits_main)
build_android_probe(ab_android_vp8_boolean android_vp8_boolean.c android_vp8_boolean_main)
build_android_probe(ab_android_vp8_header android_vp8_header.c android_vp8_header_main)
build_android_probe(ab_android_vp8_modes android_vp8_header.c android_vp8_header_main
    -DAB_VP8_MODES=1)
build_android_probe(ab_android_vp8_coefficients android_vp8_coefficients.c
    android_vp8_coefficients_main)
build_android_probe(ab_android_vp8_residuals android_vp8_header.c android_vp8_header_main
    -DAB_VP8_MODES=1 -DAB_VP8_RESIDUALS=1)
build_android_probe(ab_android_vp8_transform android_vp8_transform.c
    android_vp8_transform_main)
build_android_probe(ab_android_vp8_predict android_vp8_predict.c android_vp8_predict_main)
build_android_probe(ab_android_vp8_filter android_vp8_filter.c android_vp8_filter_main)
build_android_probe(ab_android_vp8_unfiltered android_vp8_planes.c android_vp8_planes_main)
build_android_probe(ab_android_island arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_ISLAND_ONLY=1)
build_android_probe(ab_android_contact_island arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_ISLAND_ONLY=1 -DAB_ORACLE_CONTACT_ISLAND=1)
build_android_probe(ab_android_tree arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_TREE_ONLY=1)
build_android_probe(ab_android_broadphase arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BROADPHASE_ONLY=1)
build_android_probe(ab_android_body_setup arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BODY_SETUP_ONLY=1)
build_android_probe(ab_android_body_velocity arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BODY_VELOCITY_ONLY=1)
build_android_probe(ab_android_world_clear arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_WORLD_CLEAR_ONLY=1)
build_android_probe(ab_android_object_friction arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_OBJECT_FRICTION_ONLY=1)
build_android_probe(ab_android_object_forces arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_OBJECT_FORCES_ONLY=1)
build_android_probe(ab_android_object_forces_relative arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_OBJECT_FORCES_ONLY=1 -DAB_ORACLE_OBJECT_FORCES_RELATIVE=1)
build_android_probe(ab_android_object_forces_interval arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_OBJECT_FORCES_ONLY=1 -DAB_ORACLE_OBJECT_FORCES_INTERVAL=1)
build_android_probe(ab_android_object_forces_audio arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_OBJECT_FORCES_ONLY=1 -DAB_ORACLE_OBJECT_FORCES_INTERVAL=1
    -DAB_ORACLE_OBJECT_FORCES_AUDIO=1)
build_android_probe(ab_android_body_angular arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BODY_VELOCITY_ONLY=1 -DAB_ORACLE_BODY_ANGULAR=1)
build_android_probe(ab_android_contact_state arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_CONTACT_STATE_ONLY=1)
build_android_probe(ab_android_fixture arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_FIXTURE_ONLY=1)
build_android_probe(ab_android_world_solve arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_WORLD_SOLVE_ONLY=1)
build_android_probe(ab_android_distance arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_DISTANCE_ONLY=1)
build_android_probe(ab_android_contact_report arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_ISLAND_ONLY=1 -DAB_ORACLE_CONTACT_ISLAND=1
    -DAB_ORACLE_ISLAND_REPORT=1)
build_android_probe(ab_android_world_report arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_WORLD_SOLVE_ONLY=1 -DAB_ORACLE_WORLD_REPORT=1)
build_android_probe(ab_android_world_steps arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_WORLD_STEPS_ONLY=1)
build_android_probe(ab_android_world_raycast arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_WORLD_RAYCAST_ONLY=1)
build_android_probe(ab_android_world_events arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_WORLD_STEPS_ONLY=1 -DAB_ORACLE_WORLD_EVENTS=1)
build_android_probe(ab_android_world_continuous arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_WORLD_STEPS_ONLY=1 -DAB_ORACLE_WORLD_CONTINUOUS=1)
build_android_probe(ab_android_world_continuous_events arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_WORLD_STEPS_ONLY=1
    -DAB_ORACLE_WORLD_CONTINUOUS=1 -DAB_ORACLE_WORLD_EVENTS=1)
build_android_probe(ab_android_world_rejected_impacts arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_WORLD_STEPS_ONLY=1
    -DAB_ORACLE_WORLD_CONTINUOUS=1 -DAB_ORACLE_WORLD_EVENTS=1
    -DAB_ORACLE_WORLD_MUTATIONS=1)
build_android_probe(ab_android_world_operations arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_WORLD_STEPS_ONLY=1
    -DAB_ORACLE_WORLD_CONTINUOUS=1 -DAB_ORACLE_WORLD_EVENTS=1
    -DAB_ORACLE_WORLD_OPERATIONS=1)
build_android_probe(ab_android_world_lifecycle arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_WORLD_STEPS_ONLY=1
    -DAB_ORACLE_WORLD_CONTINUOUS=1 -DAB_ORACLE_WORLD_EVENTS=1
    -DAB_ORACLE_WORLD_LIFECYCLE=1)
build_android_probe(ab_android_world_joint_lifecycle arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_WORLD_STEPS_ONLY=1
    -DAB_ORACLE_WORLD_CONTINUOUS=1 -DAB_ORACLE_WORLD_EVENTS=1
    -DAB_ORACLE_WORLD_LIFECYCLE=1 -DAB_ORACLE_WORLD_JOINT_LIFECYCLE=1)
build_android_probe(ab_android_game_joint arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_GAME_JOINT_ONLY=1)
build_android_probe(ab_android_game_polygon arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_GAME_POLYGON_ONLY=1)
build_android_probe(ab_android_game_circle arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_GAME_POLYGON_ONLY=1 -DAB_ORACLE_GAME_CIRCLE=1)
build_android_probe(ab_android_game_circle_zeros arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_GAME_POLYGON_ONLY=1 -DAB_ORACLE_GAME_CIRCLE=1
    -DAB_ORACLE_GAME_CIRCLE_ZEROS=1)
build_android_probe(ab_android_game_convex arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_GAME_POLYGON_ONLY=1 -DAB_ORACLE_GAME_CONVEX=1)
build_android_probe(ab_android_object_missing arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_OBJECT_MISSING_ONLY=1)
build_android_probe(ab_android_asset_profile arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_ASSET_PROFILE_ONLY=1)
build_android_probe(ab_android_asset_splash arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_ASSET_SPLASH_ONLY=1)
build_android_probe(ab_android_asset_profile_bundles arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_ASSET_PROFILE_BUNDLES_ONLY=1)
build_android_probe(ab_android_asset_group arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_ASSET_GROUP_ONLY=1)
build_android_probe(ab_android_asset_bundle arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_ASSET_BUNDLE_ONLY=1)
build_android_probe(ab_android_theme_asset arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_THEME_ASSET_ONLY=1)
build_android_probe(ab_android_initial_sprite_material arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_INITIAL_SPRITE_MATERIAL_ONLY=1)
build_android_probe(ab_android_game_joint_zeros arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_GAME_JOINT_ONLY=1 -DAB_ORACLE_GAME_JOINT_ZEROS=1)
build_android_probe(ab_android_game_joint_limits arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_GAME_JOINT_ONLY=1 -DAB_ORACLE_GAME_JOINT_LIMITS=1)
build_android_probe(ab_android_game_joint_parameters arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_GAME_JOINT_ONLY=1 -DAB_ORACLE_GAME_JOINT_PARAMETERS=1)
build_android_probe(ab_android_game_joint_break arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_GAME_JOINT_BREAK_ONLY=1)
build_android_probe(ab_android_game_joint_named arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_GAME_JOINT_NAMED_ONLY=1)
build_android_probe(ab_android_contact_glue arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_GAME_CONTACT_ONLY=1 -DAB_ORACLE_GAME_CONTACT_GLUE=1)
build_android_probe(ab_android_game_glue arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_GAME_JOINT_ONLY=1 -DAB_ORACLE_GAME_GLUE=1)
build_android_probe(ab_android_game_glue_policy arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_GAME_JOINT_ONLY=1 -DAB_ORACLE_GAME_GLUE=1
    -DAB_ORACLE_GAME_GLUE_POLICY=1)
build_android_probe(ab_android_separation arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_SEPARATION_ONLY=1)
build_android_probe(ab_android_impact arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_IMPACT_ONLY=1)
build_android_probe(ab_android_toi_position arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_TOI_POSITION_ONLY=1)
build_android_probe(ab_android_toi_island arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_ISLAND_ONLY=1 -DAB_ORACLE_CONTACT_ISLAND=1
    -DAB_ORACLE_ISLAND_REPORT=1 -DAB_ORACLE_TOI_ISLAND=1)
build_android_probe(ab_android_game_collision arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_GAME_COLLISION_ONLY=1)
build_android_probe(ab_android_game_rules arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_GAME_RULES_ONLY=1)
build_android_probe(ab_android_game_contact_routes arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_GAME_CONTACT_ROUTES_ONLY=1)
build_android_probe(ab_android_game_contact_dispatch arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_GAME_CONTACT_DISPATCH_ONLY=1)
build_android_probe(ab_android_game_angle arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_GAME_ANGLE_ONLY=1)
build_android_probe(ab_android_block_damage arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1)
build_android_probe(ab_android_game_contact arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_GAME_CONTACT_ONLY=1)
build_android_probe(ab_android_game_contact_end arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_GAME_CONTACT_ONLY=1 -DAB_ORACLE_GAME_CONTACT_END=1)
build_android_probe(ab_android_game_contact_world arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_GAME_CONTACT_WORLD_ONLY=1)
build_android_probe(ab_android_score arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_SCORE_ONLY=1)
build_android_probe(ab_android_level_flow arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_LEVEL_FLOW_ONLY=1)
build_android_probe(ab_android_level_bonus arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_LEVEL_BONUS_ONLY=1)
build_android_probe(ab_android_bird_retirement arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_BIRD_RETIREMENT_ONLY=1)
build_android_probe(ab_android_level_ending arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_LEVEL_ENDING_ONLY=1)
build_android_probe(ab_android_level_complete arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_LEVEL_COMPLETE_ONLY=1)
build_android_probe(ab_android_coordinates arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_COORDINATES_ONLY=1)
build_android_probe(ab_android_camera_viewport arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_CAMERA_VIEWPORT_ONLY=1)
build_android_probe(ab_android_camera_mover arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_CAMERA_MOVER_ONLY=1)
build_android_probe(ab_android_camera_limits arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_CAMERA_LIMITS_ONLY=1)
build_android_probe(ab_android_camera_sequence arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_CAMERA_SEQUENCE_ONLY=1)
build_android_probe(ab_android_camera_system arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_CAMERA_SYSTEM_ONLY=1)
build_android_probe(ab_android_camera_touch arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_CAMERA_SYSTEM_ONLY=1 -DAB_ORACLE_CAMERA_TOUCH_ONLY=1)
build_android_probe(ab_android_camera_touch_filtered arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_CAMERA_SYSTEM_ONLY=1 -DAB_ORACLE_CAMERA_TOUCH_ONLY=1
    -DAB_ORACLE_CAMERA_TOUCH_FILTERED=1)
build_android_probe(ab_android_menu_pointer arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_MENU_POINTER_ONLY=1)
build_android_probe(ab_android_integer_order arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_INTEGER_ORDER_ONLY=1)
build_android_probe(ab_android_ui_frame_geometry arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_UI_FRAME_GEOMETRY_ONLY=1)
build_android_probe(ab_android_ui_pointer arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_UI_POINTER_ONLY=1)
build_android_probe(ab_android_ui_image_hit arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_UI_IMAGE_HIT_ONLY=1)
build_android_probe(ab_android_ui_image_button arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_UI_IMAGE_BUTTON_ONLY=1)
build_android_probe(ab_android_ui_button_state arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_UI_IMAGE_BUTTON_ONLY=1 -DAB_ORACLE_UI_BUTTON_STATE=1)
build_android_probe(ab_android_ui_image_animation arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_UI_IMAGE_ANIMATION_ONLY=1)
build_android_probe(ab_android_ui_frame_draw arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_UI_FRAME_DRAW_ONLY=1)
build_android_probe(ab_android_ui_image_draw arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_UI_IMAGE_DRAW_ONLY=1)
build_android_probe(ab_android_ui_image_render arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_UI_IMAGE_DRAW_ONLY=1 -DAB_ORACLE_UI_IMAGE_RENDER=1)
build_android_probe(ab_android_ui_button_draw arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_UI_IMAGE_DRAW_ONLY=1 -DAB_ORACLE_UI_BUTTON_DRAW=1)
build_android_probe(ab_android_ui_image_resources arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_UI_IMAGE_RESOURCES_ONLY=1)
build_android_probe(ab_android_ui_localization arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_UI_LOCALIZATION_ONLY=1)
build_android_probe(ab_android_ui_init arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_UI_INIT_ONLY=1)
build_android_probe(ab_android_ui_font arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_UI_FONT_ONLY=1)
build_android_probe(ab_android_ui_value_text arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_UI_VALUE_TEXT_ONLY=1)
build_android_probe(ab_android_ui_native_value_text arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_UI_VALUE_TEXT_ONLY=1 -DAB_ORACLE_UI_NATIVE_VALUE_TEXT=1)
build_android_probe(ab_android_ui_frame_tree arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_UI_FRAME_TREE_ONLY=1)
build_android_probe(ab_android_ui_image_state arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_UI_IMAGE_STATE_ONLY=1)
build_android_probe(ab_android_text_catalog arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_TEXT_CATALOG_ONLY=1)
build_android_probe(ab_android_text_locale arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_TEXT_LOCALE_ONLY=1)
build_android_probe(ab_android_ui_text arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_UI_TEXT_ONLY=1 -DAB_ORACLE_FONT_LAYOUT_ONLY=1)
build_android_probe(ab_android_ui_text_image arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_UI_TEXT_ONLY=1 -DAB_ORACLE_FONT_LAYOUT_ONLY=1
    -DAB_ORACLE_UI_TEXT_IMAGE=1)
build_android_probe(ab_android_ui_text_visibility arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_UI_TEXT_ONLY=1 -DAB_ORACLE_FONT_LAYOUT_ONLY=1
    -DAB_ORACLE_UI_TEXT_VISIBILITY=1)
build_android_probe(ab_android_ui_text_clip arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_UI_TEXT_ONLY=1 -DAB_ORACLE_FONT_LAYOUT_ONLY=1
    -DAB_ORACLE_UI_TEXT_CLIP=1)
build_android_probe(ab_android_ui_text_wrap arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_UI_TEXT_ONLY=1 -DAB_ORACLE_FONT_LAYOUT_ONLY=1
    -DAB_ORACLE_UI_TEXT_WRAP=1)
build_android_probe(ab_android_ui_text_lines arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_UI_TEXT_ONLY=1 -DAB_ORACLE_FONT_LAYOUT_ONLY=1
    -DAB_ORACLE_UI_TEXT_LINES=1)
build_android_probe(ab_android_ui_text_metrics arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_UI_TEXT_ONLY=1 -DAB_ORACLE_FONT_LAYOUT_ONLY=1
    -DAB_ORACLE_UI_TEXT_LINES=1 -DAB_ORACLE_UI_TEXT_METRICS=1)
build_android_probe(ab_android_ui_text_bounds arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_UI_TEXT_ONLY=1 -DAB_ORACLE_FONT_LAYOUT_ONLY=1
    -DAB_ORACLE_UI_TEXT_LINES=1 -DAB_ORACLE_UI_TEXT_BOUNDS=1)
build_android_probe(ab_android_utf8_encode arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_UTF8_ONLY=1 -DAB_ORACLE_UTF8_ENCODE_ONLY=1)
build_android_probe(ab_android_camera_shake arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_CAMERA_SHAKE_ONLY=1)
build_android_probe(ab_android_camera_drag arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_CAMERA_DRAG_ONLY=1)
build_android_probe(ab_android_camera_tweens arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_CAMERA_TWEENS_ONLY=1)
build_android_probe(ab_android_script_trig arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_SCRIPT_TRIG_ONLY=1)
build_android_probe(ab_android_camera_selection arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_CAMERA_SELECTION_ONLY=1)
build_android_probe(ab_android_font_metrics arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_FONT_METRICS_ONLY=1)
build_android_probe(ab_android_utf8 arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_UTF8_ONLY=1)
build_android_probe(ab_android_object_sprite arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_OBJECT_SPRITE_ONLY=1)
build_android_probe(ab_android_image_rectangle arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_IMAGE_RECTANGLE_ONLY=1)
build_android_probe(ab_android_sprite_sized arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_SPRITE_SIZED_ONLY=1)
build_android_probe(ab_android_string_map arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_STRING_MAP_ONLY=1)
build_android_probe(ab_android_string_map_gc arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_STRING_MAP_ONLY=1 -DAB_ORACLE_STRING_MAP_GC=1)
build_android_probe(ab_android_identity_map arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_STRING_MAP_ONLY=1 -DAB_ORACLE_IDENTITY_MAP=1)
build_android_probe(ab_android_ui_menu_manager arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_UI_MENU_MANAGER_ONLY=1)
build_android_probe(ab_android_ui_menu_links arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_UI_MENU_MANAGER_ONLY=1 -DAB_ORACLE_UI_MENU_LINKS=1)
build_android_probe(ab_android_ui_menu_navigation arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_UI_MENU_MANAGER_ONLY=1 -DAB_ORACLE_UI_MENU_LINKS=1
    -DAB_ORACLE_UI_MENU_NAVIGATION=1)
build_android_probe(ab_android_ui_menu_input arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_UI_MENU_MANAGER_ONLY=1 -DAB_ORACLE_UI_MENU_LINKS=1
    -DAB_ORACLE_UI_MENU_NAVIGATION=1 -DAB_ORACLE_UI_MENU_INPUT=1)
build_android_probe(ab_android_ui_menu_keys arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_UI_MENU_MANAGER_ONLY=1 -DAB_ORACLE_UI_MENU_LINKS=1
    -DAB_ORACLE_UI_MENU_NAVIGATION=1 -DAB_ORACLE_UI_MENU_INPUT=1
    -DAB_ORACLE_UI_MENU_KEYS=1)
build_android_probe(ab_android_ui_menu_frame_keys arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_UI_MENU_MANAGER_ONLY=1 -DAB_ORACLE_UI_MENU_LINKS=1
    -DAB_ORACLE_UI_MENU_NAVIGATION=1 -DAB_ORACLE_UI_MENU_INPUT=1
    -DAB_ORACLE_UI_MENU_KEYS=1 -DAB_ORACLE_UI_MENU_FRAME_KEYS=1)
build_android_probe(ab_android_main_menu_theme arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_MAIN_MENU_THEME_ONLY=1)
build_android_probe(ab_android_main_menu_settings arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_MAIN_MENU_SETTINGS_ONLY=1)
build_android_probe(ab_android_main_menu_resources arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_MAIN_MENU_THEME_ONLY=1 -DAB_ORACLE_MAIN_MENU_RESOURCES=1)
build_android_probe(ab_android_theme_layer arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_THEME_LAYER_ONLY=1)
build_android_probe(ab_android_theme_layer_base arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_THEME_LAYER_ONLY=1
    -DAB_ORACLE_THEME_LAYER_BASE=1)
build_android_probe(ab_android_theme_layer_construct arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_THEME_LAYER_ONLY=1
    -DAB_ORACLE_THEME_LAYER_CONSTRUCT=1)
build_android_probe(ab_android_theme_layer_construct_composite arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_THEME_LAYER_ONLY=1
    -DAB_ORACLE_THEME_LAYER_CONSTRUCT=1 -DAB_ORACLE_THEME_LAYER_CONSTRUCT_COMPOSITE=1)
build_android_probe(ab_android_theme_layer_composite arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_THEME_LAYER_ONLY=1
    -DAB_ORACLE_THEME_LAYER_BASE=1 -DAB_ORACLE_THEME_LAYER_COMPOSITE=1)
build_android_probe(ab_android_theme_layer_gpu arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_THEME_LAYER_ONLY=1
    -DAB_ORACLE_THEME_LAYER_BASE=1 -DAB_ORACLE_THEME_LAYER_COMPOSITE=1
    -DAB_ORACLE_THEME_LAYER_GPU=1)
build_android_probe(ab_android_composite_bounds arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_COMPOSITE_BOUNDS_ONLY=1)
set(composite_draw_options)
if(AB_ANDROID_COMPOSITE_ROTATED_PARENT)
    list(APPEND composite_draw_options -DAB_ORACLE_COMPOSITE_ROTATED_PARENT=1)
endif()
build_android_probe(ab_android_composite_draw arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_COMPOSITE_BOUNDS_ONLY=1 -DAB_ORACLE_COMPOSITE_DRAW_ONLY=1
    ${composite_draw_options})
build_android_probe(ab_android_theme_background arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_THEME_BACKGROUND_ONLY=1)
build_android_probe(ab_android_game_rectangle arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_GAME_RECTANGLE_ONLY=1)
build_android_probe(ab_android_theme_rect arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_THEME_RECT_ONLY=1)
build_android_probe(ab_android_rubberband arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_RUBBERBAND_ONLY=1)
build_android_probe(ab_android_sling_rectangle arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_SLING_RECTANGLE_ONLY=1)
build_android_probe(ab_android_game_hud_draw arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_GAME_HUD_DRAW_ONLY=1)
build_android_probe(ab_android_rubberband_world arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_RUBBERBAND_WORLD_ONLY=1)
build_android_probe(ab_android_theme_foreground arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_THEME_FOREGROUND_ONLY=1)
build_android_probe(ab_android_theme_colors arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_THEME_COLORS_ONLY=1)
build_android_probe(ab_android_theme_scene arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_THEME_SCENE_ONLY=1)
build_android_probe(ab_android_theme_scene_fill arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_THEME_SCENE_ONLY=1 -DAB_ORACLE_THEME_SCENE_FILL=1)
build_android_probe(ab_android_image_material arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_IMAGE_MATERIAL_ONLY=1)
build_android_probe(ab_android_image_material_rectangle arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_IMAGE_MATERIAL_ONLY=1 -DAB_ORACLE_IMAGE_MATERIAL_RECTANGLE=1)
build_android_probe(ab_android_fill_rectangle arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_FILL_RECTANGLE_ONLY=1)
build_android_probe(ab_android_object_render arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_OBJECT_RENDER_ONLY=1)
build_android_probe(ab_android_random arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_RANDOM_ONLY=1)
build_android_probe(ab_android_engine_random android_engine_random.c
    android_engine_random_main)
build_android_probe(ab_android_particle_box arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_PARTICLE_BOX_ONLY=1)
build_android_probe(ab_android_particle_common arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_PARTICLE_BOX_ONLY=1 -DAB_ORACLE_PARTICLE_COMMON_ONLY=1)
build_android_probe(ab_android_particle_draw arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_PARTICLE_BOX_ONLY=1 -DAB_ORACLE_PARTICLE_DRAW_ONLY=1)
build_android_probe(ab_android_particle_zero_draw arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_PARTICLE_BOX_ONLY=1 -DAB_ORACLE_PARTICLE_DRAW_ONLY=1
    -DAB_ORACLE_PARTICLE_ZERO_SCALE=1)
build_android_probe(ab_android_particle_level arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_PARTICLE_BOX_ONLY=1 -DAB_ORACLE_PARTICLE_LEVEL_ONLY=1)
build_android_probe(ab_android_particle_menu_draw arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_PARTICLE_BOX_ONLY=1 -DAB_ORACLE_PARTICLE_DRAW_ONLY=1
    -DAB_ORACLE_PARTICLE_MENU_ONLY=1)
build_android_probe(ab_android_particle_level_draw arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_PARTICLE_BOX_ONLY=1 -DAB_ORACLE_PARTICLE_LEVEL_ONLY=1
    -DAB_ORACLE_PARTICLE_DRAW_ONLY=1)
build_android_probe(ab_android_random_startup arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_RANDOM_ONLY=1
    -DAB_ORACLE_RANDOM_STARTUP_ONLY=1)
build_android_probe(ab_android_bird_jump_overshoot arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_BIRD_JUMP_ONLY=1
    -DAB_ORACLE_BIRD_JUMP_OVERSHOOT=1)
build_android_probe(ab_android_bird_animation arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_BIRD_ANIMATION_ONLY=1)
build_android_probe(ab_android_goal_animation arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_GOAL_ANIMATION_ONLY=1)
build_android_probe(ab_android_character_animation arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_CHARACTER_ANIMATION_ONLY=1)
build_android_probe(ab_android_damage_sprite arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_DAMAGE_SPRITE_ONLY=1)
build_android_probe(ab_android_audio_group arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_AUDIO_GROUP_ONLY=1)
build_android_probe(ab_android_audio_catalog arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_AUDIO_CATALOG_ONLY=1)
build_android_probe(ab_android_audio_assets arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_AUDIO_ASSETS_ONLY=1)
build_android_probe(ab_android_audio_definition arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_AUDIO_DEFINITION_ONLY=1)
build_android_probe(ab_android_audio_resources arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_AUDIO_RESOURCES_ONLY=1)
build_android_probe(ab_android_audio_play arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_AUDIO_PLAY_ONLY=1)
build_android_probe(ab_android_font_layout arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_FONT_LAYOUT_ONLY=1)
build_android_probe(ab_android_font_image arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_FONT_LAYOUT_ONLY=1 -DAB_ORACLE_FONT_IMAGE_ONLY=1)
build_android_probe(ab_android_bird_jump arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_BIRD_JUMP_ONLY=1)
build_android_probe(ab_android_bird_damage arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BIRD_DAMAGE_ONLY=1)
build_android_probe(ab_android_museum_glue arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_MUSEUM_GLUE_ONLY=1)
build_android_probe(ab_android_museum_glue_route arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_MUSEUM_GLUE_ONLY=1
    -DAB_ORACLE_MUSEUM_GLUE_ROUTE=1)
build_android_probe(ab_android_bubble_contact arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_BUBBLE_CONTACT_ONLY=1)
build_android_probe(ab_android_damage_particles arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_DAMAGE_PARTICLES_ONLY=1)
build_android_probe(ab_android_block_collision arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_BLOCK_COLLISION_ONLY=1)
build_android_probe(ab_android_bird_notify arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_BIRD_NOTIFY_ONLY=1)
build_android_probe(ab_android_bird_prelude arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_BIRD_PRELUDE_ONLY=1)
build_android_probe(ab_android_game_behavior arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_GAME_BEHAVIOR_ONLY=1)
build_android_probe(ab_android_game_behavior_add arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_GAME_BEHAVIOR_ADD_ONLY=1)
build_android_probe(ab_android_museum_bird_behavior arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_MUSEUM_BIRD_BEHAVIOR_ONLY=1)
build_android_probe(ab_android_blues_trajectory arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_BLUES_TRAJECTORY_ONLY=1)
build_android_probe(ab_android_event_manager arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_EVENT_MANAGER_ONLY=1)
build_android_probe(ab_android_settings_statistics arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_SETTINGS_STATISTICS_ONLY=1)
build_android_probe(ab_android_classic_shot_listeners arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_CLASSIC_SHOT_LISTENERS_ONLY=1)
build_android_probe(ab_android_powerup_uses arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_POWERUP_USES_ONLY=1)
build_android_probe(ab_android_trajectory_follow arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_TRAJECTORY_FOLLOW_ONLY=1)
build_android_probe(ab_android_lua_asset_sprite arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_LUA_ASSET_SPRITE_ONLY=1)
build_android_probe(ab_android_lua_asset_sprite_smoke arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_LUA_ASSET_SPRITE_ONLY=1 -DAB_ORACLE_LUA_ASSET_SPRITE_SMOKE=1)
build_android_probe(ab_android_scene_id arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_SCENE_ID_ONLY=1)
build_android_probe(ab_android_scene_membership arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_SCENE_MEMBERSHIP_ONLY=1)
build_android_probe(ab_android_lua_asset_construct arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_LUA_ASSET_CONSTRUCT_ONLY=1)
build_android_probe(ab_android_lua_asset_renderer arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_LUA_ASSET_CONSTRUCT_ONLY=1 -DAB_ORACLE_LUA_ASSET_CONSTRUCT_FULL=1)
build_android_probe(ab_android_object_property_access arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_OBJECT_PROPERTY_ACCESS_ONLY=1)
build_android_probe(ab_android_object_definition_lookup arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_OBJECT_DEFINITION_LOOKUP_ONLY=1)
build_android_probe(ab_android_object_world_lookup arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_OBJECT_DEFINITION_LOOKUP_ONLY=1 -DAB_ORACLE_OBJECT_WORLD_LOOKUP=1)
build_android_probe(ab_android_level_properties arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_LEVEL_PROPERTIES_ONLY=1)
build_android_probe(ab_android_bird_mighty arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_BIRD_MIGHTY_ONLY=1)
build_android_probe(ab_android_bird_sprites arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_BIRD_SPRITES_ONLY=1)
build_android_probe(ab_android_bird_specialty arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_BIRD_SPECIALTY_ONLY=1)
build_android_probe(ab_android_bird_projectiles arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_BIRD_PROJECTILES_ONLY=1)
build_android_probe(ab_android_bird_collision_damage arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_BIRD_COLLISION_DAMAGE_ONLY=1)
build_android_probe(ab_android_bird_effects arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_BIRD_EFFECTS_ONLY=1)
build_android_probe(ab_android_bird_collision arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_BIRD_COLLISION_ONLY=1)
build_android_probe(ab_android_object_removal_notify arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_OBJECT_REMOVAL_NOTIFY_ONLY=1)
build_android_probe(ab_android_object_removed arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_OBJECT_REMOVED_ONLY=1)
build_android_probe(ab_android_bird_remove arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_BIRD_REMOVE_ONLY=1)
build_android_probe(ab_android_level_effect_base arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_LEVEL_EFFECT_BASE_ONLY=1)
build_android_probe(ab_android_burn_manager arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_BURN_MANAGER_ONLY=1)
build_android_probe(ab_android_bird_impact_prediction arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_BIRD_IMPACT_PREDICTION_ONLY=1)
build_android_probe(ab_android_game_raycast arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_GAME_RAYCAST_ONLY=1)
build_android_probe(ab_android_camera_smoothstep arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_CAMERA_SMOOTHSTEP_ONLY=1)
build_android_probe(ab_android_camera_follow arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_CAMERA_FOLLOW_ONLY=1)
build_android_probe(ab_android_camera_zoom_fit arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_CAMERA_ZOOM_FIT_ONLY=1)
build_android_probe(ab_android_camera_follow_system arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_CAMERA_FOLLOW_SYSTEM_ONLY=1)
build_android_probe(ab_android_camera_follow_end_sequence arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_CAMERA_FOLLOW_SYSTEM_ONLY=1 -DAB_ORACLE_CAMERA_FOLLOW_END_SEQUENCE=1)
build_android_probe(ab_android_camera_shot_sequence arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_CAMERA_FOLLOW_SYSTEM_ONLY=1 -DAB_ORACLE_CAMERA_FOLLOW_END_SEQUENCE=1
    -DAB_ORACLE_CAMERA_SHOT_SEQUENCE=1)
build_android_probe(ab_android_camera_follow_world arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_CAMERA_FOLLOW_SYSTEM_ONLY=1 -DAB_ORACLE_CAMERA_FOLLOW_END_SEQUENCE=1
    -DAB_ORACLE_CAMERA_SHOT_SEQUENCE=1 -DAB_ORACLE_CAMERA_FOLLOW_WORLD=1)
build_android_probe(ab_android_camera_follow_properties arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_CAMERA_FOLLOW_SYSTEM_ONLY=1 -DAB_ORACLE_CAMERA_FOLLOW_END_SEQUENCE=1
    -DAB_ORACLE_CAMERA_SHOT_SEQUENCE=1 -DAB_ORACLE_CAMERA_FOLLOW_WORLD=1
    -DAB_ORACLE_CAMERA_FOLLOW_PROPERTIES=1)
build_android_probe(ab_android_classic_camera arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_CAMERA_FOLLOW_SYSTEM_ONLY=1 -DAB_ORACLE_CAMERA_FOLLOW_END_SEQUENCE=1
    -DAB_ORACLE_CAMERA_SHOT_SEQUENCE=1 -DAB_ORACLE_CAMERA_FOLLOW_WORLD=1
    -DAB_ORACLE_CAMERA_FOLLOW_PROPERTIES=1 -DAB_ORACLE_CLASSIC_CAMERA=1)
build_android_probe(ab_android_classic_camera_live arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_CAMERA_FOLLOW_SYSTEM_ONLY=1 -DAB_ORACLE_CAMERA_FOLLOW_END_SEQUENCE=1
    -DAB_ORACLE_CAMERA_SHOT_SEQUENCE=1 -DAB_ORACLE_CAMERA_FOLLOW_WORLD=1
    -DAB_ORACLE_CAMERA_FOLLOW_PROPERTIES=1 -DAB_ORACLE_CLASSIC_CAMERA=1
    -DAB_ORACLE_CLASSIC_CAMERA_LIVE=1)
build_android_probe(ab_android_game_raycast_numeric arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_GAME_RAYCAST_ONLY=1
    -DAB_ORACLE_GAME_RAYCAST_NUMERIC=1)
build_android_probe(ab_android_bird_impact_world arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_GAME_RAYCAST_ONLY=1
    -DAB_ORACLE_BIRD_IMPACT_WORLD_ONLY=1)
build_android_probe(ab_android_bird_impact_world_numeric arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_GAME_RAYCAST_ONLY=1
    -DAB_ORACLE_BIRD_IMPACT_WORLD_ONLY=1 -DAB_ORACLE_BIRD_IMPACT_WORLD_NUMERIC=1)
build_android_probe(ab_android_block_removal_cursor arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_BLOCK_REMOVAL_CURSOR_ONLY=1)
build_android_probe(ab_android_block_removal_score arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_BLOCK_REMOVAL_SCORE_ONLY=1)
build_android_probe(ab_android_block_removal_effects arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_BLOCK_REMOVAL_EFFECTS_ONLY=1)
build_android_probe(ab_android_block_removal_particles arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_BLOCK_REMOVAL_PARTICLES_ONLY=1)
build_android_probe(ab_android_block_removal_detach arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_BLOCK_REMOVAL_DETACH_ONLY=1)
build_android_probe(ab_android_block_removal_cleanup arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_BLOCK_REMOVAL_CLEANUP_ONLY=1)
build_android_probe(ab_android_block_removal_spawn arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_BLOCK_REMOVAL_SPAWN_ONLY=1)
build_android_probe(ab_android_game_object_remove arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_GAME_OBJECT_REMOVE_ONLY=1)
build_android_probe(ab_android_game_object_remove_policy arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_GAME_OBJECT_REMOVE_POLICY_ONLY=1)
build_android_probe(ab_android_game_object_remove_joints arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_GAME_OBJECT_REMOVE_JOINTS_ONLY=1)
build_android_probe(ab_android_game_object_remove_teleporters arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_GAME_OBJECT_REMOVE_TELEPORTERS_ONLY=1)
build_android_probe(ab_android_block_removal_pass arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1 -DAB_ORACLE_BLOCK_REMOVAL_PASS_ONLY=1)
build_android_probe(ab_android_bird_route arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BIRD_DAMAGE_ONLY=1 -DAB_ORACLE_BIRD_ROUTE_ONLY=1)
build_android_probe(ab_android_slingshot arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BIRD_DAMAGE_ONLY=1 -DAB_ORACLE_SLINGSHOT_ONLY=1)
build_android_probe(ab_android_sling_draw arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BIRD_DAMAGE_ONLY=1 -DAB_ORACLE_SLINGSHOT_ONLY=1
    -DAB_ORACLE_SLING_DRAW_ONLY=1)
build_android_probe(ab_android_sling_delta arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BIRD_DAMAGE_ONLY=1 -DAB_ORACLE_SLINGSHOT_ONLY=1
    -DAB_ORACLE_SLING_DELTA_ONLY=1)
build_android_probe(ab_android_game_sling_mouse arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BIRD_DAMAGE_ONLY=1 -DAB_ORACLE_SLINGSHOT_ONLY=1
    -DAB_ORACLE_GAME_SLING_MOUSE_ONLY=1)
build_android_probe(ab_android_game_update_tail arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BIRD_DAMAGE_ONLY=1 -DAB_ORACLE_SLINGSHOT_ONLY=1
    -DAB_ORACLE_GAME_UPDATE_TAIL_ONLY=1)
build_android_probe(ab_android_game_update_hooks arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BIRD_DAMAGE_ONLY=1 -DAB_ORACLE_SLINGSHOT_ONLY=1
    -DAB_ORACLE_GAME_UPDATE_TAIL_ONLY=1 -DAB_ORACLE_GAME_UPDATE_HOOKS_ONLY=1)
build_android_probe(ab_android_game_update_joints arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BIRD_DAMAGE_ONLY=1 -DAB_ORACLE_SLINGSHOT_ONLY=1
    -DAB_ORACLE_GAME_UPDATE_TAIL_ONLY=1 -DAB_ORACLE_GAME_UPDATE_JOINTS_ONLY=1)
build_android_probe(ab_android_force_queue arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_FORCE_QUEUE_ONLY=1)
build_android_probe(ab_android_force_point arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_FORCE_POINT_ONLY=1)
build_android_probe(ab_android_water_effect arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_WATER_EFFECT_ONLY=1)
build_android_probe(ab_android_block_event arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_BLOCK_EVENT_ONLY=1)
build_android_probe(ab_android_physics_script arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_BLOCK_EVENT_ONLY=1 -DAB_ORACLE_PHYSICS_SCRIPT_ONLY=1)
build_android_probe(ab_android_block_script arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_BLOCK_SCRIPT_ONLY=1)
build_android_probe(ab_android_block_component arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_BLOCK_COMPONENT_ONLY=1)
build_android_probe(ab_android_flame_effect arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_FLAME_EFFECT_ONLY=1)
build_android_probe(ab_android_flame_draw arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_FLAME_DRAW_ONLY=1)
build_android_probe(ab_android_flame_apply arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_FLAME_APPLY_ONLY=1)
build_android_probe(ab_android_ambient_animation arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_AMBIENT_ANIMATION_ONLY=1)
build_android_probe(ab_android_object_animation arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_OBJECT_ANIMATION_ONLY=1)
build_android_probe(ab_android_ambient_particle arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_AMBIENT_PARTICLE_ONLY=1)
build_android_probe(ab_android_ambient_particle_coordinates arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_AMBIENT_PARTICLE_ONLY=1
    -DAB_ORACLE_AMBIENT_PARTICLE_COORDINATES_ONLY=1)
build_android_probe(ab_android_block_update arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_BLOCK_UPDATE_ONLY=1)
build_android_probe(ab_android_object_update arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_OBJECT_UPDATE_ONLY=1)
build_android_probe(ab_android_object_visual arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_OBJECT_VISUAL_ONLY=1)
build_android_probe(ab_android_object_transform arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_OBJECT_TRANSFORM_ONLY=1)
build_android_probe(ab_android_sling_pose arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BIRD_DAMAGE_ONLY=1 -DAB_ORACLE_SLINGSHOT_ONLY=1
    -DAB_ORACLE_SLING_POSE_ONLY=1)
build_android_probe(ab_android_sling_start_pose arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BIRD_DAMAGE_ONLY=1 -DAB_ORACLE_SLINGSHOT_ONLY=1
    -DAB_ORACLE_SLING_START_POSE_ONLY=1)
build_android_probe(ab_android_sling_drag arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BIRD_DAMAGE_ONLY=1 -DAB_ORACLE_SLINGSHOT_ONLY=1
    -DAB_ORACLE_SLING_DRAG_ONLY=1)
build_android_probe(ab_android_sling_impulse arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BIRD_DAMAGE_ONLY=1 -DAB_ORACLE_SLINGSHOT_ONLY=1
    -DAB_ORACLE_SLING_IMPULSE_ONLY=1)
build_android_probe(ab_android_sling_routing arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BIRD_DAMAGE_ONLY=1 -DAB_ORACLE_SLINGSHOT_ONLY=1
    -DAB_ORACLE_SLING_IMPULSE_ONLY=1 -DAB_ORACLE_SLING_ROUTING_ONLY=1)
build_android_probe(ab_android_sling_cancel arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BIRD_DAMAGE_ONLY=1 -DAB_ORACLE_SLINGSHOT_ONLY=1
    -DAB_ORACLE_SLING_CANCEL_ONLY=1)
build_android_probe(ab_android_sling_launch_host arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BIRD_DAMAGE_ONLY=1 -DAB_ORACLE_SLINGSHOT_ONLY=1
    -DAB_ORACLE_SLING_LAUNCH_HOST_ONLY=1)
build_android_probe(ab_android_classic_shot_effects arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BIRD_DAMAGE_ONLY=1 -DAB_ORACLE_SLINGSHOT_ONLY=1
    -DAB_ORACLE_CLASSIC_SHOT_EFFECTS_ONLY=1)
build_android_probe(ab_android_sling_wobble arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BIRD_DAMAGE_ONLY=1 -DAB_ORACLE_SLINGSHOT_ONLY=1
    -DAB_ORACLE_SLING_WOBBLE_ONLY=1)
build_android_probe(ab_android_bird_shot arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BIRD_DAMAGE_ONLY=1 -DAB_ORACLE_SLINGSHOT_ONLY=1
    -DAB_ORACLE_BIRD_SHOT_ONLY=1)
build_android_probe(ab_android_sling_transfer arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BIRD_DAMAGE_ONLY=1 -DAB_ORACLE_SLINGSHOT_ONLY=1
    -DAB_ORACLE_SLING_TRANSFER_ONLY=1)
build_android_probe(ab_android_sling_reload arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BIRD_DAMAGE_ONLY=1 -DAB_ORACLE_SLINGSHOT_ONLY=1
    -DAB_ORACLE_SLING_TRANSFER_ONLY=1 -DAB_ORACLE_SLING_RELOAD_ONLY=1)
build_android_probe(ab_android_sling_start arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BIRD_DAMAGE_ONLY=1 -DAB_ORACLE_SLINGSHOT_ONLY=1
    -DAB_ORACLE_SLING_TRANSFER_ONLY=1 -DAB_ORACLE_SLING_START_ONLY=1)
build_android_probe(ab_android_string_order arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_STRING_ORDER_ONLY=1)
build_android_probe(ab_android_string_growth arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_STRING_ORDER_ONLY=1 -DAB_ORACLE_STRING_GROWTH=1)
build_android_probe(ab_android_level_order arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_LEVEL_ORDER_ONLY=1)
if(DEFINED AB_ANDROID_PROBE AND AB_ANDROID_PROBE STREQUAL "ab_android_theme_gpu")
    set(android_libc "${platform_directory}/libc.so")
    file(SHA256 "${android_libc}" libc_hash)
    if(NOT libc_hash STREQUAL
        "5c5bba82404c435bf1cd0ef8e5096f58790d85c0adfbba07d873c4d6e3af2691")
        message(FATAL_ERROR "Expected the pinned Android 5.1.1 platform C ABI")
    endif()
    get_filename_component(project_directory "${source_directory}/../.." ABSOLUTE)
    set(gpu_directory "${output_directory}/theme-gpu-build")
    file(MAKE_DIRECTORY "${gpu_directory}")
    set(shader_header "${gpu_directory}/ab_sprite_shaders.h")
    file(WRITE "${shader_header}" "/* Generated from project-authored shaders. */\n")
    foreach(stage vert frag)
        if(stage STREQUAL "vert")
            set(symbol ab_sprite_vertex_source)
        else()
            set(symbol ab_sprite_fragment_source)
        endif()
        file(READ "${project_directory}/shaders/sprite.${stage}" shader_text)
        string(REPLACE "\\" "\\\\" shader_text "${shader_text}")
        string(REPLACE "\"" "\\\"" shader_text "${shader_text}")
        string(REPLACE "\n" "\\n\"\n    \"" shader_text "${shader_text}")
        file(APPEND "${shader_header}"
            "static const char ${symbol}[] =\n    \"${shader_text}\";\n")
    endforeach()
    set(gpu_sources tools/oracle/android_theme_gpu.c src/render/gles2.c
        src/render/draw.c src/core/base.c src/core/hash.c)
    set(gpu_objects)
    foreach(source IN LISTS gpu_sources)
        get_filename_component(name "${source}" NAME_WE)
        set(object "${gpu_directory}/${name}.o")
        run_checked("${AB_ANDROID_CLANG}" ${arm_options} ${c_options}
            -ffunction-sections -fdata-sections
            "-I${source_directory}/android_headers"
            "-I${project_directory}/include" "-I${gpu_directory}"
            -c "${project_directory}/${source}" -o "${object}")
        list(APPEND gpu_objects "${object}")
    endforeach()
    run_checked("${AB_ANDROID_CLANG}" ${arm_options}
        -c "${source_directory}/android_gpu_start.S"
        -o "${gpu_directory}/start.o")
    run_checked("${AB_ANDROID_LLD}" -flavor gnu -pie -e _start --hash-style=sysv
        --gc-sections --dynamic-linker=/system/bin/linker --allow-shlib-undefined
        -o "${output_directory}/ab_android_theme_gpu"
        "${gpu_directory}/start.o" ${gpu_objects} "${android_libc}" "${android_libdl}")
    set(android_probe_built TRUE)
endif()
build_android_probe(ab_android_superseed_radius arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_SUPERSEED_RADIUS_ONLY=1)

build_android_probe(ab_android_sling_scope arm.c oracle_main
    -DAB_ORACLE_ANDROID=1 -DAB_ORACLE_BLOCK_DAMAGE_ONLY=1
    -DAB_ORACLE_SLING_SCOPE_ONLY=1)

if(NOT android_probe_built)
    message(FATAL_ERROR "No matching Android reference probe was selected")
endif()
message(STATUS "Built isolated Android reference probes")
