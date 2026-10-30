cmake_minimum_required(VERSION 3.22)
project(AngryBirdsClassicC VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
file(REAL_PATH "${CMAKE_BINARY_DIR}" AB_BUILD_PATH)
file(REAL_PATH "${CMAKE_SOURCE_DIR}/Files" AB_FILES_PATH)
cmake_path(IS_PREFIX AB_FILES_PATH "${AB_BUILD_PATH}" NORMALIZE AB_BUILD_IN_FILES)
if(NOT AB_BUILD_IN_FILES)
    message(FATAL_ERROR "Keep build products in Files, for example -B Files/build.")
endif()
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/experimental:c11atomics /Zc:preprocessor /fp:strict)
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Common/CMakeLists.txt")
    message(FATAL_ERROR "Initialize Common with git submodule update --init --recursive.")
endif()
set(CC_FRAME_WIDTH 480 CACHE STRING "Logical framebuffer width")
set(CC_FRAME_HEIGHT 320 CACHE STRING "Logical framebuffer height")
set(CC_ASPECT_WIDTH 3 CACHE STRING "Display aspect numerator")
set(CC_ASPECT_HEIGHT 2 CACHE STRING "Display aspect denominator")
add_subdirectory(Common)
if(APPLE)
    set(AB_BACKEND metal CACHE STRING "Graphics backend")
else()
    set(AB_BACKEND gles2 CACHE STRING "Graphics backend")
endif()
set_property(CACHE AB_BACKEND PROPERTY STRINGS metal gles2)

option(AB_SANITIZE "Enable address and undefined behavior diagnostics" OFF)
option(AB_WARNINGS_AS_ERRORS "Treat project warnings as errors" ON)

add_library(ab_options INTERFACE)
if(MSVC)
    target_compile_options(ab_options INTERFACE /W4 /fp:strict)
    if(AB_WARNINGS_AS_ERRORS)
        target_compile_options(ab_options INTERFACE /WX)
    endif()
else()
    target_compile_options(ab_options INTERFACE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow -ffp-contract=off)
    if(AB_WARNINGS_AS_ERRORS)
        target_compile_options(ab_options INTERFACE -Werror)
    endif()
    if(AB_SANITIZE)
        target_compile_options(ab_options INTERFACE
            -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(ab_options INTERFACE -fsanitize=address,undefined)
    endif()
endif()

add_library(ab_numeric STATIC src/core/rotation.c src/core/angle.c src/core/script_angle.c
    src/core/random.c)
target_include_directories(ab_numeric PUBLIC include)
target_link_libraries(ab_numeric PUBLIC ab_options)

add_library(ab_base STATIC src/core/base.c src/core/utf8.c src/core/string_map.c
    src/core/integer_map.c
    src/core/identity_map.c src/core/z_order.c)
target_include_directories(ab_base PUBLIC include)
target_link_libraries(ab_base PUBLIC ab_options)

add_library(ab_resource STATIC
    src/core/hash.c
    src/core/json.c
    src/resource/aes.c
    src/resource/target_resources.c
    src/resource/target_data.c
    src/resource/dynamic_asset_requirements.c
    src/resource/asset_load_lists.c
    src/resource/sprite_registry.c
    src/resource/resource_owners.c
    src/resource/sprite_resource.c
    src/resource/audio_catalog.c
    src/resource/audio_definition.c
    src/resource/audio_pcm.c
    src/resource/pcm.c
    src/resource/camera_sequence.c
    src/resource/wave.c
    src/resource/mp3.c
    src/resource/mp3_side.c
    src/resource/mp3_reservoir.c
    src/resource/mp3_scalefactors.c
    src/resource/mp3_huffman.c
    src/resource/mp3_huffman_data.c
    src/resource/mp3_spectrum.c
    src/resource/mp3_magnitude.c
    src/resource/mp3_stereo.c
    src/resource/mp3_alias.c
    src/resource/mp3_transform.c
    src/resource/mp3_hybrid.c
    src/resource/mp3_synthesis.c
    src/resource/mp3_decode.c
    src/resource/sprite_set.c
    src/resource/sprite_set_registry.c
    src/resource/sprite_atlas.c
    src/resource/composite.c
    src/resource/dat.c
    src/resource/data_order.c
    src/resource/integer_order.c
    src/resource/data_layout.c
    src/resource/font.c
    src/resource/font_asset.c
    src/resource/episode_catalog.c
    src/resource/powerup_catalog.c
    src/resource/inflate.c
    src/resource/lzma.c
    src/resource/lua_chunk.c
    src/resource/lua_data.c
    src/resource/lua_data_json.c
    src/resource/theme_colors.c
    src/resource/lua_text.c
    src/resource/lua_text_write.c
    src/resource/pack.c
    src/resource/png.c
    src/resource/webp.c
    src/resource/webp_decode.c
    src/resource/webp_alpha.c
    src/resource/webp_bits.c
    src/resource/webp_lossless.c
    src/resource/webp_color.c
    src/resource/vp8_boolean.c
    src/resource/vp8_coefficients.c
    src/resource/vp8_decode.c
    src/resource/vp8_filter.c
    src/resource/vp8_header.c
    src/resource/vp8_modes.c
    src/resource/vp8_predict.c
    src/resource/vp8_residuals.c
    src/resource/vp8_tables.c
    src/resource/vp8_transform.c
    src/resource/pvr.c
    src/resource/sprite.c
    src/resource/text.c
    src/resource/text_resources.c
    src/resource/zip.c
    src/resource/axml.c)
target_include_directories(ab_resource PUBLIC include)
target_link_libraries(ab_resource PUBLIC ab_options ab_base)

add_library(ab_physics STATIC src/physics/body.c src/physics/contact.c src/physics/geometry.c
    src/physics/body_initialize.c src/physics/body_velocity.c src/physics/filter.c
    src/physics/contact_state.c
    src/physics/joint_geometry.c
    src/physics/matrix_inverse.c
    src/physics/matrix_solve.c
    src/physics/weld_initialize.c
    src/physics/weld_position.c
    src/physics/weld_velocity.c
    src/physics/joint_solver_geometry.c src/physics/distance_joint.c
    src/physics/joint_constraint.c src/physics/island_joints.c
    src/physics/joint_coordinate.c
    src/physics/revolute_initialize.c
    src/physics/revolute_velocity.c
    src/physics/revolute_position.c
    src/physics/prismatic_initialize.c
    src/physics/prismatic_velocity.c
    src/physics/prismatic_matrix.c src/physics/prismatic_position.c
    src/physics/fixture_proxy.c
    src/physics/world_solve.c
    src/physics/world_joint_graph.c
    src/physics/world_joint_definition.c
    src/physics/world_joint_lifecycle.c
    src/physics/world_joint_parameters.c
    src/physics/world_setup.c
    src/physics/world_raycast.c
    src/physics/world_pairs.c
    src/physics/world_refresh.c src/physics/world_step.c src/physics/world_toi.c
    src/physics/world_body.c src/physics/world_fixture.c src/physics/world_lifecycle.c
    src/physics/distance.c src/physics/distance_simplex.c
    src/physics/sweep.c
    src/physics/separation.c
    src/physics/impact.c
    src/physics/position_toi.c
    src/physics/broadphase.c src/physics/broadphase_pairs.c
    src/physics/island.c
    src/physics/island_toi.c
    src/physics/polygon_build.c src/physics/polygon_contact.c
    src/physics/position_point.c src/physics/position_solver.c src/physics/raycast.c
    src/physics/step_free.c
    src/physics/tree.c src/physics/tree_balance.c src/physics/tree_structure.c
    src/physics/tree_raycast.c
    src/physics/velocity_initialize.c src/physics/velocity_solver.c
    src/physics/warm_start.c
    src/physics/world_manifold.c)
target_include_directories(ab_physics PUBLIC include)
target_link_libraries(ab_physics PUBLIC ab_options ab_numeric ab_base)
if(UNIX AND NOT APPLE)
    target_link_libraries(ab_physics PUBLIC m)
endif()

include(cmake/embed_shaders.cmake)
add_library(ab_render STATIC src/render/draw.c src/render/gles2.c
    src/render/extra_draw.c
    src/render/game_sprite.c
    src/render/asset_textures.c
    src/render/font_metrics.c src/render/font_layout.c src/render/font_draw.c
    src/render/font_wrap.c src/render/ui_image_render.c src/render/screen_clip.c
    src/render/theme_layer.c
    src/render/image_points.c
    src/render/mask_draw.c
    src/render/composite_bounds.c
    src/render/composite_draw.c
    src/render/composite_resources.c
    src/render/composite_resource.c
    src/render/composite_part.c
    src/render/theme_layers.c
    src/render/theme_draw.c
    src/render/fill_rectangle.c
    src/render/rubberband.c
    src/render/slingshot_draw.c
    src/render/theme_background.c
    src/render/game_rectangle.c
    src/render/theme_foreground.c
    src/render/theme_layer_data.c
    src/render/theme_rect.c
    src/render/theme_sprite.c
    src/render/object_sprite.c src/render/image_rectangle.c src/render/sprite_scene.c
    src/render/lua_asset_sprite.c src/render/scene_id.c src/render/scene_membership.c
    src/render/lua_asset_renderer.c)
target_include_directories(ab_render PUBLIC include)
target_include_directories(ab_render PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(ab_render PUBLIC ab_resource ab_options ab_numeric)
add_library(ab_common_renderer STATIC src/render/common_renderer.c)
target_include_directories(ab_common_renderer PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(ab_common_renderer PUBLIC ab_render console_common)
if(APPLE)
    add_executable(ab_preview MACOSX_BUNDLE tools/preview.c)
    set_target_properties(ab_preview PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.tomoeko.angrybirds-classic-preview"
        MACOSX_BUNDLE_BUNDLE_NAME "Angry Birds Classic C Preview")
    add_custom_command(TARGET ab_preview POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy
            "${PROJECT_SOURCE_DIR}/tools/launch-preview.sh"
            "${CMAKE_BINARY_DIR}/angrybirds-preview" VERBATIM)
    set_property(TARGET ab_preview APPEND PROPERTY LINK_DEPENDS
        "${PROJECT_SOURCE_DIR}/tools/launch-preview.sh")
else()
    add_executable(ab_preview tools/preview.c)
endif()
set_target_properties(ab_preview PROPERTIES OUTPUT_NAME "angrybirds-preview")
target_link_libraries(ab_preview PRIVATE ab_common_renderer)
cc_add_platform(ab_preview "${AB_BACKEND}")

if(APPLE)
    add_library(ab_metal STATIC src/platform/apple/metal.c)
    target_include_directories(ab_metal PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
    target_link_libraries(ab_metal PUBLIC ab_render
        PRIVATE "-framework Foundation" "-framework Metal" objc)
    add_library(ab_window STATIC src/platform/apple/window.c)
    target_link_libraries(ab_window PUBLIC ab_metal
        PRIVATE "-framework AppKit" "-framework QuartzCore" "-framework CoreGraphics")
    add_executable(ab_window_check tools/window_check.c)
    target_link_libraries(ab_window_check PRIVATE ab_window)
    add_executable(ab_font_preview tools/font_preview.c)
    target_link_libraries(ab_font_preview PRIVATE ab_metal)
endif()

add_executable(ab_apk tools/apk.c)
add_library(ab_game STATIC src/game/object_definition.c src/game/object_physics.c
    src/game/episode_statistics.c
    src/game/episode_progress.c src/game/level_progress.c
    src/game/episode_preferences.c src/game/classic_level_request.c
    src/game/classic_level_prepare.c
    src/game/classic_start_level.c
    src/game/classic_pause_actions.c
    src/game/collision.c src/game/collision_damage.c src/game/bird_damage.c
    src/game/collision_route.c src/game/slingshot.c src/game/slingshot_body.c
    src/game/sling_scope.c
    src/game/slingshot_drag.c src/game/slingshot_launch.c src/game/bird_shot.c
    src/game/slingshot_pose.c
    src/game/game_hud.c
    src/game/game_sling_mouse.c
    src/game/game_update_tail.c
    src/game/slingshot_control.c src/game/collision_velocity.c src/game/game_contact.c
    src/game/game_contact_routes.c
    src/game/game_contact_world.c
    src/game/score.c
    src/game/classic_destruction_statistics.c
    src/game/classic_scores.c
    src/game/classic_highscores.c
    src/game/burn_manager.c
    src/game/hero_screen_tinter.c
    src/game/classic_game_globals.c
    src/game/classic_game_updates.c
    src/game/delayed_callbacks.c
    src/game/classic_goals.c
    src/game/floating_scores.c
    src/game/particles.c
    src/game/classic_particles.c
    src/game/classic_particle_assets.c
    src/game/classic_collision_achievements.c
    src/game/museum_impact.c src/game/museum_glue.c
    src/game/museum_bird_behavior.c
    src/game/blues_trajectory_behavior.c
    src/game/event_manager.c src/game/event_timer.c
    src/game/settings_statistics.c
    src/game/settings_inventory.c src/game/settings_golden_eggs.c
    src/game/shot_services.c
    src/game/classic_shot_listeners.c
    src/game/audio_bank.c
    src/game/powerup_uses.c
    src/game/process_manager.c
    src/game/classic_loader.c
    src/game/native_events.c
    src/game/classic_root.c
    src/game/classic_level5.c
    src/game/classic_audio_ramp.c
    src/game/url_requests.c
    src/game/session_timer.c
    src/game/time_format.c
    src/game/script_time.c
    src/game/giftbox_session.c
    src/game/classic_anger.c
    src/game/classic_root_queries.c
    src/game/classic_level_flow.c
    src/game/classic_game_mode.c
    src/platform/calendar_time.c
    src/game/classic_shot_effects.c
    src/game/bubble_contact.c
    src/game/damage_particles.c
    src/game/block_collision.c
    src/game/bird_collision_notify.c
    src/game/bird_collision_prelude.c
    src/game/bird_hat.c
    src/game/bird_shockwave.c
    src/game/bird_bubbles.c
    src/game/electric_shockwave.c
    src/game/game_behavior.c
    src/game/object_property_access.c
    src/game/object_definition_lookup.c
    src/game/level_properties.c
    src/game/classic_camera.c
    src/game/classic_behaviors.c
    src/game/trajectory_follow.c
    src/game/bird_collision_mighty.c
    src/game/bird_collision_sprites.c
    src/game/bird_collision_specialty.c
    src/game/bird_collision_projectiles.c
    src/game/bird_collision_damage.c
    src/game/bird_collision_effects.c
    src/game/bird_collision.c
    src/game/object_removal_notify.c
    src/game/object_removed.c
    src/game/bird_remove.c
    src/game/level_effect.c src/game/level_egg.c src/game/level_clock.c
    src/game/bird_impact_prediction.c
    src/game/bird_camera_behavior.c
    src/game/bird_impact_world.c
    src/game/level_raycast.c
    src/game/block_removal_cursor.c
    src/game/block_removal_score.c
    src/game/block_removal_effects.c
    src/game/block_removal_particles.c
    src/game/block_removal_lifecycle.c
    src/game/block_removal_spawn.c
    src/game/block_removal_pass.c
    src/game/game_object_remove.c
    src/game/game_object_removal.c
    src/game/script_sine.c src/game/bird_jump.c
    src/game/bird_animation.c src/game/goal_animation.c src/game/character_animation.c
    src/game/damage_sprite.c src/game/audio_group.c src/game/audio_play.c
    src/game/audio_mix.c
    src/game/audio_mixer.c
    src/game/audio_resources.c
    src/game/classic_audio.c
    src/game/slingshot_reload.c src/game/level_flow.c src/game/level_bonus.c
    src/game/bird_retirement.c src/game/level_ending.c src/game/level_complete.c
    src/game/coordinates.c src/game/camera_viewport.c
    src/game/camera_mover.c
    src/game/camera_smoothstep.c
    src/game/camera_follow.c
    src/game/camera_zoom_fit.c
    src/game/camera_limits.c
    src/game/camera_bootstrap.c
    src/game/camera_sequence.c
    src/game/camera_system.c
    src/game/camera_system_follow.c
    src/game/camera_follow_world.c
    src/game/camera_shake.c
    src/game/camera_drag.c
    src/game/camera_selection.c
    src/game/camera_touch.c
    src/game/menu_pointer.c
    src/game/touch_snapshot.c
    src/game/ui_frame_geometry.c
    src/game/ui_pointer.c
    src/game/ui_key.c
    src/game/ui_value_text.c
    src/game/ui_image_hit.c
    src/game/ui_image_button.c
    src/game/ui_image_button_animation.c
    src/game/ui_frame_draw.c
    src/game/ui_image_draw.c
    src/game/ui_image_button_draw.c
    src/game/ui_image_resources.c
    src/game/ui_image_state.c
    src/game/ui_widget_draw.c
    src/game/ui_widget_pointer.c
    src/game/ui_menu_manager.c
    src/game/ui_menu_navigation.c
    src/game/ui_menu_keys.c
    src/game/main_menu_theme.c
    src/game/main_menu_settings.c
    src/game/main_menu_resources.c
    src/game/asset_profile.c
    src/game/android_asset_bundles.c
    src/game/android_assets.c
    src/game/asset_groups.c
    src/game/theme_assets.c
    src/game/asset_sheet_key.c
    src/game/asset_bundles.c
    src/game/ui_localization.c
    src/game/ui_init.c
    src/game/ui_frame_tree.c
    src/game/ui_text_lines.c
    src/game/ui_text_metrics.c
    src/game/ui_text_bounds.c
    src/game/ui_text_draw.c
    src/game/frame_clock.c src/game/body_pose.c src/game/game_frame.c
    src/game/object_body.c
    src/game/classic_joint.c src/game/classic_joint_geometry.c
    src/game/classic_joints.c src/game/classic_glue.c
    src/game/classic_joint_limits.c
    src/game/classic_joint_parameters.c
    src/game/classic_joint_break.c
    src/game/classic_joint_updates.c
    src/game/floating_score_draw_plan.c
    src/game/classic_object_hooks.c
    src/game/mighty_eagle_timer.c
    src/game/classic_contact_glue.c
    src/game/level_joints.c
    src/game/object_friction.c
    src/game/object_forces.c
    src/game/object_force_interval.c
    src/game/object_force_sound.c
    src/game/object_force_step.c
    src/game/object_force_data.c
    src/game/classic_forces.c
    src/game/force_queue.c
    src/game/water_effect.c
    src/game/block_event.c
    src/game/physics_script.c
    src/game/block_script.c
    src/game/object_update.c
    src/game/object_visual.c
    src/game/object_transform.c src/game/game_circle.c src/game/game_box.c
    src/game/object_properties.c
    src/game/object_activity.c
    src/game/classic_post_physics.c
    src/game/classic_contacts.c
    src/game/classic_simulation.c
    src/game/block_component.c
    src/game/flame_effect.c
    src/game/flame_data.c
    src/game/ambient_animation.c
    src/game/ambient_animation_data.c
    src/game/ambient_particle.c
    src/game/ambient_particle_data.c
    src/game/game_physics_prepare.c
    src/game/angle.c src/game/level_bodies.c src/game/classic_level.c
    src/game/classic_level_source.c)
target_include_directories(ab_game PUBLIC include)
target_link_libraries(ab_game PUBLIC ab_resource ab_physics)
add_library(ab_asset_resources STATIC src/game/asset_resources.c
    src/game/asset_sprite_provider.c src/game/ui_widget_resources.c)
target_link_libraries(ab_asset_resources PUBLIC ab_game ab_render)

add_library(ab_scene STATIC src/render/classic_scene.c src/render/classic_visuals.c
    src/render/floating_score_draw.c
    src/render/classic_render_scene.c src/game/classic_characters.c
    src/game/classic_shots.c src/game/classic_sling.c
    src/game/classic_launch_host.c src/game/classic_playfield.c
    src/game/classic_level_runtime.c
    src/game/classic_block_scripts.c
    src/game/classic_ambient.c
    src/game/classic_flames.c
    src/game/classic_world_runtime.c
    src/game/classic_blues_child.c src/game/classic_blues_split.c
    src/game/classic_dynamics.c src/game/classic_collision_hooks.c
    src/render/classic_sling_draw.c
    src/render/classic_game_draw.c
    src/render/classic_game_render.c
    src/game/classic_block_collisions.c src/game/classic_museum_collisions.c
    src/render/particle_draw.c
    src/game/classic_frame_particles.c
    src/game/classic_frame.c
    src/game/classic_bird_collisions.c
    src/game/classic_birds.c
    src/game/classic_controller.c
    src/game/ui_widget.c
    src/game/ui_font_resources.c
    src/game/ui_text_widget.c
    src/game/ui_text_clipping.c
    src/game/ui_episode_button.c src/game/ui_level_button.c
    src/game/ui_level_facebook.c
    src/game/ui_level_golden.c
    src/game/ui_episode_services.c src/game/ui_level_services.c
    src/game/ui_episode_scroll.c src/game/ui_level_scroll.c
    src/game/ui_episode_selection.c src/game/ui_level_selection.c
    src/game/ui_level_hidden_eggs.c
    src/game/ui_level_rmf.c
    src/game/ui_level_pigs.c
    src/game/ui_level_eggs.c
    src/game/ui_cutscene.c
    src/game/ui_menu_birds.c
    src/game/ui_bg_box.c
    src/game/ui_scaling.c
    src/game/ui_main_menu_slider.c
    src/game/ui_slider.c
    src/game/ui_hud_powerups.c
    src/game/ui_powerup_effect.c
    src/game/ui_hud_buttons.c
    src/game/ui_pause_page.c
    src/game/ui_mighty_eagle_button.c
    src/game/ui_eagle_services.c
    src/game/ui_game_hud_controls.c
    src/game/ui_game_hud_layout.c
    src/game/ui_game_hud_pause.c
    src/game/ui_game_hud_services.c
    src/game/ui_animated_image.c
    src/game/ui_toggle_indicator.c
    src/game/ui_toggle_button.c
    src/game/ui_tiled_image.c
    src/game/ui_main_menu_options.c
    src/game/ui_scroll_frame.c
    src/game/classic_character_services.c
    src/game/classic_trajectories.c
    src/game/classic_game_effects.c
    src/game/classic_game_prelude.c
    src/game/classic_level_finish.c
    src/game/classic_level_ending.c
    src/game/classic_game_progress.c
    src/game/classic_game_start.c
    src/game/classic_game_powerups.c
    src/game/classic_powerup_particles.c
    src/game/classic_superseed.c
    src/game/classic_powerup_manager.c
    src/game/classic_game_level_particles.c
    src/game/classic_bird_specialty.c
    src/game/classic_bird_boost.c
    src/game/classic_boomerang.c
    src/game/classic_explosions.c
    src/game/classic_eggs.c
    src/game/classic_object_animation.c
    src/game/classic_touches.c
    src/game/classic_game_input.c
    src/game/classic_menu_input.c
    src/game/classic_game_update.c
    src/game/classic_game_runtime.c
    src/game/classic_game_mode_bindings.c
    src/game/classic_balloons.c
    src/game/classic_object_removal.c
    src/game/classic_removals.c
    src/game/classic_physics_phases.c)
target_include_directories(ab_scene PUBLIC include)
target_link_libraries(ab_scene PUBLIC ab_game ab_render)
add_library(ab_settings_file STATIC src/platform/settings_file.c)
target_include_directories(ab_settings_file PUBLIC include)
target_link_libraries(ab_settings_file PUBLIC ab_game ab_resource console_common)
add_library(ab_audio_output STATIC src/platform/audio_output.c)
target_include_directories(ab_audio_output PUBLIC include)
target_link_libraries(ab_audio_output PUBLIC ab_game console_common_audio)
target_link_libraries(ab_preview PRIVATE ab_scene ab_asset_resources ab_settings_file
    ab_audio_output console_common_capture)
add_executable(ab_theme_gpu_data tools/theme_gpu_data.c)
target_link_libraries(ab_theme_gpu_data PRIVATE ab_scene)
add_executable(ab_level_audit tools/level_audit.c)
target_link_libraries(ab_level_audit PRIVATE ab_scene ab_asset_resources)
if(APPLE)
    add_executable(ab_theme_capture tools/theme_capture.c)
    target_link_libraries(ab_theme_capture PRIVATE ab_scene ab_metal)
endif()

target_link_libraries(ab_apk PRIVATE ab_resource)

add_executable(ab_lua_data tools/lua_data.c)
target_link_libraries(ab_lua_data PRIVATE ab_resource)

add_executable(ab_lua_inspect tools/lua_inspect.c)
target_link_libraries(ab_lua_inspect PRIVATE ab_resource)

add_executable(ab_save_data tools/save_data.c)
target_link_libraries(ab_save_data PRIVATE ab_resource)

include(CTest)
if(BUILD_TESTING)
    add_executable(ab_audio_output_tests tests/audio_output.c src/platform/audio_output.c)
    target_link_libraries(ab_audio_output_tests PRIVATE ab_game console_common)
    add_test(NAME game_audio_output_queue COMMAND ab_audio_output_tests)
    add_executable(ab_preview_options_tests tests/preview_options.c)
    target_link_libraries(ab_preview_options_tests PRIVATE console_common)
    add_test(NAME preview_recording_options COMMAND ab_preview_options_tests)
    add_subdirectory(Common/tests common-tests)
    add_test(NAME preview_cli_help COMMAND ab_preview --help)
    add_executable(ab_string_map_tests tests/string_map.c)
    target_link_libraries(ab_string_map_tests PRIVATE ab_base)
    add_test(NAME string_map_edges COMMAND ab_string_map_tests)
    add_executable(ab_identity_map_tests tests/identity_map.c)
    target_link_libraries(ab_identity_map_tests PRIVATE ab_base)
    add_test(NAME identity_map_edges COMMAND ab_identity_map_tests)
    set(AB_REFERENCE_IDENTITY_MAP "" CACHE FILEPATH
        "Private original Android table-key dictionary corpus")
    if(AB_REFERENCE_IDENTITY_MAP)
        file(SHA256 "${AB_REFERENCE_IDENTITY_MAP}" identity_map_hash)
        if(NOT identity_map_hash STREQUAL
            "a94ee2eeb12fdde90d5dcbd0269be3b4ce68472023160ffae07c9cfbbc98ad2d")
            message(FATAL_ERROR "Unexpected original table-key dictionary corpus")
        endif()
        add_test(NAME identity_map_native COMMAND ab_identity_map_tests
            "${AB_REFERENCE_IDENTITY_MAP}")
    endif()
    add_executable(ab_classic_level5_tests tests/classic_level5.c)
    target_link_libraries(ab_classic_level5_tests PRIVATE ab_game)
    add_test(NAME classic_level5_edges COMMAND ab_classic_level5_tests)
    add_executable(ab_world_iteration_tests tests/world_iteration.c)
    target_link_libraries(ab_world_iteration_tests PRIVATE ab_base)
    add_test(NAME world_iteration_edges COMMAND ab_world_iteration_tests)
    set(AB_REFERENCE_WORLD_ITERATION "" CACHE FILEPATH
        "Optional original mutable world next traversal corpus")
    if(AB_REFERENCE_WORLD_ITERATION)
        add_test(NAME world_iteration_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_WORLD_ITERATION}"
                "-DCHECKER=$<TARGET_FILE:ab_world_iteration_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_world_iteration.cmake")
    endif()
    set(AB_REFERENCE_STRING_MAP "" CACHE FILEPATH
        "Optional original continuing string map mutation/layout/iteration corpus")
    set(AB_REFERENCE_STRING_MAP_GC "" CACHE FILEPATH
        "Optional original string map collection/dead-key mutation corpus")
    if(AB_REFERENCE_STRING_MAP_GC)
        add_test(NAME string_map_gc_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_STRING_MAP_GC}"
                "-DCHECKER=$<TARGET_FILE:ab_string_map_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_string_map_gc_reference.cmake")
    endif()
    if(AB_REFERENCE_STRING_MAP)
        add_test(NAME string_map_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_STRING_MAP}"
                "-DCHECKER=$<TARGET_FILE:ab_string_map_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_string_map_reference.cmake")
    endif()
    add_executable(ab_target_resource_tests tests/target_resources.c)
    add_executable(ab_lua_text_write_tests tests/lua_text_write.c)
    target_link_libraries(ab_lua_text_write_tests PRIVATE ab_resource)
    add_test(NAME saved_table_write_edges COMMAND ab_lua_text_write_tests)
    set(AB_REFERENCE_LUA_WRITE "" CACHE FILEPATH
        "Optional original saved-table writer output corpus")
    if(AB_REFERENCE_LUA_WRITE)
        add_test(NAME saved_table_write_android
            COMMAND "${CMAKE_COMMAND}" "-DREFERENCE=${AB_REFERENCE_LUA_WRITE}"
                "-DCHECKER=$<TARGET_FILE:ab_lua_text_write_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_lua_write_reference.cmake")
    endif()
    add_executable(ab_target_save_key_tests tests/target_save_key.c)
    target_link_libraries(ab_target_save_key_tests PRIVATE ab_resource)
    target_link_libraries(ab_target_resource_tests PRIVATE ab_resource)
    add_test(NAME target_resource_key_bounds COMMAND ab_target_resource_tests)
    add_executable(ab_random_tests tests/random.c)
    target_link_libraries(ab_random_tests PRIVATE ab_numeric)
    add_test(NAME script_random_edges COMMAND ab_random_tests)
    add_executable(ab_engine_random_tests tests/engine_random.c)
    target_link_libraries(ab_engine_random_tests PRIVATE ab_numeric)
    add_test(NAME engine_random_edges COMMAND ab_engine_random_tests)
    set(AB_REFERENCE_ENGINE_RANDOM "" CACHE FILEPATH
        "Optional native particle-engine random stream checkpoints")
    if(AB_REFERENCE_ENGINE_RANDOM)
        add_test(NAME engine_random_android
            COMMAND ab_engine_random_tests "${AB_REFERENCE_ENGINE_RANDOM}")
    endif()
    set(AB_REFERENCE_LIVE_RANDOM "" CACHE FILEPATH
        "Optional captured live native random arguments/results and state pairs")
    if(AB_REFERENCE_LIVE_RANDOM)
        add_test(NAME script_random_live_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_LIVE_RANDOM}"
                "-DCHECKER=$<TARGET_FILE:ab_random_tests>"
                -DKIND=live
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_random_reference.cmake")
    endif()
    set(AB_REFERENCE_RANDOM "" CACHE FILEPATH
        "Optional recorded 65536-generator/65536-script native random corpus")
    set(AB_REFERENCE_RANDOM_STARTUP "" CACHE FILEPATH
        "Optional recorded 65536-case native startup seed conversion corpus")
    if(AB_REFERENCE_RANDOM_STARTUP)
        add_test(NAME script_random_startup_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_RANDOM_STARTUP}"
                "-DCHECKER=$<TARGET_FILE:ab_random_tests>"
                -DKIND=startup
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_random_reference.cmake")
    endif()
    if(AB_REFERENCE_RANDOM)
        add_test(NAME script_random_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_RANDOM}"
                "-DCHECKER=$<TARGET_FILE:ab_random_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_random_reference.cmake")
    endif()
    add_test(NAME preview_cli_invalid_frames COMMAND ab_preview --frames 0)
    set_tests_properties(preview_cli_invalid_frames PROPERTIES WILL_FAIL TRUE)
    add_executable(ab_angle_tests tests/angle.c)
    target_link_libraries(ab_angle_tests PRIVATE ab_numeric)
    add_test(NAME angle_component_edges COMMAND ab_angle_tests)
    set(AB_REFERENCE_ANGLE_COMPONENTS "" CACHE FILEPATH
        "Optional recorded 65536-case Android atan2f component corpus")
    set(AB_REFERENCE_SCRIPT_ANGLE "" CACHE FILEPATH
        "Optional recorded 65536-case Android Lua atan2 promotion/rounding corpus")
    if(AB_REFERENCE_SCRIPT_ANGLE)
        add_test(NAME script_angle_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_SCRIPT_ANGLE}"
                "-DCHECKER=$<TARGET_FILE:ab_angle_tests>"
                -DKIND=script
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_angle_components_reference.cmake")
    endif()
    if(AB_REFERENCE_ANGLE_COMPONENTS)
        add_test(NAME angle_components_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_ANGLE_COMPONENTS}"
                "-DCHECKER=$<TARGET_FILE:ab_angle_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_angle_components_reference.cmake")
    endif()
    add_executable(ab_body_velocity_tests tests/body_velocity.c)
    target_link_libraries(ab_body_velocity_tests PRIVATE ab_physics)
    add_test(NAME physics_body_velocity_edges COMMAND ab_body_velocity_tests)
    add_executable(ab_joint_geometry_tests tests/joint_geometry.c)
    target_link_libraries(ab_joint_geometry_tests PRIVATE ab_physics)
    add_test(NAME physics_joint_geometry_edges COMMAND ab_joint_geometry_tests)
    add_executable(ab_matrix_inverse_tests tests/matrix_inverse.c)
    target_link_libraries(ab_matrix_inverse_tests PRIVATE ab_physics)
    add_test(NAME physics_matrix_inverse_edges COMMAND ab_matrix_inverse_tests)
    add_executable(ab_matrix_solve_tests tests/matrix_solve.c)
    target_link_libraries(ab_matrix_solve_tests PRIVATE ab_physics)
    add_test(NAME physics_matrix_solve_edges COMMAND ab_matrix_solve_tests)
    set(AB_REFERENCE_MATRIX_SOLVE "" CACHE FILEPATH
        "Optional recorded native symmetric 2D/3D direct matrix solves")
    if(AB_REFERENCE_MATRIX_SOLVE)
        add_test(NAME physics_matrix_solve_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_MATRIX_SOLVE}"
                "-DCHECKER=$<TARGET_FILE:ab_matrix_solve_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_matrix_solve_reference.cmake")
    endif()
    add_executable(ab_weld_initialize_tests tests/weld_initialize.c)
    target_link_libraries(ab_weld_initialize_tests PRIVATE ab_physics)
    add_test(NAME physics_weld_initialize_edges COMMAND ab_weld_initialize_tests)
    add_executable(ab_weld_velocity_tests tests/weld_initialize.c)
    target_compile_definitions(ab_weld_velocity_tests PRIVATE AB_TEST_WELD_VELOCITY=1)
    target_link_libraries(ab_weld_velocity_tests PRIVATE ab_physics)
    add_test(NAME physics_weld_velocity_edges COMMAND ab_weld_velocity_tests)
    add_executable(ab_weld_position_tests tests/weld_position.c)
    target_link_libraries(ab_weld_position_tests PRIVATE ab_physics)
    add_test(NAME physics_weld_position_edges COMMAND ab_weld_position_tests)
    add_executable(ab_distance_joint_tests tests/distance_joint.c)
    target_link_libraries(ab_distance_joint_tests PRIVATE ab_physics)
    add_test(NAME physics_distance_joint_edges COMMAND ab_distance_joint_tests)
    add_executable(ab_revolute_initialize_tests tests/revolute_initialize.c)
    target_link_libraries(ab_revolute_initialize_tests PRIVATE ab_physics)
    add_test(NAME physics_revolute_initialize_edges COMMAND ab_revolute_initialize_tests)
    add_executable(ab_revolute_velocity_tests tests/revolute_initialize.c)
    target_compile_definitions(ab_revolute_velocity_tests PRIVATE AB_TEST_REVOLUTE_VELOCITY=1)
    target_link_libraries(ab_revolute_velocity_tests PRIVATE ab_physics)
    add_test(NAME physics_revolute_velocity_edges COMMAND ab_revolute_velocity_tests)
    add_executable(ab_revolute_position_tests tests/revolute_position.c)
    target_link_libraries(ab_revolute_position_tests PRIVATE ab_physics)
    add_test(NAME physics_revolute_position_edges COMMAND ab_revolute_position_tests)
    add_executable(ab_prismatic_initialize_tests tests/prismatic_initialize.c)
    target_link_libraries(ab_prismatic_initialize_tests PRIVATE ab_physics)
    add_test(NAME physics_prismatic_initialize_edges COMMAND ab_prismatic_initialize_tests)
    add_executable(ab_prismatic_velocity_tests tests/prismatic_initialize.c)
    target_compile_definitions(ab_prismatic_velocity_tests PRIVATE AB_TEST_PRISMATIC_VELOCITY=1)
    target_link_libraries(ab_prismatic_velocity_tests PRIVATE ab_physics)
    add_test(NAME physics_prismatic_velocity_edges COMMAND ab_prismatic_velocity_tests)
    add_executable(ab_prismatic_position_tests tests/prismatic_position.c)
    target_link_libraries(ab_prismatic_position_tests PRIVATE ab_physics)
    add_test(NAME physics_prismatic_position_edges COMMAND ab_prismatic_position_tests)
    set(AB_REFERENCE_PRISMATIC_POSITION "" CACHE FILEPATH
        "Optional recorded native consecutive prismatic position/limit corrections")
    if(AB_REFERENCE_PRISMATIC_POSITION)
        add_test(NAME physics_prismatic_position_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_PRISMATIC_POSITION}"
                "-DCHECKER=$<TARGET_FILE:ab_prismatic_position_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_prismatic_position_reference.cmake")
    endif()
    set(AB_REFERENCE_PRISMATIC_VELOCITY "" CACHE FILEPATH
        "Optional recorded native consecutive prismatic velocity/motor/limit passes")
    if(AB_REFERENCE_PRISMATIC_VELOCITY)
        add_test(NAME physics_prismatic_velocity_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_PRISMATIC_VELOCITY}"
                "-DCHECKER=$<TARGET_FILE:ab_prismatic_velocity_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_prismatic_velocity_reference.cmake")
    endif()
    set(AB_REFERENCE_PRISMATIC_INITIALIZE "" CACHE FILEPATH
        "Optional recorded native prismatic construction/initialization states")
    if(AB_REFERENCE_PRISMATIC_INITIALIZE)
        add_test(NAME physics_prismatic_initialize_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_PRISMATIC_INITIALIZE}"
                "-DCHECKER=$<TARGET_FILE:ab_prismatic_initialize_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_prismatic_initialize_reference.cmake")
    endif()
    set(AB_REFERENCE_REVOLUTE_POSITION "" CACHE FILEPATH
        "Optional recorded native consecutive revolute position/limit corrections")
    if(AB_REFERENCE_REVOLUTE_POSITION)
        add_test(NAME physics_revolute_position_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_REVOLUTE_POSITION}"
                "-DCHECKER=$<TARGET_FILE:ab_revolute_position_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_revolute_position_reference.cmake")
    endif()
    set(AB_REFERENCE_REVOLUTE_VELOCITY "" CACHE FILEPATH
        "Optional recorded native consecutive revolute velocity/motor/limit passes")
    if(AB_REFERENCE_REVOLUTE_VELOCITY)
        add_test(NAME physics_revolute_velocity_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_REVOLUTE_VELOCITY}"
                "-DCHECKER=$<TARGET_FILE:ab_revolute_velocity_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_revolute_velocity_reference.cmake")
    endif()
    set(AB_REFERENCE_REVOLUTE_INITIALIZE "" CACHE FILEPATH
        "Optional recorded native revolute initialization/limit/warm-start states")
    if(AB_REFERENCE_REVOLUTE_INITIALIZE)
        add_test(NAME physics_revolute_initialize_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_REVOLUTE_INITIALIZE}"
                "-DCHECKER=$<TARGET_FILE:ab_revolute_initialize_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_revolute_initialize_reference.cmake")
    endif()
    set(AB_REFERENCE_DISTANCE_JOINT "" CACHE FILEPATH
        "Optional recorded native distance initialization/velocity/position passes")
    if(AB_REFERENCE_DISTANCE_JOINT)
        add_test(NAME physics_distance_joint_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_DISTANCE_JOINT}"
                "-DCHECKER=$<TARGET_FILE:ab_distance_joint_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_distance_joint_reference.cmake")
    endif()
    set(AB_REFERENCE_WELD_POSITION "" CACHE FILEPATH
        "Optional recorded native consecutive weld position corrections")
    if(AB_REFERENCE_WELD_POSITION)
        add_test(NAME physics_weld_position_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_WELD_POSITION}"
                "-DCHECKER=$<TARGET_FILE:ab_weld_position_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_weld_position_reference.cmake")
    endif()
    set(AB_REFERENCE_WELD_VELOCITY "" CACHE FILEPATH
        "Optional recorded native consecutive weld velocity iterations")
    if(AB_REFERENCE_WELD_VELOCITY)
        add_test(NAME physics_weld_velocity_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_WELD_VELOCITY}"
                "-DCHECKER=$<TARGET_FILE:ab_weld_velocity_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_weld_velocity_reference.cmake")
    endif()
    set(AB_REFERENCE_WELD_INITIALIZE "" CACHE FILEPATH
        "Optional recorded native weld initialization/warm-start corpus")
    if(AB_REFERENCE_WELD_INITIALIZE)
        add_test(NAME physics_weld_initialize_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_WELD_INITIALIZE}"
                "-DCHECKER=$<TARGET_FILE:ab_weld_initialize_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_weld_initialize_reference.cmake")
    endif()
    set(AB_REFERENCE_MATRIX_INVERSE "" CACHE FILEPATH
        "Optional recorded native symmetric 2D/3D matrix inverse corpus")
    if(AB_REFERENCE_MATRIX_INVERSE)
        add_test(NAME physics_matrix_inverse_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_MATRIX_INVERSE}"
                "-DCHECKER=$<TARGET_FILE:ab_matrix_inverse_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_matrix_inverse_reference.cmake")
    endif()
    set(AB_REFERENCE_JOINT_GEOMETRY "" CACHE FILEPATH
        "Optional recorded native distance/weld/revolute/prismatic geometry corpus")
    if(AB_REFERENCE_JOINT_GEOMETRY)
        add_test(NAME physics_joint_geometry_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_JOINT_GEOMETRY}"
                "-DCHECKER=$<TARGET_FILE:ab_joint_geometry_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_joint_geometry_reference.cmake")
    endif()
    add_executable(ab_object_friction_tests tests/object_friction.c)
    target_link_libraries(ab_object_friction_tests PRIVATE ab_game)
    add_test(NAME game_object_friction_edges COMMAND ab_object_friction_tests)
    add_executable(ab_object_forces_tests tests/object_forces.c)
    target_link_libraries(ab_object_forces_tests PRIVATE ab_game)
    add_test(NAME game_object_forces_edges COMMAND ab_object_forces_tests)
    set(AB_REFERENCE_OBJECT_FORCES "" CACHE FILEPATH
        "Optional recorded 4096-case native ordinary force-adder corpus")
    set(AB_REFERENCE_OBJECT_FORCES_RELATIVE "" CACHE FILEPATH
        "Optional recorded 4096-case native relative force-adder corpus")
    set(AB_REFERENCE_OBJECT_FORCES_INTERVAL "" CACHE FILEPATH
        "Optional recorded 4096-case native interval force-adder corpus")
    set(AB_REFERENCE_OBJECT_FORCES_AUDIO "" CACHE FILEPATH
        "Optional recorded 4096-case native force-pulse sound/mixer corpus")
    if(AB_REFERENCE_OBJECT_FORCES_AUDIO)
        add_test(NAME game_object_forces_audio_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_OBJECT_FORCES_AUDIO}"
                "-DCHECKER=$<TARGET_FILE:ab_object_forces_tests>"
                -DKIND=audio
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_object_forces_reference.cmake")
    endif()
    if(AB_REFERENCE_OBJECT_FORCES_INTERVAL)
        add_test(NAME game_object_forces_interval_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_OBJECT_FORCES_INTERVAL}"
                "-DCHECKER=$<TARGET_FILE:ab_object_forces_tests>"
                -DKIND=interval
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_object_forces_reference.cmake")
    endif()
    if(AB_REFERENCE_OBJECT_FORCES_RELATIVE)
        add_test(NAME game_object_forces_relative_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_OBJECT_FORCES_RELATIVE}"
                "-DCHECKER=$<TARGET_FILE:ab_object_forces_tests>"
                -DKIND=relative
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_object_forces_reference.cmake")
    endif()
    if(AB_REFERENCE_OBJECT_FORCES)
        add_test(NAME game_object_forces_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_OBJECT_FORCES}"
                "-DCHECKER=$<TARGET_FILE:ab_object_forces_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_object_forces_reference.cmake")
    endif()
    set(AB_REFERENCE_OBJECT_FRICTION "" CACHE FILEPATH
        "Optional recorded 4096-case native object-friction corpus")
    if(AB_REFERENCE_OBJECT_FRICTION)
        add_test(NAME game_object_friction_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_OBJECT_FRICTION}"
                "-DCHECKER=$<TARGET_FILE:ab_object_friction_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_object_friction_reference.cmake")
    endif()
    set(AB_REFERENCE_BODY_VELOCITY "" CACHE FILEPATH
        "Optional recorded 4096-case native GameLua velocity/impulse corpus")
    set(AB_REFERENCE_BODY_ANGULAR "" CACHE FILEPATH
        "Optional recorded 4096-case native GameLua angular velocity corpus")
    if(AB_REFERENCE_BODY_ANGULAR)
        add_test(NAME physics_body_angular_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_BODY_ANGULAR}"
                "-DCHECKER=$<TARGET_FILE:ab_body_velocity_tests>"
                -DKIND=angular
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_body_velocity_reference.cmake")
    endif()
    if(AB_REFERENCE_BODY_VELOCITY)
        add_test(NAME physics_body_velocity_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_BODY_VELOCITY}"
                "-DCHECKER=$<TARGET_FILE:ab_body_velocity_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_body_velocity_reference.cmake")
    endif()
    add_executable(ab_frame_clock_tests tests/frame_clock.c)
    target_link_libraries(ab_frame_clock_tests PRIVATE ab_game)
    add_test(NAME game_frame_clock_edges COMMAND ab_frame_clock_tests)
    set(AB_REFERENCE_FRAME_CLOCK "" CACHE FILEPATH
        "Optional recorded 108-call original GameLua update clock corpus")
    if(AB_REFERENCE_FRAME_CLOCK)
        add_test(NAME game_frame_clock_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_FRAME_CLOCK}"
                "-DCHECKER=$<TARGET_FILE:ab_frame_clock_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_frame_clock_reference.cmake")
    endif()

    add_executable(ab_sling_scope_tests tests/sling_scope.c)
    target_link_libraries(ab_sling_scope_tests PRIVATE ab_game)
    add_test(NAME sling_scope_edges COMMAND ab_sling_scope_tests)
    set(AB_REFERENCE_SLING_SCOPE "" CACHE FILEPATH
        "Private original Android Sling Scope geometry corpus")
    if(AB_REFERENCE_SLING_SCOPE)
        file(SHA256 "${AB_REFERENCE_SLING_SCOPE}" sling_scope_hash)
        if(NOT sling_scope_hash STREQUAL
            "3671d0184625f7f5f66a43b85f75f75dc67a6ebd15eaec9c6f6581fa88019349")
            message(FATAL_ERROR "Unexpected original Sling Scope geometry corpus")
        endif()
        add_test(NAME sling_scope_native COMMAND ab_sling_scope_tests
            "${AB_REFERENCE_SLING_SCOPE}")
    endif()
    add_executable(ab_slingshot_tests tests/slingshot.c)
    target_link_libraries(ab_slingshot_tests PRIVATE ab_game)
    add_test(NAME game_slingshot_edges COMMAND ab_slingshot_tests)
    add_executable(ab_bird_shot_tests tests/bird_shot.c)
    target_link_libraries(ab_bird_shot_tests PRIVATE ab_game)
    add_test(NAME game_bird_shot_edges COMMAND ab_bird_shot_tests)
    add_executable(ab_sling_cancel_tests tests/sling_cancel.c)
    target_link_libraries(ab_sling_cancel_tests PRIVATE ab_game)
    add_test(NAME game_sling_cancel_edges COMMAND ab_sling_cancel_tests)
    add_executable(ab_sling_delta_tests tests/sling_delta.c)
    target_link_libraries(ab_sling_delta_tests PRIVATE ab_game)
    add_test(NAME game_sling_delta_edges COMMAND ab_sling_delta_tests)
    add_executable(ab_game_sling_mouse_tests tests/game_sling_mouse.c)
    target_link_libraries(ab_game_sling_mouse_tests PRIVATE ab_game)
    add_test(NAME game_sling_mouse_edges COMMAND ab_game_sling_mouse_tests)
    add_executable(ab_game_update_tail_tests tests/game_update_tail.c)
    target_link_libraries(ab_game_update_tail_tests PRIVATE ab_game)
    add_test(NAME game_update_tail_edges COMMAND ab_game_update_tail_tests)
    add_executable(ab_asset_profile_tests tests/asset_profile.c)
    target_link_libraries(ab_asset_profile_tests PRIVATE ab_game)
    add_test(NAME asset_profile_edges COMMAND ab_asset_profile_tests)
    add_executable(ab_asset_profile_bundle_tests tests/asset_profile_bundles.c)
    target_link_libraries(ab_asset_profile_bundle_tests PRIVATE ab_game)
    add_test(NAME asset_profile_bundle_edges COMMAND ab_asset_profile_bundle_tests)
    set(AB_REFERENCE_ASSET_PROFILE_BUNDLES "" CACHE FILEPATH
        "Optional original Android profile bundle metadata")
    if(AB_REFERENCE_ASSET_PROFILE_BUNDLES)
        add_test(NAME asset_profile_bundle_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_ASSET_PROFILE_BUNDLES}"
                "-DCHECKER=$<TARGET_FILE:ab_asset_profile_bundle_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_asset_profile_bundles_reference.cmake")
    endif()
    add_executable(ab_asset_splash_tests tests/asset_splash.c)
    target_link_libraries(ab_asset_splash_tests PRIVATE ab_game)
    add_test(NAME asset_splash_edges COMMAND ab_asset_splash_tests)
    set(AB_REFERENCE_ASSET_SPLASH "" CACHE FILEPATH
        "Optional original Android splash resolution/profile corpus")
    if(AB_REFERENCE_ASSET_SPLASH)
        add_test(NAME asset_splash_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_ASSET_SPLASH}"
                "-DCHECKER=$<TARGET_FILE:ab_asset_splash_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_asset_splash_reference.cmake")
    endif()
    add_executable(ab_asset_group_tests tests/asset_groups.c)
    target_link_libraries(ab_asset_group_tests PRIVATE ab_game)
    add_test(NAME asset_group_edges COMMAND ab_asset_group_tests)
    add_executable(ab_resource_path_tests tests/resource_path.c)
    target_link_libraries(ab_resource_path_tests PRIVATE ab_game)
    add_test(NAME asset_resource_path_edges COMMAND ab_resource_path_tests)
    set(AB_REFERENCE_RESOURCE_PATH "" CACHE FILEPATH
        "Optional original resource PathName key corpus")
    if(AB_REFERENCE_RESOURCE_PATH)
        add_test(NAME asset_resource_path_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_RESOURCE_PATH}"
                "-DCHECKER=$<TARGET_FILE:ab_resource_path_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_resource_path_reference.cmake")
    endif()
    add_executable(ab_asset_bundle_tests tests/asset_bundles.c)
    target_link_libraries(ab_asset_bundle_tests PRIVATE ab_game)
    add_test(NAME asset_bundle_edges COMMAND ab_asset_bundle_tests)
    add_executable(ab_theme_asset_tests tests/theme_assets.c)
    target_link_libraries(ab_theme_asset_tests PRIVATE ab_asset_resources)
    add_test(NAME asset_theme_edges COMMAND ab_theme_asset_tests)
    set(AB_REFERENCE_THEME_ASSET "" CACHE FILEPATH
        "Original Android theme graphics transition corpus")
    if(AB_REFERENCE_THEME_ASSET)
        add_test(NAME asset_theme_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_THEME_ASSET}"
                "-DCHECKER=$<TARGET_FILE:ab_theme_asset_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_theme_asset_reference.cmake")
    endif()
    set(AB_REFERENCE_ASSET_BUNDLE "" CACHE FILEPATH
        "Optional original asset bundle expansion corpus")
    if(AB_REFERENCE_ASSET_BUNDLE)
        add_test(NAME asset_bundle_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_ASSET_BUNDLE}"
                "-DCHECKER=$<TARGET_FILE:ab_asset_bundle_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_asset_bundle_reference.cmake")
    endif()
    add_executable(ab_asset_load_list_tests tests/asset_load_lists.c)
    target_link_libraries(ab_asset_load_list_tests PRIVATE ab_game)
    add_test(NAME asset_load_lists_edges COMMAND ab_asset_load_list_tests)
    add_executable(ab_android_assets_tests tests/android_assets.c)
    target_link_libraries(ab_android_assets_tests PRIVATE ab_game)
    add_test(NAME asset_android_owner_edges COMMAND ab_android_assets_tests)
    add_executable(ab_asset_resource_tests tests/asset_resources.c)
    target_link_libraries(ab_asset_resource_tests PRIVATE ab_asset_resources)
    add_test(NAME asset_resources_edges COMMAND ab_asset_resource_tests)
    add_executable(ab_asset_texture_tests tests/asset_textures.c)
    target_link_libraries(ab_asset_texture_tests PRIVATE ab_asset_resources)
    add_test(NAME asset_textures_edges COMMAND ab_asset_texture_tests)
    add_executable(ab_asset_texture_gpu_tests tests/asset_textures.c)
    target_compile_definitions(ab_asset_texture_gpu_tests PRIVATE AB_ASSET_TEXTURES_COMMON=1)
    target_link_libraries(ab_asset_texture_gpu_tests PRIVATE ab_asset_resources ab_common_renderer)
    cc_add_platform(ab_asset_texture_gpu_tests "${AB_BACKEND}")
    add_executable(ab_webp_tests tests/webp.c)
    target_link_libraries(ab_webp_tests PRIVATE ab_resource)
    add_test(NAME image_webp_edges COMMAND ab_webp_tests)
    add_executable(ab_webp_bits_tests tests/webp_bits.c)
    target_link_libraries(ab_webp_bits_tests PRIVATE ab_resource)
    add_test(NAME image_webp_bits_edges COMMAND ab_webp_bits_tests)
    add_executable(ab_webp_color_tests tests/webp_color.c)
    target_link_libraries(ab_webp_color_tests PRIVATE ab_resource)
    add_test(NAME image_webp_color_edges COMMAND ab_webp_color_tests)
    add_executable(ab_vp8_boolean_tests tests/vp8_boolean.c)
    target_link_libraries(ab_vp8_boolean_tests PRIVATE ab_resource)
    add_test(NAME image_vp8_boolean_edges COMMAND ab_vp8_boolean_tests)
    add_executable(ab_vp8_header_tests tests/vp8_header.c)
    target_link_libraries(ab_vp8_header_tests PRIVATE ab_resource)
    add_test(NAME image_vp8_header_edges COMMAND ab_vp8_header_tests)
    add_executable(ab_vp8_mode_tests tests/vp8_modes.c)
    target_link_libraries(ab_vp8_mode_tests PRIVATE ab_resource)
    add_test(NAME image_vp8_modes_edges COMMAND ab_vp8_mode_tests)
    add_executable(ab_vp8_coefficient_tests tests/vp8_coefficients.c)
    target_link_libraries(ab_vp8_coefficient_tests PRIVATE ab_resource)
    add_test(NAME image_vp8_coefficients_edges COMMAND ab_vp8_coefficient_tests)
    add_executable(ab_vp8_residual_tests tests/vp8_residuals.c)
    target_link_libraries(ab_vp8_residual_tests PRIVATE ab_resource)
    add_test(NAME image_vp8_residuals_edges COMMAND ab_vp8_residual_tests)
    add_executable(ab_vp8_transform_tests tests/vp8_transform.c)
    target_link_libraries(ab_vp8_transform_tests PRIVATE ab_resource)
    add_test(NAME image_vp8_transform_edges COMMAND ab_vp8_transform_tests)
    add_executable(ab_vp8_predict_tests tests/vp8_predict.c)
    target_link_libraries(ab_vp8_predict_tests PRIVATE ab_resource)
    add_test(NAME image_vp8_predict_edges COMMAND ab_vp8_predict_tests)
    add_executable(ab_vp8_filter_tests tests/vp8_filter.c)
    target_link_libraries(ab_vp8_filter_tests PRIVATE ab_resource)
    add_test(NAME image_vp8_filter_edges COMMAND ab_vp8_filter_tests)
    add_executable(ab_vp8_decode_tests tests/vp8_decode.c)
    target_link_libraries(ab_vp8_decode_tests PRIVATE ab_resource)
    add_test(NAME image_vp8_decode_edges COMMAND ab_vp8_decode_tests)
    set(AB_REFERENCE_VP8_PLANES "" CACHE PATH
        "Optional original APK VP8 filtered/unfiltered YUV plane directory")
    add_executable(ab_webp_decode_tests tests/webp_decode.c)
    target_link_libraries(ab_webp_decode_tests PRIVATE ab_resource)
    add_test(NAME image_webp_decode_edges COMMAND ab_webp_decode_tests)
    set(AB_REFERENCE_VP8_FILTER "" CACHE FILEPATH
        "Optional original VP8 normal/simple loop filter corpus")
    if(AB_REFERENCE_VP8_FILTER)
        add_test(NAME image_vp8_filter_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_VP8_FILTER}"
                "-DCHECKER=$<TARGET_FILE:ab_vp8_filter_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_vp8_filter_reference.cmake")
    endif()
    set(AB_REFERENCE_VP8_PREDICT "" CACHE FILEPATH
        "Optional original VP8 intra pixel predictor corpus")
    if(AB_REFERENCE_VP8_PREDICT)
        add_test(NAME image_vp8_predict_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_VP8_PREDICT}"
                "-DCHECKER=$<TARGET_FILE:ab_vp8_predict_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_vp8_predict_reference.cmake")
    endif()
    set(AB_REFERENCE_VP8_TRANSFORM "" CACHE FILEPATH
        "Optional original VP8 inverse Walsh/DCT corpus")
    if(AB_REFERENCE_VP8_TRANSFORM)
        add_test(NAME image_vp8_transform_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_VP8_TRANSFORM}"
                "-DCHECKER=$<TARGET_FILE:ab_vp8_transform_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_vp8_transform_reference.cmake")
    endif()
    set(AB_REFERENCE_VP8_RESIDUALS "" CACHE PATH
        "Optional original APK VP8 residual/context capture directory")
    set(AB_REFERENCE_VP8_COEFFICIENTS "" CACHE FILEPATH
        "Optional original VP8 coefficient block corpus")
    if(AB_REFERENCE_VP8_COEFFICIENTS)
        add_test(NAME image_vp8_coefficients_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_VP8_COEFFICIENTS}"
                "-DCHECKER=$<TARGET_FILE:ab_vp8_coefficient_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_vp8_coefficients_reference.cmake")
    endif()
    set(AB_REFERENCE_VP8_MODES "" CACHE FILEPATH
        "Optional original APK VP8 macroblock modes and row arithmetic state")
    set(AB_REFERENCE_VP8_HEADER "" CACHE FILEPATH
        "Optional original APK VP8 headers and continuing arithmetic state")
    set(AB_REFERENCE_VP8_BOOLEAN "" CACHE FILEPATH
        "Optional original VP8 signed/unsigned arithmetic-field corpus")
    if(AB_REFERENCE_VP8_BOOLEAN)
        add_test(NAME image_vp8_boolean_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_VP8_BOOLEAN}"
                "-DCHECKER=$<TARGET_FILE:ab_vp8_boolean_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_vp8_boolean_reference.cmake")
    endif()
    set(AB_REFERENCE_WEBP_UPSAMPLE "" CACHE FILEPATH
        "Optional original WebP RGBA line-pair corpus")
    if(AB_REFERENCE_WEBP_UPSAMPLE)
        add_test(NAME image_webp_color_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_WEBP_UPSAMPLE}"
                "-DCHECKER=$<TARGET_FILE:ab_webp_color_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_webp_color_reference.cmake")
    endif()
    set(AB_REFERENCE_WEBP_BITS "" CACHE FILEPATH
        "Optional original VP8L bit-field reader corpus")
    if(AB_REFERENCE_WEBP_BITS)
        add_test(NAME image_webp_bits_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_WEBP_BITS}"
                "-DCHECKER=$<TARGET_FILE:ab_webp_bits_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_webp_bits_reference.cmake")
    endif()
    set(AB_REFERENCE_WEBP_ALPHA "" CACHE FILEPATH
        "Optional original WebP alpha predictor corpus")
    if(AB_REFERENCE_WEBP_ALPHA)
        add_test(NAME image_webp_alpha_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_WEBP_ALPHA}"
                "-DCHECKER=$<TARGET_FILE:ab_webp_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_webp_alpha_reference.cmake")
    endif()
    set(AB_REFERENCE_WEBP_IMAGES "" CACHE FILEPATH
        "Optional original APK WebP image decoder metadata")
    set(AB_REFERENCE_WEBP_PIXELS "" CACHE PATH
        "Optional original APK WebP RGBA reference directory")
    add_executable(ab_asset_resource_live_tests tests/asset_resources_live.c)
    target_link_libraries(ab_asset_resource_live_tests PRIVATE ab_asset_resources)
    set(AB_REFERENCE_ASSET_RESOURCE_STATE "" CACHE FILEPATH
        "Optional selected original live Android resource state")
    set(AB_REFERENCE_ASSET_IMAGE_STATE "" CACHE FILEPATH
        "Optional selected original live Android image/texture descriptors")
    add_executable(ab_sprite_registry_tests tests/sprite_registry.c)
    target_link_libraries(ab_sprite_registry_tests PRIVATE ab_resource)
    add_test(NAME asset_sprite_registry_edges COMMAND ab_sprite_registry_tests)
    add_executable(ab_resource_owner_tests tests/resource_owners.c)
    target_link_libraries(ab_resource_owner_tests PRIVATE ab_game)
    add_test(NAME asset_resource_owner_edges COMMAND ab_resource_owner_tests)
    set(AB_REFERENCE_RESOURCE_CACHE "" CACHE FILEPATH
        "Optional original no-option resource create cache-hit corpus")
    if(AB_REFERENCE_RESOURCE_CACHE)
        add_test(NAME asset_resource_cache_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_RESOURCE_CACHE}"
                "-DCHECKER=$<TARGET_FILE:ab_resource_owner_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_resource_cache_reference.cmake")
    endif()
    add_executable(ab_composite_part_tests tests/composite_part.c)
    target_link_libraries(ab_composite_part_tests PRIVATE ab_game ab_render)
    add_test(NAME asset_composite_part_edges COMMAND ab_composite_part_tests)
    add_executable(ab_composite_resource_tests tests/composite_resource.c)
    target_link_libraries(ab_composite_resource_tests PRIVATE ab_game ab_render)
    add_test(NAME asset_composite_resource_edges COMMAND ab_composite_resource_tests)
    set(AB_REFERENCE_COMPOSITE_PART "" CACHE FILEPATH
        "Optional original KA3D/RVIO composite part loader corpus")
    if(AB_REFERENCE_COMPOSITE_PART)
        add_test(NAME asset_composite_part_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_COMPOSITE_PART}"
                "-DCHECKER=$<TARGET_FILE:ab_composite_part_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_composite_part_reference.cmake")
    endif()
    add_executable(ab_sprite_resource_tests tests/sprite_resource.c)
    target_link_libraries(ab_sprite_resource_tests PRIVATE ab_game)
    add_test(NAME asset_sprite_resource_edges COMMAND ab_sprite_resource_tests)
    set(AB_REFERENCE_RESOURCE_OWNERS "" CACHE FILEPATH
        "Optional original resource ownership/registry corpus")
    if(AB_REFERENCE_RESOURCE_OWNERS)
        add_test(NAME asset_resource_owner_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_RESOURCE_OWNERS}"
                "-DCHECKER=$<TARGET_FILE:ab_resource_owner_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_resource_owners_reference.cmake")
    endif()
    set(AB_REFERENCE_SPRITE_REGISTRY "" CACHE FILEPATH
        "Optional original sprite/composite registry corpus")
    if(AB_REFERENCE_SPRITE_REGISTRY)
        add_test(NAME asset_sprite_registry_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_SPRITE_REGISTRY}"
                "-DCHECKER=$<TARGET_FILE:ab_sprite_registry_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_sprite_registry_reference.cmake")
    endif()
    set(AB_REFERENCE_ASSET_GROUP "" CACHE FILEPATH
        "Optional original asset-group management resource/list corpus")
    if(AB_REFERENCE_ASSET_GROUP)
        add_test(NAME asset_group_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_ASSET_GROUP}"
                "-DCHECKER=$<TARGET_FILE:ab_asset_group_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_asset_group_reference.cmake")
    endif()
    add_executable(ab_collision_velocity_tests tests/collision_velocity.c)
    target_link_libraries(ab_collision_velocity_tests PRIVATE ab_game)
    add_test(NAME game_collision_velocity_edges COMMAND ab_collision_velocity_tests)
    add_executable(ab_game_contact_routes_tests tests/game_contact_routes.c)
    target_link_libraries(ab_game_contact_routes_tests PRIVATE ab_game)
    add_test(NAME game_contact_routes_edges COMMAND ab_game_contact_routes_tests)
    add_executable(ab_game_contact_dispatch_tests tests/game_contact_dispatch.c)
    target_link_libraries(ab_game_contact_dispatch_tests PRIVATE ab_game)
    add_test(NAME game_contact_dispatch_edges COMMAND ab_game_contact_dispatch_tests)
    add_executable(ab_game_contact_world_tests tests/game_contact_world.c)
    target_link_libraries(ab_game_contact_world_tests PRIVATE ab_game)
    add_test(NAME game_contact_world_integration COMMAND ab_game_contact_world_tests)
    add_executable(ab_museum_glue_tests tests/museum_glue.c)
    target_link_libraries(ab_museum_glue_tests PRIVATE ab_game)
    add_test(NAME museum_glue_edges COMMAND ab_museum_glue_tests)
    add_executable(ab_bubble_contact_tests tests/bubble_contact.c)
    target_link_libraries(ab_bubble_contact_tests PRIVATE ab_game)
    add_test(NAME bubble_contact_edges COMMAND ab_bubble_contact_tests)
    add_executable(ab_damage_particles_tests tests/damage_particles.c)
    target_link_libraries(ab_damage_particles_tests PRIVATE ab_game)
    add_test(NAME damage_particles_edges COMMAND ab_damage_particles_tests)
    add_executable(ab_block_collision_tests tests/block_collision.c)
    target_link_libraries(ab_block_collision_tests PRIVATE ab_game)
    add_test(NAME block_collision_edges COMMAND ab_block_collision_tests)
    add_executable(ab_bird_notify_tests tests/bird_notify.c)
    target_link_libraries(ab_bird_notify_tests PRIVATE ab_game)
    add_test(NAME bird_notify_edges COMMAND ab_bird_notify_tests)
    add_executable(ab_bird_prelude_tests tests/bird_prelude.c)
    target_link_libraries(ab_bird_prelude_tests PRIVATE ab_game)
    add_test(NAME bird_prelude_edges COMMAND ab_bird_prelude_tests)
    add_executable(ab_game_behavior_tests tests/game_behavior.c)
    target_link_libraries(ab_game_behavior_tests PRIVATE ab_game)
    add_test(NAME game_behavior_edges COMMAND ab_game_behavior_tests)
    add_executable(ab_game_behavior_add_tests tests/game_behavior_add.c)
    target_link_libraries(ab_game_behavior_add_tests PRIVATE ab_game)
    add_test(NAME game_behavior_add_edges COMMAND ab_game_behavior_add_tests)
    add_executable(ab_museum_bird_behavior_tests tests/museum_bird_behavior.c)
    add_executable(ab_blues_trajectory_tests tests/blues_trajectory.c)
    add_executable(ab_event_manager_tests tests/event_manager.c)
    add_executable(ab_settings_statistics_tests tests/settings_statistics.c)
    target_link_libraries(ab_settings_statistics_tests PRIVATE ab_game)
    add_test(NAME settings_statistics_storage COMMAND ab_settings_statistics_tests)
    add_executable(ab_classic_level_request_tests tests/classic_level_request.c)
    target_link_libraries(ab_classic_level_request_tests PRIVATE ab_game ab_resource)
    add_test(NAME level_request_edges COMMAND ab_classic_level_request_tests)
    add_executable(ab_classic_level_prepare_tests tests/classic_level_prepare.c)
    add_executable(ab_classic_start_level_tests tests/classic_start_level.c)
    target_link_libraries(ab_classic_start_level_tests PRIVATE ab_game)
    add_executable(ab_classic_pause_action_tests tests/classic_pause_actions.c)
    target_link_libraries(ab_classic_pause_action_tests PRIVATE ab_game)
    add_test(NAME classic_pause_action_edges COMMAND ab_classic_pause_action_tests)
    add_executable(ab_powerup_catalog_tests tests/powerup_catalog.c)
    target_link_libraries(ab_powerup_catalog_tests PRIVATE ab_resource)
    target_link_libraries(ab_classic_level_prepare_tests PRIVATE ab_game ab_resource)
    add_executable(ab_settings_file_tests tests/settings_file.c)
    target_link_libraries(ab_settings_file_tests PRIVATE ab_settings_file)
    add_test(NAME settings_file_roundtrip
        COMMAND ab_settings_file_tests "${CMAKE_CURRENT_BINARY_DIR}/settings-fixture.bin")
    set(AB_REFERENCE_SETTINGS_STATISTICS "" CACHE FILEPATH
        "Optional original SettingsWrapper local/device statistics corpus")
    if(AB_REFERENCE_SETTINGS_STATISTICS)
        add_test(NAME settings_statistics_android
            COMMAND "${CMAKE_COMMAND}" "-DREFERENCE=${AB_REFERENCE_SETTINGS_STATISTICS}"
                "-DCHECKER=$<TARGET_FILE:ab_settings_statistics_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_settings_statistics_reference.cmake")
    endif()
    add_executable(ab_shot_services_tests tests/shot_services.c)
    target_link_libraries(ab_shot_services_tests PRIVATE ab_game)
    add_test(NAME shot_services_integration COMMAND ab_shot_services_tests)
    add_executable(ab_classic_shot_listeners_tests tests/classic_shot_listeners.c)
    target_link_libraries(ab_classic_shot_listeners_tests PRIVATE ab_game)
    add_test(NAME classic_shot_listeners_edges COMMAND ab_classic_shot_listeners_tests)
    add_executable(ab_superseed_radius_tests tests/superseed_radius.c)
    target_link_libraries(ab_superseed_radius_tests PRIVATE ab_scene)
    add_test(NAME superseed_radius_edges COMMAND ab_superseed_radius_tests)
    set(AB_REFERENCE_SUPERSEED_RADIUS "" CACHE FILEPATH
        "Private original Android Super Seeds radius corpus")
    if(AB_REFERENCE_SUPERSEED_RADIUS)
        file(SHA256 "${AB_REFERENCE_SUPERSEED_RADIUS}" superseed_radius_hash)
        if(NOT superseed_radius_hash STREQUAL
            "5a04f2c4e15f950aaeb35d1cc84575377cf1df74aa004d99d873b7e9d4248ebf")
            message(FATAL_ERROR "Unexpected original Super Seeds radius corpus")
        endif()
        add_test(NAME superseed_radius_native COMMAND ab_superseed_radius_tests
            "${AB_REFERENCE_SUPERSEED_RADIUS}")
    endif()
    add_executable(ab_powerup_uses_tests tests/powerup_uses.c)
    target_link_libraries(ab_powerup_uses_tests PRIVATE ab_game)
    add_test(NAME powerup_uses_storage COMMAND ab_powerup_uses_tests)
    add_executable(ab_process_manager_tests tests/process_manager.c)
    add_executable(ab_native_events_tests tests/native_events.c)
    add_executable(ab_classic_root_tests tests/classic_root.c)
    add_executable(ab_classic_audio_ramp_tests tests/classic_audio_ramp.c)
    add_executable(ab_url_requests_tests tests/url_requests.c)
    add_executable(ab_calendar_time_tests tests/calendar_time.c)
    add_executable(ab_calendar_parts_tests tests/calendar_parts.c)
    target_link_libraries(ab_calendar_parts_tests PRIVATE ab_game)
    add_test(NAME calendar_parts_edges COMMAND ab_calendar_parts_tests)
    set_tests_properties(calendar_parts_edges PROPERTIES ENVIRONMENT "TZ=UTC0")
    set(AB_REFERENCE_CALENDAR_PARTS "" CACHE FILEPATH
        "Optional original native UTC calendar parts corpus")
    if(AB_REFERENCE_CALENDAR_PARTS)
        add_test(NAME calendar_parts_android
            COMMAND "${CMAKE_COMMAND}" "-DREFERENCE=${AB_REFERENCE_CALENDAR_PARTS}"
                "-DCHECKER=$<TARGET_FILE:ab_calendar_parts_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_calendar_parts_reference.cmake")
    endif()
    add_executable(ab_session_timer_tests tests/session_timer.c)
    add_executable(ab_giftbox_session_tests tests/giftbox_session.c)
    target_link_libraries(ab_giftbox_session_tests PRIVATE ab_game)
    add_test(NAME giftbox_session_edges COMMAND ab_giftbox_session_tests)
    set_tests_properties(giftbox_session_edges PROPERTIES ENVIRONMENT "TZ=UTC0")
    add_executable(ab_classic_anger_tests tests/classic_anger.c)
    add_executable(ab_classic_root_queries_tests tests/classic_root_queries.c)
    add_executable(ab_classic_gui_queries_tests tests/classic_gui_queries.c)
    add_executable(ab_classic_level_flow_tests tests/classic_level_flow.c)
    add_executable(ab_classic_museum_pause_tests tests/classic_museum_pause.c)
    add_executable(ab_classic_game_mode_tests tests/classic_game_mode.c)
    add_executable(ab_classic_reload_geometry_tests tests/classic_reload_geometry.c)
    target_link_libraries(ab_classic_reload_geometry_tests PRIVATE ab_scene)
    add_test(NAME classic_reload_geometry_edges COMMAND ab_classic_reload_geometry_tests)
    set(AB_REFERENCE_CLASSIC_RELOAD_GEOMETRY "" CACHE FILEPATH
        "Optional original reload camera-distance corpus")
    if(AB_REFERENCE_CLASSIC_RELOAD_GEOMETRY)
        add_test(NAME classic_reload_geometry_android
            COMMAND "${CMAKE_COMMAND}" "-DREFERENCE=${AB_REFERENCE_CLASSIC_RELOAD_GEOMETRY}"
                "-DCHECKER=$<TARGET_FILE:ab_classic_reload_geometry_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_reload_geometry_reference.cmake")
    endif()
    target_link_libraries(ab_classic_game_mode_tests PRIVATE ab_game)
    add_test(NAME classic_game_mode_edges COMMAND ab_classic_game_mode_tests)
    set(AB_REFERENCE_CLASSIC_GAME_MODE "" CACHE FILEPATH
        "Optional original pause/current-game-mode corpus")
    if(AB_REFERENCE_CLASSIC_GAME_MODE)
        add_test(NAME classic_game_mode_android
            COMMAND "${CMAKE_COMMAND}" "-DREFERENCE=${AB_REFERENCE_CLASSIC_GAME_MODE}"
                "-DCHECKER=$<TARGET_FILE:ab_classic_game_mode_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_game_mode_reference.cmake")
    endif()
    target_link_libraries(ab_classic_museum_pause_tests PRIVATE ab_game)
    add_test(NAME classic_museum_pause_edges COMMAND ab_classic_museum_pause_tests)
    set(AB_REFERENCE_CLASSIC_MUSEUM_PAUSE "" CACHE FILEPATH
        "Optional original Museum/pause-menu query corpus")
    if(AB_REFERENCE_CLASSIC_MUSEUM_PAUSE)
        add_test(NAME classic_museum_pause_android
            COMMAND "${CMAKE_COMMAND}" "-DREFERENCE=${AB_REFERENCE_CLASSIC_MUSEUM_PAUSE}"
                "-DCHECKER=$<TARGET_FILE:ab_classic_museum_pause_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_museum_pause_reference.cmake")
    endif()
    target_link_libraries(ab_classic_level_flow_tests PRIVATE ab_game)
    add_test(NAME classic_level_flow_edges COMMAND ab_classic_level_flow_tests)
    set(AB_REFERENCE_CLASSIC_LEVEL_FLOW "" CACHE FILEPATH
        "Optional original level-start flow queue/callback corpus")
    if(AB_REFERENCE_CLASSIC_LEVEL_FLOW)
        add_test(NAME classic_level_flow_android
            COMMAND "${CMAKE_COMMAND}" "-DREFERENCE=${AB_REFERENCE_CLASSIC_LEVEL_FLOW}"
                "-DCHECKER=$<TARGET_FILE:ab_classic_level_flow_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_level_flow_reference.cmake")
    endif()
    target_link_libraries(ab_classic_gui_queries_tests PRIVATE ab_game)
    add_test(NAME classic_gui_queries_edges COMMAND ab_classic_gui_queries_tests)
    set(AB_REFERENCE_CLASSIC_GUI_QUERIES "" CACHE FILEPATH
        "Optional original in-game/notifications GUI query corpus")
    if(AB_REFERENCE_CLASSIC_GUI_QUERIES)
        add_test(NAME classic_gui_queries_android
            COMMAND "${CMAKE_COMMAND}" "-DREFERENCE=${AB_REFERENCE_CLASSIC_GUI_QUERIES}"
                "-DCHECKER=$<TARGET_FILE:ab_classic_gui_queries_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_gui_queries_reference.cmake")
    endif()
    target_link_libraries(ab_classic_root_queries_tests PRIVATE ab_game)
    add_test(NAME classic_root_queries_edges COMMAND ab_classic_root_queries_tests)
    set(AB_REFERENCE_CLASSIC_ROOT_QUERIES "" CACHE FILEPATH
        "Optional original menu/eagle root-query corpus")
    if(AB_REFERENCE_CLASSIC_ROOT_QUERIES)
        add_test(NAME classic_root_queries_android
            COMMAND "${CMAKE_COMMAND}" "-DREFERENCE=${AB_REFERENCE_CLASSIC_ROOT_QUERIES}"
                "-DCHECKER=$<TARGET_FILE:ab_classic_root_queries_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_root_queries_reference.cmake")
    endif()
    target_link_libraries(ab_classic_anger_tests PRIVATE ab_game)
    add_test(NAME classic_anger_edges COMMAND ab_classic_anger_tests)
    set_tests_properties(classic_anger_edges PROPERTIES ENVIRONMENT "TZ=UTC0")
    set(AB_REFERENCE_CLASSIC_ANGER "" CACHE FILEPATH
        "Optional original AngerLevel timers/events/settings corpus")
    if(AB_REFERENCE_CLASSIC_ANGER)
        add_test(NAME classic_anger_android
            COMMAND "${CMAKE_COMMAND}" "-DREFERENCE=${AB_REFERENCE_CLASSIC_ANGER}"
                "-DCHECKER=$<TARGET_FILE:ab_classic_anger_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_anger_reference.cmake")
    endif()
    target_link_libraries(ab_session_timer_tests PRIVATE ab_game)
    add_test(NAME session_timer_edges COMMAND ab_session_timer_tests)
    set_tests_properties(session_timer_edges PROPERTIES ENVIRONMENT "TZ=UTC0")
    set(AB_REFERENCE_SESSION_TIMER "" CACHE FILEPATH
        "Optional original SessioningTimer UTC state/clock/save corpus")
    if(AB_REFERENCE_SESSION_TIMER)
        add_test(NAME session_timer_android
            COMMAND "${CMAKE_COMMAND}" "-DREFERENCE=${AB_REFERENCE_SESSION_TIMER}"
                "-DCHECKER=$<TARGET_FILE:ab_session_timer_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_session_timer_reference.cmake")
    endif()
    target_link_libraries(ab_calendar_time_tests PRIVATE ab_game)
    add_test(NAME calendar_time_edges COMMAND ab_calendar_time_tests)
    set_tests_properties(calendar_time_edges PROPERTIES ENVIRONMENT "TZ=UTC0")
    set(AB_REFERENCE_CALENDAR_TIME "" CACHE FILEPATH
        "Optional original native UTC calendar conversion corpus")
    if(AB_REFERENCE_CALENDAR_TIME)
        add_test(NAME calendar_time_android
            COMMAND "${CMAKE_COMMAND}" "-DREFERENCE=${AB_REFERENCE_CALENDAR_TIME}"
                "-DCHECKER=$<TARGET_FILE:ab_calendar_time_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_calendar_time_reference.cmake")
    endif()
    target_link_libraries(ab_url_requests_tests PRIVATE ab_game)
    add_test(NAME url_requests_edges COMMAND ab_url_requests_tests)
    set(AB_REFERENCE_URL_REQUESTS "" CACHE FILEPATH
        "Optional original URLRequests timer/callback corpus")
    if(AB_REFERENCE_URL_REQUESTS)
        add_test(NAME url_requests_android
            COMMAND "${CMAKE_COMMAND}" "-DREFERENCE=${AB_REFERENCE_URL_REQUESTS}"
                "-DCHECKER=$<TARGET_FILE:ab_url_requests_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_url_requests_reference.cmake")
    endif()
    target_link_libraries(ab_classic_audio_ramp_tests PRIVATE ab_game)
    add_test(NAME classic_audio_ramp_edges COMMAND ab_classic_audio_ramp_tests)
    set(AB_REFERENCE_CLASSIC_AUDIO_RAMP "" CACHE FILEPATH
        "Optional original audio-ramp state/track-gain corpus")
    if(AB_REFERENCE_CLASSIC_AUDIO_RAMP)
        add_test(NAME classic_audio_ramp_android
            COMMAND "${CMAKE_COMMAND}" "-DREFERENCE=${AB_REFERENCE_CLASSIC_AUDIO_RAMP}"
                "-DCHECKER=$<TARGET_FILE:ab_classic_audio_ramp_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_audio_ramp_reference.cmake")
    endif()
    target_link_libraries(ab_classic_root_tests PRIVATE ab_scene)
    add_test(NAME classic_root_edges COMMAND ab_classic_root_tests)
    set(AB_REFERENCE_CLASSIC_ROOT "" CACHE FILEPATH
        "Optional original root update clock/service-order corpus")
    if(AB_REFERENCE_CLASSIC_ROOT)
        add_test(NAME classic_root_android
            COMMAND "${CMAKE_COMMAND}" "-DREFERENCE=${AB_REFERENCE_CLASSIC_ROOT}"
                "-DCHECKER=$<TARGET_FILE:ab_classic_root_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_root_reference.cmake")
    endif()
    target_link_libraries(ab_native_events_tests PRIVATE ab_game)
    add_test(NAME native_events_edges COMMAND ab_native_events_tests)
    set(AB_REFERENCE_NATIVE_EVENTS "" CACHE FILEPATH
        "Optional original native event queue/animation wrapper corpus")
    if(AB_REFERENCE_NATIVE_EVENTS)
        add_test(NAME native_events_android
            COMMAND "${CMAKE_COMMAND}" "-DREFERENCE=${AB_REFERENCE_NATIVE_EVENTS}"
                "-DCHECKER=$<TARGET_FILE:ab_native_events_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_native_events_reference.cmake")
    endif()
    target_link_libraries(ab_process_manager_tests PRIVATE ab_game)
    add_test(NAME process_manager_edges COMMAND ab_process_manager_tests)
    add_executable(ab_classic_loader_tests tests/classic_loader.c)
    target_link_libraries(ab_classic_loader_tests PRIVATE ab_game)
    add_test(NAME classic_loader_edges COMMAND ab_classic_loader_tests)
    set(AB_REFERENCE_CLASSIC_LOADER "" CACHE FILEPATH
        "Optional original Loader loading/event/frame corpus")
    if(AB_REFERENCE_CLASSIC_LOADER)
        add_test(NAME classic_loader_android
            COMMAND "${CMAKE_COMMAND}" "-DREFERENCE=${AB_REFERENCE_CLASSIC_LOADER}"
                "-DCHECKER=$<TARGET_FILE:ab_classic_loader_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_loader_reference.cmake")
    endif()
    set(AB_REFERENCE_PROCESS_MANAGER "" CACHE FILEPATH
        "Optional original ProcessManager method/state corpus")
    if(AB_REFERENCE_PROCESS_MANAGER)
        add_test(NAME process_manager_android
            COMMAND "${CMAKE_COMMAND}" "-DREFERENCE=${AB_REFERENCE_PROCESS_MANAGER}"
                "-DCHECKER=$<TARGET_FILE:ab_process_manager_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_process_manager_reference.cmake")
    endif()
    set(AB_REFERENCE_POWERUP_USES "" CACHE FILEPATH
        "Optional original PowerupManager used-list method corpus")
    if(AB_REFERENCE_POWERUP_USES)
        add_test(NAME powerup_uses_android
            COMMAND "${CMAKE_COMMAND}" "-DREFERENCE=${AB_REFERENCE_POWERUP_USES}"
                "-DCHECKER=$<TARGET_FILE:ab_powerup_uses_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_powerup_uses_reference.cmake")
    endif()
    set(AB_REFERENCE_CLASSIC_SHOT_LISTENERS "" CACHE FILEPATH
        "Optional original ordinary shot listener/event/settings corpus")
    if(AB_REFERENCE_CLASSIC_SHOT_LISTENERS)
        add_test(NAME classic_shot_listeners_android
            COMMAND "${CMAKE_COMMAND}" "-DREFERENCE=${AB_REFERENCE_CLASSIC_SHOT_LISTENERS}"
                "-DCHECKER=$<TARGET_FILE:ab_classic_shot_listeners_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_shot_listeners_reference.cmake")
    endif()
    target_link_libraries(ab_event_manager_tests PRIVATE ab_game)
    set(AB_REFERENCE_EVENT_MANAGER "" CACHE FILEPATH
        "Optional original EventManager listener/queue/delay corpus")
    if(AB_REFERENCE_EVENT_MANAGER)
        add_test(NAME event_manager_android
            COMMAND "${CMAKE_COMMAND}" "-DREFERENCE=${AB_REFERENCE_EVENT_MANAGER}"
                "-DCHECKER=$<TARGET_FILE:ab_event_manager_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_event_manager_reference.cmake")
    endif()
    target_link_libraries(ab_blues_trajectory_tests PRIVATE ab_game)
    set(AB_REFERENCE_BLUES_TRAJECTORY "" CACHE FILEPATH
        "Optional original MuseumBluesTrajectoryRenderer constructor/event corpus")
    if(AB_REFERENCE_BLUES_TRAJECTORY)
        add_test(NAME blues_trajectory_android
            COMMAND "${CMAKE_COMMAND}" "-DREFERENCE=${AB_REFERENCE_BLUES_TRAJECTORY}"
                "-DCHECKER=$<TARGET_FILE:ab_blues_trajectory_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_blues_trajectory_reference.cmake")
    endif()
    target_link_libraries(ab_museum_bird_behavior_tests PRIVATE ab_game)
    add_test(NAME museum_bird_behavior_edges COMMAND ab_museum_bird_behavior_tests)
    add_executable(ab_classic_behaviors_tests tests/classic_behaviors.c)
    target_link_libraries(ab_classic_behaviors_tests PRIVATE ab_game)
    add_test(NAME classic_behaviors_edges COMMAND ab_classic_behaviors_tests)
    add_executable(ab_classic_shots_tests tests/classic_shots.c)
    target_link_libraries(ab_classic_shots_tests PRIVATE ab_scene)
    add_executable(ab_classic_collision_hooks_tests tests/classic_collision_hooks.c)
    target_link_libraries(ab_classic_collision_hooks_tests PRIVATE ab_scene)
    add_test(NAME classic_collision_hooks_edges COMMAND ab_classic_collision_hooks_tests)
    add_executable(ab_classic_playfield_tests tests/classic_playfield.c)
    target_link_libraries(ab_classic_playfield_tests PRIVATE ab_scene ab_asset_resources)
    set(AB_REFERENCE_BLUES_CHILD "" CACHE FILEPATH
        "Private original source Blues child/native construction corpus")
    if(AB_REFERENCE_APK AND AB_REFERENCE_LIVE_SHOT AND AB_REFERENCE_BLUES_CHILD)
        add_test(NAME classic_blues_child_android
            COMMAND "${CMAKE_COMMAND}" "-DAPK=${AB_REFERENCE_APK}"
                "-DREFERENCE=${AB_REFERENCE_BLUES_CHILD}"
                "-DSHOT_REFERENCE=${AB_REFERENCE_LIVE_SHOT}"
                "-DCHECKER=$<TARGET_FILE:ab_classic_playfield_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_blues_child.cmake")
    endif()
    set(AB_REFERENCE_BLUES_SPLIT "" CACHE FILEPATH
        "Private unchanged original outer Blues branch/native child corpus")
    if(AB_REFERENCE_APK AND AB_REFERENCE_LIVE_SHOT AND AB_REFERENCE_BLUES_SPLIT)
        add_test(NAME classic_blues_split_android
            COMMAND "${CMAKE_COMMAND}" "-DAPK=${AB_REFERENCE_APK}"
                "-DREFERENCE=${AB_REFERENCE_BLUES_SPLIT}"
                "-DSHOT_REFERENCE=${AB_REFERENCE_LIVE_SHOT}"
                "-DCHECKER=$<TARGET_FILE:ab_classic_playfield_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_blues_split.cmake")
    endif()
    set(AB_REFERENCE_CLASSIC_SHOT_EFFECTS "" CACHE FILEPATH
        "Optional original ordinary shot attachment/red-special gate corpus")
    set(AB_REFERENCE_SLING_WOBBLE "" CACHE FILEPATH
        "Optional original default-sling stretch/spring corpus")
    set(AB_REFERENCE_GAME_DRAW "" CACHE FILEPATH
        "Optional original ordinary drawGame service-order corpus")
    if(AB_REFERENCE_APK AND AB_REFERENCE_LIVE_SHOT)
        add_test(NAME classic_playfield_android
            COMMAND "${CMAKE_COMMAND}" "-DAPK=${AB_REFERENCE_APK}"
                "-DREFERENCE=${AB_REFERENCE_LIVE_SHOT}"
                "-DCHECKER=$<TARGET_FILE:ab_classic_playfield_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_shots.cmake")
        add_test(NAME classic_shots_live_android
            COMMAND "${CMAKE_COMMAND}" "-DAPK=${AB_REFERENCE_APK}"
                "-DREFERENCE=${AB_REFERENCE_LIVE_SHOT}"
                "-DEFFECTS=${AB_REFERENCE_CLASSIC_SHOT_EFFECTS}"
                "-DWOBBLE=${AB_REFERENCE_SLING_WOBBLE}"
                "-DDRAW=${AB_REFERENCE_GAME_DRAW}"
                "-DCHECKER=$<TARGET_FILE:ab_classic_shots_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_shots.cmake")
    endif()
    add_executable(ab_trajectory_follow_tests tests/trajectory_follow.c)
    target_link_libraries(ab_trajectory_follow_tests PRIVATE ab_game)
    add_test(NAME trajectory_follow_edges COMMAND ab_trajectory_follow_tests)
    add_executable(ab_object_property_access_tests tests/object_property_access.c)
    target_link_libraries(ab_object_property_access_tests PRIVATE ab_game)
    add_test(NAME object_property_access_edges COMMAND ab_object_property_access_tests)
    add_executable(ab_object_definition_lookup_tests tests/object_definition_lookup.c)
    target_link_libraries(ab_object_definition_lookup_tests PRIVATE ab_game)
    add_test(NAME object_definition_lookup_edges COMMAND ab_object_definition_lookup_tests)
    add_executable(ab_object_world_lookup_tests tests/object_definition_lookup.c)
    target_compile_definitions(ab_object_world_lookup_tests PRIVATE AB_TEST_OBJECT_WORLD_LOOKUP=1)
    target_link_libraries(ab_object_world_lookup_tests PRIVATE ab_game)
    add_test(NAME object_world_lookup_edges COMMAND ab_object_world_lookup_tests)
    add_executable(ab_level_properties_tests tests/level_properties.c)
    target_link_libraries(ab_level_properties_tests PRIVATE ab_game)
    add_test(NAME level_properties_edges COMMAND ab_level_properties_tests)
    add_executable(ab_bird_mighty_tests tests/bird_mighty.c)
    target_link_libraries(ab_bird_mighty_tests PRIVATE ab_game)
    add_test(NAME bird_mighty_edges COMMAND ab_bird_mighty_tests)
    add_executable(ab_bird_sprites_tests tests/bird_sprites.c)
    target_link_libraries(ab_bird_sprites_tests PRIVATE ab_game)
    add_test(NAME bird_sprites_edges COMMAND ab_bird_sprites_tests)
    add_executable(ab_bird_specialty_tests tests/bird_specialty.c)
    target_link_libraries(ab_bird_specialty_tests PRIVATE ab_game)
    add_test(NAME bird_specialty_edges COMMAND ab_bird_specialty_tests)
    add_executable(ab_bird_projectiles_tests tests/bird_projectiles.c)
    target_link_libraries(ab_bird_projectiles_tests PRIVATE ab_game)
    add_test(NAME bird_projectiles_edges COMMAND ab_bird_projectiles_tests)
    add_executable(ab_bird_collision_damage_tests tests/bird_collision_damage.c)
    target_link_libraries(ab_bird_collision_damage_tests PRIVATE ab_game)
    add_test(NAME bird_collision_damage_edges COMMAND ab_bird_collision_damage_tests)
    add_executable(ab_bird_effects_tests tests/bird_effects.c)
    target_link_libraries(ab_bird_effects_tests PRIVATE ab_game)
    add_test(NAME bird_effects_edges COMMAND ab_bird_effects_tests)
    add_executable(ab_bird_collision_tests tests/bird_collision.c)
    target_link_libraries(ab_bird_collision_tests PRIVATE ab_game)
    add_test(NAME bird_collision_composition COMMAND ab_bird_collision_tests)
    add_executable(ab_object_removal_notify_tests tests/object_removal_notify.c)
    target_link_libraries(ab_object_removal_notify_tests PRIVATE ab_game)
    add_test(NAME object_removal_notify_edges COMMAND ab_object_removal_notify_tests)
    add_executable(ab_object_removed_tests tests/object_removed.c)
    target_link_libraries(ab_object_removed_tests PRIVATE ab_game)
    add_test(NAME object_removed_edges COMMAND ab_object_removed_tests)
    add_executable(ab_bird_remove_tests tests/bird_remove.c)
    target_link_libraries(ab_bird_remove_tests PRIVATE ab_game)
    add_test(NAME bird_remove_edges COMMAND ab_bird_remove_tests)
    add_executable(ab_level_effect_tests tests/level_effect.c)
    target_link_libraries(ab_level_effect_tests PRIVATE ab_game)
    add_test(NAME level_effect_base_edges COMMAND ab_level_effect_tests)
    add_executable(ab_burn_manager_tests tests/burn_manager.c)
    target_link_libraries(ab_burn_manager_tests PRIVATE ab_game)
    add_test(NAME burn_manager_edges COMMAND ab_burn_manager_tests)
    add_executable(ab_hero_screen_tinter_tests tests/hero_screen_tinter.c)
    target_link_libraries(ab_hero_screen_tinter_tests PRIVATE ab_game)
    add_test(NAME hero_screen_tinter_edges COMMAND ab_hero_screen_tinter_tests)
    add_executable(ab_classic_game_globals_tests tests/classic_game_globals.c)
    target_link_libraries(ab_classic_game_globals_tests PRIVATE ab_game)
    add_test(NAME classic_game_globals_edges COMMAND ab_classic_game_globals_tests)
    add_executable(ab_classic_game_updates_tests tests/classic_game_updates.c)
    target_link_libraries(ab_classic_game_updates_tests PRIVATE ab_game)
    add_test(NAME classic_game_updates_edges COMMAND ab_classic_game_updates_tests)
    add_executable(ab_integer_map_tests tests/integer_map.c)
    target_link_libraries(ab_integer_map_tests PRIVATE ab_base)
    add_test(NAME integer_map_edges COMMAND ab_integer_map_tests)
    set(AB_REFERENCE_INTEGER_MAP "" CACHE FILEPATH "Private native integer mutation trace")
    if(AB_REFERENCE_INTEGER_MAP)
        add_test(NAME integer_map_android COMMAND ${CMAKE_COMMAND}
            "-DREFERENCE=${AB_REFERENCE_INTEGER_MAP}"
            "-DCHECKER=$<TARGET_FILE:ab_integer_map_tests>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_integer_map_reference.cmake")
    endif()
    add_executable(ab_delayed_callbacks_tests tests/delayed_callbacks.c)
    target_link_libraries(ab_delayed_callbacks_tests PRIVATE ab_game)
    add_test(NAME delayed_callbacks_edges COMMAND ab_delayed_callbacks_tests)
    set(AB_REFERENCE_BURN_MANAGER "" CACHE FILEPATH
        "Optional original complete BurnManager module state trace")
    if(AB_REFERENCE_BURN_MANAGER)
        add_test(NAME burn_manager_android COMMAND ab_burn_manager_tests
            "${AB_REFERENCE_BURN_MANAGER}")
    endif()
    add_executable(ab_bird_impact_prediction_tests tests/bird_impact_prediction.c)
    target_link_libraries(ab_bird_impact_prediction_tests PRIVATE ab_game)
    add_test(NAME bird_impact_prediction_edges COMMAND ab_bird_impact_prediction_tests)
    add_executable(ab_world_raycast_tests tests/world_raycast.c)
    target_link_libraries(ab_world_raycast_tests PRIVATE ab_physics)
    add_test(NAME physics_world_raycast_edges COMMAND ab_world_raycast_tests)
    add_executable(ab_game_raycast_tests tests/game_raycast.c)
    target_link_libraries(ab_game_raycast_tests PRIVATE ab_game)
    add_test(NAME game_raycast_edges COMMAND ab_game_raycast_tests)
    add_executable(ab_game_raycast_numeric_tests tests/game_raycast.c)
    target_compile_definitions(ab_game_raycast_numeric_tests PRIVATE AB_TEST_GAME_RAYCAST_NUMERIC=1)
    target_link_libraries(ab_game_raycast_numeric_tests PRIVATE ab_game)
    add_executable(ab_bird_impact_world_tests tests/game_raycast.c)
    target_compile_definitions(ab_bird_impact_world_tests PRIVATE AB_TEST_BIRD_IMPACT_WORLD=1)
    target_link_libraries(ab_bird_impact_world_tests PRIVATE ab_game)
    add_executable(ab_bird_impact_world_numeric_tests tests/game_raycast.c)
    target_compile_definitions(ab_bird_impact_world_numeric_tests
        PRIVATE AB_TEST_BIRD_IMPACT_WORLD=1 AB_TEST_BIRD_IMPACT_WORLD_NUMERIC=1)
    target_link_libraries(ab_bird_impact_world_numeric_tests PRIVATE ab_game)
    add_executable(ab_camera_smoothstep_tests tests/camera_smoothstep.c)
    target_link_libraries(ab_camera_smoothstep_tests PRIVATE ab_game)
    add_test(NAME camera_smoothstep_edges COMMAND ab_camera_smoothstep_tests)
    add_executable(ab_camera_follow_tests tests/camera_follow.c)
    target_link_libraries(ab_camera_follow_tests PRIVATE ab_game)
    add_test(NAME camera_follow_edges COMMAND ab_camera_follow_tests)
    add_executable(ab_camera_zoom_fit_tests tests/camera_zoom_fit.c)
    target_link_libraries(ab_camera_zoom_fit_tests PRIVATE ab_game)
    add_test(NAME camera_zoom_fit_edges COMMAND ab_camera_zoom_fit_tests)
    add_executable(ab_camera_follow_system_tests tests/camera_follow_system.c)
    target_link_libraries(ab_camera_follow_system_tests PRIVATE ab_game)
    add_executable(ab_camera_follow_end_sequence_tests tests/camera_follow_system.c)
    target_compile_definitions(ab_camera_follow_end_sequence_tests
        PRIVATE AB_TEST_CAMERA_FOLLOW_END_SEQUENCE=1)
    target_link_libraries(ab_camera_follow_end_sequence_tests PRIVATE ab_game)
    add_executable(ab_camera_shot_sequence_tests tests/camera_follow_system.c)
    target_compile_definitions(ab_camera_shot_sequence_tests
        PRIVATE AB_TEST_CAMERA_FOLLOW_END_SEQUENCE=1 AB_TEST_CAMERA_SHOT_SEQUENCE=1)
    target_link_libraries(ab_camera_shot_sequence_tests PRIVATE ab_game)
    add_executable(ab_camera_follow_world_tests tests/camera_follow_system.c)
    target_compile_definitions(ab_camera_follow_world_tests PRIVATE
        AB_TEST_CAMERA_FOLLOW_END_SEQUENCE=1 AB_TEST_CAMERA_SHOT_SEQUENCE=1
        AB_TEST_CAMERA_FOLLOW_WORLD=1)
    target_link_libraries(ab_camera_follow_world_tests PRIVATE ab_game)
    add_executable(ab_camera_follow_properties_tests tests/camera_follow_system.c)
    target_compile_definitions(ab_camera_follow_properties_tests PRIVATE
        AB_TEST_CAMERA_FOLLOW_END_SEQUENCE=1 AB_TEST_CAMERA_SHOT_SEQUENCE=1
        AB_TEST_CAMERA_FOLLOW_WORLD=1 AB_TEST_CAMERA_FOLLOW_PROPERTIES=1)
    target_link_libraries(ab_camera_follow_properties_tests PRIVATE ab_game)
    add_executable(ab_classic_camera_tests tests/classic_camera.c)
    target_compile_definitions(ab_classic_camera_tests PRIVATE
        AB_TEST_CAMERA_FOLLOW_END_SEQUENCE=1 AB_TEST_CAMERA_SHOT_SEQUENCE=1
        AB_TEST_CAMERA_FOLLOW_WORLD=1 AB_TEST_CLASSIC_CAMERA=1)
    target_link_libraries(ab_classic_camera_tests PRIVATE ab_game)
    add_executable(ab_block_removal_cursor_tests tests/block_removal_cursor.c)
    target_link_libraries(ab_block_removal_cursor_tests PRIVATE ab_game)
    add_test(NAME block_removal_cursor_edges COMMAND ab_block_removal_cursor_tests)
    add_executable(ab_block_removal_score_tests tests/block_removal_score.c)
    target_link_libraries(ab_block_removal_score_tests PRIVATE ab_game)
    add_test(NAME block_removal_score_edges COMMAND ab_block_removal_score_tests)
    add_executable(ab_block_removal_effects_tests tests/block_removal_effects.c)
    target_link_libraries(ab_block_removal_effects_tests PRIVATE ab_game)
    add_test(NAME block_removal_effects_edges COMMAND ab_block_removal_effects_tests)
    add_executable(ab_block_removal_particles_tests tests/block_removal_particles.c)
    target_link_libraries(ab_block_removal_particles_tests PRIVATE ab_game)
    add_test(NAME block_removal_particles_edges COMMAND ab_block_removal_particles_tests)
    add_executable(ab_block_removal_lifecycle_tests tests/block_removal_lifecycle.c)
    target_link_libraries(ab_block_removal_lifecycle_tests PRIVATE ab_game)
    add_test(NAME block_removal_lifecycle_edges COMMAND ab_block_removal_lifecycle_tests)
    add_executable(ab_block_removal_cleanup_tests tests/block_removal_cleanup.c)
    target_link_libraries(ab_block_removal_cleanup_tests PRIVATE ab_game)
    add_executable(ab_block_removal_spawn_tests tests/block_removal_spawn.c)
    target_link_libraries(ab_block_removal_spawn_tests PRIVATE ab_game)
    add_test(NAME block_removal_spawn_edges COMMAND ab_block_removal_spawn_tests)
    add_executable(ab_block_removal_spawn_reference_tests tests/block_removal_spawn_reference.c)
    target_link_libraries(ab_block_removal_spawn_reference_tests PRIVATE ab_game)
    add_executable(ab_block_removal_pass_tests tests/block_removal_pass.c)
    target_link_libraries(ab_block_removal_pass_tests PRIVATE ab_game)
    add_test(NAME block_removal_pass_composition COMMAND ab_block_removal_pass_tests)
    add_executable(ab_block_removal_pass_reference_tests tests/block_removal_pass_reference.c)
    target_link_libraries(ab_block_removal_pass_reference_tests PRIVATE ab_game)
    add_executable(ab_game_object_remove_tests tests/game_object_remove.c)
    target_link_libraries(ab_game_object_remove_tests PRIVATE ab_game)
    add_test(NAME game_object_remove_edges COMMAND ab_game_object_remove_tests)
    add_executable(ab_game_object_remove_joints_tests tests/game_object_remove_joints.c)
    target_link_libraries(ab_game_object_remove_joints_tests PRIVATE ab_game)
    add_test(NAME game_object_remove_joints_edges COMMAND ab_game_object_remove_joints_tests)
    add_executable(ab_game_object_remove_teleporters_tests tests/game_object_remove_teleporters.c)
    target_link_libraries(ab_game_object_remove_teleporters_tests PRIVATE ab_game)
    add_test(NAME game_object_remove_teleporters_edges COMMAND ab_game_object_remove_teleporters_tests)
    add_executable(ab_score_tests tests/score.c)
    target_link_libraries(ab_score_tests PRIVATE ab_game)
    add_test(NAME game_score_edges COMMAND ab_score_tests)
    add_executable(ab_floating_score_tests tests/floating_scores.c)
    target_link_libraries(ab_floating_score_tests PRIVATE ab_game)
    add_test(NAME floating_scores_owned COMMAND ab_floating_score_tests)
    add_executable(ab_floating_score_draw_tests tests/floating_score_draw.c)
    target_link_libraries(ab_floating_score_draw_tests PRIVATE ab_scene)
    add_test(NAME floating_score_draw_edges COMMAND ab_floating_score_draw_tests)
    add_executable(ab_mighty_eagle_timer_tests tests/mighty_eagle_timer.c)
    target_link_libraries(ab_mighty_eagle_timer_tests PRIVATE ab_game)
    add_test(NAME mighty_eagle_timer_edges COMMAND ab_mighty_eagle_timer_tests)
    set(AB_REFERENCE_MIGHTY_EAGLE_TIMER "" CACHE FILEPATH
        "Optional original Mighty Eagle timer state and call boundaries")
    if(AB_REFERENCE_MIGHTY_EAGLE_TIMER)
        add_test(NAME mighty_eagle_timer_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_MIGHTY_EAGLE_TIMER}"
                "-DCHECKER=$<TARGET_FILE:ab_mighty_eagle_timer_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_mighty_eagle_timer_reference.cmake")
    endif()
    set(AB_REFERENCE_FLOATING_SCORE_DRAW "" CACHE FILEPATH
        "Optional original GameHud floating-score draw boundaries")
    if(AB_REFERENCE_FLOATING_SCORE_DRAW)
        add_test(NAME floating_score_draw_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_FLOATING_SCORE_DRAW}"
                "-DCHECKER=$<TARGET_FILE:ab_floating_score_draw_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_floating_score_draw_reference.cmake")
    endif()
    add_executable(ab_particle_tests tests/particles.c)
    target_link_libraries(ab_particle_tests PRIVATE ab_scene)
    add_test(NAME particles_owned_edges COMMAND ab_particle_tests)
    set(AB_REFERENCE_PARTICLE_BOX "" CACHE FILEPATH
        "Optional original ordinary particle creation/update snapshots")
    set(AB_REFERENCE_PARTICLE_COMMON "" CACHE FILEPATH
        "Optional original ordinary box/circle/animated particle snapshots")
    set(AB_REFERENCE_PARTICLE_LEVEL "" CACHE FILEPATH
        "Optional original level particle creation/update/layer/RNG snapshots")
    set(AB_REFERENCE_PARTICLE_LEVEL_DRAW "" CACHE FILEPATH
        "Optional original level particle draw snapshots for all/first/second layers")
    if(AB_REFERENCE_PARTICLE_BOX AND AB_REFERENCE_APK)
        add_test(NAME particles_box_android COMMAND ab_particle_tests
            "${AB_REFERENCE_APK}" "${AB_REFERENCE_PARTICLE_BOX}")
    endif()
    if(AB_REFERENCE_PARTICLE_COMMON AND AB_REFERENCE_APK)
        add_test(NAME particles_common_android COMMAND ab_particle_tests
            "${AB_REFERENCE_APK}" "${AB_REFERENCE_PARTICLE_COMMON}")
    endif()
    if(AB_REFERENCE_PARTICLE_LEVEL AND AB_REFERENCE_APK)
        add_test(NAME particles_level_android COMMAND ab_particle_tests
            "${AB_REFERENCE_APK}" "${AB_REFERENCE_PARTICLE_LEVEL}")
    endif()
    if(AB_REFERENCE_PARTICLE_LEVEL_DRAW AND AB_REFERENCE_APK)
        add_test(NAME particles_level_draw_android COMMAND ab_particle_tests
            "${AB_REFERENCE_APK}" "${AB_REFERENCE_PARTICLE_LEVEL_DRAW}")
    endif()
    set(AB_REFERENCE_PARTICLE_DRAW "" CACHE FILEPATH
        "Optional original reached world-particle draw boundaries")
    if(AB_REFERENCE_PARTICLE_DRAW AND AB_REFERENCE_APK)
        add_test(NAME particles_draw_android COMMAND ab_particle_tests
            "${AB_REFERENCE_APK}" "${AB_REFERENCE_PARTICLE_DRAW}")
    endif()
    set(AB_REFERENCE_PARTICLE_MENU_DRAW "" CACHE FILEPATH
        "Optional original reached menu-particle draw boundaries")
    if(AB_REFERENCE_PARTICLE_MENU_DRAW AND AB_REFERENCE_APK)
        add_test(NAME particles_menu_draw_android COMMAND ab_particle_tests
            "${AB_REFERENCE_APK}" "${AB_REFERENCE_PARTICLE_MENU_DRAW}")
    endif()
    add_executable(ab_level_flow_tests tests/level_flow.c)
    target_link_libraries(ab_level_flow_tests PRIVATE ab_game)
    add_test(NAME game_level_flow_edges COMMAND ab_level_flow_tests)
    add_executable(ab_level_bonus_tests tests/level_bonus.c)
    target_link_libraries(ab_level_bonus_tests PRIVATE ab_game)
    add_test(NAME game_level_bonus_edges COMMAND ab_level_bonus_tests)
    add_executable(ab_level_ending_tests tests/level_ending.c)
    target_link_libraries(ab_level_ending_tests PRIVATE ab_game)
    add_test(NAME game_level_ending_edges COMMAND ab_level_ending_tests)
    add_executable(ab_level_complete_tests tests/level_complete.c)
    target_link_libraries(ab_level_complete_tests PRIVATE ab_game)
    add_test(NAME game_level_complete_edges COMMAND ab_level_complete_tests)
    add_executable(ab_coordinate_tests tests/coordinates.c)
    target_link_libraries(ab_coordinate_tests PRIVATE ab_game)
    add_test(NAME game_coordinate_edges COMMAND ab_coordinate_tests)
    add_executable(ab_camera_viewport_tests tests/camera_viewport.c)
    target_link_libraries(ab_camera_viewport_tests PRIVATE ab_game)
    add_test(NAME game_camera_viewport_edges COMMAND ab_camera_viewport_tests)
    add_executable(ab_camera_mover_tests tests/camera_mover.c)
    target_link_libraries(ab_camera_mover_tests PRIVATE ab_game)
    add_test(NAME game_camera_mover_edges COMMAND ab_camera_mover_tests)
    add_executable(ab_camera_limits_tests tests/camera_limits.c)
    target_link_libraries(ab_camera_limits_tests PRIVATE ab_game)
    add_test(NAME game_camera_limits_edges COMMAND ab_camera_limits_tests)
    add_executable(ab_camera_bootstrap_tests tests/camera_bootstrap.c)
    target_link_libraries(ab_camera_bootstrap_tests PRIVATE ab_game)
    add_test(NAME game_camera_bootstrap_edges COMMAND ab_camera_bootstrap_tests)
    add_executable(ab_camera_sequence_tests tests/camera_sequence.c)
    target_link_libraries(ab_camera_sequence_tests PRIVATE ab_game)
    add_test(NAME game_camera_sequence_edges COMMAND ab_camera_sequence_tests)
    add_executable(ab_camera_system_tests tests/camera_system.c)
    target_link_libraries(ab_camera_system_tests PRIVATE ab_game)
    add_test(NAME game_camera_system_edges COMMAND ab_camera_system_tests)
    add_executable(ab_camera_shake_tests tests/camera_shake.c)
    target_link_libraries(ab_camera_shake_tests PRIVATE ab_game)
    add_test(NAME game_camera_shake_edges COMMAND ab_camera_shake_tests)
    add_executable(ab_camera_drag_tests tests/camera_drag.c)
    target_link_libraries(ab_camera_drag_tests PRIVATE ab_game)
    add_test(NAME game_camera_drag_edges COMMAND ab_camera_drag_tests)
    add_executable(ab_camera_tweens_tests tests/camera_tweens.c)
    target_link_libraries(ab_camera_tweens_tests PRIVATE ab_game)
    add_test(NAME game_camera_tweens_edges COMMAND ab_camera_tweens_tests)
    add_executable(ab_script_trig_tests tests/script_trig.c)
    target_link_libraries(ab_script_trig_tests PRIVATE ab_game)
    add_test(NAME script_trig_edges COMMAND ab_script_trig_tests)
    add_executable(ab_camera_selection_tests tests/camera_selection.c)
    target_link_libraries(ab_camera_selection_tests PRIVATE ab_game)
    add_test(NAME game_camera_selection_edges COMMAND ab_camera_selection_tests)
    add_executable(ab_camera_touch_tests tests/camera_touch.c)
    target_link_libraries(ab_camera_touch_tests PRIVATE ab_game)
    add_test(NAME game_camera_touch_edges COMMAND ab_camera_touch_tests)
    add_executable(ab_menu_pointer_tests tests/menu_pointer.c)
    target_link_libraries(ab_menu_pointer_tests PRIVATE ab_game)
    add_test(NAME game_menu_pointer_edges COMMAND ab_menu_pointer_tests)
    add_executable(ab_classic_menu_input_tests tests/classic_menu_input.c)
    target_link_libraries(ab_classic_menu_input_tests PRIVATE ab_scene)
    add_test(NAME classic_menu_input_edges COMMAND ab_classic_menu_input_tests)
    add_executable(ab_ui_frame_geometry_tests tests/ui_frame_geometry.c)
    target_link_libraries(ab_ui_frame_geometry_tests PRIVATE ab_game)
    add_test(NAME ui_frame_geometry_edges COMMAND ab_ui_frame_geometry_tests)
    add_executable(ab_ui_pointer_tests tests/ui_pointer.c)
    target_link_libraries(ab_ui_pointer_tests PRIVATE ab_game)
    add_test(NAME ui_pointer_edges COMMAND ab_ui_pointer_tests)
    add_executable(ab_ui_value_text_tests tests/ui_value_text.c)
    target_link_libraries(ab_ui_value_text_tests PRIVATE ab_game)
    add_test(NAME ui_value_text_edges COMMAND ab_ui_value_text_tests)
    set(AB_REFERENCE_UI_NATIVE_VALUE_TEXT "" CACHE FILEPATH
        "Optional original LuaTable native value-string conversion corpus")
    if(AB_REFERENCE_UI_NATIVE_VALUE_TEXT)
        add_test(NAME ui_native_value_text_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_UI_NATIVE_VALUE_TEXT}"
                "-DCHECKER=$<TARGET_FILE:ab_ui_value_text_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_ui_native_value_text_reference.cmake")
    endif()
    set(AB_REFERENCE_UI_VALUE_TEXT "" CACHE FILEPATH
        "Optional original plain tostring/numeric formatting corpus")
    if(AB_REFERENCE_UI_VALUE_TEXT)
        add_test(NAME ui_value_text_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_UI_VALUE_TEXT}"
                "-DCHECKER=$<TARGET_FILE:ab_ui_value_text_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_ui_value_text_reference.cmake")
    endif()
    add_executable(ab_ui_image_hit_tests tests/ui_image_hit.c)
    target_link_libraries(ab_ui_image_hit_tests PRIVATE ab_game)
    add_test(NAME ui_image_hit_edges COMMAND ab_ui_image_hit_tests)
    add_executable(ab_ui_image_button_tests tests/ui_image_button.c)
    target_link_libraries(ab_ui_image_button_tests PRIVATE ab_game)
    add_test(NAME ui_image_button_edges COMMAND ab_ui_image_button_tests)
    add_executable(ab_ui_image_animation_tests tests/ui_image_button_animation.c)
    target_link_libraries(ab_ui_image_animation_tests PRIVATE ab_game)
    add_test(NAME ui_image_animation_edges COMMAND ab_ui_image_animation_tests)
    add_executable(ab_ui_frame_draw_tests tests/ui_frame_draw.c)
    target_link_libraries(ab_ui_frame_draw_tests PRIVATE ab_game)
    add_test(NAME ui_frame_draw_edges COMMAND ab_ui_frame_draw_tests)
    add_executable(ab_ui_image_draw_tests tests/ui_image_draw.c)
    target_link_libraries(ab_ui_image_draw_tests PRIVATE ab_game)
    add_test(NAME ui_image_draw_edges COMMAND ab_ui_image_draw_tests)
    add_executable(ab_ui_button_draw_tests tests/ui_image_button_draw.c)
    target_link_libraries(ab_ui_button_draw_tests PRIVATE ab_game)
    add_test(NAME ui_button_draw_edges COMMAND ab_ui_button_draw_tests)
    add_executable(ab_ui_image_resources_tests tests/ui_image_resources.c)
    target_link_libraries(ab_ui_image_resources_tests PRIVATE ab_game)
    add_test(NAME ui_image_resources_edges COMMAND ab_ui_image_resources_tests)
    add_executable(ab_ui_image_state_tests tests/ui_image_state.c)
    target_link_libraries(ab_ui_image_state_tests PRIVATE ab_game)
    add_test(NAME ui_image_state_edges COMMAND ab_ui_image_state_tests)
    add_executable(ab_ui_widget_draw_tests tests/ui_widget_draw.c)
    target_link_libraries(ab_ui_widget_draw_tests PRIVATE ab_game)
    add_test(NAME ui_widget_draw_edges COMMAND ab_ui_widget_draw_tests)
    add_executable(ab_ui_widget_tests tests/ui_widget.c)
    target_link_libraries(ab_ui_widget_tests PRIVATE ab_scene)
    add_test(NAME ui_widget_tree_integration COMMAND ab_ui_widget_tests)
    add_executable(ab_ui_panel_tests tests/ui_panels.c)
    target_link_libraries(ab_ui_panel_tests PRIVATE ab_scene)
    add_test(NAME ui_panel_integration COMMAND ab_ui_panel_tests)
    add_executable(ab_ui_menu_slider_tests tests/ui_menu_slider.c)
    target_link_libraries(ab_ui_menu_slider_tests PRIVATE ab_scene)
    add_test(NAME ui_menu_slider_integration COMMAND ab_ui_menu_slider_tests)
    add_executable(ab_ui_slider_tests tests/ui_slider.c)
    target_link_libraries(ab_ui_slider_tests PRIVATE ab_scene)
    add_test(NAME ui_slider_integration COMMAND ab_ui_slider_tests)
    add_executable(ab_ui_hud_powerups_tests tests/ui_hud_powerups.c)
    target_link_libraries(ab_ui_hud_powerups_tests PRIVATE ab_scene)
    add_executable(ab_ui_eagle_services_tests tests/ui_eagle_services.c)
    target_link_libraries(ab_ui_eagle_services_tests PRIVATE ab_scene)
    add_test(NAME ui_eagle_services_edges COMMAND ab_ui_eagle_services_tests)
    add_executable(ab_ui_game_hud_queries_tests tests/ui_game_hud_queries.c)
    target_link_libraries(ab_ui_game_hud_queries_tests PRIVATE ab_scene)
    set(AB_REFERENCE_GAME_HUD_QUERIES "" CACHE FILEPATH
        "Optional original GameHud visibility predicate corpus")
    if(AB_REFERENCE_GAME_HUD_QUERIES)
        add_test(NAME ui_game_hud_queries_android
            COMMAND "${CMAKE_COMMAND}" "-DREFERENCE=${AB_REFERENCE_GAME_HUD_QUERIES}"
                "-DCHECKER=$<TARGET_FILE:ab_ui_game_hud_queries_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_ui_game_hud_queries_reference.cmake")
    endif()
    add_executable(ab_settings_inventory_tests tests/settings_inventory.c)
    target_link_libraries(ab_settings_inventory_tests PRIVATE ab_game)
    add_test(NAME settings_inventory_edges COMMAND ab_settings_inventory_tests)
    add_executable(ab_ui_hud_buttons_tests tests/ui_hud_buttons.c)
    target_link_libraries(ab_ui_hud_buttons_tests PRIVATE ab_scene)
    add_test(NAME ui_hud_buttons_integration COMMAND ab_ui_hud_buttons_tests)
    set(AB_REFERENCE_UI_SLIDER "" CACHE FILEPATH
        "Optional original Slider update/layout/curve corpus")
    if(AB_REFERENCE_UI_SLIDER)
        add_test(NAME ui_slider_android
            COMMAND "${CMAKE_COMMAND}" "-DREFERENCE=${AB_REFERENCE_UI_SLIDER}"
                "-DCHECKER=$<TARGET_FILE:ab_ui_slider_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_ui_slider_reference.cmake")
    endif()
    add_executable(ab_ui_scroll_frame_tests tests/ui_scroll_frame.c)
    target_link_libraries(ab_ui_scroll_frame_tests PRIVATE ab_scene)
    add_test(NAME ui_scroll_frame_integration COMMAND ab_ui_scroll_frame_tests)
    add_executable(ab_ui_toggle_controls_tests tests/ui_toggle_controls.c)
    target_link_libraries(ab_ui_toggle_controls_tests PRIVATE ab_scene)
    add_test(NAME ui_toggle_controls_integration COMMAND ab_ui_toggle_controls_tests)
    add_executable(ab_ui_tiled_image_tests tests/ui_tiled_image.c)
    target_link_libraries(ab_ui_tiled_image_tests PRIVATE ab_scene)
    add_test(NAME ui_tiled_image_integration COMMAND ab_ui_tiled_image_tests)
    if(APPLE)
        add_executable(ab_ui_widget_apk_tests tests/ui_widget_apk.c)
        target_link_libraries(ab_ui_widget_apk_tests PRIVATE ab_scene ab_asset_resources
            ab_settings_file ab_metal ab_audio_output)
        target_compile_definitions(ab_ui_widget_apk_tests PRIVATE
            AB_TEST_STATE_DIRECTORY="${CMAKE_CURRENT_BINARY_DIR}/ui-intro-fixture")
        if(AB_REFERENCE_APK)
            add_test(NAME ui_widget_apk_metal
                COMMAND ab_ui_widget_apk_tests "${AB_REFERENCE_APK}")
            add_test(NAME ui_widget_apk_skip_metal
                COMMAND ab_ui_widget_apk_tests "${AB_REFERENCE_APK}")
            add_test(NAME ui_shockwave_apk_metal
                COMMAND ab_ui_widget_apk_tests "${AB_REFERENCE_APK}")
            add_test(NAME ui_birthday_apk_metal
                COMMAND ab_ui_widget_apk_tests "${AB_REFERENCE_APK}")
            add_test(NAME ui_level_rewards_apk_metal
                COMMAND ab_ui_widget_apk_tests "${AB_REFERENCE_APK}")
            add_test(NAME ui_puffer_apk_metal
                COMMAND ab_ui_widget_apk_tests "${AB_REFERENCE_APK}")
            add_test(NAME ui_vial_effects_apk_metal
                COMMAND ab_ui_widget_apk_tests "${AB_REFERENCE_APK}")
            add_test(NAME ui_vial_contact_apk_metal
                COMMAND ab_ui_widget_apk_tests "${AB_REFERENCE_APK}")
            set_tests_properties(ui_vial_contact_apk_metal PROPERTIES
                ENVIRONMENT "AB_TEST_VIAL_CONTACT_ONLY=1")
            add_test(NAME ui_spawn_apk_metal
                COMMAND ab_ui_widget_apk_tests "${AB_REFERENCE_APK}")
            set_tests_properties(ui_spawn_apk_metal PROPERTIES
                ENVIRONMENT "AB_TEST_SPAWN_ONLY=1")
            add_test(NAME ui_goal_factory_apk_metal
                COMMAND ab_ui_widget_apk_tests "${AB_REFERENCE_APK}")
            set_tests_properties(ui_goal_factory_apk_metal PROPERTIES
                ENVIRONMENT "AB_TEST_GOAL_FACTORY_ONLY=1")
            add_test(NAME ui_blue_vial_apk_metal
                COMMAND ab_ui_widget_apk_tests "${AB_REFERENCE_APK}")
            set_tests_properties(ui_blue_vial_apk_metal PROPERTIES
                ENVIRONMENT "AB_TEST_BLUE_VIAL_ONLY=1")
            add_test(NAME ui_pink_vial_apk_metal
                COMMAND ab_ui_widget_apk_tests "${AB_REFERENCE_APK}")
            set_tests_properties(ui_pink_vial_apk_metal PROPERTIES
                ENVIRONMENT "AB_TEST_PINK_VIAL_ONLY=1")
            add_test(NAME ui_vial_chain_apk_metal
                COMMAND ab_ui_widget_apk_tests "${AB_REFERENCE_APK}")
            set_tests_properties(ui_vial_chain_apk_metal PROPERTIES
                ENVIRONMENT "AB_TEST_VIAL_CHAIN_ONLY=1")
            add_test(NAME ui_soundboard_apk_metal
                COMMAND ab_ui_widget_apk_tests "${AB_REFERENCE_APK}")
            set_tests_properties(ui_soundboard_apk_metal PROPERTIES
                ENVIRONMENT "AB_TEST_SOUNDBOARD_ONLY=1")
            add_test(NAME ui_keyboard_apk_metal
                COMMAND ab_ui_widget_apk_tests "${AB_REFERENCE_APK}")
            set_tests_properties(ui_keyboard_apk_metal PROPERTIES
                ENVIRONMENT "AB_TEST_KEYBOARD_ONLY=1")
            add_test(NAME ui_sequencer_apk_metal
                COMMAND ab_ui_widget_apk_tests "${AB_REFERENCE_APK}")
            set_tests_properties(ui_sequencer_apk_metal PROPERTIES
                ENVIRONMENT "AB_TEST_SEQUENCER_ONLY=1")
            add_test(NAME ui_radio_apk_metal
                COMMAND ab_ui_widget_apk_tests "${AB_REFERENCE_APK}")
            set_tests_properties(ui_radio_apk_metal PROPERTIES
                ENVIRONMENT "AB_TEST_RADIO_ONLY=1")
            add_test(NAME ui_accordion_apk_metal
                COMMAND ab_ui_widget_apk_tests "${AB_REFERENCE_APK}")
            set_tests_properties(ui_accordion_apk_metal PROPERTIES
                ENVIRONMENT "AB_TEST_ACCORDION_ONLY=1")
            add_test(NAME ui_rocket_apk_metal
                COMMAND ab_ui_widget_apk_tests "${AB_REFERENCE_APK}")
            set_tests_properties(ui_rocket_apk_metal PROPERTIES
                ENVIRONMENT "AB_TEST_ROCKET_ONLY=1")
            add_test(NAME ui_mode_egg_apk_metal
                COMMAND ab_ui_widget_apk_tests "${AB_REFERENCE_APK}")
            add_test(NAME ui_physics_replay_apk_metal
                COMMAND ab_ui_widget_apk_tests "${AB_REFERENCE_APK}")
            set_tests_properties(ui_physics_replay_apk_metal PROPERTIES
                ENVIRONMENT "AB_TEST_PHYSICS_REPLAY_ONLY=1")
            set_tests_properties(ui_mode_egg_apk_metal PROPERTIES
                ENVIRONMENT "AB_TEST_MODE_EGG_ONLY=1")
            set_tests_properties(ui_vial_effects_apk_metal PROPERTIES
                ENVIRONMENT "AB_TEST_VIAL_EFFECTS_ONLY=1")
            set_tests_properties(ui_puffer_apk_metal PROPERTIES
                ENVIRONMENT "AB_TEST_PUFFER_ONLY=1")
            set_tests_properties(ui_level_rewards_apk_metal PROPERTIES
                ENVIRONMENT "AB_TEST_LEVEL_REWARDS_ONLY=1")
            set_tests_properties(ui_birthday_apk_metal PROPERTIES
                ENVIRONMENT "AB_TEST_BIRTHDAY_ONLY=1")
            set_tests_properties(ui_shockwave_apk_metal PROPERTIES
                ENVIRONMENT "AB_TEST_SHOCKWAVE_ONLY=1")
            set_tests_properties(ui_widget_apk_skip_metal PROPERTIES
                ENVIRONMENT "AB_TEST_SKIP_END=1")
            set_tests_properties(ui_widget_apk_metal ui_widget_apk_skip_metal
                ui_shockwave_apk_metal ui_birthday_apk_metal
                ui_level_rewards_apk_metal ui_puffer_apk_metal
                ui_vial_effects_apk_metal ui_mode_egg_apk_metal
                ui_vial_contact_apk_metal ui_spawn_apk_metal ui_goal_factory_apk_metal
                ui_blue_vial_apk_metal ui_pink_vial_apk_metal ui_vial_chain_apk_metal
                ui_soundboard_apk_metal ui_keyboard_apk_metal ui_sequencer_apk_metal
                ui_radio_apk_metal ui_accordion_apk_metal
                ui_rocket_apk_metal
                ui_physics_replay_apk_metal PROPERTIES
                SKIP_RETURN_CODE 77 RESOURCE_LOCK ab_ui_intro_profile)
            set(AB_REFERENCE_OBJECT_ANIMATION "" CACHE FILEPATH
                "Private original Android object-animation corpus")
            if(AB_REFERENCE_OBJECT_ANIMATION)
                file(SHA256 "${AB_REFERENCE_OBJECT_ANIMATION}" object_animation_hash)
                if(NOT object_animation_hash STREQUAL
                    "f018fa43bd0c2a5d435bb7d9b4b035f7ebf4c963e23587ac40f7424231dac575")
                    message(FATAL_ERROR "Unexpected original object-animation corpus")
                endif()
                set_property(TEST ui_widget_apk_metal ui_widget_apk_skip_metal
                    APPEND PROPERTY ENVIRONMENT
                    "AB_OBJECT_ANIMATION_REFERENCE=${AB_REFERENCE_OBJECT_ANIMATION}")
            endif()
        endif()
    endif()
    add_executable(ab_ui_menu_manager_tests tests/ui_menu_manager.c)
    target_link_libraries(ab_ui_menu_manager_tests PRIVATE ab_game)
    add_test(NAME ui_menu_manager_edges COMMAND ab_ui_menu_manager_tests)
    add_executable(ab_ui_key_tests tests/ui_key.c)
    target_link_libraries(ab_ui_key_tests PRIVATE ab_game)
    add_test(NAME ui_key_edges COMMAND ab_ui_key_tests)
    add_executable(ab_main_menu_theme_tests tests/main_menu_theme.c)
    target_link_libraries(ab_main_menu_theme_tests PRIVATE ab_game)
    add_test(NAME main_menu_theme_edges COMMAND ab_main_menu_theme_tests)
    add_executable(ab_main_menu_settings_tests tests/main_menu_settings.c)
    target_link_libraries(ab_main_menu_settings_tests PRIVATE ab_game)
    add_test(NAME main_menu_settings_edges COMMAND ab_main_menu_settings_tests)
    add_executable(ab_theme_layer_tests tests/theme_layer.c)
    add_executable(ab_mask_draw_tests tests/mask_draw.c)
    add_executable(ab_z_order_tests tests/z_order.c)
    add_executable(ab_level_egg_tests tests/level_egg.c)
    add_executable(ab_level_clock_tests tests/level_clock.c)
    target_link_libraries(ab_level_clock_tests PRIVATE ab_game)
    add_test(NAME level_clock_edges COMMAND ab_level_clock_tests)
    set(AB_REFERENCE_LEVEL_CLOCK "" CACHE FILEPATH "Private pinned clock-level trace")
    if(AB_REFERENCE_LEVEL_CLOCK)
        file(SHA256 "${AB_REFERENCE_LEVEL_CLOCK}" level_clock_hash)
        if(NOT level_clock_hash STREQUAL
            "9ff3f589c6f9c3618484abbc27ab2c5772aad749dba1ee88af462a691430cf63")
            message(FATAL_ERROR "Expected the pinned original clock-level trace")
        endif()
        add_test(NAME level_clock_native COMMAND ab_level_clock_tests "${AB_REFERENCE_LEVEL_CLOCK}")
    endif()
    target_link_libraries(ab_level_egg_tests PRIVATE ab_game)
    add_test(NAME level_egg_edges COMMAND ab_level_egg_tests)
    set(AB_REFERENCE_LEVEL_EGG "" CACHE FILEPATH "Private pinned level egg-hook trace")
    if(AB_REFERENCE_LEVEL_EGG)
        file(SHA256 "${AB_REFERENCE_LEVEL_EGG}" level_egg_hash)
        if(NOT level_egg_hash STREQUAL
            "978d42407af14ea84370b5a47773291fd852bfc004ebb7321b9f8827df921f90")
            message(FATAL_ERROR "Expected the pinned original level egg-hook trace")
        endif()
        add_test(NAME level_egg_native COMMAND ab_level_egg_tests "${AB_REFERENCE_LEVEL_EGG}")
    endif()
    add_executable(ab_extra_draw_tests tests/extra_draw.c)
    target_link_libraries(ab_extra_draw_tests PRIVATE ab_render)
    add_test(NAME extra_draw_edges COMMAND ab_extra_draw_tests)
    set(AB_REFERENCE_EXTRA_DRAW "" CACHE FILEPATH "Private pinned original extra-draw trace")
    if(AB_REFERENCE_EXTRA_DRAW)
        file(SHA256 "${AB_REFERENCE_EXTRA_DRAW}" extra_draw_hash)
        if(NOT extra_draw_hash STREQUAL
            "bf640813916e25070395dc90a4a7eba35cd8016af7873d8e2ec7b4775aa9d85b")
            message(FATAL_ERROR "Expected the recorded original extra-draw trace")
        endif()
        add_test(NAME extra_draw_native COMMAND ab_extra_draw_tests "${AB_REFERENCE_EXTRA_DRAW}")
    endif()
    target_link_libraries(ab_z_order_tests PRIVATE ab_base)
    add_test(NAME z_order_edges COMMAND ab_z_order_tests)
    set(AB_REFERENCE_Z_ORDER "" CACHE FILEPATH "Private pinned native numeric z-order trace")
    if(AB_REFERENCE_Z_ORDER)
        file(SHA256 "${AB_REFERENCE_Z_ORDER}" z_order_hash)
        if(NOT z_order_hash STREQUAL
            "6ebc71055fa6405e4590e9ddd476e019064cb96ab9ed1b26a6a24d1ff7c7075d")
            message(FATAL_ERROR "Expected the recorded native numeric z-order trace")
        endif()
        add_test(NAME z_order_native COMMAND ab_z_order_tests "${AB_REFERENCE_Z_ORDER}")
    endif()
    target_link_libraries(ab_mask_draw_tests PRIVATE ab_render)
    add_test(NAME mask_draw_edges COMMAND ab_mask_draw_tests)
    set(AB_REFERENCE_MASK_DRAW "" CACHE FILEPATH "Private pinned native masked-image trace")
    if(AB_REFERENCE_MASK_DRAW)
        file(SHA256 "${AB_REFERENCE_MASK_DRAW}" mask_draw_hash)
        if(NOT mask_draw_hash STREQUAL
            "9ea8a2ecbcd1e652d575f8e47e0f83f90001b55a085cbd21a036cbf7024204bc")
            message(FATAL_ERROR "Expected the recorded native masked-image trace")
        endif()
        add_test(NAME mask_draw_native COMMAND ab_mask_draw_tests "${AB_REFERENCE_MASK_DRAW}")
    endif()
    target_link_libraries(ab_theme_layer_tests PRIVATE ab_render)
    add_test(NAME theme_layer_edges COMMAND ab_theme_layer_tests)
    add_executable(ab_theme_background_tests tests/theme_background.c)
    target_link_libraries(ab_theme_background_tests PRIVATE ab_render)
    add_test(NAME theme_background_edges COMMAND ab_theme_background_tests)
    add_executable(ab_game_rectangle_tests tests/game_rectangle.c)
    target_link_libraries(ab_game_rectangle_tests PRIVATE ab_render)
    add_test(NAME game_rectangle_edges COMMAND ab_game_rectangle_tests)
    add_executable(ab_theme_foreground_tests tests/theme_foreground.c)
    target_link_libraries(ab_theme_foreground_tests PRIVATE ab_render)
    add_test(NAME theme_foreground_edges COMMAND ab_theme_foreground_tests)
    add_executable(ab_theme_colors_tests tests/theme_colors.c)
    target_link_libraries(ab_theme_colors_tests PRIVATE ab_resource)
    add_test(NAME theme_colors_edges COMMAND ab_theme_colors_tests)
    set(AB_REFERENCE_THEME_COLORS "" CACHE FILEPATH
        "Optional original ThemeComposition color construction")
    if(AB_REFERENCE_THEME_COLORS)
        add_test(NAME theme_colors_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_THEME_COLORS}"
                "-DCHECKER=$<TARGET_FILE:ab_theme_colors_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_theme_colors_reference.cmake")
    endif()
    set(AB_REFERENCE_THEME_FOREGROUND "" CACHE FILEPATH
        "Optional original foreground ground-fill/order/state corpus")
    if(AB_REFERENCE_THEME_FOREGROUND)
        add_test(NAME theme_foreground_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_THEME_FOREGROUND}"
                "-DCHECKER=$<TARGET_FILE:ab_theme_foreground_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_theme_foreground_reference.cmake")
    endif()
    set(AB_REFERENCE_GAME_RECTANGLE "" CACHE FILEPATH
        "Optional original GameLua rectangle context-fill arguments")
    if(AB_REFERENCE_GAME_RECTANGLE)
        add_test(NAME game_rectangle_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_GAME_RECTANGLE}"
                "-DCHECKER=$<TARGET_FILE:ab_game_rectangle_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_game_rectangle_reference.cmake")
    endif()
    set(AB_REFERENCE_THEME_BACKGROUND "" CACHE FILEPATH
        "Optional original ThemeComposition background order/color/state")
    if(AB_REFERENCE_THEME_BACKGROUND)
        add_test(NAME theme_background_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_THEME_BACKGROUND}"
                "-DCHECKER=$<TARGET_FILE:ab_theme_background_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_theme_background_reference.cmake")
    endif()
    add_executable(ab_theme_layer_base_tests tests/theme_layer_base.c)
    target_link_libraries(ab_theme_layer_base_tests PRIVATE ab_render)
    add_test(NAME theme_layer_base_edges COMMAND ab_theme_layer_base_tests)
    add_executable(ab_theme_layer_data_tests tests/theme_layer_data.c)
    target_link_libraries(ab_theme_layer_data_tests PRIVATE ab_render)
    add_test(NAME theme_layer_data_edges COMMAND ab_theme_layer_data_tests)
    add_executable(ab_theme_rect_tests tests/theme_rect.c)
    target_link_libraries(ab_theme_rect_tests PRIVATE ab_render)
    add_test(NAME theme_rect_edges COMMAND ab_theme_rect_tests)
    set(AB_REFERENCE_THEME_RECT "" CACHE FILEPATH
        "Optional pinned Android rectangle-theme constructor/draw corpus")
    if(AB_REFERENCE_THEME_RECT)
        add_test(NAME theme_rect_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_THEME_RECT}"
                "-DCHECKER=$<TARGET_FILE:ab_theme_rect_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_theme_rect_reference.cmake")
    endif()
    add_executable(ab_theme_catalog_tests tests/theme_catalog.c)
    target_link_libraries(ab_theme_catalog_tests PRIVATE ab_resource)
    add_executable(ab_composite_bounds_tests tests/composite_bounds.c)
    add_executable(ab_composite_draw_tests tests/composite_draw.c)
    target_link_libraries(ab_composite_draw_tests PRIVATE ab_render)
    add_test(NAME composite_draw_edges COMMAND ab_composite_draw_tests)
    target_link_libraries(ab_composite_bounds_tests PRIVATE ab_render)
    add_test(NAME composite_bounds_edges COMMAND ab_composite_bounds_tests)
    add_executable(ab_theme_bindings_tests tests/theme_bindings.c)
    target_link_libraries(ab_theme_bindings_tests PRIVATE ab_render)
    add_executable(ab_theme_scene_tests tests/theme_scene.c)
    target_link_libraries(ab_theme_scene_tests PRIVATE ab_render)
    add_executable(ab_theme_gpu_tests tests/theme_gpu.c)
    target_link_libraries(ab_theme_gpu_tests PRIVATE ab_resource)
    add_executable(ab_image_material_tests tests/image_material.c)
    target_link_libraries(ab_image_material_tests PRIVATE ab_render)
    add_test(NAME image_material_edges COMMAND ab_image_material_tests)
    add_executable(ab_fill_rectangle_tests tests/fill_rectangle.c)
    target_link_libraries(ab_fill_rectangle_tests PRIVATE ab_render)
    add_test(NAME fill_rectangle_edges COMMAND ab_fill_rectangle_tests)
    add_executable(ab_rubberband_tests tests/rubberband.c)
    target_link_libraries(ab_rubberband_tests PRIVATE ab_render)
    add_test(NAME rubberband_edges COMMAND ab_rubberband_tests)
    add_executable(ab_slingshot_draw_tests tests/slingshot_draw.c)
    target_link_libraries(ab_slingshot_draw_tests PRIVATE ab_render)
    add_test(NAME slingshot_draw_edges COMMAND ab_slingshot_draw_tests)
    add_executable(ab_slingshot_pose_tests tests/slingshot_pose.c)
    target_link_libraries(ab_slingshot_pose_tests PRIVATE ab_game)
    add_test(NAME slingshot_pose_edges COMMAND ab_slingshot_pose_tests)
    add_executable(ab_game_hud_tests tests/game_hud.c)
    target_link_libraries(ab_game_hud_tests PRIVATE ab_game)
    add_test(NAME game_hud_edges COMMAND ab_game_hud_tests)
    set(AB_REFERENCE_GAME_HUD "" CACHE FILEPATH
        "Optional original GameHud draw score-branch boundaries")
    if(AB_REFERENCE_GAME_HUD)
        add_test(NAME game_hud_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_GAME_HUD}"
                "-DCHECKER=$<TARGET_FILE:ab_game_hud_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_game_hud_reference.cmake")
    endif()
    set(AB_REFERENCE_SLINGSHOT_POSE "" CACHE FILEPATH
        "Optional original calculateRubberBand and coordinate transform trace")
    if(AB_REFERENCE_SLINGSHOT_POSE)
        add_test(NAME slingshot_pose_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_SLINGSHOT_POSE}"
                "-DCHECKER=$<TARGET_FILE:ab_slingshot_pose_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_slingshot_pose_reference.cmake")
    endif()
    set(AB_REFERENCE_SLINGSHOT_START_POSE "" CACHE FILEPATH
        "Optional original setPositionByStartObject vector trace")
    if(AB_REFERENCE_SLINGSHOT_START_POSE)
        add_test(NAME slingshot_start_pose_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_SLINGSHOT_START_POSE}"
                "-DCHECKER=$<TARGET_FILE:ab_slingshot_pose_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_slingshot_start_pose_reference.cmake")
    endif()
    set(AB_REFERENCE_SLINGSHOT_DRAW "" CACHE FILEPATH
        "Optional original SlingshotSystem back/holder/front draw boundaries")
    if(AB_REFERENCE_SLINGSHOT_DRAW)
        add_test(NAME slingshot_draw_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_SLINGSHOT_DRAW}"
                "-DCHECKER=$<TARGET_FILE:ab_slingshot_draw_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_slingshot_draw_reference.cmake")
    endif()
    set(AB_REFERENCE_SLINGSHOT_RECTANGLE "" CACHE FILEPATH
        "Optional original ordinary sling Resources/Sprite/GL_Image vertices")
    if(AB_REFERENCE_SLINGSHOT_RECTANGLE)
        add_test(NAME slingshot_rectangle_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_SLINGSHOT_RECTANGLE}"
                "-DCHECKER=$<TARGET_FILE:ab_slingshot_draw_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_slingshot_rectangle_reference.cmake")
    endif()
    set(AB_REFERENCE_RUBBERBAND "" CACHE FILEPATH
        "Optional original GameLua rubberband image vertices")
    if(AB_REFERENCE_RUBBERBAND)
        add_test(NAME rubberband_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_RUBBERBAND}"
                "-DCHECKER=$<TARGET_FILE:ab_rubberband_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_rubberband_reference.cmake")
    endif()
    set(AB_REFERENCE_RUBBERBAND_WORLD "" CACHE FILEPATH
        "Optional original Android SlingshotSystem camera and image vertices")
    if(AB_REFERENCE_RUBBERBAND_WORLD)
        add_test(NAME rubberband_world_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_RUBBERBAND_WORLD}"
                "-DCHECKER=$<TARGET_FILE:ab_rubberband_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_rubberband_world_reference.cmake")
    endif()
    set(AB_REFERENCE_FILL_RECTANGLE "" CACHE FILEPATH
        "Optional original reset-state GL_Context fill output")
    if(AB_REFERENCE_FILL_RECTANGLE)
        add_test(NAME fill_rectangle_android
            COMMAND "${CMAKE_COMMAND}"
                "-DAPK=${AB_REFERENCE_APK}"
                "-DREFERENCE=${AB_REFERENCE_FILL_RECTANGLE}"
                "-DCHECKER=$<TARGET_FILE:ab_fill_rectangle_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_fill_rectangle_reference.cmake")
    endif()
    set(AB_REFERENCE_IMAGE_MATERIAL "" CACHE FILEPATH
        "Optional original point-image default shader choices")
    set(AB_REFERENCE_IMAGE_RECTANGLE_MATERIAL "" CACHE FILEPATH
        "Optional original rectangle-image default shader choices")
    set(AB_REFERENCE_THEME_SCENE "" CACHE FILEPATH
        "Optional native theme1 construction and CPU drawing graph")
    set(AB_REFERENCE_THEME_SCENE_FILL "" CACHE FILEPATH
        "Optional native theme1 graph including context-fill CPU batches")
    set(AB_REFERENCE_THEME_BINDINGS "" CACHE FILEPATH
        "Optional selected live theme1 sprite/composite metadata and bounds")
    set(AB_REFERENCE_THEME_IMAGE_FORMATS "" CACHE FILEPATH
        "Optional selected live theme1 native image formats/dimensions")
    set(AB_REFERENCE_THEME_GPU_NATIVE "" CACHE FILEPATH
        "Optional original level-one framebuffer before presentation")
    set(AB_REFERENCE_THEME_GPU_PNG "" CACHE FILEPATH
        "Optional original PNG from the same framebuffer capture")
    set(AB_REFERENCE_THEME_GPU_GLES2 "" CACHE FILEPATH
        "Optional first-party Android GLES2 capture of the cached scene")
    set(AB_REFERENCE_THEME_GPU_INPUT "" CACHE FILEPATH
        "Optional cached scene input used by the first-party Android GPU probe")
    set(AB_REFERENCE_SLING_GPU_GLES2 "" CACHE FILEPATH
        "Optional first-party Android GLES2 capture with the idle sling")
    set(AB_REFERENCE_SLING_GPU_INPUT "" CACHE FILEPATH
        "Optional cached idle sling scene used by the Android GPU probe")
    set(AB_REFERENCE_HUD_GPU_GLES2 "" CACHE FILEPATH
        "Optional first-party idle score HUD GLES2 framebuffer")
    set(AB_REFERENCE_HUD_GPU_INPUT "" CACHE FILEPATH
        "Optional first-party idle score HUD GLES2 cached input")
    set(AB_REFERENCE_IDLE_GPU_GLES2 "" CACHE FILEPATH
        "Optional first-party complete idle frame GLES2 framebuffer")
    set(AB_REFERENCE_IDLE_GPU_INPUT "" CACHE FILEPATH
        "Optional first-party complete idle frame GLES2 cached input")
    set(AB_REFERENCE_COMPOSITE_BOUNDS "" CACHE FILEPATH
        "Optional original CompoSprite transformed integer bounds")
    set(AB_REFERENCE_COMPOSITE_DRAW "" CACHE FILEPATH
        "Optional original CompoSprite transformed point quads")
    set(AB_REFERENCE_COMPOSITE_CONTEXT_DRAW "" CACHE FILEPATH
        "Optional original CompoSprite rotated and reflected parent-context quads")
    if(AB_REFERENCE_COMPOSITE_DRAW)
        add_test(NAME composite_draw_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_COMPOSITE_DRAW}"
                "-DCHECKER=$<TARGET_FILE:ab_composite_draw_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_composite_draw_reference.cmake")
    endif()
    if(AB_REFERENCE_COMPOSITE_CONTEXT_DRAW)
        add_test(NAME composite_draw_context_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_COMPOSITE_CONTEXT_DRAW}"
                "-DCHECKER=$<TARGET_FILE:ab_composite_draw_tests>"
                -DROTATED_PARENT=ON
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_composite_draw_reference.cmake")
    endif()
    if(AB_REFERENCE_COMPOSITE_BOUNDS)
        add_test(NAME composite_bounds_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_COMPOSITE_BOUNDS}"
                "-DCHECKER=$<TARGET_FILE:ab_composite_bounds_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_composite_bounds_reference.cmake")
    endif()
    set(AB_REFERENCE_THEME_CATALOG "" CACHE FILEPATH
        "Optional original complete classic theme catalog typed values")
    set(AB_REFERENCE_THEME_LAYER_CONSTRUCT "" CACHE FILEPATH
        "Optional original ordinary ThemeLayer constructors")
    if(AB_REFERENCE_THEME_LAYER_CONSTRUCT)
        add_test(NAME theme_layer_data_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_THEME_LAYER_CONSTRUCT}"
                "-DCHECKER=$<TARGET_FILE:ab_theme_layer_data_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_theme_layer_construct_reference.cmake")
    endif()
    set(AB_REFERENCE_THEME_LAYER_CONSTRUCT_COMPOSITE "" CACHE FILEPATH
        "Optional original ThemeLayer constructors using composite metadata")
    if(AB_REFERENCE_THEME_LAYER_CONSTRUCT_COMPOSITE)
        add_test(NAME theme_layer_data_composite_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_THEME_LAYER_CONSTRUCT_COMPOSITE}"
                "-DCHECKER=$<TARGET_FILE:ab_theme_layer_data_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_theme_layer_construct_composite_reference.cmake")
    endif()
    set(AB_REFERENCE_THEME_LAYER_BASE "" CACHE FILEPATH
        "Optional original ThemeLayer/DrawUtils/Sprite base image draws")
    if(AB_REFERENCE_THEME_LAYER_BASE)
        add_test(NAME theme_layer_base_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_THEME_LAYER_BASE}"
                "-DCHECKER=$<TARGET_FILE:ab_theme_layer_base_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_theme_layer_base_reference.cmake")
    endif()
    set(AB_REFERENCE_THEME_LAYER_COMPOSITE "" CACHE FILEPATH
        "Optional original ThemeLayer/DrawUtils composite image draws")
    if(AB_REFERENCE_THEME_LAYER_COMPOSITE)
        add_test(NAME theme_layer_composite_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_THEME_LAYER_COMPOSITE}"
                "-DCHECKER=$<TARGET_FILE:ab_theme_layer_base_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_theme_layer_composite_reference.cmake")
    endif()
    set(AB_REFERENCE_THEME_LAYER_GPU "" CACHE FILEPATH
        "Optional original theme composite/GL image CPU batch geometry")
    if(AB_REFERENCE_THEME_LAYER_GPU)
        add_test(NAME theme_layer_image_batch_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_THEME_LAYER_GPU}"
                "-DCHECKER=$<TARGET_FILE:ab_theme_layer_base_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_theme_layer_gpu_reference.cmake")
    endif()
    set(AB_REFERENCE_THEME_LAYER "" CACHE FILEPATH
        "Optional original ThemeLayer horizontal parallax transforms")
    if(AB_REFERENCE_THEME_LAYER)
        add_test(NAME theme_layer_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_THEME_LAYER}"
                "-DCHECKER=$<TARGET_FILE:ab_theme_layer_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_theme_layer_reference.cmake")
    endif()
    add_executable(ab_main_menu_resources_tests tests/main_menu_resources.c)
    target_link_libraries(ab_main_menu_resources_tests PRIVATE ab_game)
    set(AB_REFERENCE_MAIN_MENU_RESOURCES "" CACHE FILEPATH
        "Optional original main-menu theme layout using original settings/theme data")
    set(AB_REFERENCE_MAIN_MENU_SETTINGS "" CACHE FILEPATH
        "Optional original main-menu settings getters/setters and raw defaults")
    if(AB_REFERENCE_MAIN_MENU_SETTINGS)
        add_test(NAME main_menu_settings_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_MAIN_MENU_SETTINGS}"
                "-DCHECKER=$<TARGET_FILE:ab_main_menu_settings_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_main_menu_settings_reference.cmake")
    endif()
    set(AB_REFERENCE_MAIN_MENU_THEME "" CACHE FILEPATH
        "Optional original main-menu theme layout/camera/water corpus")
    if(AB_REFERENCE_MAIN_MENU_THEME)
        add_test(NAME main_menu_theme_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_MAIN_MENU_THEME}"
                "-DCHECKER=$<TARGET_FILE:ab_main_menu_theme_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_main_menu_theme_reference.cmake")
    endif()
    add_executable(ab_ui_menu_navigation_tests tests/ui_menu_navigation.c)
    target_link_libraries(ab_ui_menu_navigation_tests PRIVATE ab_game)
    add_test(NAME ui_menu_navigation_edges COMMAND ab_ui_menu_navigation_tests)
    set(AB_REFERENCE_UI_MENU_MANAGER "" CACHE FILEPATH
        "Optional original full-class menu lifecycle/update/draw corpus")
    if(AB_REFERENCE_UI_MENU_MANAGER)
        add_test(NAME ui_menu_manager_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_UI_MENU_MANAGER}"
                "-DCHECKER=$<TARGET_FILE:ab_ui_menu_manager_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_ui_menu_manager_reference.cmake")
    endif()
    set(AB_REFERENCE_UI_MENU_LINKS "" CACHE FILEPATH
        "Optional original menu link registration/result routing corpus")
    if(AB_REFERENCE_UI_MENU_LINKS)
        add_test(NAME ui_menu_links_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_UI_MENU_LINKS}"
                "-DCHECKER=$<TARGET_FILE:ab_ui_menu_manager_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_ui_menu_links_reference.cmake")
    endif()
    set(AB_REFERENCE_UI_MENU_NAVIGATION "" CACHE FILEPATH
        "Optional original menu events composed with canonical frame operations")
    if(AB_REFERENCE_UI_MENU_NAVIGATION)
        add_test(NAME ui_menu_navigation_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_UI_MENU_NAVIGATION}"
                "-DCHECKER=$<TARGET_FILE:ab_ui_menu_manager_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_ui_menu_navigation_reference.cmake")
    endif()
    set(AB_REFERENCE_UI_MENU_INPUT "" CACHE FILEPATH
        "Optional original current-root pointer routing and input order corpus")
    if(AB_REFERENCE_UI_MENU_INPUT)
        add_test(NAME ui_menu_input_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_UI_MENU_INPUT}"
                "-DCHECKER=$<TARGET_FILE:ab_ui_menu_manager_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_ui_menu_input_reference.cmake")
    endif()
    set(AB_REFERENCE_UI_MENU_KEYS "" CACHE FILEPATH
        "Optional original menu keyboard routing, shortcuts and native key-map order")
    if(AB_REFERENCE_UI_MENU_KEYS)
        add_test(NAME ui_menu_keys_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_UI_MENU_KEYS}"
                "-DCHECKER=$<TARGET_FILE:ab_ui_menu_manager_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_ui_menu_keys_reference.cmake")
    endif()
    set(AB_REFERENCE_UI_MENU_FRAME_KEYS "" CACHE FILEPATH
        "Optional original keyboard routing composed with Frame child traversal")
    if(AB_REFERENCE_UI_MENU_FRAME_KEYS)
        add_test(NAME ui_menu_frame_keys_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_UI_MENU_FRAME_KEYS}"
                "-DCHECKER=$<TARGET_FILE:ab_ui_menu_manager_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_ui_menu_frame_keys_reference.cmake")
    endif()
    set(AB_REFERENCE_UI_IMAGE_STATE "" CACHE FILEPATH
        "Optional original full-class Image/ImageButton constructor/entry corpus")
    if(AB_REFERENCE_UI_IMAGE_STATE)
        add_test(NAME ui_image_state_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_UI_IMAGE_STATE}"
                "-DCHECKER=$<TARGET_FILE:ab_ui_image_state_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_ui_image_state_reference.cmake")
    endif()
    add_executable(ab_ui_localization_tests tests/ui_localization.c)
    target_link_libraries(ab_ui_localization_tests PRIVATE ab_game)
    add_test(NAME ui_localization_edges COMMAND ab_ui_localization_tests)
    add_executable(ab_ui_init_tests tests/ui_init.c)
    target_link_libraries(ab_ui_init_tests PRIVATE ab_game)
    add_test(NAME ui_init_edges COMMAND ab_ui_init_tests)
    add_executable(ab_ui_frame_tree_tests tests/ui_frame_tree.c)
    target_link_libraries(ab_ui_frame_tree_tests PRIVATE ab_game)
    add_test(NAME ui_frame_tree_edges COMMAND ab_ui_frame_tree_tests)
    set(AB_REFERENCE_UI_FRAME_TREE "" CACHE FILEPATH
        "Optional original Frame child hierarchy/lifecycle corpus")
    if(AB_REFERENCE_UI_FRAME_TREE)
        add_test(NAME ui_frame_tree_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_UI_FRAME_TREE}"
                "-DCHECKER=$<TARGET_FILE:ab_ui_frame_tree_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_ui_frame_tree_reference.cmake")
    endif()
    add_executable(ab_ui_text_lines_tests tests/ui_text_lines.c)
    target_link_libraries(ab_ui_text_lines_tests PRIVATE ab_game ab_render)
    add_test(NAME ui_text_lines_edges COMMAND ab_ui_text_lines_tests)
    add_executable(ab_ui_text_bounds_tests tests/ui_text_bounds.c)
    target_link_libraries(ab_ui_text_bounds_tests PRIVATE ab_game ab_render)
    add_test(NAME ui_text_bounds_edges COMMAND ab_ui_text_bounds_tests)
    set(AB_REFERENCE_UI_TEXT_BOUNDS "" CACHE FILEPATH
        "Optional original Text bounds/correction/native-font corpus")
    if(AB_REFERENCE_UI_TEXT_BOUNDS)
        add_test(NAME ui_text_bounds_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_UI_TEXT_BOUNDS}"
                "-DCHECKER=$<TARGET_FILE:ab_ui_text_bounds_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_ui_text_bounds_reference.cmake")
    endif()
    set(AB_REFERENCE_UI_TEXT_LINES "" CACHE FILEPATH
        "Optional original Text.clip/native-wrap/constructor composition corpus")
    if(AB_REFERENCE_UI_TEXT_LINES)
        add_test(NAME ui_text_lines_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_UI_TEXT_LINES}"
                "-DCHECKER=$<TARGET_FILE:ab_ui_text_lines_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_ui_text_lines_reference.cmake")
    endif()
    set(AB_REFERENCE_UI_TEXT_METRICS "" CACHE FILEPATH
        "Optional original Text width/height/leading/native-font corpus")
    if(AB_REFERENCE_UI_TEXT_METRICS)
        add_test(NAME ui_text_metrics_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_UI_TEXT_METRICS}"
                "-DCHECKER=$<TARGET_FILE:ab_ui_text_lines_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_ui_text_metrics_reference.cmake")
    endif()
    set(AB_REFERENCE_UI_INIT "" CACHE FILEPATH
        "Optional original plain Frame/Text constructor corpus")
    if(AB_REFERENCE_UI_INIT)
        add_test(NAME ui_init_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_UI_INIT}"
                "-DCHECKER=$<TARGET_FILE:ab_ui_init_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_ui_init_reference.cmake")
    endif()
    set(AB_REFERENCE_UI_FONT "" CACHE FILEPATH
        "Optional original Text callable font/init/setter/layout corpus")
    if(AB_REFERENCE_UI_FONT)
        add_test(NAME ui_font_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_UI_FONT}"
                "-DCHECKER=$<TARGET_FILE:ab_ui_init_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_ui_font_reference.cmake")
    endif()
    add_executable(ab_ui_text_tests tests/ui_text_draw.c)
    target_link_libraries(ab_ui_text_tests PRIVATE ab_game ab_render)
    add_test(NAME ui_text_edges COMMAND ab_ui_text_tests)
    add_executable(ab_ui_text_visibility_tests tests/ui_text_visibility.c)
    target_link_libraries(ab_ui_text_visibility_tests PRIVATE ab_game)
    add_test(NAME ui_text_visibility_edges COMMAND ab_ui_text_visibility_tests)
    add_executable(ab_ui_text_clip_tests tests/ui_text_clip.c)
    target_link_libraries(ab_ui_text_clip_tests PRIVATE ab_game ab_render)
    add_test(NAME ui_text_clip_edges COMMAND ab_ui_text_clip_tests)
    set(AB_REFERENCE_UI_TEXT_CLIP "" CACHE FILEPATH
        "Optional original native prepared clipped-line traversal corpus")
    if(AB_REFERENCE_UI_TEXT_CLIP)
        add_test(NAME ui_text_clip_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_UI_TEXT_CLIP}"
                "-DCHECKER=$<TARGET_FILE:ab_ui_text_clip_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_ui_text_clip_reference.cmake")
    endif()
    set(AB_REFERENCE_UI_TEXT_VISIBILITY "" CACHE FILEPATH
        "Optional original native clipped-text vertical visibility corpus")
    if(AB_REFERENCE_UI_TEXT_VISIBILITY)
        add_test(NAME ui_text_visibility_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_UI_TEXT_VISIBILITY}"
                "-DCHECKER=$<TARGET_FILE:ab_ui_text_visibility_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_ui_text_visibility_reference.cmake")
    endif()
    set(AB_REFERENCE_UI_TEXT_IMAGE "" CACHE FILEPATH
        "Optional original native UI text through GLES2 image batch geometry")
    if(AB_REFERENCE_UI_TEXT_IMAGE)
        add_test(NAME ui_text_image_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_UI_TEXT_IMAGE}"
                "-DCHECKER=$<TARGET_FILE:ab_ui_text_tests>"
                -DIMAGE=ON
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_ui_text_reference.cmake")
    endif()
    set(AB_REFERENCE_UI_TEXT "" CACHE FILEPATH
        "Optional original native unclipped UI text drawing corpus")
    if(AB_REFERENCE_UI_TEXT)
        add_test(NAME ui_text_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_UI_TEXT}"
                "-DCHECKER=$<TARGET_FILE:ab_ui_text_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_ui_text_reference.cmake")
    endif()
    add_executable(ab_text_catalog_tests tests/text_catalog.c)
    target_link_libraries(ab_text_catalog_tests PRIVATE ab_resource)
    add_executable(ab_text_resources_tests tests/text_resources.c)
    target_link_libraries(ab_text_resources_tests PRIVATE ab_game)
    add_test(NAME text_resource_edges COMMAND ab_text_resources_tests)
    add_executable(ab_text_locale_tests tests/text_locale.c)
    target_link_libraries(ab_text_locale_tests PRIVATE ab_resource)
    set(AB_REFERENCE_TEXT_LOCALE "" CACHE FILEPATH
        "Optional original Android GameLua locale choice corpus")
    if(AB_REFERENCE_TEXT_LOCALE)
        add_test(NAME text_locale_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_TEXT_LOCALE}"
                "-DCHECKER=$<TARGET_FILE:ab_text_locale_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_text_locale_reference.cmake")
    endif()
    set(AB_REFERENCE_TEXT_CATALOG "" CACHE FILEPATH
        "Optional original native TEXT catalog parsing corpus")
    if(AB_REFERENCE_TEXT_CATALOG AND AB_REFERENCE_APK)
        add_test(NAME target_text_catalog_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_TEXT_CATALOG}"
                "-DAPK=${AB_REFERENCE_APK}"
                "-DCHECKER=$<TARGET_FILE:ab_text_catalog_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_text_catalog_reference.cmake")
    endif()
    set(AB_REFERENCE_UI_LOCALIZATION "" CACHE FILEPATH
        "Optional original localization group/cache/fallback corpus")
    if(AB_REFERENCE_UI_LOCALIZATION)
        add_test(NAME ui_localization_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_UI_LOCALIZATION}"
                "-DCHECKER=$<TARGET_FILE:ab_ui_localization_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_ui_localization_reference.cmake")
    endif()
    set(AB_REFERENCE_UI_IMAGE_RESOURCES "" CACHE FILEPATH
        "Optional original Image/ImageButton localization/sprite binding corpus")
    if(AB_REFERENCE_UI_IMAGE_RESOURCES)
        add_test(NAME ui_image_resources_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_UI_IMAGE_RESOURCES}"
                "-DCHECKER=$<TARGET_FILE:ab_ui_image_resources_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_ui_image_resources_reference.cmake")
    endif()
    set(AB_REFERENCE_UI_BUTTON_DRAW "" CACHE FILEPATH
        "Optional original ImageButton/Image/Frame draw graph corpus")
    if(AB_REFERENCE_UI_BUTTON_DRAW)
        add_test(NAME ui_button_draw_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_UI_BUTTON_DRAW}"
                "-DCHECKER=$<TARGET_FILE:ab_ui_button_draw_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_ui_button_draw_reference.cmake")
    endif()
    set(AB_REFERENCE_UI_IMAGE_DRAW "" CACHE FILEPATH
        "Optional original Image draw state/sprite/culling corpus")
    if(AB_REFERENCE_UI_IMAGE_DRAW)
        add_test(NAME ui_image_draw_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_UI_IMAGE_DRAW}"
                "-DCHECKER=$<TARGET_FILE:ab_ui_image_draw_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_ui_image_draw_reference.cmake")
    endif()
    set(AB_REFERENCE_UI_FRAME_DRAW "" CACHE FILEPATH
        "Optional original Frame draw/tag/filter/transform corpus")
    if(AB_REFERENCE_UI_FRAME_DRAW)
        add_test(NAME ui_frame_draw_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_UI_FRAME_DRAW}"
                "-DCHECKER=$<TARGET_FILE:ab_ui_frame_draw_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_ui_frame_draw_reference.cmake")
    endif()
    set(AB_REFERENCE_UI_IMAGE_ANIMATION "" CACHE FILEPATH
        "Optional original ImageButton hover/pressed animation corpus")
    if(AB_REFERENCE_UI_IMAGE_ANIMATION)
        add_test(NAME ui_image_animation_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_UI_IMAGE_ANIMATION}"
                "-DCHECKER=$<TARGET_FILE:ab_ui_image_animation_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_ui_image_animation_reference.cmake")
    endif()
    set(AB_REFERENCE_UI_IMAGE_BUTTON "" CACHE FILEPATH
        "Optional original ImageButton pointer state and ordered boundary corpus")
    if(AB_REFERENCE_UI_IMAGE_BUTTON)
        add_test(NAME ui_image_button_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_UI_IMAGE_BUTTON}"
                "-DCHECKER=$<TARGET_FILE:ab_ui_image_button_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_ui_image_button_reference.cmake")
    endif()
    set(AB_REFERENCE_UI_IMAGE_HIT "" CACHE FILEPATH
        "Optional original ImageButton rectangle and native strict hit corpus")
    set(AB_REFERENCE_UI_BUTTON_STATE "" CACHE FILEPATH
        "Optional original typed ImageButton flags/replies and callback snapshots")
    if(AB_REFERENCE_UI_BUTTON_STATE)
        add_test(NAME ui_button_state_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_UI_BUTTON_STATE}"
                "-DCHECKER=$<TARGET_FILE:ab_ui_image_button_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_ui_button_state_reference.cmake")
    endif()
    if(AB_REFERENCE_UI_IMAGE_HIT)
        add_test(NAME ui_image_hit_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_UI_IMAGE_HIT}"
                "-DCHECKER=$<TARGET_FILE:ab_ui_image_hit_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_ui_image_hit_reference.cmake")
    endif()
    set(AB_REFERENCE_UI_POINTER "" CACHE FILEPATH
        "Optional original Frame pointer dispatch/reply/callback corpus")
    if(AB_REFERENCE_UI_POINTER)
        add_test(NAME ui_pointer_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_UI_POINTER}"
                "-DCHECKER=$<TARGET_FILE:ab_ui_pointer_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_ui_pointer_reference.cmake")
    endif()
    set(AB_REFERENCE_UI_FRAME_GEOMETRY "" CACHE FILEPATH
        "Optional original UI ancestor geometry and invisible-button hit corpus")
    if(AB_REFERENCE_UI_FRAME_GEOMETRY)
        add_test(NAME ui_frame_geometry_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_UI_FRAME_GEOMETRY}"
                "-DCHECKER=$<TARGET_FILE:ab_ui_frame_geometry_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_ui_frame_geometry_reference.cmake")
    endif()
    add_executable(ab_utf8_tests tests/utf8.c)
    add_executable(ab_utf8_encode_tests tests/utf8_encode.c)
    target_link_libraries(ab_utf8_encode_tests PRIVATE ab_base)
    add_test(NAME utf8_encode_edges COMMAND ab_utf8_encode_tests)
    set(AB_REFERENCE_UTF8_ENCODE "" CACHE FILEPATH
        "Optional original UTFConverter encoding/string conversion corpus")
    if(AB_REFERENCE_UTF8_ENCODE)
        add_test(NAME utf8_encode_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_UTF8_ENCODE}"
                "-DCHECKER=$<TARGET_FILE:ab_utf8_encode_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_utf8_encode_reference.cmake")
    endif()
    target_link_libraries(ab_utf8_tests PRIVATE ab_base)
    add_test(NAME utf8_legacy_bounds COMMAND ab_utf8_tests)
    set(AB_REFERENCE_UTF8 "" CACHE FILEPATH
        "Optional native 65536-step and 4096-string UTF-8 conversion corpus")
    if(AB_REFERENCE_UTF8)
        add_test(NAME utf8_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_UTF8}"
                "-DCHECKER=$<TARGET_FILE:ab_utf8_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_utf8_reference.cmake")
    endif()
    add_executable(ab_font_metrics_tests tests/font_metrics.c)
    add_executable(ab_font_wrap_tests tests/font_wrap.c)
    target_link_libraries(ab_font_wrap_tests PRIVATE ab_render)
    add_test(NAME font_wrap_edges COMMAND ab_font_wrap_tests)
    set(AB_REFERENCE_FONT_WRAP "" CACHE FILEPATH
        "Optional original GameLua clipText line selection corpus")
    if(AB_REFERENCE_FONT_WRAP)
        add_test(NAME font_wrap_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_FONT_WRAP}"
                "-DCHECKER=$<TARGET_FILE:ab_font_wrap_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_font_wrap_reference.cmake")
    endif()
    target_link_libraries(ab_font_metrics_tests PRIVATE ab_render)
    add_test(NAME font_metric_edges COMMAND ab_font_metrics_tests)
    add_executable(ab_font_layout_tests tests/font_layout.c)
    target_link_libraries(ab_font_layout_tests PRIVATE ab_render)
    add_test(NAME font_layout_edges COMMAND ab_font_layout_tests)
    add_executable(ab_object_sprite_tests tests/object_sprite.c)
    target_link_libraries(ab_object_sprite_tests PRIVATE ab_render)
    add_test(NAME object_sprite_edges COMMAND ab_object_sprite_tests)
    add_executable(ab_lua_asset_sprite_tests tests/lua_asset_sprite.c)
    target_link_libraries(ab_lua_asset_sprite_tests PRIVATE ab_render)
    add_test(NAME lua_asset_sprite_edges COMMAND ab_lua_asset_sprite_tests)
    add_executable(ab_lua_asset_binding_tests tests/lua_asset_binding.c)
    target_link_libraries(ab_lua_asset_binding_tests PRIVATE ab_render)
    add_test(NAME lua_asset_sprite_binding COMMAND ab_lua_asset_binding_tests)
    add_executable(ab_lua_asset_renderer_tests tests/lua_asset_renderer.c)
    target_link_libraries(ab_lua_asset_renderer_tests PRIVATE ab_render)
    add_test(NAME lua_asset_renderer_edges COMMAND ab_lua_asset_renderer_tests)
    set(AB_REFERENCE_LUA_ASSET_RENDERER "" CACHE FILEPATH
        "Optional original GameLua renderer construction and registration corpus")
    if(AB_REFERENCE_LUA_ASSET_RENDERER)
        add_test(NAME lua_asset_renderer_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_LUA_ASSET_RENDERER}"
                "-DCHECKER=$<TARGET_FILE:ab_lua_asset_renderer_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_lua_asset_renderer_reference.cmake")
    endif()
    add_executable(ab_scene_id_tests tests/scene_id.c)
    target_link_libraries(ab_scene_id_tests PRIVATE ab_render)
    add_test(NAME scene_id_edges COMMAND ab_scene_id_tests)
    add_executable(ab_scene_membership_tests tests/scene_membership.c)
    target_link_libraries(ab_scene_membership_tests PRIVATE ab_render)
    add_test(NAME scene_membership_edges COMMAND ab_scene_membership_tests)
    set(AB_REFERENCE_SCENE_MEMBERSHIP "" CACHE FILEPATH
        "Optional original scene insertion/removal and numeric disposal corpus")
    if(AB_REFERENCE_SCENE_MEMBERSHIP)
        add_test(NAME scene_membership_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_SCENE_MEMBERSHIP}"
                "-DCHECKER=$<TARGET_FILE:ab_scene_membership_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_scene_membership_reference.cmake")
    endif()
    set(AB_REFERENCE_SCENE_ID "" CACHE FILEPATH
        "Optional original scene counter and integer-to-number corpus")
    if(AB_REFERENCE_SCENE_ID)
        add_test(NAME scene_id_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_SCENE_ID}"
                "-DCHECKER=$<TARGET_FILE:ab_scene_id_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_scene_id_reference.cmake")
    endif()
    add_executable(ab_sprite_scene_tests tests/sprite_scene.c)
    target_link_libraries(ab_sprite_scene_tests PRIVATE ab_render)
    add_test(NAME sprite_scene_order_and_draw COMMAND ab_sprite_scene_tests)
    set(AB_REFERENCE_SCENE_ORDER "" CACHE FILEPATH
        "Optional retained native scene layer/vector ordering capture")
    if(AB_REFERENCE_SCENE_ORDER)
        add_test(NAME sprite_scene_order_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_SCENE_ORDER}"
                "-DCHECKER=$<TARGET_FILE:ab_sprite_scene_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_scene_order_reference.cmake")
    endif()
    add_executable(ab_image_rectangle_tests tests/image_rectangle.c)
    target_link_libraries(ab_image_rectangle_tests PRIVATE ab_render)
    add_test(NAME image_rectangle_edges COMMAND ab_image_rectangle_tests)
    add_executable(ab_sprite_sized_tests tests/sprite_sized.c)
    target_link_libraries(ab_sprite_sized_tests PRIVATE ab_render)
    add_test(NAME sprite_sized_edges COMMAND ab_sprite_sized_tests)
    add_executable(ab_screen_clip_tests tests/screen_clip.c)
    target_link_libraries(ab_screen_clip_tests PRIVATE ab_render)
    add_test(NAME screen_clip_edges COMMAND ab_screen_clip_tests)
    add_executable(ab_ui_image_render_tests tests/ui_image_render.c)
    target_link_libraries(ab_ui_image_render_tests PRIVATE ab_scene)
    add_test(NAME ui_image_render_edges COMMAND ab_ui_image_render_tests)
    set(AB_REFERENCE_UI_IMAGE_RENDER "" CACHE FILEPATH
        "Optional original UI Image/GameLua/Sprite/GL batch composition corpus")
    if(AB_REFERENCE_UI_IMAGE_RENDER)
        add_test(NAME ui_image_render_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_UI_IMAGE_RENDER}"
                "-DCHECKER=$<TARGET_FILE:ab_ui_image_render_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_ui_image_render_reference.cmake")
    endif()
    set(AB_REFERENCE_SPRITE_SIZED "" CACHE FILEPATH
        "Optional original explicitly-sized Sprite rectangles and UVs")
    if(AB_REFERENCE_SPRITE_SIZED)
        add_test(NAME sprite_sized_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_SPRITE_SIZED}"
                "-DCHECKER=$<TARGET_FILE:ab_sprite_sized_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_sprite_sized_reference.cmake")
    endif()
    set(AB_REFERENCE_IMAGE_RECTANGLE "" CACHE FILEPATH
        "Optional recorded 4096-case native image rectangle corpus")
    if(AB_REFERENCE_IMAGE_RECTANGLE)
        add_test(NAME image_rectangle_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_IMAGE_RECTANGLE}"
                "-DCHECKER=$<TARGET_FILE:ab_image_rectangle_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_image_rectangle_reference.cmake")
    endif()
    set(AB_REFERENCE_LUA_ASSET_SPRITE "" CACHE FILEPATH
        "Optional recorded 4096-case native ordinary Lua asset sprite corpus")
    if(AB_REFERENCE_LUA_ASSET_SPRITE)
        add_test(NAME lua_asset_sprite_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_LUA_ASSET_SPRITE}"
                "-DCHECKER=$<TARGET_FILE:ab_lua_asset_sprite_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_lua_asset_sprite_reference.cmake")
    endif()
    set(AB_REFERENCE_OBJECT_SPRITE "" CACHE FILEPATH
        "Optional recorded 4096-case native object sprite transform corpus")
    if(AB_REFERENCE_OBJECT_SPRITE)
        add_test(NAME object_sprite_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_OBJECT_SPRITE}"
                "-DCHECKER=$<TARGET_FILE:ab_object_sprite_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_object_sprite_reference.cmake")
    endif()
    set(AB_REFERENCE_OBJECT_RENDER "" CACHE FILEPATH
        "Optional recorded 4096-case native ordinary object renderer corpus")
    if(AB_REFERENCE_OBJECT_RENDER)
        add_test(NAME object_render_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_OBJECT_RENDER}"
                "-DCHECKER=$<TARGET_FILE:ab_object_sprite_tests>"
                -DKIND=state
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_object_sprite_reference.cmake")
    endif()
    set(AB_REFERENCE_FONT_LAYOUT "" CACHE FILEPATH
        "Optional recorded 4096-case native bitmap-font glyph layout corpus")
    if(AB_REFERENCE_FONT_LAYOUT)
        add_test(NAME font_layout_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_FONT_LAYOUT}"
                "-DCHECKER=$<TARGET_FILE:ab_font_layout_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_font_layout_reference.cmake")
    endif()
    set(AB_REFERENCE_FONT_IMAGE "" CACHE FILEPATH
        "Optional recorded 4096-case native bitmap-font image boundary corpus")
    if(AB_REFERENCE_FONT_IMAGE)
        add_test(NAME font_image_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_FONT_IMAGE}"
                "-DCHECKER=$<TARGET_FILE:ab_font_layout_tests>"
                -DKIND=image
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_font_layout_reference.cmake")
    endif()
    set(AB_REFERENCE_FONT_METRICS "" CACHE FILEPATH
        "Optional recorded 4096-case native bitmap-font range metric corpus")
    if(AB_REFERENCE_FONT_METRICS)
        add_test(NAME font_metrics_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_FONT_METRICS}"
                "-DCHECKER=$<TARGET_FILE:ab_font_metrics_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_font_metrics_reference.cmake")
    endif()
    set(AB_REFERENCE_CAMERA_VIEWPORT "" CACHE FILEPATH
        "Optional recorded 4096-case original CameraSystem viewport corpus")
    set(AB_REFERENCE_CAMERA_MOVER "" CACHE FILEPATH
        "Optional original CameraMover command/state/default cubic corpus")
    set(AB_REFERENCE_CAMERA_LIMITS "" CACHE FILEPATH
        "Optional original camera limit initialization/projection/input corpus")
    set(AB_REFERENCE_CAMERA_SEQUENCE "" CACHE FILEPATH
        "Optional original camera sequence manager command/state/boundary corpus")
    set(AB_REFERENCE_CAMERA_SYSTEM "" CACHE FILEPATH
        "Optional integrated original camera controller corpus with APK sequences")
    set(AB_REFERENCE_CAMERA_TOUCH "" CACHE FILEPATH
        "Optional original touch FSM and composed camera corpus with APK sequences")
    set(AB_REFERENCE_CAMERA_TOUCH_FILTERED "" CACHE FILEPATH
        "Optional integrated original menu filtering/touch copy/camera corpus")
    set(AB_REFERENCE_MENU_POINTER "" CACHE FILEPATH
        "Optional original menu left-button/wheel filtering corpus")
    if(AB_REFERENCE_MENU_POINTER)
        add_test(NAME classic_menu_input_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_MENU_POINTER}"
                "-DCHECKER=$<TARGET_FILE:ab_classic_menu_input_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_menu_pointer_reference.cmake")
        add_test(NAME game_menu_pointer_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_MENU_POINTER}"
                "-DCHECKER=$<TARGET_FILE:ab_menu_pointer_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_menu_pointer_reference.cmake")
    endif()
    set(AB_REFERENCE_CAMERA_SHAKE "" CACHE FILEPATH
        "Optional original shake command/state/shared random corpus")
    set(AB_REFERENCE_CAMERA_DRAG "" CACHE FILEPATH
        "Optional original drag command/buffer/native coordinate corpus")
    set(AB_REFERENCE_CAMERA_TWEENS "" CACHE FILEPATH
        "Optional original mover/builtin tween selection/evaluation corpus")
    set(AB_REFERENCE_SCRIPT_TRIG "" CACHE FILEPATH
        "Optional original float Lua sine/cosine corpus")
    set(AB_REFERENCE_CAMERA_SELECTION "" CACHE FILEPATH
        "Optional original nearest/directional snap and zoom query corpus")
    if(AB_REFERENCE_CAMERA_SELECTION)
        add_test(NAME game_camera_selection_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_CAMERA_SELECTION}"
                "-DCHECKER=$<TARGET_FILE:ab_camera_selection_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_camera_selection_reference.cmake")
    endif()
    if(AB_REFERENCE_SCRIPT_TRIG)
        add_test(NAME script_trig_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_SCRIPT_TRIG}"
                "-DCHECKER=$<TARGET_FILE:ab_script_trig_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_script_trig_reference.cmake")
    endif()
    if(AB_REFERENCE_CAMERA_TWEENS)
        add_test(NAME game_camera_tweens_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_CAMERA_TWEENS}"
                "-DCHECKER=$<TARGET_FILE:ab_camera_tweens_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_camera_tweens_reference.cmake")
    endif()
    if(AB_REFERENCE_CAMERA_DRAG)
        add_test(NAME game_camera_drag_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_CAMERA_DRAG}"
                "-DCHECKER=$<TARGET_FILE:ab_camera_drag_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_camera_drag_reference.cmake")
    endif()
    if(AB_REFERENCE_CAMERA_SHAKE)
        add_test(NAME game_camera_shake_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_CAMERA_SHAKE}"
                "-DCHECKER=$<TARGET_FILE:ab_camera_shake_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_camera_shake_reference.cmake")
    endif()
    if(AB_REFERENCE_CAMERA_SEQUENCE)
        add_test(NAME game_camera_sequence_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_CAMERA_SEQUENCE}"
                "-DCHECKER=$<TARGET_FILE:ab_camera_sequence_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_camera_sequence_reference.cmake")
    endif()
    if(AB_REFERENCE_CAMERA_LIMITS)
        add_test(NAME game_camera_limits_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_CAMERA_LIMITS}"
                "-DCHECKER=$<TARGET_FILE:ab_camera_limits_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_camera_limits_reference.cmake")
    endif()
    if(AB_REFERENCE_CAMERA_MOVER)
        add_test(NAME game_camera_mover_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_CAMERA_MOVER}"
                "-DCHECKER=$<TARGET_FILE:ab_camera_mover_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_camera_mover_reference.cmake")
    endif()
    if(AB_REFERENCE_CAMERA_VIEWPORT)
        add_test(NAME game_camera_viewport_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_CAMERA_VIEWPORT}"
                "-DCHECKER=$<TARGET_FILE:ab_camera_viewport_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_camera_viewport_reference.cmake")
    endif()
    set(AB_REFERENCE_LIVE_CAMERA_VIEWPORT "" CACHE FILEPATH
        "Optional captured Lua viewport inputs and running native camera fields")
    if(AB_REFERENCE_LIVE_CAMERA_VIEWPORT)
        add_test(NAME game_camera_viewport_live
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_LIVE_CAMERA_VIEWPORT}"
                "-DCHECKER=$<TARGET_FILE:ab_camera_viewport_tests>"
                -DKIND=live
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_camera_viewport_reference.cmake")
    endif()
    set(AB_REFERENCE_COORDINATES "" CACHE FILEPATH
        "Optional recorded 4096-case native camera coordinate corpus")
    if(AB_REFERENCE_COORDINATES)
        add_test(NAME game_coordinates_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_COORDINATES}"
                "-DCHECKER=$<TARGET_FILE:ab_coordinate_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_coordinates_reference.cmake")
    endif()
    set(AB_REFERENCE_LEVEL_COMPLETE "" CACHE FILEPATH
        "Optional recorded 4096-case original completion-initialization corpus")
    if(AB_REFERENCE_LEVEL_COMPLETE)
        add_test(NAME game_level_complete_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_LEVEL_COMPLETE}"
                "-DCHECKER=$<TARGET_FILE:ab_level_complete_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_level_complete_reference.cmake")
    endif()
    set(AB_REFERENCE_LEVEL_ENDING "" CACHE FILEPATH
        "Optional recorded 4096-case original ending-delay/event corpus")
    if(AB_REFERENCE_LEVEL_ENDING)
        add_test(NAME game_level_ending_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_LEVEL_ENDING}"
                "-DCHECKER=$<TARGET_FILE:ab_level_ending_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_level_ending_reference.cmake")
    endif()
    add_executable(ab_bird_retirement_tests tests/bird_retirement.c)
    target_link_libraries(ab_bird_retirement_tests PRIVATE ab_game)
    add_test(NAME game_bird_retirement_edges COMMAND ab_bird_retirement_tests)
    set(AB_REFERENCE_BIRD_RETIREMENT "" CACHE FILEPATH
        "Optional recorded 4096-case original ordinary bird-retirement corpus")
    if(AB_REFERENCE_BIRD_RETIREMENT)
        add_test(NAME game_bird_retirement_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_BIRD_RETIREMENT}"
                "-DCHECKER=$<TARGET_FILE:ab_bird_retirement_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_bird_retirement_reference.cmake")
    endif()
    set(AB_REFERENCE_LEVEL_BONUS "" CACHE FILEPATH
        "Optional recorded 4096-case original remaining-bird bonus corpus")
    if(AB_REFERENCE_LEVEL_BONUS)
        add_test(NAME game_level_bonus_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_LEVEL_BONUS}"
                "-DCHECKER=$<TARGET_FILE:ab_level_bonus_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_level_bonus_reference.cmake")
    endif()
    set(AB_REFERENCE_LIVE_BONUS "" CACHE FILEPATH
        "Optional captured first-level completion/remaining-bird score cadence")
    if(AB_REFERENCE_LIVE_BONUS)
        add_test(NAME game_level_bonus_live
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_LIVE_BONUS}"
                "-DCHECKER=$<TARGET_FILE:ab_level_bonus_tests>"
                -DKIND=live
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_level_bonus_reference.cmake")
    endif()
    set(AB_REFERENCE_LEVEL_FLOW "" CACHE FILEPATH
        "Optional recorded 4096-case original completion/goals/before-ending corpus")
    if(AB_REFERENCE_LEVEL_FLOW)
        add_test(NAME game_level_flow_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_LEVEL_FLOW}"
                "-DCHECKER=$<TARGET_FILE:ab_level_flow_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_level_flow_reference.cmake")
    endif()
    add_executable(ab_bird_jump_tests tests/bird_jump.c)
    target_link_libraries(ab_bird_jump_tests PRIVATE ab_game)
    add_test(NAME game_bird_jump_edges COMMAND ab_bird_jump_tests)
    add_executable(ab_wave_tests tests/wave.c)
    target_link_libraries(ab_wave_tests PRIVATE ab_resource)
    add_test(NAME resource_wave_pcm16_edges COMMAND ab_wave_tests)
    add_executable(ab_mp3_tests tests/mp3.c)
    target_link_libraries(ab_mp3_tests PRIVATE ab_resource)
    add_test(NAME resource_mp3_structure_edges COMMAND ab_mp3_tests)
    add_executable(ab_mp3_side_tests tests/mp3_side.c)
    target_link_libraries(ab_mp3_side_tests PRIVATE ab_resource)
    add_test(NAME resource_mp3_side_edges COMMAND ab_mp3_side_tests)
    add_executable(ab_mp3_reservoir_tests tests/mp3_reservoir.c)
    target_link_libraries(ab_mp3_reservoir_tests PRIVATE ab_resource)
    add_test(NAME resource_mp3_reservoir_edges COMMAND ab_mp3_reservoir_tests)
    add_executable(ab_mp3_scalefactor_tests tests/mp3_scalefactors.c)
    target_link_libraries(ab_mp3_scalefactor_tests PRIVATE ab_resource)
    add_test(NAME resource_mp3_scalefactor_edges COMMAND ab_mp3_scalefactor_tests)
    add_executable(ab_mp3_huffman_tests tests/mp3_huffman.c)
    target_link_libraries(ab_mp3_huffman_tests PRIVATE ab_resource)
    add_test(NAME resource_mp3_huffman_edges COMMAND ab_mp3_huffman_tests)
    add_executable(ab_mp3_spectrum_tests tests/mp3_spectrum.c)
    target_link_libraries(ab_mp3_spectrum_tests PRIVATE ab_resource)
    add_test(NAME resource_mp3_spectrum_edges COMMAND ab_mp3_spectrum_tests)
    add_executable(ab_mp3_gain_tests tests/mp3_gain.c)
    target_link_libraries(ab_mp3_gain_tests PRIVATE ab_resource)
    add_test(NAME resource_mp3_gain_edges COMMAND ab_mp3_gain_tests)
    add_executable(ab_mp3_requantize_tests tests/mp3_requantize.c)
    target_link_libraries(ab_mp3_requantize_tests PRIVATE ab_resource)
    add_test(NAME resource_mp3_requantize_edges COMMAND ab_mp3_requantize_tests)
    add_executable(ab_mp3_pristine_tests tests/mp3_pristine.c)
    target_link_libraries(ab_mp3_pristine_tests PRIVATE ab_resource)
    add_executable(ab_mp3_stereo_alias_tests tests/mp3_stereo_alias.c)
    target_link_libraries(ab_mp3_stereo_alias_tests PRIVATE ab_resource)
    add_test(NAME resource_mp3_stereo_alias_edges COMMAND ab_mp3_stereo_alias_tests)
    add_executable(ab_mp3_long_tests tests/mp3_transform.c)
    target_link_libraries(ab_mp3_long_tests PRIVATE ab_resource)
    add_test(NAME resource_mp3_long_edges COMMAND ab_mp3_long_tests)
    add_executable(ab_mp3_short_tests tests/mp3_transform.c)
    target_compile_definitions(ab_mp3_short_tests PRIVATE AB_TEST_MP3_SHORT=1)
    target_link_libraries(ab_mp3_short_tests PRIVATE ab_resource)
    add_test(NAME resource_mp3_short_edges COMMAND ab_mp3_short_tests)
    set(AB_REFERENCE_MP3_SHORT "" CACHE FILEPATH
        "Optional native MPEG-2 short-transform corpus")
    if(AB_REFERENCE_MP3_SHORT)
        add_test(NAME resource_mp3_short_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_MP3_SHORT}"
                "-DCHECKER=$<TARGET_FILE:ab_mp3_short_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_mp3_short_reference.cmake")
    endif()
    add_executable(ab_mp3_hybrid_tests tests/mp3_hybrid.c)
    target_link_libraries(ab_mp3_hybrid_tests PRIVATE ab_resource)
    add_test(NAME resource_mp3_hybrid_edges COMMAND ab_mp3_hybrid_tests)
    add_executable(ab_mp3_synthesis_transform_tests tests/mp3_synthesis_transform.c)
    target_link_libraries(ab_mp3_synthesis_transform_tests PRIVATE ab_resource)
    add_test(NAME resource_mp3_synthesis_transform_edges
        COMMAND ab_mp3_synthesis_transform_tests)
    add_executable(ab_mp3_synthesis_tests tests/mp3_synthesis.c)
    target_link_libraries(ab_mp3_synthesis_tests PRIVATE ab_resource)
    add_test(NAME resource_mp3_synthesis_edges COMMAND ab_mp3_synthesis_tests)
    foreach(variant MONO STEREO 24000)
        set(AB_REFERENCE_MP3_SYNTHESIS_${variant} "" CACHE FILEPATH
            "Optional native MPEG-2 ${variant} synthesis corpus")
        if(AB_REFERENCE_MP3_SYNTHESIS_${variant})
            string(TOLOWER "${variant}" kind)
            add_test(NAME resource_mp3_synthesis_${kind}_android
                COMMAND "${CMAKE_COMMAND}"
                    "-DREFERENCE=${AB_REFERENCE_MP3_SYNTHESIS_${variant}}"
                    "-DCHECKER=$<TARGET_FILE:ab_mp3_synthesis_tests>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_mp3_synthesis_reference.cmake")
        endif()
    endforeach()
    set(AB_REFERENCE_MP3_SYNTHESIS_TRANSFORM "" CACHE FILEPATH
        "Optional native MPEG-2 synthesis-transform corpus")
    if(AB_REFERENCE_MP3_SYNTHESIS_TRANSFORM)
        add_test(NAME resource_mp3_synthesis_transform_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_MP3_SYNTHESIS_TRANSFORM}"
                "-DCHECKER=$<TARGET_FILE:ab_mp3_synthesis_transform_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_mp3_synthesis_transform_reference.cmake")
    endif()
    add_executable(ab_mp3_windows_tests tests/mp3_windows.c)
    target_link_libraries(ab_mp3_windows_tests PRIVATE ab_resource)
    add_test(NAME resource_mp3_windows_edges COMMAND ab_mp3_windows_tests)
    set(AB_REFERENCE_MP3_WINDOWS "" CACHE FILEPATH
        "Optional native MPEG-2 hybrid-window corpus")
    if(AB_REFERENCE_MP3_WINDOWS)
        add_test(NAME resource_mp3_windows_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_MP3_WINDOWS}"
                "-DCHECKER=$<TARGET_FILE:ab_mp3_windows_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_mp3_windows_reference.cmake")
    endif()
    set(AB_REFERENCE_MP3_LONG "" CACHE FILEPATH
        "Optional native MPEG-2 long-transform corpus")
    if(AB_REFERENCE_MP3_LONG)
        add_test(NAME resource_mp3_long_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_MP3_LONG}"
                "-DCHECKER=$<TARGET_FILE:ab_mp3_long_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_mp3_long_reference.cmake")
    endif()
    foreach(variant 16000 22050 24000 POWER)
        set(AB_REFERENCE_MP3_REQUANTIZE_${variant} "" CACHE FILEPATH
            "Optional native MPEG-2 ${variant} requantization corpus")
        set(AB_REFERENCE_MP3_REQUANTIZE_SOURCE_${variant} "" CACHE FILEPATH
            "Optional first-party ${variant} requantization input")
        if(AB_REFERENCE_MP3_REQUANTIZE_${variant} AND
           AB_REFERENCE_MP3_REQUANTIZE_SOURCE_${variant})
            string(TOLOWER "${variant}" kind)
            add_test(NAME resource_mp3_requantize_${kind}_android
                COMMAND "${CMAKE_COMMAND}"
                    "-DREFERENCE=${AB_REFERENCE_MP3_REQUANTIZE_${variant}}"
                    "-DSOURCE=${AB_REFERENCE_MP3_REQUANTIZE_SOURCE_${variant}}"
                    "-DCHECKER=$<TARGET_FILE:ab_mp3_requantize_tests>"
                    "-DVARIANT=${variant}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_mp3_requantize_reference.cmake")
        endif()
    endforeach()
    set(AB_REFERENCE_MP3_GAIN "" CACHE FILEPATH
        "Optional native MPEG-2 complete quarter-step gain table")
    if(AB_REFERENCE_MP3_GAIN)
        add_test(NAME resource_mp3_gain_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_MP3_GAIN}"
                "-DCHECKER=$<TARGET_FILE:ab_mp3_gain_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_mp3_gain_reference.cmake")
    endif()
    foreach(rate 16000 22050 24000)
        set(AB_REFERENCE_MP3_REGIONS_${rate} "" CACHE FILEPATH
            "Optional native MPEG-2 ${rate} region-count corpus")
        if(AB_REFERENCE_MP3_REGIONS_${rate})
            add_test(NAME resource_mp3_regions_${rate}_android
                COMMAND "${CMAKE_COMMAND}"
                    "-DREFERENCE=${AB_REFERENCE_MP3_REGIONS_${rate}}"
                    "-DCHECKER=$<TARGET_FILE:ab_mp3_side_tests>"
                    "-DVARIANT=${rate}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_mp3_region_reference.cmake")
        endif()
        set(AB_REFERENCE_MP3_SPECTRUM_${rate} "" CACHE FILEPATH
            "Optional native MPEG-2 ${rate} quantized spectrum corpus")
        set(AB_REFERENCE_MP3_SPECTRUM_SOURCE_${rate} "" CACHE FILEPATH
            "Optional first-party ${rate} spectral input packets")
        if(AB_REFERENCE_MP3_SPECTRUM_${rate} AND
           AB_REFERENCE_MP3_SPECTRUM_SOURCE_${rate})
            add_test(NAME resource_mp3_spectrum_${rate}_android
                COMMAND "${CMAKE_COMMAND}"
                    "-DREFERENCE=${AB_REFERENCE_MP3_SPECTRUM_${rate}}"
                    "-DSOURCE=${AB_REFERENCE_MP3_SPECTRUM_SOURCE_${rate}}"
                    "-DCHECKER=$<TARGET_FILE:ab_mp3_spectrum_tests>"
                    "-DVARIANT=${rate}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_mp3_spectrum_reference.cmake")
        endif()
    endforeach()
    foreach(variant LOW ESCAPE_A ESCAPE_B MAGNITUDE QUAD POWER)
        set(AB_REFERENCE_MP3_HUFFMAN_${variant} "" CACHE FILEPATH
            "Optional native MPEG-2 ${variant} Huffman corpus")
        set(AB_REFERENCE_MP3_HUFFMAN_SOURCE_${variant} "" CACHE FILEPATH
            "Optional first-party ${variant} Huffman input packets")
        if(AB_REFERENCE_MP3_HUFFMAN_${variant} AND
           AB_REFERENCE_MP3_HUFFMAN_SOURCE_${variant})
            string(TOLOWER "${variant}" kind)
            add_test(NAME resource_mp3_huffman_${kind}_android
                COMMAND "${CMAKE_COMMAND}"
                    "-DREFERENCE=${AB_REFERENCE_MP3_HUFFMAN_${variant}}"
                    "-DSOURCE=${AB_REFERENCE_MP3_HUFFMAN_SOURCE_${variant}}"
                    "-DCHECKER=$<TARGET_FILE:ab_mp3_huffman_tests>"
                    "-DVARIANT=${variant}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_mp3_huffman_reference.cmake")
        endif()
    endforeach()
    foreach(variant MONO STEREO 24000)
        set(AB_REFERENCE_MP3_SIDE_${variant} "" CACHE FILEPATH
            "Optional native MPEG-2 ${variant} side-information corpus")
        if(AB_REFERENCE_MP3_SIDE_${variant})
            string(TOLOWER "${variant}" kind)
            add_test(NAME resource_mp3_side_${kind}_android
                COMMAND "${CMAKE_COMMAND}"
                    "-DREFERENCE=${AB_REFERENCE_MP3_SIDE_${variant}}"
                    "-DCHECKER=$<TARGET_FILE:ab_mp3_side_tests>"
                    "-DVARIANT=${variant}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_mp3_side_reference.cmake")
        endif()
        set(AB_REFERENCE_MP3_RESERVOIR_${variant} "" CACHE FILEPATH
            "Optional native MPEG-2 ${variant} reservoir windows")
        set(AB_REFERENCE_MP3_SOURCE_${variant} "" CACHE FILEPATH
            "Optional pinned ${variant} MP3 source for decoder comparisons")
        set(AB_REFERENCE_MP3_PRISTINE_${variant} "" CACHE FILEPATH
            "Optional pristine native ${variant} spectral capture")
        set(AB_REFERENCE_MP3_STEREO_ALIAS_${variant} "" CACHE FILEPATH
            "Optional native ${variant} stereo and alias capture")
        set(AB_REFERENCE_MP3_HYBRID_${variant} "" CACHE FILEPATH
            "Optional native ${variant} hybrid state capture")
        if(AB_REFERENCE_MP3_HYBRID_${variant} AND AB_REFERENCE_MP3_SOURCE_${variant})
            string(TOLOWER "${variant}" kind)
            add_test(NAME resource_mp3_hybrid_${kind}_android
                COMMAND "${CMAKE_COMMAND}"
                    "-DREFERENCE=${AB_REFERENCE_MP3_HYBRID_${variant}}"
                    "-DSOURCE=${AB_REFERENCE_MP3_SOURCE_${variant}}"
                    "-DCHECKER=$<TARGET_FILE:ab_mp3_hybrid_tests>"
                    "-DVARIANT=${variant}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_mp3_hybrid_reference.cmake")
        endif()
        if(AB_REFERENCE_MP3_STEREO_ALIAS_${variant} AND AB_REFERENCE_MP3_SOURCE_${variant})
            string(TOLOWER "${variant}" kind)
            add_test(NAME resource_mp3_stereo_alias_${kind}_android
                COMMAND "${CMAKE_COMMAND}"
                    "-DREFERENCE=${AB_REFERENCE_MP3_STEREO_ALIAS_${variant}}"
                    "-DSOURCE=${AB_REFERENCE_MP3_SOURCE_${variant}}"
                    "-DCHECKER=$<TARGET_FILE:ab_mp3_stereo_alias_tests>"
                    "-DVARIANT=${variant}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_mp3_stereo_alias_reference.cmake")
        endif()
        if(AB_REFERENCE_MP3_PRISTINE_${variant} AND AB_REFERENCE_MP3_SOURCE_${variant})
            string(TOLOWER "${variant}" kind)
            add_test(NAME resource_mp3_pristine_${kind}_android
                COMMAND "${CMAKE_COMMAND}"
                    "-DREFERENCE=${AB_REFERENCE_MP3_PRISTINE_${variant}}"
                    "-DSOURCE=${AB_REFERENCE_MP3_SOURCE_${variant}}"
                    "-DCHECKER=$<TARGET_FILE:ab_mp3_pristine_tests>"
                    "-DVARIANT=${variant}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_mp3_pristine_reference.cmake")
        endif()
        if(AB_REFERENCE_MP3_RESERVOIR_${variant} AND AB_REFERENCE_MP3_SOURCE_${variant})
            string(TOLOWER "${variant}" kind)
            add_test(NAME resource_mp3_reservoir_${kind}_android
                COMMAND "${CMAKE_COMMAND}"
                    "-DREFERENCE=${AB_REFERENCE_MP3_RESERVOIR_${variant}}"
                    "-DSOURCE=${AB_REFERENCE_MP3_SOURCE_${variant}}"
                    "-DCHECKER=$<TARGET_FILE:ab_mp3_reservoir_tests>"
                    "-DVARIANT=${variant}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_mp3_reservoir_reference.cmake")
        endif()
    endforeach()
    foreach(variant MONO STEREO 24000 NORMAL INTENSITY)
        set(AB_REFERENCE_MP3_SCALEFACTORS_${variant} "" CACHE FILEPATH
            "Optional native MPEG-2 ${variant} scalefactor corpus")
        set(AB_REFERENCE_MP3_SOURCE_${variant} "" CACHE FILEPATH
            "Optional pinned ${variant} MP3 source for decoder comparisons")
        if(AB_REFERENCE_MP3_SCALEFACTORS_${variant} AND AB_REFERENCE_MP3_SOURCE_${variant})
            string(TOLOWER "${variant}" kind)
            add_test(NAME resource_mp3_scalefactors_${kind}_android
                COMMAND "${CMAKE_COMMAND}"
                    "-DREFERENCE=${AB_REFERENCE_MP3_SCALEFACTORS_${variant}}"
                    "-DSOURCE=${AB_REFERENCE_MP3_SOURCE_${variant}}"
                    "-DCHECKER=$<TARGET_FILE:ab_mp3_scalefactor_tests>"
                    "-DVARIANT=${variant}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_mp3_scalefactors_reference.cmake")
        endif()
    endforeach()
    set(AB_REFERENCE_MP3_FORMAT "" CACHE FILEPATH
        "Optional complete native MP3 output format/length and independent frame manifest")
    set(AB_REFERENCE_WAVE_PCM16 "" CACHE FILEPATH
        "Optional independent PCM reference hashes for all target WAV entries")
    add_executable(ab_audio_group_tests tests/audio_group.c)
    target_link_libraries(ab_audio_group_tests PRIVATE ab_game)
    add_test(NAME game_audio_group_edges COMMAND ab_audio_group_tests)
    add_executable(ab_audio_play_tests tests/audio_play.c)
    target_link_libraries(ab_audio_play_tests PRIVATE ab_game)
    add_test(NAME game_audio_play_edges COMMAND ab_audio_play_tests)
    add_executable(ab_audio_mix_tests tests/audio_mix.c)
    target_link_libraries(ab_audio_mix_tests PRIVATE ab_game)
    add_test(NAME game_audio_mix_edges COMMAND ab_audio_mix_tests)
    add_executable(ab_audio_mixer_tests tests/audio_mixer.c)
    target_link_libraries(ab_audio_mixer_tests PRIVATE ab_game)
    add_test(NAME game_audio_mixer_edges COMMAND ab_audio_mixer_tests)
    add_executable(ab_audio_resources_tests tests/audio_resources.c)
    target_link_libraries(ab_audio_resources_tests PRIVATE ab_game)
    add_test(NAME game_audio_resources_edges COMMAND ab_audio_resources_tests)
    add_executable(ab_audio_pcm_tests tests/audio_pcm.c)
    add_executable(ab_audio_bank_tests tests/audio_bank.c)
    target_link_libraries(ab_audio_bank_tests PRIVATE ab_game)
    if(AB_REFERENCE_APK AND AB_REFERENCE_MP3_FORMAT)
        add_test(NAME audio_bank_apk
            COMMAND "${CMAKE_COMMAND}" "-DAPK=${AB_REFERENCE_APK}"
                "-DREFERENCE=${AB_REFERENCE_MP3_FORMAT}"
                "-DCHECKER=$<TARGET_FILE:ab_audio_bank_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_audio_bank_reference.cmake")
    endif()
    target_link_libraries(ab_audio_pcm_tests PRIVATE ab_game)
    add_test(NAME resource_audio_pcm_edges COMMAND ab_audio_pcm_tests)
    set(AB_REFERENCE_AUDIO_MIXER_STATE "" CACHE FILEPATH
        "Optional original 8192-command mixer lifecycle corpus")
    set(AB_REFERENCE_AUDIO_MIXER_STATE_EXTENDED "" CACHE FILEPATH
        "Optional original mixer lifecycle corpus with ordered removal of live voices")
    set(AB_REFERENCE_AUDIO_MIXER_CLIPS "" CACHE FILEPATH
        "Optional original mixer lifecycle corpus with clip-identity operations")
    if(AB_REFERENCE_AUDIO_MIXER_CLIPS)
        add_test(NAME game_audio_mixer_clips_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_AUDIO_MIXER_CLIPS}"
                "-DCHECKER=$<TARGET_FILE:ab_audio_mixer_tests>"
                -DCLIPS=ON
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_audio_mixer_reference.cmake")
    endif()
    if(AB_REFERENCE_AUDIO_MIXER_STATE)
        add_test(NAME game_audio_mixer_state_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_AUDIO_MIXER_STATE}"
                "-DCHECKER=$<TARGET_FILE:ab_audio_mixer_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_audio_mixer_reference.cmake")
    endif()
    if(AB_REFERENCE_AUDIO_MIXER_STATE_EXTENDED)
        add_test(NAME game_audio_mixer_state_order_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_AUDIO_MIXER_STATE_EXTENDED}"
                "-DCHECKER=$<TARGET_FILE:ab_audio_mixer_tests>"
                -DEXTENDED=ON
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_audio_mixer_reference.cmake")
    endif()
    set(AB_REFERENCE_AUDIO_MIX "" CACHE FILEPATH
        "Optional original equal-rate signed-16 accumulation/output mixer corpus")
    set(AB_REFERENCE_AUDIO_MIX_MULTIPLE "" CACHE FILEPATH
        "Optional original two-clip equal-rate signed-16 mixer corpus")
    if(AB_REFERENCE_AUDIO_MIX_MULTIPLE)
        add_test(NAME game_audio_mix_multiple_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_AUDIO_MIX_MULTIPLE}"
                "-DCHECKER=$<TARGET_FILE:ab_audio_mix_tests>"
                -DMULTIPLE=ON
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_audio_mix_reference.cmake")
    endif()
    if(AB_REFERENCE_AUDIO_MIX)
        add_test(NAME game_audio_mix_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_AUDIO_MIX}"
                "-DCHECKER=$<TARGET_FILE:ab_audio_mix_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_audio_mix_reference.cmake")
    endif()
    set(AB_REFERENCE_AUDIO_PLAY "" CACHE FILEPATH
        "Optional original playAudio wrapper request/default/metadata corpus")
    if(AB_REFERENCE_AUDIO_PLAY)
        add_test(NAME game_audio_play_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_AUDIO_PLAY}"
                "-DCHECKER=$<TARGET_FILE:ab_audio_play_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_audio_play_reference.cmake")
    endif()
    add_executable(ab_audio_definition_tests tests/audio_definition.c)
    target_link_libraries(ab_audio_definition_tests PRIVATE ab_resource)
    add_test(NAME resource_audio_definition_edges COMMAND ab_audio_definition_tests)
    set(AB_REFERENCE_AUDIO_DEFINITION "" CACHE FILEPATH
        "Optional original createAudio paths, stream requests and sound metadata")
    if(AB_REFERENCE_AUDIO_DEFINITION)
        add_test(NAME resource_audio_definition_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_AUDIO_DEFINITION}"
                "-DCHECKER=$<TARGET_FILE:ab_audio_definition_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_audio_definition_reference.cmake")
    endif()
    set(AB_REFERENCE_AUDIO_GROUP "" CACHE FILEPATH
        "Optional original audio-name group choices and shared RNG corpus")
    set(AB_REFERENCE_AUDIO_CATALOG "" CACHE FILEPATH
        "Optional pinned decoded audio.lua catalog resource")
    set(AB_REFERENCE_AUDIO_CATALOG_NATIVE "" CACHE FILEPATH
        "Optional complete original audio-group catalog corpus")
    set(AB_REFERENCE_AUDIO_ASSETS "" CACHE FILEPATH
        "Optional complete original ordered audio registration properties")
    set(AB_REFERENCE_AUDIO_RESOURCES "" CACHE FILEPATH
        "Optional original resolved audio paths, stream requests and stored metadata")
    if(AB_REFERENCE_AUDIO_CATALOG AND AB_REFERENCE_AUDIO_RESOURCES)
        add_test(NAME resource_audio_paths_android
            COMMAND "${CMAKE_COMMAND}"
                "-DDECODED=${AB_REFERENCE_AUDIO_CATALOG}"
                "-DREFERENCE=${AB_REFERENCE_AUDIO_RESOURCES}"
                "-DCHECKER=$<TARGET_FILE:ab_audio_group_tests>"
                -DRESOURCES=ON
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_audio_catalog_reference.cmake")
    endif()
    if(AB_REFERENCE_AUDIO_CATALOG AND AB_REFERENCE_AUDIO_ASSETS)
        add_test(NAME resource_audio_assets_android
            COMMAND "${CMAKE_COMMAND}"
                "-DDECODED=${AB_REFERENCE_AUDIO_CATALOG}"
                "-DREFERENCE=${AB_REFERENCE_AUDIO_ASSETS}"
                "-DCHECKER=$<TARGET_FILE:ab_audio_group_tests>"
                -DASSETS=ON
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_audio_catalog_reference.cmake")
    endif()
    if(AB_REFERENCE_AUDIO_CATALOG AND AB_REFERENCE_AUDIO_CATALOG_NATIVE)
        add_test(NAME resource_audio_catalog_android
            COMMAND "${CMAKE_COMMAND}"
                "-DDECODED=${AB_REFERENCE_AUDIO_CATALOG}"
                "-DREFERENCE=${AB_REFERENCE_AUDIO_CATALOG_NATIVE}"
                "-DCHECKER=$<TARGET_FILE:ab_audio_group_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_audio_catalog_reference.cmake")
    endif()
    if(AB_REFERENCE_AUDIO_GROUP)
        add_test(NAME game_audio_group_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_AUDIO_GROUP}"
                "-DCHECKER=$<TARGET_FILE:ab_audio_group_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_audio_group_reference.cmake")
    endif()
    add_executable(ab_damage_sprite_tests tests/damage_sprite.c)
    target_link_libraries(ab_damage_sprite_tests PRIVATE ab_game)
    add_test(NAME game_damage_sprite_edges COMMAND ab_damage_sprite_tests)
    add_executable(ab_initial_sprite_material_tests tests/initial_sprite_material.c)
    target_link_libraries(ab_initial_sprite_material_tests PRIVATE ab_game)
    set(AB_REFERENCE_INITIAL_SPRITE_MATERIAL "" CACHE FILEPATH
        "Original constructor-normalized material strength and sprite fields")
    if(AB_REFERENCE_INITIAL_SPRITE_MATERIAL AND AB_REFERENCE_APK)
        add_test(NAME game_initial_sprite_material_android_apk
            COMMAND "${CMAKE_COMMAND}"
                "-DAPK=${AB_REFERENCE_APK}"
                "-DREFERENCE=${AB_REFERENCE_INITIAL_SPRITE_MATERIAL}"
                "-DCHECKER=$<TARGET_FILE:ab_initial_sprite_material_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_initial_sprite_material.cmake")
    endif()
    set(AB_REFERENCE_DAMAGE_SPRITE "" CACHE FILEPATH
        "Optional original typed damage-sprite interval selection corpus")
    if(AB_REFERENCE_DAMAGE_SPRITE)
        add_test(NAME game_damage_sprite_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_DAMAGE_SPRITE}"
                "-DCHECKER=$<TARGET_FILE:ab_damage_sprite_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_damage_sprite_reference.cmake")
    endif()
    add_executable(ab_character_animation_tests tests/character_animation.c)
    target_link_libraries(ab_character_animation_tests PRIVATE ab_game)
    add_test(NAME game_character_animation_edges COMMAND ab_character_animation_tests)
    set(AB_REFERENCE_CHARACTER_ANIMATION "" CACHE FILEPATH
        "Optional original multi-object animation frames with shared native RNG")
    if(AB_REFERENCE_CHARACTER_ANIMATION)
        add_test(NAME game_character_animation_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_CHARACTER_ANIMATION}"
                "-DCHECKER=$<TARGET_FILE:ab_character_animation_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_character_animation_reference.cmake")
    endif()
    add_executable(ab_goal_animation_tests tests/goal_animation.c)
    target_link_libraries(ab_goal_animation_tests PRIVATE ab_game)
    add_test(NAME game_goal_animation_edges COMMAND ab_goal_animation_tests)
    set(AB_REFERENCE_GOAL_ANIMATION "" CACHE FILEPATH
        "Optional recorded original goal expression/oink animation corpus")
    if(AB_REFERENCE_GOAL_ANIMATION)
        add_test(NAME game_goal_animation_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_GOAL_ANIMATION}"
                "-DCHECKER=$<TARGET_FILE:ab_goal_animation_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_goal_animation_reference.cmake")
    endif()
    add_executable(ab_bird_animation_tests tests/bird_animation.c)
    target_link_libraries(ab_bird_animation_tests PRIVATE ab_game)
    add_test(NAME game_bird_animation_edges COMMAND ab_bird_animation_tests)
    set(AB_REFERENCE_BIRD_ANIMATION "" CACHE FILEPATH
        "Optional original bird sprite timers and ordered callback corpus")
    if(AB_REFERENCE_BIRD_ANIMATION)
        add_test(NAME game_bird_animation_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_BIRD_ANIMATION}"
                "-DCHECKER=$<TARGET_FILE:ab_bird_animation_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_bird_animation_reference.cmake")
    endif()
    add_executable(ab_sling_reload_tests tests/sling_reload.c)
    target_link_libraries(ab_sling_reload_tests PRIVATE ab_game)
    add_test(NAME game_sling_reload_edges COMMAND ab_sling_reload_tests)
    set(AB_REFERENCE_SLING_RELOAD "" CACHE FILEPATH
        "Optional recorded 4096-case original reload timer/next-bird corpus")
    if(AB_REFERENCE_SLING_RELOAD)
        add_test(NAME game_sling_reload_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_SLING_RELOAD}"
                "-DCHECKER=$<TARGET_FILE:ab_sling_reload_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_sling_reload_reference.cmake")
    endif()
    set(AB_REFERENCE_BIRD_JUMP "" CACHE FILEPATH
        "Optional recorded 4096-case original idle bird-jump corpus")
    set(AB_REFERENCE_BIRD_JUMP_OVERSHOOT "" CACHE FILEPATH
        "Optional original jump corpus with controlled random upper-bound overshoots")
    if(AB_REFERENCE_BIRD_JUMP_OVERSHOOT)
        add_test(NAME game_bird_jump_overshoot_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_BIRD_JUMP_OVERSHOOT}"
                "-DCHECKER=$<TARGET_FILE:ab_bird_jump_tests>"
                -DKIND=overshoot
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_bird_jump_reference.cmake")
    endif()
    if(AB_REFERENCE_BIRD_JUMP)
        add_test(NAME game_bird_jump_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_BIRD_JUMP}"
                "-DCHECKER=$<TARGET_FILE:ab_bird_jump_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_bird_jump_reference.cmake")
    endif()
    set(AB_REFERENCE_SCORE "" CACHE FILEPATH
        "Optional recorded 4096-pair original break/damage scoring corpus")
    if(AB_REFERENCE_SCORE)
        add_test(NAME game_score_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_SCORE}"
                "-DCHECKER=$<TARGET_FILE:ab_score_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_score_reference.cmake")
    endif()
    set(AB_REFERENCE_GAME_CONTACT "" CACHE FILEPATH
        "Optional recorded 4096-case native contact/deferred-velocity corpus")
    if(AB_REFERENCE_GAME_CONTACT)
        add_test(NAME game_collision_velocity_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_GAME_CONTACT}"
                "-DCHECKER=$<TARGET_FILE:ab_collision_velocity_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_game_contact_reference.cmake")
    endif()
    set(AB_REFERENCE_GAME_CONTACT_END "" CACHE FILEPATH
        "Optional original 4096-case basic end-contact guard/callback corpus")
    if(AB_REFERENCE_GAME_CONTACT_END)
        add_test(NAME game_contact_end_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_GAME_CONTACT_END}"
                "-DCHECKER=$<TARGET_FILE:ab_collision_velocity_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_game_contact_end_reference.cmake")
    endif()
    set(AB_REFERENCE_GAME_CONTACT_ROUTES "" CACHE FILEPATH
        "Optional original 512-case begin/end/pre-solve routing table corpus")
    if(AB_REFERENCE_GAME_CONTACT_ROUTES)
        add_test(NAME game_contact_routes_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_GAME_CONTACT_ROUTES}"
                "-DCHECKER=$<TARGET_FILE:ab_game_contact_routes_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_game_contact_routes_reference.cmake")
    endif()
    set(AB_REFERENCE_GAME_CONTACT_DISPATCH "" CACHE FILEPATH
        "Optional original 4096-case outer contact event dispatch corpus")
    if(AB_REFERENCE_GAME_CONTACT_DISPATCH)
        add_test(NAME game_contact_dispatch_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_GAME_CONTACT_DISPATCH}"
                "-DCHECKER=$<TARGET_FILE:ab_game_contact_dispatch_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_game_contact_dispatch_reference.cmake")
    endif()
    set(AB_REFERENCE_GAME_CONTACT_WORLD "" CACHE FILEPATH
        "Optional original 64-world contact listener integration corpus")
    if(AB_REFERENCE_GAME_CONTACT_WORLD)
        add_test(NAME game_contact_world_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_GAME_CONTACT_WORLD}"
                "-DCHECKER=$<TARGET_FILE:ab_game_contact_world_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_game_contact_world_reference.cmake")
    endif()
    set(AB_REFERENCE_MUSEUM_GLUE "" CACHE FILEPATH
        "Optional original 4096-case museum glue helper corpus")
    if(AB_REFERENCE_MUSEUM_GLUE)
        add_test(NAME museum_glue_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_MUSEUM_GLUE}"
                "-DCHECKER=$<TARGET_FILE:ab_museum_glue_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_museum_glue_reference.cmake")
    endif()
    set(AB_REFERENCE_BUBBLE_CONTACT "" CACHE FILEPATH
        "Optional original 4096-case native bubble contact corpus")
    if(AB_REFERENCE_BUBBLE_CONTACT)
        add_test(NAME bubble_contact_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_BUBBLE_CONTACT}"
                "-DCHECKER=$<TARGET_FILE:ab_bubble_contact_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_bubble_contact_reference.cmake")
    endif()
    set(AB_REFERENCE_MUSEUM_GLUE_ROUTE "" CACHE FILEPATH
        "Optional original 8192-case post-damage glue routing corpus")
    if(AB_REFERENCE_MUSEUM_GLUE_ROUTE)
        add_test(NAME museum_glue_route_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_MUSEUM_GLUE_ROUTE}"
                "-DCHECKER=$<TARGET_FILE:ab_museum_glue_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_museum_glue_route_reference.cmake")
    endif()
    set(AB_REFERENCE_DAMAGE_PARTICLES "" CACHE FILEPATH
        "Optional original 4096-case damage-state particle helper corpus")
    if(AB_REFERENCE_DAMAGE_PARTICLES)
        add_test(NAME damage_particles_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_DAMAGE_PARTICLES}"
                "-DCHECKER=$<TARGET_FILE:ab_damage_particles_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_damage_particles_reference.cmake")
    endif()
    set(AB_REFERENCE_BLOCK_COLLISION "" CACHE FILEPATH
        "Optional original 4096-case block collision callback corpus")
    if(AB_REFERENCE_BLOCK_COLLISION)
        add_test(NAME block_collision_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_BLOCK_COLLISION}"
                "-DCHECKER=$<TARGET_FILE:ab_block_collision_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_block_collision_reference.cmake")
    endif()
    set(AB_REFERENCE_BIRD_NOTIFY "" CACHE FILEPATH
        "Optional original 4096-case bird collision notification prefix corpus")
    if(AB_REFERENCE_BIRD_NOTIFY)
        add_test(NAME bird_notify_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_BIRD_NOTIFY}"
                "-DCHECKER=$<TARGET_FILE:ab_bird_notify_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_bird_notify_reference.cmake")
    endif()
    set(AB_REFERENCE_BIRD_PRELUDE "" CACHE FILEPATH
        "Optional original 4096-case bird collision hat/score prelude corpus")
    if(AB_REFERENCE_BIRD_PRELUDE)
        add_test(NAME bird_prelude_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_BIRD_PRELUDE}"
                "-DCHECKER=$<TARGET_FILE:ab_bird_prelude_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_bird_prelude_reference.cmake")
    endif()
    set(AB_REFERENCE_GAME_BEHAVIOR "" CACHE FILEPATH
        "Optional original 4096-case GameObject behavior dispatch corpus")
    if(AB_REFERENCE_GAME_BEHAVIOR)
        add_test(NAME game_behavior_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_GAME_BEHAVIOR}"
                "-DCHECKER=$<TARGET_FILE:ab_game_behavior_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_game_behavior_reference.cmake")
    endif()
    set(AB_REFERENCE_GAME_BEHAVIOR_ADD "" CACHE FILEPATH
        "Optional original 2048-case GameObject behavior construction corpus")
    if(AB_REFERENCE_GAME_BEHAVIOR_ADD)
        add_test(NAME game_behavior_add_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_GAME_BEHAVIOR_ADD}"
                "-DCHECKER=$<TARGET_FILE:ab_game_behavior_add_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_game_behavior_add_reference.cmake")
    endif()
    set(AB_REFERENCE_MUSEUM_BIRD_BEHAVIOR "" CACHE FILEPATH
        "Optional actual legacy Museum bird constructor/event corpus")
    if(AB_REFERENCE_MUSEUM_BIRD_BEHAVIOR)
        add_test(NAME museum_bird_behavior_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_MUSEUM_BIRD_BEHAVIOR}"
                "-DCHECKER=$<TARGET_FILE:ab_museum_bird_behavior_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_museum_bird_behavior_reference.cmake")
    endif()
    set(AB_REFERENCE_TRAJECTORY_FOLLOW "" CACHE FILEPATH
        "Optional original 8192-operation trajectory follow/emission corpus")
    if(AB_REFERENCE_TRAJECTORY_FOLLOW)
        add_test(NAME trajectory_follow_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_TRAJECTORY_FOLLOW}"
                "-DCHECKER=$<TARGET_FILE:ab_trajectory_follow_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_trajectory_follow_reference.cmake")
    endif()
    set(AB_REFERENCE_OBJECT_PROPERTY_ACCESS "" CACHE FILEPATH
        "Optional original 4096-case ObjectFunctions property access corpus")
    if(AB_REFERENCE_OBJECT_PROPERTY_ACCESS)
        add_test(NAME object_property_access_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_OBJECT_PROPERTY_ACCESS}"
                "-DCHECKER=$<TARGET_FILE:ab_object_property_access_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_object_property_access_reference.cmake")
    endif()
    set(AB_REFERENCE_OBJECT_DEFINITION_LOOKUP "" CACHE FILEPATH
        "Optional original 4096-case getObjectDefinition corpus")
    if(AB_REFERENCE_OBJECT_DEFINITION_LOOKUP)
        add_test(NAME object_definition_lookup_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_OBJECT_DEFINITION_LOOKUP}"
                "-DCHECKER=$<TARGET_FILE:ab_object_definition_lookup_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_object_definition_lookup_reference.cmake")
    endif()
    set(AB_REFERENCE_OBJECT_WORLD_LOOKUP "" CACHE FILEPATH
        "Optional original 4096-case getObject scalar/table-key corpus")
    if(AB_REFERENCE_OBJECT_WORLD_LOOKUP)
        add_test(NAME object_world_lookup_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_OBJECT_WORLD_LOOKUP}"
                "-DCHECKER=$<TARGET_FILE:ab_object_world_lookup_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_object_world_lookup_reference.cmake")
    endif()
    set(AB_REFERENCE_LEVEL_PROPERTIES "" CACHE FILEPATH
        "Optional original 2048-case GameObject/mode/Museum property corpus")
    if(AB_REFERENCE_LEVEL_PROPERTIES)
        add_test(NAME level_properties_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_LEVEL_PROPERTIES}"
                "-DCHECKER=$<TARGET_FILE:ab_level_properties_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_level_properties_reference.cmake")
    endif()
    set(AB_REFERENCE_BIRD_MIGHTY "" CACHE FILEPATH
        "Optional original 4096-case mighty-bird collision-function corpus")
    if(AB_REFERENCE_BIRD_MIGHTY)
        add_test(NAME bird_mighty_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_BIRD_MIGHTY}"
                "-DCHECKER=$<TARGET_FILE:ab_bird_mighty_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_bird_mighty_reference.cmake")
    endif()
    set(AB_REFERENCE_BIRD_SPRITES "" CACHE FILEPATH
        "Optional original 4096-case bird collision flag/sprite corpus")
    if(AB_REFERENCE_BIRD_SPRITES)
        add_test(NAME bird_sprites_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_BIRD_SPRITES}"
                "-DCHECKER=$<TARGET_FILE:ab_bird_sprites_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_bird_sprites_reference.cmake")
    endif()
    set(AB_REFERENCE_BIRD_SPECIALTY "" CACHE FILEPATH
        "Optional original 8192-case bird collision specialty corpus")
    if(AB_REFERENCE_BIRD_SPECIALTY)
        add_test(NAME bird_specialty_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_BIRD_SPECIALTY}"
                "-DCHECKER=$<TARGET_FILE:ab_bird_specialty_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_bird_specialty_reference.cmake")
    endif()
    set(AB_REFERENCE_BIRD_PROJECTILES "" CACHE FILEPATH
        "Optional original 4096-case bird boomerang/grenade collision corpus")
    if(AB_REFERENCE_BIRD_PROJECTILES)
        add_test(NAME bird_projectiles_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_BIRD_PROJECTILES}"
                "-DCHECKER=$<TARGET_FILE:ab_bird_projectiles_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_bird_projectiles_reference.cmake")
    endif()
    set(AB_REFERENCE_BIRD_COLLISION_DAMAGE "" CACHE FILEPATH
        "Optional original 4096-case bird final damage-sprite refresh corpus")
    if(AB_REFERENCE_BIRD_COLLISION_DAMAGE)
        add_test(NAME bird_collision_damage_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_BIRD_COLLISION_DAMAGE}"
                "-DCHECKER=$<TARGET_FILE:ab_bird_collision_damage_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_bird_collision_damage_reference.cmake")
    endif()
    set(AB_REFERENCE_BIRD_EFFECTS "" CACHE FILEPATH
        "Optional original 4096-case bird contact sound/particle corpus")
    if(AB_REFERENCE_BIRD_EFFECTS)
        add_test(NAME bird_effects_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_BIRD_EFFECTS}"
                "-DCHECKER=$<TARGET_FILE:ab_bird_effects_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_bird_effects_reference.cmake")
        add_test(NAME bird_collision_effects_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_BIRD_EFFECTS}"
                "-DCHECKER=$<TARGET_FILE:ab_bird_effects_tests>"
                -DCOMPOSE=ON
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_bird_effects_reference.cmake")
    endif()
    set(AB_REFERENCE_BIRD_COLLISION "" CACHE FILEPATH
        "Optional original 256-case coupled bird callback corpus")
    if(AB_REFERENCE_BIRD_COLLISION)
        add_test(NAME bird_collision_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_BIRD_COLLISION}"
                "-DCHECKER=$<TARGET_FILE:ab_bird_collision_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_bird_collision_reference.cmake")
    endif()
    set(AB_REFERENCE_OBJECT_REMOVAL_NOTIFY "" CACHE FILEPATH
        "Optional original 4096-case object removal definition/level notification corpus")
    if(AB_REFERENCE_OBJECT_REMOVAL_NOTIFY)
        add_test(NAME object_removal_notify_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_OBJECT_REMOVAL_NOTIFY}"
                "-DCHECKER=$<TARGET_FILE:ab_object_removal_notify_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_object_removal_notify_reference.cmake")
    endif()
    set(AB_REFERENCE_OBJECT_REMOVED "" CACHE FILEPATH
        "Private original onObjectRemoved callback corpus")
    if(AB_REFERENCE_OBJECT_REMOVED)
        add_test(NAME object_removed_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_OBJECT_REMOVED}"
                "-DCHECKER=$<TARGET_FILE:ab_object_removed_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_object_removed_reference.cmake")
    endif()
    set(AB_REFERENCE_BIRD_REMOVE "" CACHE FILEPATH
        "Private original complete removeBird callback corpus")
    if(AB_REFERENCE_BIRD_REMOVE)
        add_test(NAME bird_remove_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_BIRD_REMOVE}"
                "-DCHECKER=$<TARGET_FILE:ab_bird_remove_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_bird_remove_reference.cmake")
    endif()
    set(AB_REFERENCE_LEVEL_EFFECT_BASE "" CACHE FILEPATH
        "Private original base LevelEffect method corpus")
    if(AB_REFERENCE_LEVEL_EFFECT_BASE)
        add_test(NAME level_effect_base_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_LEVEL_EFFECT_BASE}"
                "-DCHECKER=$<TARGET_FILE:ab_level_effect_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_level_effect_base_reference.cmake")
    endif()
    set(AB_REFERENCE_BIRD_IMPACT_PREDICTION "" CACHE FILEPATH
        "Private original PhysicsQueries bird impact prediction corpus")
    if(AB_REFERENCE_BIRD_IMPACT_PREDICTION)
        add_test(NAME bird_impact_prediction_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_BIRD_IMPACT_PREDICTION}"
                "-DCHECKER=$<TARGET_FILE:ab_bird_impact_prediction_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_bird_impact_prediction_reference.cmake")
    endif()
    set(AB_REFERENCE_WORLD_RAYCAST "" CACHE FILEPATH
        "Private original native world/tree raycast corpus")
    if(AB_REFERENCE_WORLD_RAYCAST)
        add_test(NAME physics_world_raycast_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_WORLD_RAYCAST}"
                "-DCHECKER=$<TARGET_FILE:ab_world_raycast_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_world_raycast_reference.cmake")
    endif()
    set(AB_REFERENCE_GAME_RAYCAST "" CACHE FILEPATH
        "Private original GameLua named world raycast corpus")
    if(AB_REFERENCE_GAME_RAYCAST)
        add_test(NAME game_raycast_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_GAME_RAYCAST}"
                "-DCHECKER=$<TARGET_FILE:ab_game_raycast_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_game_raycast_reference.cmake")
    endif()
    set(AB_REFERENCE_BIRD_IMPACT_WORLD "" CACHE FILEPATH
        "Private original PhysicsQueries and GameLua physical raycast corpus")
    if(AB_REFERENCE_BIRD_IMPACT_WORLD)
        add_test(NAME bird_impact_world_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_BIRD_IMPACT_WORLD}"
                "-DCHECKER=$<TARGET_FILE:ab_bird_impact_world_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_bird_impact_world_reference.cmake")
    endif()
    set(AB_REFERENCE_CAMERA_SMOOTHSTEP "" CACHE FILEPATH
        "Private original smoothstep/easeSmoothStep helper corpus")
    if(AB_REFERENCE_CAMERA_SMOOTHSTEP)
        add_test(NAME camera_smoothstep_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_CAMERA_SMOOTHSTEP}"
                "-DCHECKER=$<TARGET_FILE:ab_camera_smoothstep_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_camera_smoothstep_reference.cmake")
    endif()
    set(AB_REFERENCE_CAMERA_FOLLOW "" CACHE FILEPATH
        "Private original FollowBirdCameraMovementBehavior graph corpus")
    if(AB_REFERENCE_CAMERA_FOLLOW)
        add_test(NAME camera_follow_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_CAMERA_FOLLOW}"
                "-DCHECKER=$<TARGET_FILE:ab_camera_follow_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_camera_follow_reference.cmake")
    endif()
    set(AB_REFERENCE_CAMERA_ZOOM_FIT "" CACHE FILEPATH
        "Private original projected camera zoom-fit numeric corpus")
    if(AB_REFERENCE_CAMERA_ZOOM_FIT)
        add_test(NAME camera_zoom_fit_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_CAMERA_ZOOM_FIT}"
                "-DCHECKER=$<TARGET_FILE:ab_camera_zoom_fit_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_camera_zoom_fit_reference.cmake")
    endif()
    set(AB_REFERENCE_CAMERA_FOLLOW_SYSTEM "" CACHE FILEPATH
        "Private original composed camera owner/follow/mover/limit corpus")
    if(AB_REFERENCE_CAMERA_FOLLOW_SYSTEM)
        add_test(NAME camera_follow_system_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_CAMERA_FOLLOW_SYSTEM}"
                "-DCHECKER=$<TARGET_FILE:ab_camera_follow_system_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_camera_follow_system_reference.cmake")
    endif()
    set(AB_REFERENCE_CAMERA_FOLLOW_END_SEQUENCE "" CACHE FILEPATH
        "Private original composed camera follow/end-sequence corpus")
    if(AB_REFERENCE_CAMERA_FOLLOW_END_SEQUENCE AND AB_REFERENCE_APK)
        add_test(NAME camera_follow_end_sequence_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_CAMERA_FOLLOW_END_SEQUENCE}"
                "-DAPK=${AB_REFERENCE_APK}"
                "-DCHECKER=$<TARGET_FILE:ab_camera_follow_end_sequence_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_camera_follow_end_sequence_reference.cmake")
    endif()
    set(AB_REFERENCE_GAME_RAYCAST_NUMERIC "" CACHE FILEPATH
        "Private original nonfinite/extreme endpoint GameLua ray corpus")
    if(AB_REFERENCE_GAME_RAYCAST_NUMERIC)
        add_test(NAME game_raycast_numeric_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_GAME_RAYCAST_NUMERIC}"
                "-DCHECKER=$<TARGET_FILE:ab_game_raycast_numeric_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_game_raycast_numeric_reference.cmake")
    endif()
    set(AB_REFERENCE_CAMERA_SHOT_SEQUENCE "" CACHE FILEPATH
        "Private original shot/follow/end camera sequence corpus")
    if(AB_REFERENCE_CAMERA_SHOT_SEQUENCE AND AB_REFERENCE_APK)
        add_test(NAME camera_shot_sequence_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_CAMERA_SHOT_SEQUENCE}"
                "-DAPK=${AB_REFERENCE_APK}"
                "-DCHECKER=$<TARGET_FILE:ab_camera_shot_sequence_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_camera_shot_sequence_reference.cmake")
    endif()
    set(AB_REFERENCE_CAMERA_FOLLOW_WORLD "" CACHE FILEPATH
        "Private original composed camera/predictor/world corpus")
    if(AB_REFERENCE_CAMERA_FOLLOW_WORLD AND AB_REFERENCE_APK)
        add_test(NAME camera_follow_world_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_CAMERA_FOLLOW_WORLD}"
                "-DAPK=${AB_REFERENCE_APK}"
                "-DCHECKER=$<TARGET_FILE:ab_camera_follow_world_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_camera_follow_world_reference.cmake")
    endif()
    set(AB_REFERENCE_CAMERA_FOLLOW_PROPERTIES "" CACHE FILEPATH
        "Private original composed camera/predictor/world/object-property corpus")
    if(AB_REFERENCE_CAMERA_FOLLOW_PROPERTIES AND AB_REFERENCE_APK)
        add_test(NAME camera_follow_properties_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_CAMERA_FOLLOW_PROPERTIES}"
                "-DAPK=${AB_REFERENCE_APK}"
                "-DCHECKER=$<TARGET_FILE:ab_camera_follow_properties_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_camera_follow_properties_reference.cmake")
    endif()
    set(AB_REFERENCE_CLASSIC_CAMERA "" CACHE FILEPATH
        "Private original frozen-level owned-camera corpus")
    if(AB_REFERENCE_CLASSIC_CAMERA AND AB_REFERENCE_APK)
        add_test(NAME classic_camera_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_CLASSIC_CAMERA}"
                "-DAPK=${AB_REFERENCE_APK}"
                "-DCHECKER=$<TARGET_FILE:ab_classic_camera_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_camera_reference.cmake")
    endif()
    set(AB_REFERENCE_CLASSIC_CAMERA_LIVE "" CACHE FILEPATH
        "Private original owned-camera live-cache/late-launch corpus")
    if(AB_REFERENCE_CLASSIC_CAMERA_LIVE AND AB_REFERENCE_APK)
        add_test(NAME classic_camera_live_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_CLASSIC_CAMERA_LIVE}"
                "-DAPK=${AB_REFERENCE_APK}"
                "-DCHECKER=$<TARGET_FILE:ab_classic_camera_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_camera_reference.cmake")
    endif()
    set(AB_REFERENCE_BIRD_IMPACT_WORLD_NUMERIC "" CACHE FILEPATH
        "Private original vertical/zero-velocity coupled prediction corpus")
    if(AB_REFERENCE_BIRD_IMPACT_WORLD_NUMERIC)
        add_test(NAME bird_impact_world_numeric_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_BIRD_IMPACT_WORLD_NUMERIC}"
                "-DCHECKER=$<TARGET_FILE:ab_bird_impact_world_numeric_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_bird_impact_world_numeric_reference.cmake")
    endif()
    set(AB_REFERENCE_GAME_OBJECT_REMOVE "" CACHE FILEPATH
        "Original native GameLua object-removal policy trace")
    if(AB_REFERENCE_GAME_OBJECT_REMOVE)
        add_test(NAME game_object_remove_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_GAME_OBJECT_REMOVE}"
                "-DCHECKER=$<TARGET_FILE:ab_game_object_remove_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_game_object_remove_reference.cmake")
    endif()
    set(AB_REFERENCE_GAME_OBJECT_REMOVE_JOINTS "" CACHE FILEPATH
        "Original native GameLua object/joint removal trace")
    if(AB_REFERENCE_GAME_OBJECT_REMOVE_JOINTS)
        add_test(NAME game_object_remove_joints_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_GAME_OBJECT_REMOVE_JOINTS}"
                "-DCHECKER=$<TARGET_FILE:ab_game_object_remove_joints_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_game_object_remove_joints_reference.cmake")
    endif()
    set(AB_REFERENCE_GAME_OBJECT_REMOVE_TELEPORTERS "" CACHE FILEPATH
        "Original native GameLua object/teleporter removal trace")
    if(AB_REFERENCE_GAME_OBJECT_REMOVE_TELEPORTERS)
        add_test(NAME game_object_remove_teleporters_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_GAME_OBJECT_REMOVE_TELEPORTERS}"
                "-DCHECKER=$<TARGET_FILE:ab_game_object_remove_teleporters_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_game_object_remove_teleporters_reference.cmake")
    endif()
    set(AB_REFERENCE_BLOCK_REMOVAL_SPAWN "" CACHE FILEPATH
        "Original Android removal spawn trace")
    if(AB_REFERENCE_BLOCK_REMOVAL_SPAWN)
        add_test(NAME block_removal_spawn_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_BLOCK_REMOVAL_SPAWN}"
                "-DCHECKER=$<TARGET_FILE:ab_block_removal_spawn_reference_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_block_removal_spawn_reference.cmake")
    endif()
    set(AB_REFERENCE_BLOCK_REMOVAL_PASS "" CACHE FILEPATH
        "Original Android complete removal pass with native physical/joint effects")
    if(AB_REFERENCE_BLOCK_REMOVAL_PASS)
        add_test(NAME block_removal_pass_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_BLOCK_REMOVAL_PASS}"
                "-DCHECKER=$<TARGET_FILE:ab_block_removal_pass_reference_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_block_removal_pass_reference.cmake")
    endif()
    set(AB_REFERENCE_BLOCK_REMOVAL_CLEANUP "" CACHE FILEPATH
        "Original Android removal cleanup trace")
    if(AB_REFERENCE_BLOCK_REMOVAL_CLEANUP)
        add_test(NAME block_removal_cleanup_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_BLOCK_REMOVAL_CLEANUP}"
                "-DCHECKER=$<TARGET_FILE:ab_block_removal_cleanup_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_block_removal_cleanup_reference.cmake")
        add_test(NAME block_removal_cleanup_composed_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_BLOCK_REMOVAL_CLEANUP}"
                "-DCHECKER=$<TARGET_FILE:ab_block_removal_cleanup_tests>"
                -DCOMPOSE=ON
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_block_removal_cleanup_reference.cmake")
    endif()
    set(AB_REFERENCE_BLOCK_REMOVAL_DETACH "" CACHE FILEPATH
        "Original Android removal detach trace")
    if(AB_REFERENCE_BLOCK_REMOVAL_DETACH)
        add_test(NAME block_removal_detach_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_BLOCK_REMOVAL_DETACH}"
                "-DCHECKER=$<TARGET_FILE:ab_block_removal_lifecycle_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_block_removal_detach_reference.cmake")
    endif()
    set(AB_REFERENCE_BLOCK_REMOVAL_PARTICLES "" CACHE FILEPATH
        "Original Android removal particles trace")
    if(AB_REFERENCE_BLOCK_REMOVAL_PARTICLES)
        add_test(NAME block_removal_particles_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_BLOCK_REMOVAL_PARTICLES}"
                "-DCHECKER=$<TARGET_FILE:ab_block_removal_particles_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_block_removal_particles_reference.cmake")
    endif()
    set(AB_REFERENCE_BLOCK_REMOVAL_EFFECTS "" CACHE FILEPATH
        "Original Android removal effects trace")
    if(AB_REFERENCE_BLOCK_REMOVAL_EFFECTS)
        add_test(NAME block_removal_effects_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_BLOCK_REMOVAL_EFFECTS}"
                "-DCHECKER=$<TARGET_FILE:ab_block_removal_effects_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_block_removal_effects_reference.cmake")
    endif()
    set(AB_REFERENCE_BLOCK_REMOVAL_SCORE "" CACHE FILEPATH
        "Original Android removal scoring trace")
    if(AB_REFERENCE_BLOCK_REMOVAL_SCORE)
        add_test(NAME block_removal_score_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_BLOCK_REMOVAL_SCORE}"
                "-DCHECKER=$<TARGET_FILE:ab_block_removal_score_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_block_removal_score_reference.cmake")
    endif()
    set(AB_REFERENCE_BLOCK_REMOVAL_CURSOR "" CACHE FILEPATH
        "Optional original 4096-case removal traversal/rocket/selection corpus")
    if(AB_REFERENCE_BLOCK_REMOVAL_CURSOR)
        add_test(NAME block_removal_cursor_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_BLOCK_REMOVAL_CURSOR}"
                "-DCHECKER=$<TARGET_FILE:ab_block_removal_cursor_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_block_removal_cursor_reference.cmake")
    endif()
    set(AB_REFERENCE_GAME_UPDATE_TAIL "" CACHE FILEPATH
        "Optional original ordinary game update tail corpus")
    if(AB_REFERENCE_GAME_UPDATE_TAIL)
        add_test(NAME game_update_tail_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_GAME_UPDATE_TAIL}"
                "-DCHECKER=$<TARGET_FILE:ab_game_update_tail_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_game_update_tail_reference.cmake")
    endif()
    set(AB_REFERENCE_GAME_UPDATE_JOINTS "" CACHE FILEPATH
        "Optional original game tail with motor checks and deferred joint destruction")
    if(AB_REFERENCE_GAME_UPDATE_JOINTS)
        add_test(NAME game_update_joints_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_GAME_UPDATE_JOINTS}"
                "-DCHECKER=$<TARGET_FILE:ab_game_update_tail_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_game_update_joints_reference.cmake")
    endif()
    set(AB_REFERENCE_GAME_UPDATE_HOOKS "" CACHE FILEPATH
        "Optional original game tail with registered object updates and pruning")
    if(AB_REFERENCE_GAME_UPDATE_HOOKS)
        add_test(NAME game_update_hooks_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_GAME_UPDATE_HOOKS}"
                "-DCHECKER=$<TARGET_FILE:ab_game_update_tail_tests>"
                -DHOOKS=ON
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_game_update_tail_reference.cmake")
    endif()
    set(AB_REFERENCE_GAME_SLING_MOUSE "" CACHE FILEPATH
        "Optional original ordinary mouse sling input phase corpus")
    if(AB_REFERENCE_GAME_SLING_MOUSE)
        add_test(NAME game_sling_mouse_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_GAME_SLING_MOUSE}"
                "-DCHECKER=$<TARGET_FILE:ab_game_sling_mouse_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_game_sling_mouse_reference.cmake")
    endif()
    set(AB_REFERENCE_SLING_DELTA "" CACHE FILEPATH
        "Optional original delta-based sling accumulator/reset corpus")
    if(AB_REFERENCE_SLING_DELTA)
        add_test(NAME game_sling_delta_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_SLING_DELTA}"
                "-DCHECKER=$<TARGET_FILE:ab_sling_delta_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_sling_delta_reference.cmake")
    endif()
    set(AB_REFERENCE_SLING_CANCEL "" CACHE FILEPATH
        "Optional recorded 4096-case original-Lua drag cancellation/reset corpus")
    if(AB_REFERENCE_SLING_CANCEL)
        add_test(NAME game_sling_cancel_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_SLING_CANCEL}"
                "-DCHECKER=$<TARGET_FILE:ab_sling_cancel_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_sling_cancel_reference.cmake")
    endif()
    set(AB_REFERENCE_BIRD_SHOT "" CACHE FILEPATH
        "Optional recorded 4096-case original-Lua launch transition corpus")
    set(AB_REFERENCE_LIVE_SHOT "" CACHE FILEPATH
        "Optional recorded live first-level launch impulse/body capture")
    if(AB_REFERENCE_BIRD_SHOT)
        add_test(NAME game_bird_shot_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_BIRD_SHOT}"
                "-DCHECKER=$<TARGET_FILE:ab_bird_shot_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_bird_shot_reference.cmake")
    endif()
    if(AB_REFERENCE_LIVE_SHOT)
        add_test(NAME game_live_shot_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_LIVE_SHOT}"
                "-DCHECKER=$<TARGET_FILE:ab_bird_shot_tests>"
                -DKIND=live
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_bird_shot_reference.cmake")
    endif()
    set(AB_REFERENCE_SLINGSHOT "" CACHE FILEPATH
        "Optional recorded 4096-case original-Lua launch velocity corpus")
    set(AB_REFERENCE_SLING_TRANSFER "" CACHE FILEPATH
        "Optional recorded 4096-case original-Lua active sling-transfer corpus")
    set(AB_REFERENCE_SLING_START "" CACHE FILEPATH
        "Optional recorded 4096-case original-Lua sling initialization corpus")
    set(AB_REFERENCE_SLING_DRAG "" CACHE FILEPATH
        "Optional recorded 4096-case original-Lua stretch/vertical-limit/audio corpus")
    set(AB_REFERENCE_SLING_IMPULSE "" CACHE FILEPATH
        "Optional recorded 4096-case original-Lua actual launch impulse corpus")
    set(AB_REFERENCE_SLING_ROUTING "" CACHE FILEPATH
        "Optional recorded 4096-case original-Lua release/statistics corpus")
    if(AB_REFERENCE_SLING_ROUTING)
        add_test(NAME game_sling_routing_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_SLING_ROUTING}"
                "-DCHECKER=$<TARGET_FILE:ab_slingshot_tests>"
                -DKIND=routing
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_slingshot_reference.cmake")
    endif()
    if(AB_REFERENCE_SLING_IMPULSE)
        add_test(NAME game_sling_impulse_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_SLING_IMPULSE}"
                "-DCHECKER=$<TARGET_FILE:ab_slingshot_tests>"
                -DKIND=impulse
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_slingshot_reference.cmake")
    endif()
    if(AB_REFERENCE_SLING_DRAG)
        add_test(NAME game_sling_drag_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_SLING_DRAG}"
                "-DCHECKER=$<TARGET_FILE:ab_slingshot_tests>"
                -DKIND=drag
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_slingshot_reference.cmake")
    endif()
    if(AB_REFERENCE_SLING_START)
        add_test(NAME game_sling_start_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_SLING_START}"
                "-DCHECKER=$<TARGET_FILE:ab_slingshot_tests>"
                -DKIND=start
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_slingshot_reference.cmake")
    endif()
    if(AB_REFERENCE_SLING_TRANSFER)
        add_test(NAME game_sling_transfer_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_SLING_TRANSFER}"
                "-DCHECKER=$<TARGET_FILE:ab_slingshot_tests>"
                -DKIND=transfer
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_slingshot_reference.cmake")
    endif()
    if(AB_REFERENCE_SLINGSHOT)
        add_test(NAME game_slingshot_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_SLINGSHOT}"
                "-DCHECKER=$<TARGET_FILE:ab_slingshot_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_slingshot_reference.cmake")
    endif()
    add_executable(ab_data_order_tests tests/data_order.c)
    add_executable(ab_integer_order_tests tests/integer_order.c)
    target_link_libraries(ab_integer_order_tests PRIVATE ab_resource)
    add_test(NAME integer_order_edges COMMAND ab_integer_order_tests)
    set(AB_REFERENCE_INTEGER_ORDER "" CACHE FILEPATH
        "Optional original fresh integer table and repeated-copy order corpus")
    if(AB_REFERENCE_INTEGER_ORDER)
        add_test(NAME integer_order_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_INTEGER_ORDER}"
                "-DCHECKER=$<TARGET_FILE:ab_integer_order_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_integer_order_reference.cmake")
    endif()
    target_link_libraries(ab_data_order_tests PRIVATE ab_resource)
    add_test(NAME data_string_order_edges COMMAND ab_data_order_tests)
    set(AB_REFERENCE_STRING_ORDER "" CACHE FILEPATH
        "Optional recorded 4096-case Android string-table order corpus")
    set(AB_REFERENCE_STRING_GROWTH "" CACHE FILEPATH
        "Optional recorded 4096-case growing Android string-table order corpus")
    set(AB_REFERENCE_LEVEL_ONE "" CACHE FILEPATH
        "Optional decoded pack1/Level1.lua from the pinned APK")
    set(AB_REFERENCE_LEVEL_ORDER "" CACHE FILEPATH
        "Optional recorded native pack1/Level1.lua object order")
    set(AB_REFERENCE_CREATED_LEVEL_ORDER "" CACHE FILEPATH
        "Optional recorded running first-level object-table order")
    set(AB_REFERENCE_LEVEL_CONSTRUCTORS "" CACHE PATH
        "Optional numbered directory of all 655 decoded target level constructors")
    set(AB_REFERENCE_LEVEL_CONSTRUCTOR_MANIFEST "" CACHE FILEPATH
        "Optional pinned manifest for all decoded level constructors")
    set(AB_REFERENCE_ALL_LEVEL_ORDERS "" CACHE FILEPATH
        "Optional recorded native order/allocation hints for all 655 levels")
    if(AB_REFERENCE_LEVEL_CONSTRUCTORS AND AB_REFERENCE_LEVEL_CONSTRUCTOR_MANIFEST
        AND AB_REFERENCE_ALL_LEVEL_ORDERS)
        add_test(NAME level_all_native_object_orders
            COMMAND "${CMAKE_COMMAND}"
                "-DDIRECTORY=${AB_REFERENCE_LEVEL_CONSTRUCTORS}"
                "-DMANIFEST=${AB_REFERENCE_LEVEL_CONSTRUCTOR_MANIFEST}"
                "-DREFERENCE=${AB_REFERENCE_ALL_LEVEL_ORDERS}"
                "-DCHECKER=$<TARGET_FILE:ab_data_order_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_all_level_orders_reference.cmake")
    endif()
    if(AB_REFERENCE_STRING_ORDER)
        add_test(NAME data_string_order_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_STRING_ORDER}"
                "-DCHECKER=$<TARGET_FILE:ab_data_order_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_string_order_reference.cmake")
    endif()
    if(AB_REFERENCE_STRING_GROWTH)
        add_test(NAME data_string_growth_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_STRING_GROWTH}"
                "-DCHECKER=$<TARGET_FILE:ab_data_order_tests>"
                -DKIND=growth
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_string_order_reference.cmake")
    endif()
    if(AB_REFERENCE_LEVEL_ONE AND AB_REFERENCE_LEVEL_ORDER)
        add_test(NAME level_one_native_object_order
            COMMAND "${CMAKE_COMMAND}"
                "-DLEVEL=${AB_REFERENCE_LEVEL_ONE}"
                "-DREFERENCE=${AB_REFERENCE_LEVEL_ORDER}"
                "-DCHECKER=$<TARGET_FILE:ab_data_order_tests>"
                -DKIND=level
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_string_order_reference.cmake")
    endif()
    if(AB_REFERENCE_LEVEL_ONE AND AB_REFERENCE_CREATED_LEVEL_ORDER)
        add_test(NAME level_one_native_created_order
            COMMAND "${CMAKE_COMMAND}"
                "-DLEVEL=${AB_REFERENCE_LEVEL_ONE}"
                "-DREFERENCE=${AB_REFERENCE_CREATED_LEVEL_ORDER}"
                "-DCHECKER=$<TARGET_FILE:ab_data_order_tests>"
                -DKIND=created-level
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_string_order_reference.cmake")
    endif()
    add_executable(ab_definition_tests tests/object_definition.c)
    target_link_libraries(ab_definition_tests PRIVATE ab_scene)
    add_test(NAME classic_object_definition_edges COMMAND ab_definition_tests)
    add_executable(ab_sling_launch_tests tests/sling_launch.c)
    target_link_libraries(ab_sling_launch_tests PRIVATE ab_game)
    add_test(NAME sling_launch_edges COMMAND ab_sling_launch_tests)
    set(AB_REFERENCE_SLING_LAUNCH_HOST "" CACHE FILEPATH
        "Optional original launch/release ordered mutable service corpus")
    if(AB_REFERENCE_SLING_LAUNCH_HOST)
        add_test(NAME sling_launch_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_SLING_LAUNCH_HOST}"
                "-DCHECKER=$<TARGET_FILE:ab_sling_launch_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_sling_launch_reference.cmake")
    endif()
    add_executable(ab_classic_level_tests tests/classic_level.c)
    target_link_libraries(ab_classic_level_tests PRIVATE ab_game)
    add_test(NAME classic_level_owner_edges COMMAND ab_classic_level_tests)
    add_executable(ab_defined_box_apk_tests tests/defined_box_apk.c)
    target_link_libraries(ab_defined_box_apk_tests PRIVATE ab_game)
    if(AB_REFERENCE_APK)
        add_test(NAME defined_box_apk COMMAND ab_defined_box_apk_tests "${AB_REFERENCE_APK}")
    endif()
    add_executable(ab_bird_hat_tests tests/bird_hat.c)
    target_link_libraries(ab_bird_hat_tests PRIVATE ab_game)
    add_test(NAME bird_hat_edges COMMAND ab_bird_hat_tests)
    add_executable(ab_bird_shockwave_tests tests/bird_shockwave.c)
    target_link_libraries(ab_bird_shockwave_tests PRIVATE ab_game)
    add_test(NAME bird_shockwave_edges COMMAND ab_bird_shockwave_tests)
    add_executable(ab_bird_bubbles_tests tests/bird_bubbles.c)
    target_link_libraries(ab_bird_bubbles_tests PRIVATE ab_game)
    add_test(NAME bird_bubbles_edges COMMAND ab_bird_bubbles_tests)
    add_executable(ab_electric_shockwave_tests tests/electric_shockwave.c)
    target_link_libraries(ab_electric_shockwave_tests PRIVATE ab_game)
    add_test(NAME electric_shockwave_edges COMMAND ab_electric_shockwave_tests)
    set(AB_REFERENCE_BIRD_HAT "" CACHE FILEPATH "Private pinned native hat lifecycle trace")
    if(AB_REFERENCE_BIRD_HAT)
        file(SHA256 "${AB_REFERENCE_BIRD_HAT}" bird_hat_hash)
        if(NOT bird_hat_hash STREQUAL
            "2d8bc9986f2a2104df1513fe38510bab571619b1346defa59680c74baa4b1638")
            message(FATAL_ERROR "Expected the pinned native hat lifecycle trace")
        endif()
        add_test(NAME bird_hat_native COMMAND ab_bird_hat_tests "${AB_REFERENCE_BIRD_HAT}")
    endif()
    add_executable(ab_object_missing_tests tests/object_missing.c)
    target_link_libraries(ab_object_missing_tests PRIVATE ab_game)
    add_test(NAME game_object_missing_edges COMMAND ab_object_missing_tests)
    set(AB_REFERENCE_OBJECT_MISSING "" CACHE FILEPATH
        "Optional original ObjectCreation early missing-definition branch corpus")
    if(AB_REFERENCE_OBJECT_MISSING)
        add_test(NAME game_object_missing_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_OBJECT_MISSING}"
                "-DCHECKER=$<TARGET_FILE:ab_object_missing_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_object_missing_reference.cmake")
    endif()
    add_executable(ab_force_data_tests tests/object_force_data.c)
    target_link_libraries(ab_force_data_tests PRIVATE ab_game)
    add_test(NAME game_object_force_data_edges COMMAND ab_force_data_tests)
    add_executable(ab_force_queue_tests tests/force_queue.c)
    target_link_libraries(ab_force_queue_tests PRIVATE ab_game)
    add_test(NAME game_force_queue_edges COMMAND ab_force_queue_tests)
    add_executable(ab_water_effect_tests tests/water_effect.c)
    target_link_libraries(ab_water_effect_tests PRIVATE ab_game)
    add_test(NAME game_water_effect_edges COMMAND ab_water_effect_tests)
    add_executable(ab_block_event_tests tests/block_event.c)
    target_link_libraries(ab_block_event_tests PRIVATE ab_game)
    add_test(NAME game_block_event_edges COMMAND ab_block_event_tests)
    add_executable(ab_block_script_tests tests/block_script.c)
    target_link_libraries(ab_block_script_tests PRIVATE ab_game)
    add_test(NAME game_block_script_edges COMMAND ab_block_script_tests)
    add_executable(ab_block_update_tests tests/block_update.c)
    target_link_libraries(ab_block_update_tests PRIVATE ab_game)
    add_test(NAME game_block_update_edges COMMAND ab_block_update_tests)
    add_executable(ab_object_update_tests tests/object_update.c)
    target_link_libraries(ab_object_update_tests PRIVATE ab_game)
    add_test(NAME game_object_update_edges COMMAND ab_object_update_tests)
    add_executable(ab_object_visual_tests tests/object_visual.c)
    target_link_libraries(ab_object_visual_tests PRIVATE ab_game)
    add_test(NAME game_object_visual_edges COMMAND ab_object_visual_tests)
    add_executable(ab_object_transform_tests tests/object_transform.c)
    add_executable(ab_game_circle_tests tests/game_circle.c)
    add_executable(ab_game_circle_resize_tests tests/game_circle.c)
    target_compile_definitions(ab_game_circle_resize_tests PRIVATE AB_TEST_RESIZE_CIRCLE=1)
    target_link_libraries(ab_game_circle_resize_tests PRIVATE ab_game)
    add_test(NAME game_circle_resize_edges COMMAND ab_game_circle_resize_tests)
    set(AB_REFERENCE_RESIZE_CIRCLE "" CACHE FILEPATH "Private pinned native circle-resize trace")
    if(AB_REFERENCE_RESIZE_CIRCLE)
        add_test(NAME game_circle_resize_android
            COMMAND "${CMAKE_COMMAND}" "-DREFERENCE=${AB_REFERENCE_RESIZE_CIRCLE}"
                "-DCHECKER=$<TARGET_FILE:ab_game_circle_resize_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_resize_circle_reference.cmake")
    endif()
    add_executable(ab_game_box_tests tests/game_box.c)
    target_link_libraries(ab_game_box_tests PRIVATE ab_game)
    add_test(NAME game_box_edges COMMAND ab_game_box_tests)
    set(AB_REFERENCE_CREATE_BOX "" CACHE FILEPATH "Private pinned native box-constructor trace")
    if(AB_REFERENCE_CREATE_BOX)
        file(SHA256 "${AB_REFERENCE_CREATE_BOX}" create_box_hash)
        if(NOT create_box_hash STREQUAL
            "4852bee479a33054de9c08222e97080687a4243ab3f8361284af6a2dcc326c7f")
            message(FATAL_ERROR "Expected the pinned native box-constructor trace")
        endif()
        add_test(NAME game_box_native COMMAND ab_game_box_tests "${AB_REFERENCE_CREATE_BOX}")
    endif()
    target_link_libraries(ab_game_circle_tests PRIVATE ab_game)
    add_test(NAME game_circle_edges COMMAND ab_game_circle_tests)
    set(AB_REFERENCE_CREATE_CIRCLE "" CACHE FILEPATH
        "Private original Android createCircleLua corpus")
    if(AB_REFERENCE_CREATE_CIRCLE)
        add_test(NAME game_circle_create_android
            COMMAND "${CMAKE_COMMAND}" "-DREFERENCE=${AB_REFERENCE_CREATE_CIRCLE}"
                "-DCHECKER=$<TARGET_FILE:ab_game_circle_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_create_circle_reference.cmake")
    endif()
    target_link_libraries(ab_object_transform_tests PRIVATE ab_game)
    add_test(NAME game_object_transform_edges COMMAND ab_object_transform_tests)
    add_executable(ab_object_properties_tests tests/object_properties.c)
    target_link_libraries(ab_object_properties_tests PRIVATE ab_game)
    add_test(NAME game_object_properties_edges COMMAND ab_object_properties_tests)
    add_executable(ab_object_activity_tests tests/object_activity.c)
    target_link_libraries(ab_object_activity_tests PRIVATE ab_game)
    add_test(NAME game_object_activity_edges COMMAND ab_object_activity_tests)
    set(AB_REFERENCE_OBJECT_ACTIVITY "" CACHE FILEPATH
        "Optional controlled and live native object-activity corpus")
    if(AB_REFERENCE_OBJECT_ACTIVITY)
        add_test(NAME game_object_activity_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_OBJECT_ACTIVITY}"
                "-DCHECKER=$<TARGET_FILE:ab_object_activity_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_object_activity.cmake")
    endif()
    set(AB_REFERENCE_OBJECT_PROPERTIES "" CACHE FILEPATH
        "Optional live native GameLua object-property publication corpus")
    if(AB_REFERENCE_OBJECT_PROPERTIES)
        add_test(NAME game_object_properties_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_OBJECT_PROPERTIES}"
                "-DCHECKER=$<TARGET_FILE:ab_object_properties_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_object_properties.cmake")
    endif()
    set(AB_REFERENCE_OBJECT_TRANSFORM "" CACHE FILEPATH
        "Optional original Android GameLua position/rotation assignment corpus")
    if(AB_REFERENCE_OBJECT_TRANSFORM)
        add_test(NAME game_object_transform_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_OBJECT_TRANSFORM}"
                "-DCHECKER=$<TARGET_FILE:ab_object_transform_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_object_transform.cmake")
    endif()
    set(AB_REFERENCE_OBJECT_VISUAL "" CACHE FILEPATH
        "Optional original Android GameLua/SpriteAsset sprite assignment corpus")
    if(AB_REFERENCE_OBJECT_VISUAL)
        add_test(NAME game_object_visual_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_OBJECT_VISUAL}"
                "-DCHECKER=$<TARGET_FILE:ab_object_visual_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_object_visual.cmake")
    endif()
    set(AB_REFERENCE_OBJECT_UPDATE "" CACHE FILEPATH
        "Optional original Android per-object hook lifecycle and dispatch corpus")
    if(AB_REFERENCE_OBJECT_UPDATE)
        add_test(NAME game_object_update_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_OBJECT_UPDATE}"
                "-DCHECKER=$<TARGET_FILE:ab_object_update_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_object_update.cmake")
    endif()
    set(AB_REFERENCE_BLOCK_UPDATE "" CACHE FILEPATH
        "Optional original Android registered block-script update corpus")
    if(AB_REFERENCE_BLOCK_UPDATE)
        add_test(NAME game_block_update_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_BLOCK_UPDATE}"
                "-DCHECKER=$<TARGET_FILE:ab_block_update_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_block_update.cmake")
    endif()
    add_executable(ab_block_component_tests tests/block_component.c)
    target_link_libraries(ab_block_component_tests PRIVATE ab_game)
    add_test(NAME game_block_component_edges COMMAND ab_block_component_tests)
    add_executable(ab_flame_effect_tests tests/flame_effect.c)
    target_link_libraries(ab_flame_effect_tests PRIVATE ab_game)
    add_test(NAME game_flame_effect_edges COMMAND ab_flame_effect_tests)
    add_executable(ab_flame_draw_tests tests/flame_draw.c)
    target_link_libraries(ab_flame_draw_tests PRIVATE ab_game)
    add_test(NAME game_flame_draw_edges COMMAND ab_flame_draw_tests)
    add_executable(ab_flame_apply_tests tests/flame_apply.c)
    target_link_libraries(ab_flame_apply_tests PRIVATE ab_game)
    add_test(NAME game_flame_apply_edges COMMAND ab_flame_apply_tests)
    add_executable(ab_ambient_animation_tests tests/ambient_animation.c)
    target_link_libraries(ab_ambient_animation_tests PRIVATE ab_game)
    add_test(NAME game_ambient_animation_edges COMMAND ab_ambient_animation_tests)
    add_executable(ab_ambient_particle_tests tests/ambient_particle.c)
    target_link_libraries(ab_ambient_particle_tests PRIVATE ab_game)
    add_test(NAME game_ambient_particle_edges COMMAND ab_ambient_particle_tests)
    set(AB_REFERENCE_AMBIENT_PARTICLE "" CACHE FILEPATH
        "Optional original Android AmbientParticles four-update corpus")
    if(AB_REFERENCE_AMBIENT_PARTICLE)
        add_test(NAME game_ambient_particle_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_AMBIENT_PARTICLE}"
                "-DCHECKER=$<TARGET_FILE:ab_ambient_particle_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_ambient_particle.cmake")
    endif()
    set(AB_REFERENCE_AMBIENT_PARTICLE_COORDINATES "" CACHE FILEPATH
        "Optional original Android AmbientParticles with native coordinate transform")
    if(AB_REFERENCE_AMBIENT_PARTICLE_COORDINATES)
        add_test(NAME game_ambient_particle_coordinates_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_AMBIENT_PARTICLE_COORDINATES}"
                "-DCHECKER=$<TARGET_FILE:ab_ambient_particle_tests>"
                -DCOORDINATES=ON
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_ambient_particle.cmake")
    endif()
    set(AB_REFERENCE_AMBIENT_ANIMATION "" CACHE FILEPATH
        "Optional original Android AmbientAnimations four-update corpus")
    if(AB_REFERENCE_AMBIENT_ANIMATION)
        add_test(NAME game_ambient_animation_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_AMBIENT_ANIMATION}"
                "-DCHECKER=$<TARGET_FILE:ab_ambient_animation_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_ambient_animation.cmake")
    endif()
    set(AB_REFERENCE_FLAME_APPLY "" CACHE FILEPATH
        "Optional original Android whole FlameEffect construction corpus")
    if(AB_REFERENCE_FLAME_APPLY)
        add_test(NAME game_flame_apply_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_FLAME_APPLY}"
                "-DCHECKER=$<TARGET_FILE:ab_flame_apply_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_flame_apply.cmake")
    endif()
    set(AB_REFERENCE_FLAME_DRAW "" CACHE FILEPATH
        "Optional original Android FlameEffect draw/rotate corpus")
    if(AB_REFERENCE_FLAME_DRAW)
        add_test(NAME game_flame_draw_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_FLAME_DRAW}"
                "-DCHECKER=$<TARGET_FILE:ab_flame_draw_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_flame_draw.cmake")
    endif()
    set(AB_REFERENCE_FLAME_EFFECT "" CACHE FILEPATH
        "Optional original Android FlameEffect per-object update corpus")
    if(AB_REFERENCE_FLAME_EFFECT)
        add_test(NAME game_flame_effect_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_FLAME_EFFECT}"
                "-DCHECKER=$<TARGET_FILE:ab_flame_effect_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_flame_effect.cmake")
    endif()
    set(AB_REFERENCE_BLOCK_COMPONENT "" CACHE FILEPATH
        "Optional original Android three-script component predicate corpus")
    if(AB_REFERENCE_BLOCK_COMPONENT)
        add_test(NAME game_block_component_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_BLOCK_COMPONENT}"
                "-DCHECKER=$<TARGET_FILE:ab_block_component_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_block_component.cmake")
    endif()
    if(AB_REFERENCE_APK)
        add_test(NAME game_block_component_catalog_apk
            COMMAND ab_block_component_tests --apk "${AB_REFERENCE_APK}")
    endif()
    set(AB_REFERENCE_BLOCK_SCRIPT "" CACHE FILEPATH
        "Optional original Android block-script registration/application corpus")
    if(AB_REFERENCE_BLOCK_SCRIPT)
        add_test(NAME game_block_script_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_BLOCK_SCRIPT}"
                "-DCHECKER=$<TARGET_FILE:ab_block_script_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_block_script.cmake")
    endif()
    set(AB_REFERENCE_BLOCK_EVENT "" CACHE FILEPATH
        "Optional original Android per-object/global block event corpus")
    set(AB_REFERENCE_PHYSICS_SCRIPT "" CACHE FILEPATH
        "Optional original Android updatePhysicsStep/block-event composition")
    if(AB_REFERENCE_PHYSICS_SCRIPT)
        add_test(NAME game_physics_script_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_PHYSICS_SCRIPT}"
                "-DCHECKER=$<TARGET_FILE:ab_block_event_tests>"
                -DPHYSICS=ON
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_block_event.cmake")
    endif()
    if(AB_REFERENCE_BLOCK_EVENT)
        add_test(NAME game_block_event_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_BLOCK_EVENT}"
                "-DCHECKER=$<TARGET_FILE:ab_block_event_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_block_event.cmake")
    endif()
    set(AB_REFERENCE_WATER_EFFECT "" CACHE FILEPATH
        "Optional original Android WaterEffect constructor corpus")
    if(AB_REFERENCE_WATER_EFFECT)
        add_test(NAME game_water_effect_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_WATER_EFFECT}"
                "-DCHECKER=$<TARGET_FILE:ab_water_effect_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_water_effect.cmake")
    endif()
    if(AB_REFERENCE_APK)
        add_test(NAME game_water_effect_levels_apk
            COMMAND ab_water_effect_tests --apk "${AB_REFERENCE_APK}")
    endif()
    set(AB_REFERENCE_FORCE_QUEUE "" CACHE FILEPATH
        "Optional original Android queued-force add/apply/clear corpus")
    set(AB_REFERENCE_FORCE_POINT "" CACHE FILEPATH
        "Optional original Android GameLua point-force corpus")
    if(AB_REFERENCE_FORCE_QUEUE)
        add_test(NAME game_force_queue_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_FORCE_QUEUE}"
                "-DCHECKER=$<TARGET_FILE:ab_force_queue_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_force_queue.cmake")
    endif()
    if(AB_REFERENCE_FORCE_POINT)
        add_test(NAME game_force_point_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_FORCE_POINT}"
                "-DCHECKER=$<TARGET_FILE:ab_force_queue_tests>"
                -DPOINT=ON
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_force_queue.cmake")
    endif()
    set(AB_REFERENCE_CLASSIC_FORCE_PROPERTIES "" CACHE FILEPATH
        "Optional captured native level-one object-map force properties")
    if(AB_REFERENCE_APK)
        add_test(NAME game_object_force_catalog_apk
            COMMAND ab_force_data_tests --apk "${AB_REFERENCE_APK}")
        if(AB_REFERENCE_CLASSIC_FORCE_PROPERTIES)
            add_test(NAME classic_native_apk_force_properties
                COMMAND "${CMAKE_COMMAND}"
                    "-DAPK=${AB_REFERENCE_APK}"
                    "-DREFERENCE=${AB_REFERENCE_CLASSIC_FORCE_PROPERTIES}"
                    "-DCHECKER=$<TARGET_FILE:ab_force_data_tests>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_force_properties.cmake")
        endif()
    endif()
    add_executable(ab_game_collision_tests tests/game_collision.c)
    target_link_libraries(ab_game_collision_tests PRIVATE ab_game)
    add_test(NAME game_collision_edges COMMAND ab_game_collision_tests)
    add_executable(ab_damage_tests tests/collision_damage.c)
    target_link_libraries(ab_damage_tests PRIVATE ab_game)
    add_test(NAME game_legacy_block_damage_edges COMMAND ab_damage_tests)
    set(AB_REFERENCE_BLOCK_DAMAGE "" CACHE FILEPATH
        "Optional recorded 4096-case original-Lua legacy block collision corpus")
    set(AB_REFERENCE_BIRD_DAMAGE "" CACHE FILEPATH
        "Optional recorded 4096-case original-Lua bird impact corpus")
    set(AB_REFERENCE_BIRD_ROUTE "" CACHE FILEPATH
        "Optional recorded 4096-case original-Lua enclosing bird collision corpus")
    if(AB_REFERENCE_BIRD_ROUTE)
        add_test(NAME game_bird_route_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_BIRD_ROUTE}"
                "-DCHECKER=$<TARGET_FILE:ab_damage_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_bird_route_reference.cmake")
    endif()
    if(AB_REFERENCE_BIRD_DAMAGE)
        add_test(NAME game_bird_damage_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_BIRD_DAMAGE}"
                "-DCHECKER=$<TARGET_FILE:ab_damage_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_bird_damage_reference.cmake")
    endif()
    if(AB_REFERENCE_BLOCK_DAMAGE)
        add_test(NAME game_legacy_block_damage_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_BLOCK_DAMAGE}"
                "-DCHECKER=$<TARGET_FILE:ab_damage_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_block_damage_reference.cmake")
    endif()
    set(AB_REFERENCE_GAME_COLLISION "" CACHE FILEPATH
        "Optional recorded 4096-case Android GameLua collision corpus")
    set(AB_REFERENCE_GAME_RULES "" CACHE FILEPATH
        "Optional recorded 4096-case Android collision setup/update corpus")
    if(AB_REFERENCE_GAME_RULES)
        add_test(NAME game_rules_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_GAME_RULES}"
                "-DCHECKER=$<TARGET_FILE:ab_game_collision_tests>"
                -DKIND=rules
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_game_collision_reference.cmake")
    endif()
    if(AB_REFERENCE_GAME_COLLISION)
        add_test(NAME game_collision_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_GAME_COLLISION}"
                "-DCHECKER=$<TARGET_FILE:ab_game_collision_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_game_collision_reference.cmake")
    endif()
    set(AB_REFERENCE_CLASSIC_BLOCKS "" CACHE FILEPATH
        "Optional decoded classic blocks.lua from the pinned APK")
    set(AB_REFERENCE_CLASSIC_DEFINITIONS "" CACHE FILEPATH
        "Optional captured native classic definition fields")
    set(AB_REFERENCE_CLASSIC_FIXTURES "" CACHE FILEPATH
        "Optional captured native classic level fixture geometry")
    set(AB_REFERENCE_CLASSIC_BODIES "" CACHE FILEPATH
        "Optional captured first-step native level-1 body/fixture states")
    set(AB_REFERENCE_CLASSIC_STEPS "" CACHE FILEPATH
        "Optional captured eight-step fresh native level-1 physics sequence")
    set(AB_REFERENCE_CLASSIC_FRAMES "" CACHE FILEPATH
        "Optional captured twelve-frame native level-1 clock/physics/sling/pose sequence")
    set(AB_REFERENCE_CLASSIC_LEVEL_CAMERA "" CACHE FILEPATH
        "Optional actual first-level launch/camera/world update comparison")
    set(AB_REFERENCE_CLASSIC_BEHAVIORS "" CACHE FILEPATH
        "Optional actual first-level script behavior snapshot comparison")
    set(AB_REFERENCE_CLASSIC_PHYSICAL_FIELDS "" CACHE FILEPATH
        "Optional actual first-level raw controllable field comparison")
    set(AB_REFERENCE_CLASSIC_LEVEL_CAMERA_COLD_REPEAT "" CACHE FILEPATH
        "Optional independent cold first-level camera/world comparison")
    set(AB_REFERENCE_CLASSIC_LEVEL_TRAJECTORY "" CACHE FILEPATH
        "Optional actual first-level trajectory/behavior/camera/world comparison")
    set(AB_REFERENCE_CLASSIC_LEVEL_TRAJECTORY_REPEAT "" CACHE FILEPATH
        "Optional independent cold first-level trajectory/behavior comparison")
    set(AB_REFERENCE_CLASSIC_LEVEL_SCENE "" CACHE FILEPATH
        "Optional actual first-level scene IDs/order/renderer/world comparison")
    set(AB_REFERENCE_CLASSIC_FLIGHT "" CACHE FILEPATH
        "Optional captured twelve-frame pre-impact native level-1 flight sequence")
    set(AB_REFERENCE_CLASSIC_IMPACT "" CACHE FILEPATH
        "Optional captured 24-frame native level-1 impact/strength/score sequence")
    set(AB_REFERENCE_CLASSIC_RELOAD "" CACHE FILEPATH
        "Optional 24-frame native impact sequence with generated next-bird reload")
    set(AB_REFERENCE_CLASSIC_IDLE "" CACHE FILEPATH
        "Optional 24-frame native impact sequence with generated queue idle/reload motion")
    set(AB_REFERENCE_CLASSIC_RETRY "" CACHE FILEPATH
        "Optional 38-frame native retry sequence with supplied initial proxy identifiers")
    set(AB_REFERENCE_CLASSIC_RESULTS "" CACHE FILEPATH
        "Optional 96-frame native retry/completion/retirement/bonus projection")
    set(AB_REFERENCE_CLASSIC_RESULTS_QUEUE "" CACHE FILEPATH
        "Optional 96-frame native result projection with generated queue/history")
    set(AB_REFERENCE_GAME_ANGLE "" CACHE FILEPATH
        "Optional recorded 65536-case Android game angle-wrap corpus")
    if(AB_REFERENCE_GAME_ANGLE)
        add_test(NAME game_angle_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_GAME_ANGLE}"
                "-DCHECKER=$<TARGET_FILE:ab_definition_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_game_angle_reference.cmake")
    endif()
    if(AB_REFERENCE_CLASSIC_BLOCKS AND AB_REFERENCE_LEVEL_ONE AND
        AB_REFERENCE_CLASSIC_BODIES)
        add_test(NAME classic_native_initial_bodies
            COMMAND "${CMAKE_COMMAND}"
                "-DBLOCKS=${AB_REFERENCE_CLASSIC_BLOCKS}"
                "-DLEVEL=${AB_REFERENCE_LEVEL_ONE}"
                "-DREFERENCE=${AB_REFERENCE_CLASSIC_BODIES}"
                "-DCHECKER=$<TARGET_FILE:ab_definition_tests>"
                -DKIND=bodies
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_reference.cmake")
    endif()
    if(AB_REFERENCE_CLASSIC_BLOCKS AND AB_REFERENCE_LEVEL_ONE AND
        AB_REFERENCE_CLASSIC_BODIES AND AB_REFERENCE_CLASSIC_STEPS)
        add_test(NAME classic_native_level_steps
            COMMAND "${CMAKE_COMMAND}"
                "-DBLOCKS=${AB_REFERENCE_CLASSIC_BLOCKS}"
                "-DLEVEL=${AB_REFERENCE_LEVEL_ONE}"
                "-DBODIES=${AB_REFERENCE_CLASSIC_BODIES}"
                "-DREFERENCE=${AB_REFERENCE_CLASSIC_STEPS}"
                "-DCHECKER=$<TARGET_FILE:ab_definition_tests>"
                -DKIND=steps
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_reference.cmake")
    endif()
    if(AB_REFERENCE_CLASSIC_BLOCKS AND AB_REFERENCE_LEVEL_ONE AND
        AB_REFERENCE_CLASSIC_BODIES AND AB_REFERENCE_CLASSIC_FRAMES)
        add_test(NAME classic_native_level_frames
            COMMAND "${CMAKE_COMMAND}"
                "-DBLOCKS=${AB_REFERENCE_CLASSIC_BLOCKS}"
                "-DLEVEL=${AB_REFERENCE_LEVEL_ONE}"
                "-DBODIES=${AB_REFERENCE_CLASSIC_BODIES}"
                "-DREFERENCE=${AB_REFERENCE_CLASSIC_FRAMES}"
                "-DCHECKER=$<TARGET_FILE:ab_definition_tests>"
                -DKIND=frames
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_reference.cmake")
        if(AB_REFERENCE_CLASSIC_FLIGHT)
            add_test(NAME classic_native_level_flight
                COMMAND "${CMAKE_COMMAND}"
                    "-DBLOCKS=${AB_REFERENCE_CLASSIC_BLOCKS}"
                    "-DLEVEL=${AB_REFERENCE_LEVEL_ONE}"
                    "-DBODIES=${AB_REFERENCE_CLASSIC_BODIES}"
                    "-DINITIAL_FRAMES=${AB_REFERENCE_CLASSIC_FRAMES}"
                    "-DREFERENCE=${AB_REFERENCE_CLASSIC_FLIGHT}"
                    "-DCHECKER=$<TARGET_FILE:ab_definition_tests>"
                    -DKIND=flight
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_reference.cmake")
        endif()
        if(AB_REFERENCE_CLASSIC_IMPACT)
            add_test(NAME classic_native_level_impact
                COMMAND "${CMAKE_COMMAND}"
                    "-DBLOCKS=${AB_REFERENCE_CLASSIC_BLOCKS}"
                    "-DLEVEL=${AB_REFERENCE_LEVEL_ONE}"
                    "-DBODIES=${AB_REFERENCE_CLASSIC_BODIES}"
                    "-DINITIAL_FRAMES=${AB_REFERENCE_CLASSIC_FRAMES}"
                    "-DREFERENCE=${AB_REFERENCE_CLASSIC_IMPACT}"
                    "-DCHECKER=$<TARGET_FILE:ab_definition_tests>"
                    -DKIND=impact
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_reference.cmake")
        endif()
        if(AB_REFERENCE_CLASSIC_RELOAD)
            add_test(NAME classic_native_level_reload
                COMMAND "${CMAKE_COMMAND}"
                    "-DBLOCKS=${AB_REFERENCE_CLASSIC_BLOCKS}"
                    "-DLEVEL=${AB_REFERENCE_LEVEL_ONE}"
                    "-DBODIES=${AB_REFERENCE_CLASSIC_BODIES}"
                    "-DINITIAL_FRAMES=${AB_REFERENCE_CLASSIC_FRAMES}"
                    "-DREFERENCE=${AB_REFERENCE_CLASSIC_RELOAD}"
                    "-DCHECKER=$<TARGET_FILE:ab_definition_tests>"
                    -DKIND=impact-reload
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_reference.cmake")
        endif()
        if(AB_REFERENCE_CLASSIC_IDLE)
            add_test(NAME classic_native_level_idle
                COMMAND "${CMAKE_COMMAND}"
                    "-DBLOCKS=${AB_REFERENCE_CLASSIC_BLOCKS}"
                    "-DLEVEL=${AB_REFERENCE_LEVEL_ONE}"
                    "-DBODIES=${AB_REFERENCE_CLASSIC_BODIES}"
                    "-DINITIAL_FRAMES=${AB_REFERENCE_CLASSIC_FRAMES}"
                    "-DREFERENCE=${AB_REFERENCE_CLASSIC_IDLE}"
                    "-DCHECKER=$<TARGET_FILE:ab_definition_tests>"
                    -DKIND=impact-idle
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_reference.cmake")
        endif()
        if(AB_REFERENCE_CLASSIC_RETRY)
            add_test(NAME classic_native_level_retry
                COMMAND "${CMAKE_COMMAND}"
                    "-DBLOCKS=${AB_REFERENCE_CLASSIC_BLOCKS}"
                    "-DLEVEL=${AB_REFERENCE_LEVEL_ONE}"
                    "-DBODIES=${AB_REFERENCE_CLASSIC_BODIES}"
                    "-DINITIAL_FRAMES=${AB_REFERENCE_CLASSIC_FRAMES}"
                    "-DREFERENCE=${AB_REFERENCE_CLASSIC_RETRY}"
                    "-DCHECKER=$<TARGET_FILE:ab_definition_tests>"
                    -DKIND=retry
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_reference.cmake")
        endif()
        if(AB_REFERENCE_CLASSIC_RESULTS)
            add_test(NAME classic_native_level_results
                COMMAND "${CMAKE_COMMAND}"
                    "-DBLOCKS=${AB_REFERENCE_CLASSIC_BLOCKS}"
                    "-DLEVEL=${AB_REFERENCE_LEVEL_ONE}"
                    "-DBODIES=${AB_REFERENCE_CLASSIC_BODIES}"
                    "-DINITIAL_FRAMES=${AB_REFERENCE_CLASSIC_FRAMES}"
                    "-DREFERENCE=${AB_REFERENCE_CLASSIC_RESULTS}"
                    "-DCHECKER=$<TARGET_FILE:ab_definition_tests>"
                    -DKIND=results
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_reference.cmake")
        endif()
        if(AB_REFERENCE_CLASSIC_RESULTS_QUEUE)
            add_test(NAME classic_native_level_results_queue
                COMMAND "${CMAKE_COMMAND}"
                    "-DBLOCKS=${AB_REFERENCE_CLASSIC_BLOCKS}"
                    "-DLEVEL=${AB_REFERENCE_LEVEL_ONE}"
                    "-DBODIES=${AB_REFERENCE_CLASSIC_BODIES}"
                    "-DINITIAL_FRAMES=${AB_REFERENCE_CLASSIC_FRAMES}"
                    "-DREFERENCE=${AB_REFERENCE_CLASSIC_RESULTS_QUEUE}"
                    "-DCHECKER=$<TARGET_FILE:ab_definition_tests>"
                    -DKIND=results-queue
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_reference.cmake")
        endif()
    endif()
    foreach(variant DEFINITIONS FIXTURES)
        if(AB_REFERENCE_CLASSIC_BLOCKS AND AB_REFERENCE_CLASSIC_${variant})
            string(TOLOWER "${variant}" kind)
            add_test(NAME classic_native_${kind}
                COMMAND "${CMAKE_COMMAND}"
                    "-DBLOCKS=${AB_REFERENCE_CLASSIC_BLOCKS}"
                    "-DREFERENCE=${AB_REFERENCE_CLASSIC_${variant}}"
                    "-DCHECKER=$<TARGET_FILE:ab_definition_tests>"
                    "-DKIND=${kind}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_reference.cmake")
        endif()
    endforeach()
    add_executable(ab_resource_tests tests/resource.c)
    target_link_libraries(ab_resource_tests PRIVATE ab_resource)
    add_executable(ab_lua_text_tests tests/lua_text.c)
    target_link_libraries(ab_lua_text_tests PRIVATE ab_resource)
    add_test(NAME serialized_table_bounds COMMAND ab_lua_text_tests)
    add_executable(ab_lua_literal_range_tests tests/lua_literal_range.c)
    target_link_libraries(ab_lua_literal_range_tests PRIVATE ab_resource)
    add_test(NAME lua_literal_range_bounds COMMAND ab_lua_literal_range_tests)
    add_executable(ab_event_timer_tests tests/event_timer.c)
    target_link_libraries(ab_event_timer_tests PRIVATE ab_game)
    add_test(NAME event_timer_deadlines COMMAND ab_event_timer_tests)
    add_executable(ab_level_progress_tests tests/level_progress.c)
    target_link_libraries(ab_level_progress_tests PRIVATE ab_game)
    add_test(NAME level_progress_gates COMMAND ab_level_progress_tests)
    add_executable(ab_episode_catalog_tests tests/episode_catalog.c)
    target_link_libraries(ab_episode_catalog_tests PRIVATE ab_resource)
    add_executable(ab_episode_statistics_tests tests/episode_statistics.c)
    target_link_libraries(ab_episode_statistics_tests PRIVATE ab_game ab_resource)
    if(AB_REFERENCE_APK)
        add_test(NAME episode_catalog_apk
            COMMAND ab_episode_catalog_tests "${AB_REFERENCE_APK}")
        add_test(NAME episode_statistics_apk
            COMMAND ab_episode_statistics_tests "${AB_REFERENCE_APK}")
    endif()
    add_executable(ab_draw_tests tests/draw.c)
    target_link_libraries(ab_draw_tests PRIVATE ab_render)
    add_test(NAME draw_geometry_and_bounds COMMAND ab_draw_tests)
    add_executable(ab_sprite_atlas_tests tests/sprite_atlas.c)
    target_link_libraries(ab_sprite_atlas_tests PRIVATE ab_render)
    add_test(NAME sprite_atlas_ownership_and_bounds COMMAND ab_sprite_atlas_tests)
    add_executable(ab_classic_scene_tests tests/classic_scene.c)
    target_link_libraries(ab_classic_scene_tests PRIVATE ab_scene ab_asset_resources)
    add_executable(ab_classic_scene_gpu_tests tests/classic_scene.c)
    target_compile_definitions(ab_classic_scene_gpu_tests PRIVATE AB_CLASSIC_SCENE_COMMON=1)
    target_link_libraries(ab_classic_scene_gpu_tests PRIVATE ab_scene ab_asset_resources ab_common_renderer)
    cc_add_platform(ab_classic_scene_gpu_tests "${AB_BACKEND}")
    add_executable(ab_classic_post_physics_tests tests/classic_post_physics.c)
    target_link_libraries(ab_classic_post_physics_tests PRIVATE ab_game)
    add_test(NAME classic_post_physics_owner_edges COMMAND ab_classic_post_physics_tests)
    add_executable(ab_classic_membership_tests tests/classic_membership.c)
    target_link_libraries(ab_classic_membership_tests PRIVATE ab_game)
    set(AB_REFERENCE_CLASSIC_REMOVED_POST "" CACHE FILEPATH
        "Original native post-physics traversal after physical object removal")
    add_executable(ab_classic_preview_tests tests/classic_preview.c)
    target_link_libraries(ab_classic_preview_tests PRIVATE ab_scene)
    # Explicit native-window diagnostic; keep unattended CTest runs headless.
    add_executable(ab_common_clip_tests tests/common_clip.c)
    target_link_libraries(ab_common_clip_tests PRIVATE ab_common_renderer)
    cc_add_platform(ab_common_clip_tests "${AB_BACKEND}")
    add_executable(ab_common_mask_tests tests/common_mask.c)
    target_link_libraries(ab_common_mask_tests PRIVATE ab_common_renderer)
    cc_add_platform(ab_common_mask_tests "${AB_BACKEND}")
    add_executable(ab_gles2_tests tests/gles2.c)
    target_link_libraries(ab_gles2_tests PRIVATE ab_render)
    add_test(NAME gles2_ownership_and_submission COMMAND ab_gles2_tests)
    if(APPLE)
        add_executable(ab_metal_tests tests/metal.c)
        target_link_libraries(ab_metal_tests PRIVATE ab_metal)
        add_test(NAME metal_native_pixels COMMAND ab_metal_tests)
        set_tests_properties(metal_native_pixels PROPERTIES SKIP_RETURN_CODE 77)
    endif()
    find_program(AB_GLSLANG_VALIDATOR NAMES glslangValidator)
    if(AB_GLSLANG_VALIDATOR)
        add_test(NAME common_sprite_shader_es2_link COMMAND "${AB_GLSLANG_VALIDATOR}"
            --glsl-version 100 -l
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders/common_sprite.vert"
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders/common_sprite.frag")
        add_test(NAME gles2_shader_link COMMAND "${AB_GLSLANG_VALIDATOR}"
            --glsl-version 100 -l
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders/sprite.vert"
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders/sprite.frag")
        add_test(NAME gles2_shader_high_precision_link COMMAND "${AB_GLSLANG_VALIDATOR}"
            --glsl-version 100 -DAB_HIGH_PRECISION_UV=1 -l
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders/sprite.vert"
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders/sprite.frag")
    endif()
    add_test(NAME resource_bounds_and_formats COMMAND ab_resource_tests)
    add_executable(ab_physics_tests tests/physics.c)
    target_link_libraries(ab_physics_tests PRIVATE ab_physics)
    add_test(NAME physics_geometry_edges COMMAND ab_physics_tests)
    set(AB_REFERENCE_TOI_POSITION "" CACHE FILEPATH
        "Optional recorded 4096-case Android continuous-position corpus")
    if(AB_REFERENCE_TOI_POSITION)
        add_test(NAME physics_android_toi_position
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_TOI_POSITION}"
                "-DCHECKER=$<TARGET_FILE:ab_physics_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_toi_position_reference.cmake")
    endif()
    add_executable(ab_body_setup_tests tests/body_setup.c)
    target_link_libraries(ab_body_setup_tests PRIVATE ab_physics)
    add_test(NAME physics_body_setup_edges COMMAND ab_body_setup_tests)
    add_executable(ab_contact_state_tests tests/contact_state.c)
    target_link_libraries(ab_contact_state_tests PRIVATE ab_physics)
    add_test(NAME physics_contact_state_edges COMMAND ab_contact_state_tests)
    add_executable(ab_fixture_tests tests/fixture.c)
    target_link_libraries(ab_fixture_tests PRIVATE ab_physics)
    add_test(NAME physics_fixture_edges COMMAND ab_fixture_tests)
    add_executable(ab_distance_tests tests/distance.c)
    target_link_libraries(ab_distance_tests PRIVATE ab_physics)
    add_test(NAME physics_distance_edges COMMAND ab_distance_tests)
    add_executable(ab_world_tests tests/world.c)
    target_link_libraries(ab_world_tests PRIVATE ab_physics)
    add_test(NAME physics_world_edges COMMAND ab_world_tests)
    set(AB_REFERENCE_WORLD_CLEAR "" CACHE FILEPATH
        "Optional recorded 4096-case native world force-clear corpus")
    if(AB_REFERENCE_WORLD_CLEAR)
        add_test(NAME physics_world_clear_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_WORLD_CLEAR}"
                "-DCHECKER=$<TARGET_FILE:ab_world_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_world_clear_reference.cmake")
    endif()
    add_executable(ab_separation_tests tests/separation.c)
    target_link_libraries(ab_separation_tests PRIVATE ab_physics)
    add_test(NAME physics_sweep_and_separation_edges COMMAND ab_separation_tests)
    add_executable(ab_impact_tests tests/impact.c)
    target_link_libraries(ab_impact_tests PRIVATE ab_physics)
    add_test(NAME physics_impact_edges COMMAND ab_impact_tests)
    set(AB_REFERENCE_IMPACT "" CACHE FILEPATH
        "Optional recorded 4096-case Android time-of-impact corpus")
    if(AB_REFERENCE_IMPACT)
        add_test(NAME physics_android_impact
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_IMPACT}"
                "-DCHECKER=$<TARGET_FILE:ab_impact_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_impact_reference.cmake")
    endif()
    set(AB_REFERENCE_SEPARATION "" CACHE FILEPATH
        "Optional recorded 4096-axis/20480-query Android separation corpus")
    if(AB_REFERENCE_SEPARATION)
        add_test(NAME physics_android_separation
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_SEPARATION}"
                "-DCHECKER=$<TARGET_FILE:ab_separation_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_separation_reference.cmake")
    endif()
    set(AB_REFERENCE_WORLD_STEPS "" CACHE FILEPATH
        "Optional recorded 16-scene/4096-step Android world corpus")
    set(AB_REFERENCE_WORLD_EVENTS "" CACHE FILEPATH
        "Optional recorded 16-scene/4096-step Android world event corpus")
    set(AB_REFERENCE_WORLD_CONTINUOUS "" CACHE FILEPATH
        "Optional recorded 16-scene/4096-step Android continuous world corpus")
    set(AB_REFERENCE_WORLD_CONTINUOUS_EVENTS "" CACHE FILEPATH
        "Optional recorded 16-scene/4096-step Android continuous world events")
    set(AB_REFERENCE_WORLD_REJECTED_IMPACTS "" CACHE FILEPATH
        "Optional recorded 4096-step Android rejected continuous-impact events")
    set(AB_REFERENCE_WORLD_OPERATIONS "" CACHE FILEPATH
        "Optional recorded 4096-operation/step Android world state corpus")
    set(AB_REFERENCE_WORLD_LIFECYCLE "" CACHE FILEPATH
        "Optional recorded 4096-operation/step Android body/fixture lifecycle corpus")
    foreach(variant CONTINUOUS CONTINUOUS_EVENTS REJECTED_IMPACTS OPERATIONS LIFECYCLE)
        if(AB_REFERENCE_WORLD_${variant})
            string(TOLOWER "${variant}" kind)
            string(REPLACE "_" "-" kind "${kind}")
            string(TOLOWER "${variant}" test_suffix)
            add_test(NAME physics_android_world_${test_suffix}
                COMMAND "${CMAKE_COMMAND}"
                    "-DREFERENCE=${AB_REFERENCE_WORLD_${variant}}"
                    "-DCHECKER=$<TARGET_FILE:ab_world_tests>"
                    "-DKIND=${kind}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_world_steps_reference.cmake")
        endif()
    endforeach()
    if(AB_REFERENCE_WORLD_EVENTS)
        add_test(NAME physics_android_world_events
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_WORLD_EVENTS}"
                "-DCHECKER=$<TARGET_FILE:ab_world_tests>"
                -DKIND=events
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_world_steps_reference.cmake")
    endif()
    if(AB_REFERENCE_WORLD_STEPS)
        add_test(NAME physics_android_world_steps
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_WORLD_STEPS}"
                "-DCHECKER=$<TARGET_FILE:ab_world_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_world_steps_reference.cmake")
    endif()
    set(AB_REFERENCE_DISTANCE "" CACHE FILEPATH
        "Optional recorded 4096-case Android distance and overlap corpus")
    if(AB_REFERENCE_DISTANCE)
        add_test(NAME physics_android_distance
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_DISTANCE}"
                "-DCHECKER=$<TARGET_FILE:ab_distance_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_distance_reference.cmake")
    endif()
    set(AB_REFERENCE_FIXTURE "" CACHE FILEPATH
        "Optional recorded 4096-operation Android fixture-proxy corpus")
    if(AB_REFERENCE_FIXTURE)
        add_test(NAME physics_android_fixture
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_FIXTURE}"
                "-DCHECKER=$<TARGET_FILE:ab_fixture_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_fixture_reference.cmake")
    endif()
    set(AB_REFERENCE_CONTACT_STATE "" CACHE FILEPATH
        "Optional recorded 4096-case Android persistent-contact corpus")
    if(AB_REFERENCE_CONTACT_STATE)
        add_test(NAME physics_android_contact_state
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_CONTACT_STATE}"
                "-DCHECKER=$<TARGET_FILE:ab_contact_state_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_contact_state_reference.cmake")
    endif()
    set(AB_REFERENCE_BODY_SETUP "" CACHE FILEPATH
        "Optional recorded 4096-case Android body-setup and filter corpus")
    if(AB_REFERENCE_BODY_SETUP)
        add_test(NAME physics_android_body_setup
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_BODY_SETUP}"
                "-DCHECKER=$<TARGET_FILE:ab_body_setup_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_body_setup_reference.cmake")
    endif()
    add_executable(ab_tree_tests tests/tree.c)
    target_link_libraries(ab_tree_tests PRIVATE ab_physics)
    add_test(NAME physics_tree_edges COMMAND ab_tree_tests)
    add_executable(ab_broadphase_tests tests/broadphase.c)
    target_link_libraries(ab_broadphase_tests PRIVATE ab_physics)
    add_test(NAME physics_broadphase_edges COMMAND ab_broadphase_tests)
    set(AB_REFERENCE_BROADPHASE "" CACHE FILEPATH
        "Optional recorded 4096-operation Android broadphase corpus")
    if(AB_REFERENCE_BROADPHASE)
        add_test(NAME physics_android_broadphase
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_BROADPHASE}"
                "-DCHECKER=$<TARGET_FILE:ab_broadphase_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_broadphase_reference.cmake")
    endif()
    set(AB_REFERENCE_TREE "" CACHE FILEPATH
        "Optional recorded 4096-operation Android dynamic-tree corpus")
    if(AB_REFERENCE_TREE)
        add_test(NAME physics_android_tree
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_TREE}"
                "-DCHECKER=$<TARGET_FILE:ab_tree_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_tree_reference.cmake")
    endif()
    add_executable(ab_step_free_tests tests/step_free.c)
    target_link_libraries(ab_step_free_tests PRIVATE ab_physics)
    add_test(NAME physics_free_integration_edges COMMAND ab_step_free_tests)
    add_executable(ab_joint_island_tests tests/joint_island.c)
    target_link_libraries(ab_joint_island_tests PRIVATE ab_physics)
    add_test(NAME physics_joint_island_edges COMMAND ab_joint_island_tests)
    add_executable(ab_world_joint_tests tests/world_joint.c)
    target_link_libraries(ab_world_joint_tests PRIVATE ab_physics)
    add_executable(ab_game_joint_tests tests/game_joint.c)
    target_link_libraries(ab_game_joint_tests PRIVATE ab_game)
    add_test(NAME game_joint_edges COMMAND ab_game_joint_tests)
    add_executable(ab_game_joint_break_tests tests/game_joint_break.c)
    target_link_libraries(ab_game_joint_break_tests PRIVATE ab_game)
    add_test(NAME game_joint_break_edges COMMAND ab_game_joint_break_tests)
    add_executable(ab_game_joint_updates_tests tests/game_joint_updates.c)
    target_link_libraries(ab_game_joint_updates_tests PRIVATE ab_game)
    add_test(NAME game_joint_updates COMMAND ab_game_joint_updates_tests)
    add_executable(ab_game_joint_named_tests tests/game_joint_named.c)
    target_link_libraries(ab_game_joint_named_tests PRIVATE ab_game)
    add_test(NAME game_joint_named_edges COMMAND ab_game_joint_named_tests)
    set(AB_REFERENCE_GAME_JOINT_NAMED "" CACHE FILEPATH
        "Optional original named immediate/deferred joint removal corpus")
    if(AB_REFERENCE_GAME_JOINT_NAMED)
        add_test(NAME game_joint_named_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_GAME_JOINT_NAMED}"
                "-DCHECKER=$<TARGET_FILE:ab_game_joint_named_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_game_joint_named_reference.cmake")
    endif()
    add_executable(ab_contact_glue_tests tests/contact_glue.c)
    target_link_libraries(ab_contact_glue_tests PRIVATE ab_game)
    add_test(NAME game_contact_glue_edges COMMAND ab_contact_glue_tests)
    set(AB_REFERENCE_CONTACT_GLUE "" CACHE FILEPATH
        "Optional original begin-contact glue queue corpus")
    if(AB_REFERENCE_CONTACT_GLUE)
        add_test(NAME game_contact_glue_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_CONTACT_GLUE}"
                "-DCHECKER=$<TARGET_FILE:ab_contact_glue_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_contact_glue_reference.cmake")
    endif()
    set(AB_REFERENCE_GAME_JOINT_BREAK "" CACHE FILEPATH
        "Optional original breakable-joint deferred dispatch corpus")
    if(AB_REFERENCE_GAME_JOINT_BREAK)
        add_test(NAME game_joint_break_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_GAME_JOINT_BREAK}"
                "-DCHECKER=$<TARGET_FILE:ab_game_joint_break_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_game_joint_break_reference.cmake")
    endif()
    add_executable(ab_game_polygon_tests tests/game_polygon.c)
    target_link_libraries(ab_game_polygon_tests PRIVATE ab_game)
    set(AB_REFERENCE_GAME_POLYGON "" CACHE FILEPATH
        "Optional native GameObject triangle fixture, mass and metadata corpus")
    if(AB_REFERENCE_GAME_POLYGON)
        add_test(NAME game_polygon_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_GAME_POLYGON}"
                "-DCHECKER=$<TARGET_FILE:ab_game_polygon_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_game_polygon_reference.cmake")
    endif()
    set(AB_REFERENCE_GAME_CIRCLE "" CACHE FILEPATH
        "Optional native GameObject explicit/derived circle radius, mass and metadata")
    if(AB_REFERENCE_GAME_CIRCLE)
        add_test(NAME game_circle_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_GAME_CIRCLE}"
                "-DCHECKER=$<TARGET_FILE:ab_game_polygon_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_game_circle_reference.cmake")
    endif()
    set(AB_REFERENCE_GAME_CIRCLE_ZEROS "" CACHE FILEPATH
        "Optional native zero/signed-zero/underflow circle fixtures and metadata")
    if(AB_REFERENCE_GAME_CIRCLE_ZEROS)
        add_test(NAME game_circle_zeros_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_GAME_CIRCLE_ZEROS}"
                "-DCHECKER=$<TARGET_FILE:ab_game_polygon_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_game_circle_zeros_reference.cmake")
    endif()
    set(AB_REFERENCE_GAME_CONVEX "" CACHE FILEPATH
        "Optional native convex polygon fixtures with three through eight source vertices")
    if(AB_REFERENCE_GAME_CONVEX)
        add_test(NAME game_convex_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_GAME_CONVEX}"
                "-DCHECKER=$<TARGET_FILE:ab_game_polygon_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_game_convex_reference.cmake")
    endif()
    set(AB_REFERENCE_GAME_JOINT_PARAMETERS "" CACHE FILEPATH
        "Optional native joint parameter changes, duplicate names and saved metadata")
    if(AB_REFERENCE_GAME_JOINT_PARAMETERS)
        add_test(NAME game_joint_parameters_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_GAME_JOINT_PARAMETERS}"
                "-DCHECKER=$<TARGET_FILE:ab_game_joint_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_game_joint_parameters_reference.cmake")
    endif()
    set(AB_REFERENCE_GAME_JOINT_LIMITS "" CACHE FILEPATH
        "Optional native motor-limit checks, coordinates and unchanged-speed setters")
    if(AB_REFERENCE_GAME_JOINT_LIMITS)
        add_test(NAME game_joint_limits_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_GAME_JOINT_LIMITS}"
                "-DCHECKER=$<TARGET_FILE:ab_game_joint_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_game_joint_limits_reference.cmake")
    endif()
    set(AB_REFERENCE_GAME_JOINT_ZEROS "" CACHE FILEPATH
        "Optional native joint constructor signed-zero combinations")
    if(AB_REFERENCE_GAME_JOINT_ZEROS)
        add_test(NAME game_joint_zeros_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_GAME_JOINT_ZEROS}"
                "-DCHECKER=$<TARGET_FILE:ab_game_joint_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_game_joint_zeros_reference.cmake")
    endif()
    set(AB_REFERENCE_GAME_GLUE_POLICY "" CACHE FILEPATH
        "Optional native game joint registry and mixed/locked glue queue policy")
    if(AB_REFERENCE_GAME_GLUE_POLICY)
        add_test(NAME game_glue_policy_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_GAME_GLUE_POLICY}"
                "-DCHECKER=$<TARGET_FILE:ab_game_joint_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_game_glue_policy_reference.cmake")
    endif()
    set(AB_REFERENCE_GAME_GLUE "" CACHE FILEPATH
        "Optional native deferred glue metadata and queued weld definitions")
    if(AB_REFERENCE_GAME_GLUE)
        add_test(NAME game_glue_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_GAME_GLUE}"
                "-DCHECKER=$<TARGET_FILE:ab_game_joint_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_game_glue_reference.cmake")
    endif()
    set(AB_REFERENCE_GAME_JOINT "" CACHE FILEPATH
        "Optional native game joint constructors, coordinate modes and metadata")
    if(AB_REFERENCE_GAME_JOINT)
        add_test(NAME game_joint_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_GAME_JOINT}"
                "-DCHECKER=$<TARGET_FILE:ab_game_joint_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_game_joint_reference.cmake")
    endif()
    add_test(NAME physics_world_joint_edges COMMAND ab_world_joint_tests)
    set(AB_REFERENCE_WORLD_JOINT_LIFECYCLE "" CACHE FILEPATH
        "Optional native joint lifecycle and consecutive full-world steps")
    if(AB_REFERENCE_WORLD_JOINT_LIFECYCLE)
        add_test(NAME physics_world_joint_lifecycle_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_WORLD_JOINT_LIFECYCLE}"
                "-DCHECKER=$<TARGET_FILE:ab_world_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_world_joint_lifecycle_reference.cmake")
    endif()
    set(AB_REFERENCE_WORLD_JOINT "" CACHE FILEPATH
        "Optional recorded native world graphs containing contacts and all four joint kinds")
    if(AB_REFERENCE_WORLD_JOINT)
        add_test(NAME physics_world_joint_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_WORLD_JOINT}"
                "-DCHECKER=$<TARGET_FILE:ab_world_joint_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_world_joint_reference.cmake")
    endif()
    set(AB_REFERENCE_JOINT_ISLAND "" CACHE FILEPATH
        "Optional recorded native islands containing contacts and all four joint kinds")
    if(AB_REFERENCE_JOINT_ISLAND)
        add_test(NAME physics_joint_island_android
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_JOINT_ISLAND}"
                "-DCHECKER=$<TARGET_FILE:ab_joint_island_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_joint_island_reference.cmake")
    endif()
    set(AB_REFERENCE_TOI_ISLAND "" CACHE FILEPATH
        "Optional recorded 4096-case Android continuous-island corpus")
    if(AB_REFERENCE_TOI_ISLAND)
        add_test(NAME physics_android_toi_island
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_TOI_ISLAND}"
                "-DCHECKER=$<TARGET_FILE:ab_step_free_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_toi_island_reference.cmake")
    endif()
    set(AB_REFERENCE_CONTACT_REPORT "" CACHE FILEPATH
        "Optional recorded 4096-case Android island post-solve corpus")
    set(AB_REFERENCE_WORLD_REPORT "" CACHE FILEPATH
        "Optional recorded 4096-case Android world post-solve corpus")
    foreach(reference_kind CONTACT_REPORT WORLD_REPORT)
        if(AB_REFERENCE_${reference_kind})
            string(TOLOWER "${reference_kind}" reference_name)
            add_test(NAME "physics_android_${reference_name}"
                COMMAND "${CMAKE_COMMAND}"
                    "-DREFERENCE=${AB_REFERENCE_${reference_kind}}"
                    "-DCHECKER=$<TARGET_FILE:ab_step_free_tests>"
                    "-DKIND=${reference_kind}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_report_reference.cmake")
        endif()
    endforeach()
    set(AB_REFERENCE_WORLD_SOLVE "" CACHE FILEPATH
        "Optional recorded 4096-case Android world-island traversal corpus")
    if(AB_REFERENCE_WORLD_SOLVE)
        add_test(NAME physics_android_world_solve
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_WORLD_SOLVE}"
                "-DCHECKER=$<TARGET_FILE:ab_step_free_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_world_solve_reference.cmake")
    endif()
    set(AB_REFERENCE_FREE_ISLAND "" CACHE FILEPATH
        "Optional recorded 4096-case Android free-island corpus")
    if(AB_REFERENCE_FREE_ISLAND)
        add_test(NAME physics_android_free_islands
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_FREE_ISLAND}"
                "-DCHECKER=$<TARGET_FILE:ab_step_free_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_free_island_reference.cmake")
    endif()
    set(AB_REFERENCE_CONTACT_ISLAND "" CACHE FILEPATH
        "Optional recorded 4096-case Android contact-island corpus")
    if(AB_REFERENCE_CONTACT_ISLAND)
        add_test(NAME physics_android_contact_islands
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_CONTACT_ISLAND}"
                "-DCHECKER=$<TARGET_FILE:ab_step_free_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_contact_island_reference.cmake")
    endif()
    include(cmake/arm_oracle.cmake)
    add_executable(ab_rotation_tests tests/rotation.c)
    target_link_libraries(ab_rotation_tests PRIVATE ab_numeric)
    add_test(NAME rotation_numeric_edges COMMAND ab_rotation_tests)
    set(AB_REFERENCE_EXTENDED_MATH "" CACHE FILEPATH
        "Optional captured 65536-case Android 5.1.1 math corpus")
    foreach(reference_kind ARM_MATH EXTENDED_MATH)
        if(AB_REFERENCE_${reference_kind})
            add_test(NAME "rotation_android_${reference_kind}"
                COMMAND "${CMAKE_COMMAND}"
                    "-DREFERENCE=${AB_REFERENCE_${reference_kind}}"
                    "-DCHECKER=$<TARGET_FILE:ab_rotation_tests>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_math_reference.cmake")
        endif()
    endforeach()
    set(AB_REFERENCE_APK "" CACHE FILEPATH "Optional local reference APK for audit")
    if(AB_REFERENCE_APK)
        add_test(NAME target_save_key_apk
            COMMAND ab_target_save_key_tests "${AB_REFERENCE_APK}")
        add_test(NAME level_request_apk
            COMMAND ab_classic_level_request_tests "${AB_REFERENCE_APK}")
        add_test(NAME level_prepare_apk
            COMMAND ab_classic_level_prepare_tests "${AB_REFERENCE_APK}")
        add_test(NAME start_level_apk
            COMMAND ab_classic_start_level_tests "${AB_REFERENCE_APK}")
        add_test(NAME powerup_catalog_apk
            COMMAND ab_powerup_catalog_tests "${AB_REFERENCE_APK}")
        add_test(NAME ui_hud_powerups_apk
            COMMAND ab_ui_hud_powerups_tests "${AB_REFERENCE_APK}")
        add_test(NAME classic_level_runtime_apk
            COMMAND ab_classic_playfield_tests "${AB_REFERENCE_APK}" --fresh)
    endif()
    if(AB_REFERENCE_APK)
        if(AB_REFERENCE_VP8_PLANES)
            foreach(variant filtered unfiltered)
                set(bypass OFF)
                if(variant STREQUAL unfiltered)
                    set(bypass ON)
                endif()
                add_test(NAME image_vp8_decode_${variant}_android_apk
                    COMMAND "${CMAKE_COMMAND}"
                        "-DAPK=${AB_REFERENCE_APK}"
                        "-DREFERENCE=${AB_REFERENCE_VP8_PLANES}"
                        "-DBYPASS=${bypass}"
                        "-DCHECKER=$<TARGET_FILE:ab_vp8_decode_tests>"
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_vp8_decode_reference.cmake")
            endforeach()
        endif()
        if(AB_REFERENCE_VP8_RESIDUALS)
            add_test(NAME image_vp8_residuals_android_apk
                COMMAND "${CMAKE_COMMAND}"
                    "-DAPK=${AB_REFERENCE_APK}"
                    "-DREFERENCE=${AB_REFERENCE_VP8_RESIDUALS}"
                    "-DCHECKER=$<TARGET_FILE:ab_vp8_residual_tests>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_vp8_residuals_reference.cmake")
        endif()
        if(AB_REFERENCE_VP8_MODES)
            add_test(NAME image_vp8_modes_android_apk
                COMMAND "${CMAKE_COMMAND}"
                    "-DAPK=${AB_REFERENCE_APK}"
                    "-DREFERENCE=${AB_REFERENCE_VP8_MODES}"
                    "-DCHECKER=$<TARGET_FILE:ab_vp8_mode_tests>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_vp8_modes_reference.cmake")
        endif()
        if(AB_REFERENCE_VP8_HEADER)
            add_test(NAME image_vp8_header_android_apk
                COMMAND "${CMAKE_COMMAND}"
                    "-DAPK=${AB_REFERENCE_APK}"
                    "-DREFERENCE=${AB_REFERENCE_VP8_HEADER}"
                    "-DCHECKER=$<TARGET_FILE:ab_vp8_header_tests>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_vp8_header_reference.cmake")
        endif()
        add_test(NAME image_webp_container_apk
            COMMAND "${CMAKE_COMMAND}"
                "-DAPK=${AB_REFERENCE_APK}"
                "-DCHECKER=$<TARGET_FILE:ab_webp_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_webp_apk.cmake")
        if(AB_REFERENCE_WEBP_IMAGES)
            add_test(NAME image_webp_metadata_android_apk
                COMMAND "${CMAKE_COMMAND}"
                    "-DAPK=${AB_REFERENCE_APK}"
                    "-DREFERENCE=${AB_REFERENCE_WEBP_IMAGES}"
                    "-DCHECKER=$<TARGET_FILE:ab_webp_tests>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_webp_metadata_reference.cmake")
        endif()
        if(AB_REFERENCE_WEBP_PIXELS)
            add_test(NAME image_webp_decode_android_apk
                COMMAND "${CMAKE_COMMAND}"
                    "-DAPK=${AB_REFERENCE_APK}"
                    "-DPIXELS=${AB_REFERENCE_WEBP_PIXELS}"
                    "-DDECODE=ON"
                    "-DCHECKER=$<TARGET_FILE:ab_webp_decode_tests>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_webp_pixels_reference.cmake")
            add_test(NAME image_webp_alpha_decode_android_apk
                COMMAND "${CMAKE_COMMAND}"
                    "-DAPK=${AB_REFERENCE_APK}"
                    "-DPIXELS=${AB_REFERENCE_WEBP_PIXELS}"
                    "-DCHECKER=$<TARGET_FILE:ab_webp_tests>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_webp_pixels_reference.cmake")
            add_test(NAME image_webp_color_planes_android_apk
                COMMAND "${CMAKE_COMMAND}"
                    "-DAPK=${AB_REFERENCE_APK}"
                    "-DPIXELS=${AB_REFERENCE_WEBP_PIXELS}"
                    "-DCOLOR=ON"
                    "-DCHECKER=$<TARGET_FILE:ab_webp_color_tests>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_webp_pixels_reference.cmake")
        endif()
        add_test(NAME asset_load_lists_apk
            COMMAND "${CMAKE_COMMAND}"
                "-DAPK=${AB_REFERENCE_APK}"
                "-DCHECKER=$<TARGET_FILE:ab_asset_load_list_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_asset_load_lists_apk.cmake")
        add_test(NAME asset_android_owner_apk
            COMMAND "${CMAKE_COMMAND}"
                "-DAPK=${AB_REFERENCE_APK}"
                "-DCHECKER=$<TARGET_FILE:ab_android_assets_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_android_assets_apk.cmake")
        add_test(NAME asset_sprite_resource_apk
            COMMAND "${CMAKE_COMMAND}"
                "-DAPK=${AB_REFERENCE_APK}"
                "-DCHECKER=$<TARGET_FILE:ab_sprite_resource_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_sprite_resource_apk.cmake")
        add_test(NAME asset_composite_resource_apk
            COMMAND "${CMAKE_COMMAND}"
                "-DAPK=${AB_REFERENCE_APK}"
                "-DCHECKER=$<TARGET_FILE:ab_composite_resource_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_composite_resource_apk.cmake")
        add_test(NAME asset_resources_apk
            COMMAND "${CMAKE_COMMAND}"
                "-DAPK=${AB_REFERENCE_APK}"
                "-DCHECKER=$<TARGET_FILE:ab_asset_resource_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_asset_resources_apk.cmake")
        add_test(NAME asset_theme_resources_apk
            COMMAND "${CMAKE_COMMAND}"
                "-DAPK=${AB_REFERENCE_APK}"
                "-DCHECKER=$<TARGET_FILE:ab_theme_asset_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_theme_asset_apk.cmake")
        add_test(NAME asset_textures_apk
            COMMAND "${CMAKE_COMMAND}"
                "-DAPK=${AB_REFERENCE_APK}"
                "-DCHECKER=$<TARGET_FILE:ab_asset_texture_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_asset_textures_apk.cmake")
        if(AB_REFERENCE_ASSET_RESOURCE_STATE)
            if(AB_REFERENCE_ASSET_IMAGE_STATE)
                add_test(NAME asset_images_live_android_apk
                    COMMAND "${CMAKE_COMMAND}"
                        "-DAPK=${AB_REFERENCE_APK}"
                        "-DREFERENCE=${AB_REFERENCE_ASSET_RESOURCE_STATE}"
                        "-DIMAGES=${AB_REFERENCE_ASSET_IMAGE_STATE}"
                        "-DCHECKER=$<TARGET_FILE:ab_asset_resource_live_tests>"
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_asset_resources_live_reference.cmake")
            endif()
            add_test(NAME asset_resources_live_android_apk
                COMMAND "${CMAKE_COMMAND}"
                    "-DAPK=${AB_REFERENCE_APK}"
                    "-DREFERENCE=${AB_REFERENCE_ASSET_RESOURCE_STATE}"
                    "-DCHECKER=$<TARGET_FILE:ab_asset_resource_live_tests>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_asset_resources_live_reference.cmake")
        endif()
    endif()
    set(AB_REFERENCE_ASSET_PROFILE "" CACHE FILEPATH
        "Optional original Android asset profile/default-size corpus")
    if(AB_REFERENCE_ASSET_PROFILE AND AB_REFERENCE_APK)
        add_test(NAME asset_profile_android_apk
            COMMAND "${CMAKE_COMMAND}"
                "-DREFERENCE=${AB_REFERENCE_ASSET_PROFILE}"
                "-DAPK=${AB_REFERENCE_APK}"
                "-DCHECKER=$<TARGET_FILE:ab_asset_profile_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_asset_profile_reference.cmake")
    endif()
    set(AB_REFERENCE_KEY "" CACHE FILEPATH "Optional private resource key for recovery checks")
    set(AB_REFERENCE_CLASSIC_STARTUP "" CACHE FILEPATH
        "Optional captured native startup timers, dimensions and random state")
    set(AB_REFERENCE_CLASSIC_READY "" CACHE FILEPATH
        "Optional captured post-timer character states and group orders")
    set(AB_REFERENCE_CLASSIC_SPRITES "" CACHE FILEPATH
        "Optional captured initial damage/blink/smile object fields")
    set(AB_REFERENCE_SPRITE_BINDINGS "" CACHE FILEPATH
        "Optional captured native ordinary object sprite bindings")
    set(AB_REFERENCE_CLASSIC_SCENE "" CACHE FILEPATH
        "Optional captured native level-object renderer order and bindings")
    set(AB_REFERENCE_CLASSIC_POST "" CACHE FILEPATH
        "Optional captured native post-physics object passes")
    set(AB_REFERENCE_CAMERA_BOOTSTRAP "" CACHE FILEPATH
        "Optional captured level-one camera snaps, dimensions and limit fields")
    if(AB_REFERENCE_APK)
        if(AB_REFERENCE_THEME_GPU_NATIVE AND AB_REFERENCE_THEME_GPU_PNG AND
           AB_REFERENCE_THEME_GPU_GLES2 AND AB_REFERENCE_THEME_GPU_INPUT)
            add_test(NAME theme_gpu_android_apk
                COMMAND "${CMAKE_COMMAND}"
                    "-DAPK=${AB_REFERENCE_APK}"
                    "-DNATIVE=${AB_REFERENCE_THEME_GPU_NATIVE}"
                    "-DPNG=${AB_REFERENCE_THEME_GPU_PNG}"
                    "-DGLES2=${AB_REFERENCE_THEME_GPU_GLES2}"
                    "-DINPUT=${AB_REFERENCE_THEME_GPU_INPUT}"
                    "-DCHECKER=$<TARGET_FILE:ab_theme_gpu_tests>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_theme_gpu_reference.cmake")
        endif()
        if(AB_REFERENCE_THEME_GPU_NATIVE AND AB_REFERENCE_THEME_GPU_PNG AND
           AB_REFERENCE_SLING_GPU_GLES2 AND AB_REFERENCE_SLING_GPU_INPUT)
            add_test(NAME slingshot_gpu_android_apk
                COMMAND "${CMAKE_COMMAND}"
                    -DSLING=ON
                    "-DAPK=${AB_REFERENCE_APK}"
                    "-DNATIVE=${AB_REFERENCE_THEME_GPU_NATIVE}"
                    "-DPNG=${AB_REFERENCE_THEME_GPU_PNG}"
                    "-DGLES2=${AB_REFERENCE_SLING_GPU_GLES2}"
                    "-DINPUT=${AB_REFERENCE_SLING_GPU_INPUT}"
                    "-DCHECKER=$<TARGET_FILE:ab_theme_gpu_tests>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_theme_gpu_reference.cmake")
        endif()
        if(AB_REFERENCE_THEME_GPU_NATIVE AND AB_REFERENCE_THEME_GPU_PNG AND
           AB_REFERENCE_HUD_GPU_GLES2 AND AB_REFERENCE_HUD_GPU_INPUT)
            add_test(NAME game_hud_gpu_android_apk
                COMMAND "${CMAKE_COMMAND}"
                    -DHUD=ON
                    "-DAPK=${AB_REFERENCE_APK}"
                    "-DNATIVE=${AB_REFERENCE_THEME_GPU_NATIVE}"
                    "-DPNG=${AB_REFERENCE_THEME_GPU_PNG}"
                    "-DGLES2=${AB_REFERENCE_HUD_GPU_GLES2}"
                    "-DINPUT=${AB_REFERENCE_HUD_GPU_INPUT}"
                    "-DCHECKER=$<TARGET_FILE:ab_theme_gpu_tests>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_theme_gpu_reference.cmake")
        endif()
        if(AB_REFERENCE_THEME_GPU_NATIVE AND AB_REFERENCE_THEME_GPU_PNG AND
           AB_REFERENCE_IDLE_GPU_GLES2 AND AB_REFERENCE_IDLE_GPU_INPUT)
            add_test(NAME idle_frame_gpu_android_apk
                COMMAND "${CMAKE_COMMAND}"
                    -DIDLE=ON
                    "-DAPK=${AB_REFERENCE_APK}"
                    "-DNATIVE=${AB_REFERENCE_THEME_GPU_NATIVE}"
                    "-DPNG=${AB_REFERENCE_THEME_GPU_PNG}"
                    "-DGLES2=${AB_REFERENCE_IDLE_GPU_GLES2}"
                    "-DINPUT=${AB_REFERENCE_IDLE_GPU_INPUT}"
                    "-DCHECKER=$<TARGET_FILE:ab_theme_gpu_tests>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_theme_gpu_reference.cmake")
        endif()
        if(AB_REFERENCE_THEME_SCENE_FILL)
            add_test(NAME theme_scene_fill_android_apk
                COMMAND "${CMAKE_COMMAND}"
                    "-DAPK=${AB_REFERENCE_APK}"
                    "-DREFERENCE=${AB_REFERENCE_THEME_SCENE_FILL}"
                    "-DCHECKER=$<TARGET_FILE:ab_theme_scene_tests>"
                    -DFILL=ON
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_theme_scene_reference.cmake")
        endif()
        if(AB_REFERENCE_THEME_IMAGE_FORMATS AND AB_REFERENCE_THEME_BINDINGS)
            add_test(NAME theme_image_formats_android_apk
                COMMAND "${CMAKE_COMMAND}"
                    "-DAPK=${AB_REFERENCE_APK}"
                    "-DBINDINGS=${AB_REFERENCE_THEME_BINDINGS}"
                    "-DREFERENCE=${AB_REFERENCE_THEME_IMAGE_FORMATS}"
                    "-DCHECKER=$<TARGET_FILE:ab_theme_bindings_tests>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_theme_image_formats_reference.cmake")
        endif()
        if(AB_REFERENCE_IMAGE_MATERIAL)
            add_test(NAME image_material_android_apk
                COMMAND "${CMAKE_COMMAND}"
                    "-DAPK=${AB_REFERENCE_APK}"
                    "-DREFERENCE=${AB_REFERENCE_IMAGE_MATERIAL}"
                    "-DCHECKER=$<TARGET_FILE:ab_image_material_tests>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_image_material_reference.cmake")
        endif()
        if(AB_REFERENCE_IMAGE_RECTANGLE_MATERIAL)
            add_test(NAME image_rectangle_material_android_apk
                COMMAND "${CMAKE_COMMAND}"
                    "-DAPK=${AB_REFERENCE_APK}"
                    "-DREFERENCE=${AB_REFERENCE_IMAGE_RECTANGLE_MATERIAL}"
                    "-DCHECKER=$<TARGET_FILE:ab_image_material_tests>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_image_rectangle_material_reference.cmake")
        endif()
        if(AB_REFERENCE_THEME_SCENE)
            add_test(NAME theme_scene_android_apk
                COMMAND "${CMAKE_COMMAND}"
                    "-DAPK=${AB_REFERENCE_APK}"
                    "-DREFERENCE=${AB_REFERENCE_THEME_SCENE}"
                    "-DCHECKER=$<TARGET_FILE:ab_theme_scene_tests>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_theme_scene_reference.cmake")
        endif()
        if(AB_REFERENCE_THEME_BINDINGS)
            add_test(NAME theme_bindings_android_apk
                COMMAND "${CMAKE_COMMAND}"
                    "-DAPK=${AB_REFERENCE_APK}"
                    "-DREFERENCE=${AB_REFERENCE_THEME_BINDINGS}"
                    "-DCHECKER=$<TARGET_FILE:ab_theme_bindings_tests>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_theme_bindings_reference.cmake")
        endif()
        add_test(NAME theme_catalog_apk
            COMMAND "${CMAKE_COMMAND}"
                "-DAPK=${AB_REFERENCE_APK}"
                "-DCHECKER=$<TARGET_FILE:ab_theme_catalog_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_theme_catalog.cmake")
        if(AB_REFERENCE_THEME_CATALOG)
            add_test(NAME theme_catalog_android
                COMMAND "${CMAKE_COMMAND}"
                    "-DAPK=${AB_REFERENCE_APK}"
                    "-DREFERENCE=${AB_REFERENCE_THEME_CATALOG}"
                    "-DCHECKER=$<TARGET_FILE:ab_theme_catalog_tests>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_theme_catalog.cmake")
        endif()
        add_test(NAME main_menu_resources_apk
            COMMAND "${CMAKE_COMMAND}"
                "-DAPK=${AB_REFERENCE_APK}"
                "-DCHECKER=$<TARGET_FILE:ab_main_menu_resources_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_main_menu_resources.cmake")
        if(AB_REFERENCE_MAIN_MENU_RESOURCES)
            add_test(NAME main_menu_resources_android
                COMMAND "${CMAKE_COMMAND}"
                    "-DAPK=${AB_REFERENCE_APK}"
                    "-DREFERENCE=${AB_REFERENCE_MAIN_MENU_RESOURCES}"
                    "-DCHECKER=$<TARGET_FILE:ab_main_menu_resources_tests>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_main_menu_resources.cmake")
        endif()
        add_test(NAME classic_loaded_preview_apk
            COMMAND "${CMAKE_COMMAND}"
                "-DAPK=${AB_REFERENCE_APK}"
                "-DCHECKER=$<TARGET_FILE:ab_classic_preview_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_preview.cmake")
        if(AB_REFERENCE_CAMERA_SYSTEM)
            add_test(NAME game_camera_system_android_apk
                COMMAND "${CMAKE_COMMAND}"
                    "-DAPK=${AB_REFERENCE_APK}"
                    "-DREFERENCE=${AB_REFERENCE_CAMERA_SYSTEM}"
                    "-DCHECKER=$<TARGET_FILE:ab_camera_system_tests>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_camera_system_reference.cmake")
        endif()
        if(AB_REFERENCE_CAMERA_TOUCH)
            add_test(NAME game_camera_touch_android_apk
                COMMAND "${CMAKE_COMMAND}"
                    "-DAPK=${AB_REFERENCE_APK}"
                    "-DREFERENCE=${AB_REFERENCE_CAMERA_TOUCH}"
                    "-DCHECKER=$<TARGET_FILE:ab_camera_touch_tests>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_camera_touch_reference.cmake")
        endif()
        if(AB_REFERENCE_CAMERA_TOUCH_FILTERED)
            add_test(NAME game_camera_touch_filtered_android_apk
                COMMAND "${CMAKE_COMMAND}"
                    "-DAPK=${AB_REFERENCE_APK}"
                    "-DREFERENCE=${AB_REFERENCE_CAMERA_TOUCH_FILTERED}"
                    "-DCHECKER=$<TARGET_FILE:ab_camera_touch_tests>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_camera_touch_reference.cmake")
        endif()
        add_test(NAME game_camera_sequences_apk
            COMMAND "${CMAKE_COMMAND}"
                "-DAPK=${AB_REFERENCE_APK}"
                "-DCHECKER=$<TARGET_FILE:ab_camera_sequence_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_camera_sequence_assets.cmake")
        if(AB_REFERENCE_CAMERA_BOOTSTRAP)
            add_test(NAME game_camera_bootstrap_apk
                COMMAND "${CMAKE_COMMAND}"
                    "-DAPK=${AB_REFERENCE_APK}"
                    "-DREFERENCE=${AB_REFERENCE_CAMERA_BOOTSTRAP}"
                    "-DCHECKER=$<TARGET_FILE:ab_camera_bootstrap_tests>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_camera_bootstrap_reference.cmake")
        endif()
        if(AB_REFERENCE_CLASSIC_REMOVED_POST)
            add_test(NAME classic_native_apk_removed_post
                COMMAND "${CMAKE_COMMAND}"
                    "-DAPK=${AB_REFERENCE_APK}"
                    "-DREFERENCE=${AB_REFERENCE_CLASSIC_REMOVED_POST}"
                    "-DCHECKER=$<TARGET_FILE:ab_classic_membership_tests>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_removed_post_reference.cmake")
        endif()
        if(AB_REFERENCE_CLASSIC_SCENE)
            add_test(NAME classic_native_apk_scene
                COMMAND "${CMAKE_COMMAND}"
                    "-DAPK=${AB_REFERENCE_APK}"
                    "-DREFERENCE=${AB_REFERENCE_CLASSIC_SCENE}"
                    "-DCHECKER=$<TARGET_FILE:ab_classic_scene_tests>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_scene_reference.cmake")
            if(AB_REFERENCE_CLASSIC_POST)
                add_test(NAME classic_native_apk_post_physics
                    COMMAND "${CMAKE_COMMAND}"
                        "-DAPK=${AB_REFERENCE_APK}"
                        "-DREFERENCE=${AB_REFERENCE_CLASSIC_SCENE}"
                        "-DPOST_REFERENCE=${AB_REFERENCE_CLASSIC_POST}"
                        "-DCHECKER=$<TARGET_FILE:ab_classic_scene_tests>"
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_scene_reference.cmake")
            endif()
        endif()
        if(AB_REFERENCE_SPRITE_BINDINGS)
            add_test(NAME resource_sprite_bindings_apk
                COMMAND "${CMAKE_COMMAND}"
                    "-DAPK=${AB_REFERENCE_APK}"
                    "-DREFERENCE=${AB_REFERENCE_SPRITE_BINDINGS}"
                    "-DCHECKER=$<TARGET_FILE:ab_sprite_atlas_tests>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_sprite_bindings_reference.cmake")
        endif()
        if(AB_REFERENCE_MP3_FORMAT)
            add_test(NAME resource_mp3_structure_apk
                COMMAND "${CMAKE_COMMAND}"
                    "-DAPK=${AB_REFERENCE_APK}"
                    "-DREFERENCE=${AB_REFERENCE_MP3_FORMAT}"
                    "-DCHECKER=$<TARGET_FILE:ab_mp3_tests>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_mp3_reference.cmake")
        endif()
        add_test(NAME resource_audio_pcm_apk
            COMMAND "${CMAKE_COMMAND}"
                "-DAPK=${AB_REFERENCE_APK}"
                "-DCHECKER=$<TARGET_FILE:ab_audio_pcm_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_audio_pcm_reference.cmake")
        if(AB_REFERENCE_AUDIO_RESOURCES)
            add_test(NAME resource_audio_paths_apk
                COMMAND "${CMAKE_COMMAND}"
                    "-DAPK=${AB_REFERENCE_APK}"
                    "-DREFERENCE=${AB_REFERENCE_AUDIO_RESOURCES}"
                    "-DCHECKER=$<TARGET_FILE:ab_audio_group_tests>"
                    -DRESOURCES=ON
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_audio_catalog_reference.cmake")
        endif()
        if(AB_REFERENCE_AUDIO_ASSETS)
            add_test(NAME resource_audio_assets_apk
                COMMAND "${CMAKE_COMMAND}"
                    "-DAPK=${AB_REFERENCE_APK}"
                    "-DREFERENCE=${AB_REFERENCE_AUDIO_ASSETS}"
                    "-DCHECKER=$<TARGET_FILE:ab_audio_group_tests>"
                    -DASSETS=ON
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_audio_catalog_reference.cmake")
        endif()
        if(AB_REFERENCE_WAVE_PCM16)
            add_test(NAME resource_wave_pcm16_apk
                COMMAND "${CMAKE_COMMAND}"
                    "-DAPK=${AB_REFERENCE_APK}"
                    "-DREFERENCE=${AB_REFERENCE_WAVE_PCM16}"
                    "-DCHECKER=$<TARGET_FILE:ab_wave_tests>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_wave_reference.cmake")
        endif()
        if(AB_REFERENCE_AUDIO_CATALOG_NATIVE)
            add_test(NAME resource_audio_catalog_apk
                COMMAND "${CMAKE_COMMAND}"
                    "-DAPK=${AB_REFERENCE_APK}"
                    "-DREFERENCE=${AB_REFERENCE_AUDIO_CATALOG_NATIVE}"
                    "-DCHECKER=$<TARGET_FILE:ab_audio_group_tests>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_audio_catalog_reference.cmake")
        endif()
        if(AB_REFERENCE_CLASSIC_PHYSICAL_FIELDS)
            add_test(NAME classic_native_apk_physical_fields
                COMMAND "${CMAKE_COMMAND}"
                    "-DAPK=${AB_REFERENCE_APK}"
                    "-DREFERENCE=${AB_REFERENCE_CLASSIC_PHYSICAL_FIELDS}"
                    "-DCHECKER=$<TARGET_FILE:ab_classic_behaviors_tests>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_physical_fields_reference.cmake")
        endif()
        if(AB_REFERENCE_CLASSIC_BEHAVIORS)
            add_test(NAME classic_native_apk_behaviors
                COMMAND "${CMAKE_COMMAND}"
                    "-DAPK=${AB_REFERENCE_APK}"
                    "-DREFERENCE=${AB_REFERENCE_CLASSIC_BEHAVIORS}"
                    "-DCHECKER=$<TARGET_FILE:ab_classic_behaviors_tests>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_behaviors_reference.cmake")
        endif()
        if(AB_REFERENCE_CLASSIC_BODIES)
            if(AB_REFERENCE_CLASSIC_SPRITES)
                add_test(NAME classic_native_apk_initial_sprites
                    COMMAND "${CMAKE_COMMAND}"
                        "-DAPK=${AB_REFERENCE_APK}"
                        "-DBODIES=${AB_REFERENCE_CLASSIC_BODIES}"
                        "-DSPRITES=${AB_REFERENCE_CLASSIC_SPRITES}"
                        "-DCHECKER=$<TARGET_FILE:ab_definition_tests>"
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_apk.cmake")
            endif()
            if(AB_REFERENCE_CLASSIC_READY)
                add_test(NAME classic_native_apk_character_state
                    COMMAND "${CMAKE_COMMAND}"
                        "-DAPK=${AB_REFERENCE_APK}"
                        "-DBODIES=${AB_REFERENCE_CLASSIC_BODIES}"
                        "-DSTARTUP=${AB_REFERENCE_CLASSIC_READY}"
                        "-DCHECKER=$<TARGET_FILE:ab_definition_tests>"
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_apk.cmake")
            endif()
            if(AB_REFERENCE_CLASSIC_STARTUP)
                add_test(NAME classic_native_apk_startup
                    COMMAND "${CMAKE_COMMAND}"
                        "-DAPK=${AB_REFERENCE_APK}"
                        "-DBODIES=${AB_REFERENCE_CLASSIC_BODIES}"
                        "-DSTARTUP=${AB_REFERENCE_CLASSIC_STARTUP}"
                        "-DCHECKER=$<TARGET_FILE:ab_definition_tests>"
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_apk.cmake")
            endif()
            add_test(NAME classic_native_apk_initial_bodies
                COMMAND "${CMAKE_COMMAND}"
                    "-DAPK=${AB_REFERENCE_APK}"
                    "-DBODIES=${AB_REFERENCE_CLASSIC_BODIES}"
                    "-DCHECKER=$<TARGET_FILE:ab_definition_tests>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_apk.cmake")
            if(AB_REFERENCE_CLASSIC_FRAMES)
                add_test(NAME classic_native_apk_level_frames
                    COMMAND "${CMAKE_COMMAND}"
                        "-DAPK=${AB_REFERENCE_APK}"
                        "-DBODIES=${AB_REFERENCE_CLASSIC_BODIES}"
                        "-DFRAMES=${AB_REFERENCE_CLASSIC_FRAMES}"
                        "-DCHECKER=$<TARGET_FILE:ab_definition_tests>"
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_apk.cmake")
                if(AB_REFERENCE_CLASSIC_LEVEL_CAMERA)
                    add_test(NAME classic_native_apk_level_camera
                        COMMAND "${CMAKE_COMMAND}"
                            "-DAPK=${AB_REFERENCE_APK}"
                            "-DBODIES=${AB_REFERENCE_CLASSIC_BODIES}"
                            "-DFRAMES=${AB_REFERENCE_CLASSIC_FRAMES}"
                            "-DCAMERA=${AB_REFERENCE_CLASSIC_LEVEL_CAMERA}"
                            "-DCHECKER=$<TARGET_FILE:ab_definition_tests>"
                            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_apk.cmake")
                endif()
                if(AB_REFERENCE_CLASSIC_LEVEL_CAMERA_COLD_REPEAT)
                    add_test(NAME classic_native_apk_level_camera_cold_repeat
                        COMMAND "${CMAKE_COMMAND}"
                            "-DAPK=${AB_REFERENCE_APK}"
                            "-DBODIES=${AB_REFERENCE_CLASSIC_BODIES}"
                            "-DFRAMES=${AB_REFERENCE_CLASSIC_FRAMES}"
                            "-DCAMERA=${AB_REFERENCE_CLASSIC_LEVEL_CAMERA_COLD_REPEAT}"
                            "-DCHECKER=$<TARGET_FILE:ab_definition_tests>"
                            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_apk.cmake")
                endif()
                if(AB_REFERENCE_CLASSIC_LEVEL_TRAJECTORY)
                    add_test(NAME classic_native_apk_level_trajectory
                        COMMAND "${CMAKE_COMMAND}"
                            "-DAPK=${AB_REFERENCE_APK}"
                            "-DBODIES=${AB_REFERENCE_CLASSIC_BODIES}"
                            "-DFRAMES=${AB_REFERENCE_CLASSIC_FRAMES}"
                            "-DCAMERA=${AB_REFERENCE_CLASSIC_LEVEL_TRAJECTORY}"
                            "-DCHECKER=$<TARGET_FILE:ab_definition_tests>"
                            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_apk.cmake")
                endif()
                if(AB_REFERENCE_CLASSIC_LEVEL_TRAJECTORY_REPEAT)
                    add_test(NAME classic_native_apk_level_trajectory_repeat
                        COMMAND "${CMAKE_COMMAND}"
                            "-DAPK=${AB_REFERENCE_APK}"
                            "-DBODIES=${AB_REFERENCE_CLASSIC_BODIES}"
                            "-DFRAMES=${AB_REFERENCE_CLASSIC_FRAMES}"
                            "-DCAMERA=${AB_REFERENCE_CLASSIC_LEVEL_TRAJECTORY_REPEAT}"
                            "-DCHECKER=$<TARGET_FILE:ab_definition_tests>"
                            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_apk.cmake")
                endif()
                if(AB_REFERENCE_CLASSIC_LEVEL_SCENE)
                    add_test(NAME classic_native_apk_level_scene
                        COMMAND "${CMAKE_COMMAND}"
                            "-DAPK=${AB_REFERENCE_APK}"
                            "-DBODIES=${AB_REFERENCE_CLASSIC_BODIES}"
                            "-DFRAMES=${AB_REFERENCE_CLASSIC_FRAMES}"
                            "-DCAMERA=${AB_REFERENCE_CLASSIC_LEVEL_SCENE}"
                            "-DCHECKER=$<TARGET_FILE:ab_definition_tests>"
                            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_classic_apk.cmake")
                endif()
            endif()
        endif()
        add_test(NAME target_auto_key_level_resource
            COMMAND "${CMAKE_COMMAND}"
                "-DTOOL=$<TARGET_FILE:ab_apk>"
                "-DAPK=${AB_REFERENCE_APK}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_auto_key_resources.cmake")
        add_test(NAME target_scene_preview_input_bounds
            COMMAND "${CMAKE_COMMAND}"
                "-DPREVIEW=$<TARGET_FILE:ab_preview>"
                "-DAPK=${AB_REFERENCE_APK}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_scene_preview_input.cmake")
        add_test(NAME target_apk_integrity COMMAND ab_apk verify "${AB_REFERENCE_APK}")
        add_test(NAME target_apk_manifest COMMAND ab_apk manifest "${AB_REFERENCE_APK}")
        add_test(NAME target_png_pixels COMMAND ab_apk inventory-png "${AB_REFERENCE_APK}")
        add_test(NAME target_pvr_pixels COMMAND ab_apk inventory-pvr "${AB_REFERENCE_APK}")
        add_test(NAME target_sprite_metadata COMMAND ab_apk inventory-sprites "${AB_REFERENCE_APK}")
        add_test(NAME target_font_metadata COMMAND ab_apk inventory-fonts "${AB_REFERENCE_APK}")
        add_test(NAME target_text_metadata COMMAND ab_apk inventory-text "${AB_REFERENCE_APK}")
        add_test(NAME target_composite_metadata COMMAND ab_apk inventory-composites "${AB_REFERENCE_APK}")
        if(AB_REFERENCE_KEY)
            add_test(NAME target_resource_key_from_apk COMMAND ab_target_resource_tests
                "${AB_REFERENCE_APK}" "${AB_REFERENCE_KEY}")
            add_test(NAME target_lua_recovery COMMAND ab_apk inventory-lua
                "${AB_REFERENCE_APK}" "${AB_REFERENCE_KEY}")
            add_test(NAME target_level_constructors COMMAND ab_apk inventory-levels
                "${AB_REFERENCE_APK}" "${AB_REFERENCE_KEY}")
        endif()
    endif()
endif()

find_program(AB_CLANG_FORMAT NAMES clang-format clang-format-19
    HINTS /opt/local/libexec/llvm-19/bin)
if(AB_CLANG_FORMAT)
    file(GLOB_RECURSE AB_FORMAT_FILES CONFIGURE_DEPENDS
        include/*.h src/*.h src/*.c tools/*.h tools/*.c tests/*.h tests/*.c)
    add_custom_target(ab-check-format
        COMMAND "${AB_CLANG_FORMAT}" --dry-run --Werror ${AB_FORMAT_FILES}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        VERBATIM)
endif()

set(AB_RUNTIME_DESTINATION "Files/build")
if(WIN32)
    set(AB_RUNTIME_DESTINATION "Files/build/Release")
endif()
install(TARGETS ab_apk ab_lua_data ab_lua_inspect ab_save_data ab_preview
    RUNTIME DESTINATION "${AB_RUNTIME_DESTINATION}" COMPONENT Runtime
    BUNDLE DESTINATION "Files/build" COMPONENT Runtime)
if(APPLE)
    install(PROGRAMS tools/launch-preview.sh DESTINATION "Files/build"
        RENAME angrybirds-preview COMPONENT Runtime)
endif()
install(FILES README.md DESTINATION "." COMPONENT Runtime)
install(FILES Common/LICENSE DESTINATION "." RENAME LICENSE.Console_Common
    COMPONENT Runtime)
