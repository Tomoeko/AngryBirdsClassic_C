# Android reference environment

This environment runs the unmodified, pinned Android 5.2.0 APK for private
research. It supplies native observations and comparison traces; it is separate
from the first-party runtime. No SDK implementation source or game binary is
committed, and configuration/builds never download SDK packages.

All tools, images, AVD state, captures, and logs are retained under
`Files/AngryBirdsC/android-reference/`. The user had no reference device or prior
captures available. The local reference installation uses official binary SDK
packages from the [Android repository](https://developer.android.com/studio)
and the [emulator archive](https://developer.android.com/studio/emulator_archive).
Private package manifests retain download URLs, sizes, and hashes.

## Recorded profile

| Component | Configuration |
| --- | --- |
| Host | macOS on Apple Silicon |
| Emulator | 32.1.15, build 10696886, `darwin_aarch64` ARM headless engine |
| System image | Google APIs Android 5.1.1, API 22, `armeabi-v7a`, revision 26 |
| Kernel override | Official API 23 ARM revision 33 `kernel-ranchu` binary |
| Kernel SHA-256 | `40eaa855293648f87b67566ff4eb0cba8d32736a3ee56d0cf5fb5f07e9a774ef` |
| Platform tools | 37.0.1 |
| CPU and memory | Cortex-A15, one emulated core, 1,024 MiB guest RAM |
| Display | Physical 320 x 480, density 160; landscape logical 480 x 320 |
| Graphics | SwiftShader indirect, core GLES2; Vulkan disabled |
| Audio | Silent `none` host backend with initialized Android primary output; Android master output muted at user request; guest microphone input disabled |
| State | Private AVD `AB520`; snapshots disabled; no Google account |
| Connectivity | Airplane mode enabled; guest Wi-Fi and mobile data disabled |

The API 22 image's own kernel has `CONFIG_GOLDFISH_AUDIO` disabled. It boots but
does not expose `/dev/eac`; Android has no audio output, and the target aborts
when it plays an uninitialized `AudioTrack`. The verified API 23 kernel enables
that driver and exposes `/dev/eac`; Android reports a primary output and the
target reaches its menus. The system image and APK remain unchanged. This
kernel override is part of the reference configuration, not a stock API 22 AVD
claim. Kernel configurations and the failed startup logs are retained privately.

The tested ARM engine is launched directly. Its process-local library search
path includes the emulator's `lib64/gles_swiftshader`, `lib64`, and Qt library
directory. The ordinary launcher rejects this ARM configuration. Emulator
37.2.12 was also tested and replaced after compatibility failures; its package
manifest and failure logs remain as evidence.

ADB uses a dedicated server on port 5041 and this AVD's port pair 5580/5581.
Reconnect explicitly to `127.0.0.1:5581` after restarting the emulator; an existing
ADB server may otherwise retain no device entry. Scope every command to the
selected serial. No other devices are part of this reference work.

Keep emulator master output muted while working. On this pinned Android 5.1.1
image, `service call audio 11 i32 1 i32 0 s16 com.android.shell i32 0` sets master
mute. `service call audio 12` returns a successful parcel followed by boolean
`00000001` when master mute is enabled. These transaction numbers are specific
to the pinned image's [audio service interface](https://raw.githubusercontent.com/aosp-mirror/platform_frameworks_base/android-5.1.1_r38/media/java/android/media/IAudioService.aidl).
Verify master mute after restarts and reapply if necessary. This keeps system
sounds and game audio quiet without removing the output device.

The current headless engine uses `-audio none` and `QEMU_AUDIO_DRV=none`.
With the kernel override, `/dev/eac` and the primary 44,100 Hz stereo PCM16
output remain available; the unmodified APK reaches its main menu and master
mute still reports true. This configuration replaces CoreAudio after repeated
host audio-DMA/PFLASH failures during boot or app restart. The private
`silent-backend-audio-check.json`, startup log and menu capture retain the
verification. These observations apply to the pinned emulator build, rather
than assuming identical behavior from every emulator's audio switches.
Cold app restart can still make this emulator unresponsive; a later host sample
again shows goldfish audio/PFLASH paths even with the silent backend. Restart the
isolated guest for a fresh capture when this occurs. The backend keeps output
silent but does not establish a fix for that emulator failure.

Earlier captures also muted the media stream. Android's volume-mute key 164
toggles that state without removing the audio device; `dumpsys audio`
reports `STREAM_MUSIC` mute count. Verify after restarts and
reapply only when it is unmuted, since that key toggles the state. Do not replace
this with `-no-audio`, which breaks the target's `AudioTrack` initialization.

## Captures and observations

A later display profile uses physical 768 x 1024 at density 160, with orientation
1 and a logical 1024 x 768 landscape viewport. After a cold guest restart, the
unmodified pinned APK reaches its splash and main menu at that size. A temporary
system-not-responding dialog was dismissed with Wait during loading. ADB PNG
captures retain the raw portrait scanout; a separate native-tool rotation makes
the inspected menu upright. Android master mute was reapplied and queried true.
This supplies a higher-resolution native menu reference, without establishing
whole-scene or timing parity for the C implementation. Earlier captures below
use the original 480 x 320 logical profile unless stated otherwise.

The unmodified APK has supplied startup, the optional birthdate prompt, the
main menu, episode/level selection, first-level launch/collisions, a level-clear
result, retry, and pause. The recorded first shot clears level 1 with 31,480
points and two stars. After force-stopping and restarting the APK, level 1
retains two stars and level 2 is unlocked. The birthdate prompt was dismissed without
entering personal data. Google Play services is disabled in this AVD because
the system image's service repeatedly crashes; offline service behavior must
therefore be evaluated within that recorded configuration.

Native `screencap` files preserve physical 320 x 480 pixels. `dumpsys display`
records landscape orientation 1 and a 480 x 320 logical viewport. Derived
`*-display.png` files apply only a lossless minus-90-degree pixel rotation for
inspection; original captures remain intact. Input coordinates are logical
landscape coordinates. Event logs record host timestamps around operations;
they are not frame-indexed or simulation-tick traces.

These initial captures do not establish first-party visual parity. Replay
seeds, frame timing, complete gameplay, abilities, audio, save/reload, results,
and service coverage still need recovery and comparison.

A later paired shot capture retains 24 update frames through early collisions,
four body removals and scoring changes. The compact first twelve frames support
`AB_REFERENCE_CLASSIC_FLIGHT` and exclude impact/destruction frames. Its
311,734-byte trace SHA-256 is
`f49be54c2969c59731b809ccb84fd8e3c9223859e8eb6a3b949a3f6f6431b402`.
The comparison first builds from original static level data, reproduces the
initial transfer frames, settles for 512 bounded steps with the leading bird
held, and verifies non-bird dynamic bodies are asleep. It then recomputes
stretch and applies the measured launch. All 23 body states, ordered contacts
and non-bird fat/tight fixture bounds match 36 flight steps in Debug sanitizers
and Release. Queued-bird poses and initial flight clock are explicit inputs;
all three birds' fat bounds and flight pose histories remain excluded.

`AB_REFERENCE_CLASSIC_IMPACT` uses all 24 frames and adds initial logical scalars,
before/after logical state, body-presence markers and queue body types. Its
718,777-byte trace SHA-256 is
`2ea62620e6aad9628617dca154d2345aec853afac2075b3ab069710836bb6114`.
The replay's 72 steps and 31 C callbacks reproduce four removals and score 9,380.
Queue poses/types, initial clock and logical scalars are inputs; bird fat bounds,
pose histories, general retained removal-table mutation and script/presentation
effects are outside the comparison. No separate native callback trace is claimed.

`AB_REFERENCE_CLASSIC_RELOAD` uses a separate 720,982-byte extended impact trace
with SHA-256
`4d6fe9d928aa48a2baa448a68ef123d712c7b4d4c9fe33106b7d6068bae0308d`.
The initial partial reload timer is an input; later timer/fired/fill/ready/index
values and the next bird's transfer/held poses from frame 9 onward are outputs.
Earlier idle motion and the other queued bird's poses remain supplied captures.

`AB_REFERENCE_CLASSIC_IDLE` uses a 727,300-byte further extension with SHA-256
`9c102c03748ea4601d001532f1b9427827579e66b28ee95712f7a4052fb41b87`.
Both queued birds' poses are generated and compared throughout the 24 frames.
An initial jump phase and three jump draws are inferred boundary inputs; the
original global RNG state/order is not recovered. Native initial logical state,
partial clocks and the impact corpus's exclusions still apply.

A separate native retry capture retains 96 paired update frames through level
completion (frame 37) and remaining-bird bonuses (frames 73/79), ending at
31,380. Its 34,332,301-byte JSON SHA-256 is
`c3ada92b43613e519954f6431be458a79ca21b15d0daf85f99d0e08eb5d1477e`.
It includes full queue jump fields. Normal detach resumed the target, and Android master
mute was verified before and after with the silent host backend retained.
Provenance is `analysis/level-results-native.json`.

`AB_REFERENCE_CLASSIC_RETRY` compares a 1,141,431-byte 38-frame prefix, with
SHA-256 `8273b7392900b7c499aa8f268eca2e05757aea71350adb36f2ecee9ed1035eb9`.
The reference adapter supplies initial native leaf identifiers to the source
constructor/settling pass, then compares 114 steps, body/contact/bounds and
logical damage/score states through score 11,380. Initial logical scalars,
sling/clock values and queue poses/types remain inputs. Completion decisions,
bonuses and the rest of the 96-frame run remain outside this comparison.

`AB_REFERENCE_CLASSIC_RESULTS` checks a 2,766,446-byte extension, SHA-256
`c378c67126bb9d39eca93d0a7ac8eabf6d1578961301f81b5962e31fe4e0aede`.
All 96 frames/288 steps, body/contact/bounds, logical damage/score and ordinary
retirement timers match. The scene projection predicts completion on frame 37,
bird retirement on 42 and bonuses on 73/79 through score 31,380. Queue poses,
retry identifiers, initial logical/clock/sling state and controller/camera
configuration are supplied. Full mode exit, controller effects, pose history,
bird fat bounds and presentation remain open. Provenance is
`analysis/level-results-replay-native.json`.

`AB_REFERENCE_CLASSIC_RESULTS_QUEUE` uses a 2,851,856-byte extension with SHA-256
`c804941071ec9a50a67515ba72da79490ecf8b3659f3261f9314196f0f31ea58`.
Both queued birds' poses, full jump/reload state, history slots and rendered
poses are outputs for all 96 frames. Captured initial queue state/positions,
histories and reload timer remain inputs. The 25 inferred random boundary
inputs include ten valid but unobserved discarded draws. Original global RNG
and full creation/rendering/UI policy remain open. Provenance is
`analysis/level-results-queue-native.json`.


## First-party Android comparison tools

`AB_REFERENCE_FONT_METRICS` checks 4,096 native bitmap-font UTF-32 range
measurements. Build `ab_android_font_metrics`, select `font-metrics` in
`run_android_oracle.cmake`, and use `ab_font_metrics_tests` as checker. Font
spacing, signed glyph dimensions, tree entries and UTF-32 text are controlled
inputs; actual range/getter methods execute through the original font vtable.
No font constructor, graphics context or script execution is needed.

`AB_REFERENCE_FONT_LAYOUT` checks 4,096 native UTF-32 layouts and all glyph
callbacks, including renderer rotation-origin restoration. Build
`ab_android_font_layout`, select `font-layout` in the same runner, and use
`ab_font_layout_tests`. Controlled context-state and custom-glyph callbacks
retain their arguments; native Sprite/Image drawing is outside this corpus.

`AB_REFERENCE_FONT_IMAGE` separately checks the normal font/Sprite/Image
boundary, retaining local rectangle origins/sizes, rotation origins and UVs.
Build `ab_android_font_image`, select `font-image`, and use
`ab_font_layout_tests` as checker. Sprite UVs and Image callbacks are controlled;
native GL transforms, shader selection and final pixels are not exercised.

`AB_REFERENCE_COORDINATES` checks 4,096 original camera conversion sets with
25 output words each. Build `ab_android_coordinates`, select oracle kind
`coordinates` in `run_android_oracle.cmake`, and use `ab_coordinate_tests` as
checker. Camera top-left/zoom fields are controlled inputs; the original point,
delta and scalar methods execute through the native getters. No Lua chunks,
graphics context or complete camera construction is needed.

`AB_REFERENCE_CAMERA_VIEWPORT` checks 4,096 original CameraSystem extent/shake
sets. Build `ab_android_camera_viewport` and run `run_android_camera_viewport.cmake`
with `AB_REFERENCE_CAMERA_VIEWPORT_CHUNKS` selecting the eight private unchanged
prototypes. `AB_REFERENCE_LIVE_CAMERA_VIEWPORT` separately checks four retained
native camera fields from captured Lua inputs. Movement, shake generation and
full rendering remain outside these arithmetic comparisons.

`tools/oracle/android_math.c` samples 4,096 reproducible float angles using the
reference Android C library's actual `sinf` and `cosf`. Its trace records the
FPSCR controls. The recorded platform libraries are private binary research
inputs, with hashes checked by the explicit build script.

`tools/oracle/arm.c`, compiled with `AB_ORACLE_ANDROID`, runs the same geometry,
contact, body-mass, warm-start, velocity-solve, initialization, and single-point
position-correction corpus inside
Android. It loads the pinned library through Android's dynamic loader, with
native library constructors and real platform imports. It does not invoke
`JNI_OnLoad` or start the game. Both endpoint state and fixture/constraint
layouts remain controlled probe inputs; this is not a complete stepped world.
The APK is launched separately for the UI captures.

The recorded Android corpus is 30,441,357 bytes, SHA-256
`039c93d9bd30e7f60fb704dffa3e0e22e69e6ffb0b6b9c8bc657c94e7133f82f`.
Every tested result matches the first-party host implementation. The trace also
contains a 25,669,936-byte prefix matching the isolated ARM corpus byte for byte
when the latter uses captured Android math values. An Android-only extension
compares complete position passes, with state checked after every one of
22,510 iterations across 4,096 cases. The probe sets FPSCR controls to zero; the game's own
floating-point environment remains a separate observation task.

Build the tools offline with installed Clang and LLD and the recorded private
platform library directory. Commands below run from the repository root:

```sh
cmake \
    -DAB_ANDROID_LIBM="$PWD/Files/AngryBirdsC/android-reference/platform-libraries/libm.so" \
    -DAB_ANDROID_MATH_OUTPUT_DIR="$PWD/Files/AngryBirdsC/android-reference" \
    -P cmake/build_android_math.cmake
```

The sibling `libdl.so` must match the recorded Android platform. The script
builds math, geometry, literal-data, island, tree, broadphase, body-setup,
contact-state, fixture, world-solve, distance, and post-solve report probes;
none links original game code
into the executable. With the reference AVD already connected, run the latter
using the explicit, hash-checked deployment script:

```sh
cmake \
    -DAB_ADB="$PWD/Files/AngryBirdsC/android-reference/sdk/platform-tools/adb" \
    -DAB_ADB_SERIAL=127.0.0.1:5581 \
    -DAB_REFERENCE_ARM_LIBRARY="$PWD/Files/AngryBirdsC/target/lib/armeabi-v7a/libAngryBirdsClassic.so" \
    -DAB_ANDROID_ORACLE="$PWD/Files/AngryBirdsC/android-reference/ab_android_oracle" \
    -DAB_ANDROID_TRACE="$PWD/Files/AngryBirdsC/android-reference/android-geometry.txt" \
    -DAB_PHYSICS_CHECKER="$PWD/Files/AngryBirdsC/build/ab_physics_tests" \
    -P cmake/run_android_oracle.cmake
```

The runner writes only the task's guest directory `/data/local/tmp/ab520` and
private host traces. It recognizes the old Android linker's specific PIE
diagnostic and checks the resulting corpus without hiding other startup
diagnostics. Successful runs retain the normalized corpus, the exact startup
prefix, and the raw-output hash. The original byte stream can be reconstructed
by concatenating the prefix and corpus; duplicate full traces are removed.
Failed runs retain raw output for diagnosis.

Additional probe kinds use the same runner, with `AB_ANDROID_ORACLE_KIND` and
the corresponding executable/checker selected explicitly:

| Kind | Probe | Host checker |
| --- | --- | --- |
| `free-island` | `ab_android_island` | `ab_step_free_tests` |
| `contact-island` | `ab_android_contact_island` | `ab_step_free_tests` |
| `tree` | `ab_android_tree` | `ab_tree_tests` |
| `broadphase` | `ab_android_broadphase` | `ab_broadphase_tests` |
| `body-setup` | `ab_android_body_setup` | `ab_body_setup_tests` |
| `body-velocity` | `ab_android_body_velocity` | `ab_body_velocity_tests` |
| `contact-state` | `ab_android_contact_state` | `ab_contact_state_tests` |
| `fixture` | `ab_android_fixture` | `ab_fixture_tests` |
| `world-solve` | `ab_android_world_solve` | `ab_step_free_tests` |
| `distance` | `ab_android_distance` | `ab_distance_tests` |
| `contact-report` | `ab_android_contact_report` | `ab_step_free_tests` |
| `world-report` | `ab_android_world_report` | `ab_step_free_tests` |
| `world-steps` | `ab_android_world_steps` | `ab_world_tests` |
| `world-events` | `ab_android_world_events` | `ab_world_tests` |
| `world-continuous` | `ab_android_world_continuous` | `ab_world_tests` |
| `world-continuous-events` | `ab_android_world_continuous_events` | `ab_world_tests` |
| `world-rejected-impacts` | `ab_android_world_rejected_impacts` | `ab_world_tests` |
| `world-operations` | `ab_android_world_operations` | `ab_world_tests` |
| `world-lifecycle` | `ab_android_world_lifecycle` | `ab_world_tests` |
| `separation` | `ab_android_separation` | `ab_separation_tests` |
| `impact` | `ab_android_impact` | `ab_impact_tests` |
| `toi-position` | `ab_android_toi_position` | `ab_physics_tests --toi-position` |
| `toi-island` | `ab_android_toi_island` | `ab_step_free_tests` |
| `game-collision` | `ab_android_game_collision` | `ab_game_collision_tests` |
| `game-rules` | `ab_android_game_rules` | `ab_game_collision_tests` |
| `game-angle` | `ab_android_game_angle` | `ab_definition_tests --angles` |
| `block-damage` | `ab_android_block_damage` | `ab_damage_tests` |
| `bird-damage` | `ab_android_bird_damage` | `ab_damage_tests` |
| `bird-route` | `ab_android_bird_route` | `ab_damage_tests` |
| `slingshot` | `ab_android_slingshot` | `ab_slingshot_tests` |
| `sling-transfer` | `ab_android_sling_transfer` | `ab_slingshot_tests` |
| `sling-start` | `ab_android_sling_start` | `ab_slingshot_tests` |
| `string-order` | `ab_android_string_order` | `ab_data_order_tests` |
| `string-growth` | `ab_android_string_growth` | `ab_data_order_tests` |

Each kind has a checked completion marker because this Android version's ADB
does not reliably propagate guest process failure status. Their optional CTest
inputs are `AB_REFERENCE_FREE_ISLAND`, `AB_REFERENCE_CONTACT_ISLAND`,
`AB_REFERENCE_TREE`, `AB_REFERENCE_BROADPHASE`, `AB_REFERENCE_BODY_SETUP`, and
`AB_REFERENCE_CONTACT_STATE`, `AB_REFERENCE_FIXTURE`, `AB_REFERENCE_WORLD_SOLVE`,
`AB_REFERENCE_DISTANCE`, `AB_REFERENCE_CONTACT_REPORT`, and
`AB_REFERENCE_WORLD_REPORT`, `AB_REFERENCE_WORLD_STEPS`, `AB_REFERENCE_WORLD_EVENTS`,
`AB_REFERENCE_SEPARATION`, `AB_REFERENCE_IMPACT`, `AB_REFERENCE_TOI_POSITION`, and
`AB_REFERENCE_TOI_ISLAND`, `AB_REFERENCE_WORLD_CONTINUOUS`, and
`AB_REFERENCE_WORLD_CONTINUOUS_EVENTS`, `AB_REFERENCE_WORLD_REJECTED_IMPACTS`, and
`AB_REFERENCE_WORLD_OPERATIONS`, `AB_REFERENCE_WORLD_LIFECYCLE`,
`AB_REFERENCE_GAME_COLLISION`, and
`AB_REFERENCE_GAME_RULES`, `AB_REFERENCE_GAME_ANGLE`, `AB_REFERENCE_BLOCK_DAMAGE`,
`AB_REFERENCE_BIRD_DAMAGE`, `AB_REFERENCE_BIRD_ROUTE`,
and `AB_REFERENCE_STRING_ORDER`, `AB_REFERENCE_STRING_GROWTH`;
each checks a pinned corpus
hash before comparing results. Original library execution remains private
reference work, separate from first-party runtime dependencies.

`AB_REFERENCE_BODY_VELOCITY` checks direct native GameLua velocity/impulse calls
using a controlled one-entry object map and original Lua argument conversion.
Its 811,077-byte corpus SHA-256 is
`af87bcdfdda30cfb6d1227b4fdda46e4f3ce9f29036f4b7f63ada16302573e80`.
It includes subnormal wake thresholds and 32 signed-zero controls; full launch
events and gameplay integration are separate.

`AB_REFERENCE_BODY_ANGULAR` checks 4,096 additional native angular velocity
assignments and 1,072 wakes; its corpus hash is
`dc93f7a2f1f5ea753f4f2e15249a64500ba4bdcd2f37f00a3ad0f7b92063b47e`.
`AB_REFERENCE_FRAME_CLOCK` checks timer/remainder bits in 108 complete original
updates. Its 6,836-byte corpus hash is
`2d23f804a349987b79e3836c07bdba8c8038fc0de092fb2869e6c16a1c09cec5`.
`AB_REFERENCE_ANGLE_COMPONENTS` checks 65,536 direct Android atan2f results; its
corpus hash is
`de3be0f18348145164b6b28867148eefd55268a0e24920e5524a9f62df55b79d`.
The common runner kinds are `body-angular` and `angle-components`; the frame
clock uses paired debugger captures of actual GameLua updates.

`AB_REFERENCE_SCRIPT_ANGLE` checks the separate double atan2 promotion/rounding
sequence used by Lua. Its 65,536-case corpus hash is
`3e6c4225df971210c9a320b6647d6d11637dbdd90ddd8ed685c9c23bffa1bbcc`;
the common runner kind is `script-angle`.
`AB_REFERENCE_SLING_DRAG` checks 4,096 original default stretch passes, including
their vertical constraint, normalized stretch/state and 4,471 audio boundaries.
Its corpus hash is
`7e1cc832960b121e5a02bc874e20862acf6703e1fce96c15f66b851db1021e1c`.
Use the slingshot wrapper with kind `sling-drag` to pin and stage its four original
chunks; original code executes only in the private Android reference process.

`AB_REFERENCE_SLING_IMPULSE` checks 4,096 original launchBird calls through the
launchBirdWithImpulse boundary. Use wrapper kind `sling-impulse`; its corpus hash is
`1b172a35f25d094a28ad2d88c1778a739c63a12f7c06a5f8367144e810bfb630`.
This verifies actual impulse arithmetic and default/explicit vectors, with
statistics eligibility suppressed and later launch side effects outside scope.

`AB_REFERENCE_BIRD_SHOT` checks the following launchBirdWithImpulse state
transition: 4,096 cases and 59,392 ordered host boundaries, including optional
hooks, last-bird handling, sprite/audio requests and callback vector/mass changes.
Use wrapper kind `bird-shot`; the 13,731,901-byte corpus SHA-256 is
`ce78d933dc359b096b12b8db2dee7baddd232aaa31841a4fb4b933b84a17cad2`.
`AB_REFERENCE_LIVE_SHOT` checks one real first-level swipe's impulse and resulting
body velocity. Its compact trace SHA-256 is
`eebe91cb537840a48484902ac677b2639b2ce5a700ec7e658a6563d10e9373b1`.
The private paired GameLua::applyImpulse capture retains all selected scene
fields; only the first bird's two linear velocity components changed. Debugger
stops affect wall clock, so this does not establish an ordinary full-shot replay.

`AB_REFERENCE_SLING_ROUTING` checks release threshold and backwards-statistic
routing in another 4,096 original calls: 1,537 launches, 2,559 cancellations and
577 backwards increments before unconditional total-shot updates. It covers
strict equality, signed zero, infinities and NaNs, plus nil/false/true/numeric
zero/string eagle-bait and special-mode fields. Wrapper kind `sling-routing`
retains the original launch arithmetic and release/threshold methods; cancel
and launch endpoints are controlled boundaries. The 1,032,261-byte trace SHA-256
is `46e4182c06d5184f83f3946219b704d3e5034896514595409b46dcb9de89f0d0`.

`AB_REFERENCE_SLING_CANCEL` checks another 4,096 original cancellations and
band resets, including 2,048 position boundaries and 4,096 laser-sound queries.
Wrapper kind `sling-cancel` retains original cancel/reset/getter methods with
controlled root-vector/scalar state and position/audio boundaries. Its trace
SHA-256 is `cfab3cfa31eb852986195f9c158c66d85c843a8b2df179e0ff584b1cd1199b46`.

`AB_REFERENCE_SLING_RELOAD` checks 4,096 reload timer transitions through the
original default transfer function. Build `ab_android_sling_reload` and select
wrapper kind `sling-reload`. It records next-bird lookup and conditional arc
boundaries, with a retained original launch-cue upvalue seeded per case. Its
trace SHA-256 is
`00e34f6f04d86b40a8a88d5be1dc680cb70d867bd289c371b7eb4e5c401cf7bb`.

`AB_REFERENCE_GAME_CONTACT` checks deferred-velocity selection through 4,096
original native calls, as described below. `AB_REFERENCE_SCORE` checks 4,096
break/damage pairs from the unchanged selected gamelogic functions. To capture
them, build `ab_android_score`, supply `AB_REFERENCE_SCORE_CHUNKS` pointing to
the private selected-prototype directory, and run `run_android_score.cmake`
with the usual Android oracle parameters. The runner pins both prototypes,
executes them in the original native Lua implementation and verifies the
completion marker. Runtime scoring uses first-party C. The trace SHA-256 is
`03b709140edfa2d8a28669552092e905abf3dd613a65c273b417bba0ba64faba`.

Completion predicates and hook ordering have a separate
`AB_REFERENCE_LEVEL_FLOW` corpus: build `ab_android_level_flow`
and run `run_android_level_flow.cmake` with `AB_REFERENCE_LEVEL_FLOW_CHUNKS`.
For the bonus prefix, build `ab_android_level_bonus` and run
`run_android_level_bonus.cmake` with `AB_REFERENCE_LEVEL_BONUS_CHUNKS`.
`AB_REFERENCE_LEVEL_BONUS` checks 4,096 controlled original calls;
`AB_REFERENCE_LIVE_BONUS` checks the later captured score cadence with explicit
completion/queue inputs. All selected original chunks stay under private Files.

`AB_REFERENCE_LEVEL_ENDING` checks another 4,096 original ending-delay passes
and ordered win-query/event requests. Build `ab_android_level_ending` and run
`run_android_level_ending.cmake` with `AB_REFERENCE_LEVEL_ENDING_CHUNKS`. It
uses the same unchanged updateLevelEnding prototype, with the bonus timer
held positive and boolean completion/win results. Host callbacks are nonmutating.

`AB_REFERENCE_LEVEL_COMPLETE` checks 4,096 original completion-initialization
transitions and 25,598 ordered boundary snapshots, including mutable callbacks.
Build `ab_android_level_complete` and run `run_android_level_complete.cmake`
with `AB_REFERENCE_LEVEL_COMPLETE_CHUNKS`. The unchanged selected prototype's
fresh closed slingshot upvalue is seeded through checked ARM32 fields because
its stripped debug names prevent the original public setter from addressing it.
The controlled table is rooted globally; no instruction or prototype is changed.

`AB_REFERENCE_BIRD_RETIREMENT` checks 4,096 ordinary removal cases. Build
`ab_android_bird_retirement` and run `run_android_bird_retirement.cmake` with
`AB_REFERENCE_BIRD_RETIREMENT_CHUNKS` selecting the private unchanged MuseumMode
prototype. Specialty branches are held inactive; squared speed, definition and
camera lookup results are controlled, and destruction is recorded as a boundary.

`AB_REFERENCE_BIRD_JUMP` checks 4,096 original idle-jump updates with recorded
random/position/rotation boundaries. Build `ab_android_bird_jump` and run
`run_android_bird_jump.cmake` with `AB_REFERENCE_BIRD_JUMP_CHUNK` pointing to the
private unchanged selected object-animation prototype, plus the usual oracle
parameters. It pins the 2,905-byte prototype, controls only one idle bird and
the boundary calls, and leaves unrelated animation paths inactive. The
1,359,349-byte trace SHA-256 is
`16f14516f96718f91261dfaff1c64c4055e893dbaf41e667d3b34b5a7bdc323c`.

The contact comparison covers 4,096
original native basicBeginContact calls. The isolated oracle uses original
LuaState/LuaObject/LuaTable constructors and cleanup, controlled native object/
fixture/map records, and one C collision callback. It records 3,600 callbacks,
3,592 proposals and 2,441 stored-vector changes; guards, geometry and disable
fields are checked reference boundaries rather than a complete C bridge.
Runner kind `game-contact` requires only the pinned library. The trace SHA-256
is `98c43d4829877de592c63c4519bd917807c416b9b21b7cde2e3a9e09aa4d6672`.

The string-growth corpus checks 4,096 tables and 39,030 unique string entries,
including native hash capacities and rehash traversal. Its SHA-256 is
`742cad34d7d0fbed567a448a707935599496b12ba812fc785c32c6c6bdb157e2`.
`AB_REFERENCE_CREATED_LEVEL_ORDER` with `AB_REFERENCE_LEVEL_ONE` additionally
checks the 23-name runtime first-level table against a selected live memory
capture; this is separate from its constructor order.

`AB_REFERENCE_CLASSIC_STEPS` with `AB_REFERENCE_CLASSIC_BLOCKS`,
`AB_REFERENCE_LEVEL_ONE` and `AB_REFERENCE_CLASSIC_BODIES` checks eight complete
fresh-load physics steps. The normalized 201,733-byte trace SHA-256 is
`6beb03233c721592de073cd83247b2efc0f4151a87e1745f77512912ebea1565`.
It supplies observed sling poses as external transforms, excludes that bird's
fat proxy, and checks all 23 body states and ordered contact data. Full paired
snapshots and normalization provenance remain in private analysis storage.

`AB_REFERENCE_CLASSIC_FRAMES` uses the same three supporting inputs for twelve
complete update frames through sling transfer and holding. The 321,931-byte
normalized trace hash is
`5655026c5480f320b33c428fa35f1e1df73bef064b31db2614cc91c67ad3c082`.
Clock arithmetic, 26 physics steps, signed body-angle synchronization and authored
transfer/hold code reproduce all 23 body states, ordered contacts, every tight/fat
bound, both history slots and interpolated poses. Only the initial partial timer,
sling vectors and per-frame physics
enable/delta are supplied; full queue/callback/presentation integration remains open.

For `block-damage`, use `cmake/run_android_block_damage.cmake` with the same
runner arguments plus `AB_REFERENCE_COLLISION_CHUNKS`, the private directory
containing recovered `class.lua`, `vector2.lua` and `museum-collisions.lua`.
The wrapper checks each target chunk hash before staging those three files and
invoking the common runner. Original Lua executes the unmodified legacy helper;
controlled C bindings provide object metadata and record boundary calls.
The 1,916,995-byte corpus SHA-256 is
`c724ef5f0a2024cded7456baef7c6d4d90227d2cf5026e35818c5082ea7586ab`.
It compares 4,096 cases, all strength/dead-block outputs and 12,288 boundary
calls. The stripped chunk has no debug upvalue names: the probe reads the
measured closure/upvalue ABI and copies the selected original function value
into a native pushnil-reserved stack slot. It changes no original prototype or
closure. Actual callback effects, bird paths and full game routing remain open.

The same wrapper accepts `AB_ANDROID_ORACLE_KIND=bird-damage` with
`ab_android_bird_damage`. It executes the original bird helper with eight
controlled arguments and records strength/dead-block state, contact-disabling
return values, deferred velocity data and ordered boundary calls. All 4,096
cases match, including 642 immediate and 633 deferred velocities. The corpus
SHA-256 is `1dfa7956f108ee54425c6a874cea4842121936b970bc92b6ae3b1380384b4af2`.
That helper corpus alone does not establish outer routing or live integration.

`AB_ANDROID_ORACLE_KIND=bird-route` with `ab_android_bird_route` uses the same
wrapper and calls the original enclosing `MuseumCollisions.onCollision`.
It checks both single-bird object orders and bird/eagle pairs, including ties,
6,493 boundary calls and their argument types. Its 1,705,644-byte corpus SHA-256
is `997e304c554bbca13edd7a203821ae4d786a792dbbeeeeb6b1f513023458646e`.
This establishes the primary routing and measured argument mapping; glue,
material selection and actual callback effects remain separate tasks.

For `slingshot`, `sling-transfer` and `sling-start`, use `cmake/run_android_slingshot.cmake`
and `AB_REFERENCE_SLINGSHOT_CHUNKS`, containing the four hash-pinned original
`class.lua`, `vector2.lua`, `utils.lua` and `slingshot.lua` chunks. The original
method and distance helper execute only inside the oracle. Controlled getters
supply sling properties. Transfer initialization is bypassed with explicit
state; transform/type boundaries are recorded, while unrelated camera/audio/
event effects are stubbed. Optional CTest inputs are `AB_REFERENCE_SLINGSHOT`
and `AB_REFERENCE_SLING_TRANSFER`, `AB_REFERENCE_SLING_START`. The initialization
oracle invokes original level reset and transfer setup, with unrelated hooks
stubbed; it checks saved/current Y, jump state, phase/height and 8,192 transforms.
Its corpus SHA-256 is
`d345da9a720939fb1a4746b70c93d9da7717c73ff35ed3197329b1e3ff1c7ff4`.
The 479,293-byte launch corpus SHA-256 is
`15507639665ac5964d10fa7132768e45f7a34b921c7d98170cc72399532d1a4d`;
the 852,623-byte transfer corpus SHA-256 is
`f8a15e9b5ade1c5905303f9bc76aca45b1db07b74e9dde895a107298fbbd8775`.

The lifecycle corpus contains 16 scenes with 256 operation/step pairs each.
It repeatedly creates and removes bodies and compound fixtures, reuses freed
slots/proxies, and checks both immediate states and following continuous steps.
All 512 body creations/removals, 768 fixture creations, 192 explicit fixture
removals, 10,460 collision callbacks and 576 implicit fixture notifications
match. The 15,393,315-byte corpus SHA-256 is
`02eab3d0d45122791281c80108f8f15566c01f6e7896eac5f5a4166e7af48c6d`.
Native analysis addresses are world creation `0x8e05ac`, body destruction
`0x8e08a8`, fixture creation `0x8deab0`, and fixture destruction `0x8debd8`;
subtract `0x10000` for ELF virtual addresses. The oracle's explicit body island
index initialization excludes uninitialized allocator bytes. Active state,
list links normalized to logical IDs, fixture fields/proxies, contacts, cached
impact fields and callback payloads are compared. Joints remain unsupported.

The optional isolated nonzero-angle CTest accepts the captured math table:

```sh
cmake -S . -B Files/AngryBirdsC/build \
    -DAB_REFERENCE_ARM_MATH="$PWD/Files/AngryBirdsC/android-reference/math-native.bin"
```

This adds `physics_native_arm_android_math` alongside the zero-angle comparison
when the pinned ARM library is configured. The table is reference input only;
it is never a runtime trigonometric implementation. Native host `sinf`/`cosf`
and double-based alternatives each differ from Android on some sampled values.
The first-party `ab_numeric` rotation module now matches the baseline corpus
and all 65,536 extended samples. It uses authored binary64 polynomial/range
calculations and returns binary32 results without platform sine/cosine calls.
Its explicit supported domain is finite angles with magnitude at most
`0x1.921fb4p+28` radians. Larger arguments fail without changing output;
floating-point status flags are outside this numeric-result contract. Complete
position passes use this module. Larger-angle reduction remains recovery work.
