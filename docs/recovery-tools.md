# Angry Birds Classic C recovery tools

A first-party C reimplementation of Angry Birds Android 5.2.0 is in progress.
The sole reference input is:

```text
Files/com.rovio.angrybirds_5.2.0-12200520.apk
```

`Files/` is relative to the repository root.
The APK remains a private, unchanged research input. Its hash and the current
verification status are recorded in [ROADMAP.md](../ROADMAP.md).

The APK's SHA-256 is
`b524b6b4b0ad999f46828546dee1bb047d6e00f724285a2e8c9f4b34a8bee7ad`.
The renamed input has been checked against that hash. Its manifest confirms
package `com.rovio.angrybirds`, version `5.2.0`, version code `12200520`, minimum
API `9`, and target API `22`. Native libraries are present for `armeabi` and
`armeabi-v7a`; initial native analysis uses the latter.

The deprecated implementation has been deleted. The current C11 resource tools
read ZIP entries with first-party DEFLATE, validate CRC-32 and SHA-256, read
Android binary manifests, and recover encrypted Lua resources using first-party
AES-CBC, LZMA, and a bounded single-file 7z reader. All 1,529 Lua resources
(12,988,374 decoded bytes) match independent recovery hashes. The pinned APK
passes structural and integrity checks
for all 3,481 entries, totaling 236,017,954 decoded bytes. This validates resource
access. Native startup, menu, first-level completion, and restart/progression
captures are retained in an isolated Android reference environment. There is no
playable first-party runtime or verified gameplay parity yet; wider captures
and complete comparisons remain outstanding.

Run these commands from the repository root with an installed C11 compiler,
CMake, and Ninja. Configuration and builds do not download dependencies.

```sh
cmake -S . -B Files/build -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug -DAB_SANITIZE=ON \
    -DAB_REFERENCE_APK="$PWD/Files/com.rovio.angrybirds_5.2.0-12200520.apk"
cmake --build Files/build
ctest --test-dir Files/build --output-on-failure
```

`AB_SANITIZE` enables address and undefined-behavior diagnostics on Clang/GCC.
The optional `AB_REFERENCE_APK` setting enables checks against the private APK;
synthetic format and malformed-input tests also run without it.

```sh
Files/build/ab_apk verify \
    Files/com.rovio.angrybirds_5.2.0-12200520.apk
Files/build/ab_apk manifest \
    Files/com.rovio.angrybirds_5.2.0-12200520.apk
Files/build/ab_apk inventory \
    Files/com.rovio.angrybirds_5.2.0-12200520.apk \
    > Files/AngryBirdsC/apk-inventory-c.json
```

`verify` requires the pinned APK hash and checks every entry's structure,
decompression, and CRC. `inventory` emits entry paths, sizes, compression methods,
CRC values, and decoded SHA-256 values. These archive commands preserve scripts
as stored; encrypted-script recovery uses the separate commands below.
`extract APK ENTRY NEW_OUTPUT_FILE` extracts one entry to an explicit new path,
checks its CRC, and refuses to overwrite existing files. ZIP64, split archives,
ZIP encryption, and methods other than stored/DEFLATE are explicitly unsupported.
The input limit is 128 MiB; the per-entry decoded limit is 32 MiB.

`inventory-png APK` decodes all 786 PNG entries with our own code and emits their
dimensions and RGBA8 hashes. All pixel buffers match independent ImageMagick
decoding exactly. Supported PNGs are non-interlaced, 8-bit grayscale, RGB,
indexed, grayscale-alpha, and RGBA, including palettes and transparency keys.
All five row filters, chunk CRCs, and zlib Adler-32 are checked. PNG metadata is
retained in the APK; color management and Android nine-patch layout semantics
are not applied by this pixel reader.

`inventory-pvr APK` decodes all 243 PVR entries and emits RGBA8 hashes. Every
buffer matches an independent installed PVRTexTool conversion. The reader
supports the APK's single-level, single-surface PVR v2/v3 RGBA4444, RGB565,
RGBA8888, and ETC1 variants. It validates dimensions, byte counts, channel masks,
and ETC1 differential ranges. Unsupported metadata, layouts, and formats produce
explicit errors. Packed colors are normalized to RGBA8 for inspection; matching
native GPU sampling and blending remains rendering work.

When ClangFormat is installed and detected, check source formatting with
`cmake --build Files/build --target ab-check-format`.

`decrypt APK ENTRY [PRIVATE_KEY_FILE] NEW_OUTPUT_FILE` writes the decrypted
container. `recover-lua` with the same arguments also decompresses it.
`inventory-lua APK [PRIVATE_KEY_FILE]` parses all Lua prototypes, instructions,
constants, and debug sections and emits decoded hashes and structure counts.
It covers 8,559 functions and 1,559,025 instruction words across 1,529 chunks.
`inventory-levels` with the same arguments also recovers every level's static
table constructors; all 655 level-related chunks pass. These commands
require the pinned APK. The key is read from its verified library data; the
optional private key file overrides that read for research. Keys and original
resource data are never bundled with project source. Set CMake `AB_REFERENCE_APK`
for automatic recovery checks; `AB_REFERENCE_KEY` enables the private-key comparison.

The 7z reader currently supports the target's plain-header, single-file,
single-LZMA1-coder variant. Other 7z layouts and codecs produce explicit errors.
Recovered bytecode is research input; it is not executed by a bundled Lua VM.
`find-lua-string APK [PRIVATE_KEY_FILE] EXACT_STRING` searches string constants
across all chunks and reports prototype offsets, constant indices, and global
read/write instruction indices. It does not execute the scripts.

`inventory-sprites APK` reads all 237 KA3D/SPRT sprite sheets and exports their
5,089 named regions, texture references, rectangles, and pivots. Every field
matches an independent parse. Signed coordinates and pivots, including an
original negative atlas coordinate and a zero-area sprite, are retained.
This establishes metadata recovery; rendering and layout parity remain open.

`inventory-fonts APK` exports all 37 bitmap-font files, including 12,577 glyphs,
their unsigned identifiers, signed rectangles and pivots, leading, and tracking.
All records match an independent parse. The native font's ascent/descent
calculation and bounded UTF-32 layout are recovered; Unicode decoding and
complete text rendering are still pending.

`AB_REFERENCE_TEXT_CATALOG` with `AB_REFERENCE_APK` enables the original native
TEXT parser and prepared resource lookup comparison. Build `ab_android_text_catalog`
with `cmake/build_android_math.cmake`, then use `cmake/run_android_text_catalog.cmake`
with `AB_REFERENCE_TEXT_CATALOG_RAW` pointing to the four plain DAT files extracted
from `assets/data/localization/`. The staging script pins their hashes. The isolated
original parser receives raw bytes through a first-party JNI FileReader adapter;
locale lists, native string maps and 4,808 lookup triplets are compared to C.
No JNI adapter or original library is used by the runtime. Locale/bootstrap
loading and native missing-key cache allocation remain open.

`AB_REFERENCE_TEXT_LOCALE` checks 4,096 original Android GameLua locale choices.
Build `ab_android_text_locale` and use the same raw-data staging script with
`AB_ANDROID_ORACLE_KIND=text-locale`. The original platform string conversion and
selection routine execute against controlled Locale.toString inputs and BASIC
locale subsets. The C selector preserves Japanese-prefix normalization and the
English fallback even when unavailable. Live OS mapping remains separate work.

`inventory-text APK` exports all four localization catalogs: 29 locale groups,
510 identifiers, and 4,576 translation entries. All bytes match an independent
parse, including 726 empty translations. Exported strings use byte-preserving
JSON escapes. The catalog API supports exact lookup; native locale fallback
and text formatting remain pending.

`inventory-composites APK` exports all 52 KA3D/RVIO composite files: 842 named
layouts and 7,954 ordered parts. Position, sprite references, original scale
and rotation bits, and flip flags match an independent parse. This recovers
layout metadata; native transform application and rendered output remain open.

The shared rendering foundation builds unrotated SPRT quads and retains their
order in a preallocated draw list. Focused checks cover anchors, negative
pivots/dimensions, UVs, and affine arithmetic order recovered from native code.
The initial GLES2 backend consumes those commands through native entry points
supplied by a platform context. It uploads RGBA8 without changing stored
channels, retains order, supports explicit blend factors, and reuses its shader
program and vertex buffer. Its GLSL ES 1.00 shaders pass installed-validator
checks for both UV precision variants. Ownership and submission tests use a
command recorder, not a GPU.

On Apple platforms, the separate `ab_metal` library consumes the same basic
draw list through a C boundary to native Metal. Its shader compiles with the
installed Apple tools. Offscreen execution on the local GPU passes exact RGBA8
orientation, five blend-mode, opacity, and overlapping-command checks. This
validates those synthetic cases; comparisons to the Android build remain open.
The Apple `ab_window` boundary presents a fixed-size Metal surface, queues
frames asynchronously and maps native pointer/key/close events into C data.
Its explicit `ab_window_check --background` check completes eight textured
frames and controlled native pointer down/drag/up and close events without
taking keyboard focus. This is platform validation; Android input/aspect
policy, clipping, effects and game-frame presentation are still pending.

`ab_metal_capture` provides bounded, explicit offscreen RGBA8 readback. The
native GPU pixel comparisons cover both RGBA8 and BGRA8 captures. The Apple
diagnostic `ab_font_preview` reads a font and its PNG/PVR atlas from the pinned
APK, lays out printable ASCII and saves a new 480x320 PPM. For example:

```sh
Files/build/ab_font_preview \
    Files/com.rovio.angrybirds_5.2.0-12200520.apk \
    assets/data/fonts/480x320/FONT_SCORE.dat 31380 \
    Files/AngryBirdsC/font-preview.ppm
```

This diagnostic uses an authored text/background sample. It does not reproduce
a game screen or establish final text-pixel parity with Android.

`ab_lua_data TARGET_BYTECODE [PREREQUISITE_BYTECODE ...]` exports a straight-line data constructor
to JSON. Keys retain their types, number values include original binary32 bits,
and string bytes use byte-preserving JSON escapes. For example, after recovering
`assets/data/levels/pack1/Level1.lua` into ignored storage:

```sh
Files/build/ab_lua_data \
    Files/AngryBirdsC/analysis/Level1.lua.firstparty \
    > Files/AngryBirdsC/analysis/Level1.data.json
```

This static recovery pass rejects calls, control flow, closures, and unresolved
external globals. Export order is deterministic for inspection; it does not
establish the original runtime's table-iteration or draw order. Level semantics,
physics-world behavior, and gameplay still require implementation and native comparisons.
Prerequisite chunks run in the listed order before the target. Their exported
globals initialize the next chunk through a shallow copy; imported tables stay
read-only and every input remains alive until export finishes. The combined
input, metadata, and recovered-data budget is 32 MiB, with at most 32 files.
Numeric addition, subtraction, multiplication, and division use binary32;
division by zero, nonfinite results, and implicit numeric coercion are rejected.

The classic `scripts/blocks.lua` catalog also recovers statically: 618 block
definitions and 28 materials. The new `ab_game` property view resolves selected
material defaults and density/draw-order overrides without executing Lua.
All 129 captured scalar properties across the first level's eight definitions
match the running APK. Initial circle/box fixture geometry matches all shape
and material bits for 22 captured level-1 fixtures, using the actual sprite
bounds and measured physics scale. A separate first-step capture now checks
all 23 fixtures, including ground, and collider/collision/sensor/gravity metadata.
Defined body fields and tight bounds match for 22 objects; the leading bird's
subsequent sling pose remains excluded. Allocator bytes and pointers are excluded.
Remaining shape types and complete gameplay/replay behavior remain open.

`ab_level_bodies_create` assembles ordinary museum-mode circle/box bodies from
the recovered catalog and level constructor order, then creates ground last.
It uses caller-owned object/order storage and a sprite-size callback. Both scale
factors and ground position are explicit inputs; deriving level bounds remains
open. Creation attaches fixtures before applying each object's angle. Queue
physics can then set an object's rotation to zero and its body type to static.
`ab_level_bodies_order` supplies the growing runtime table's iteration order;
queue random/timer state and movement onto the sling remain open.
Game angle normalization uses an authored integer-significand remainder followed
by binary32 addition, matching 65,536 Android import samples across the full
finite float range. No platform remainder implementation is used at runtime.

`ab_lua_inspect BYTECODE PROTOTYPE_OFFSET` prints constants, child offsets,
and instruction fields for private static research. Optional CTest inputs
`AB_REFERENCE_CLASSIC_BLOCKS`, `AB_REFERENCE_CLASSIC_DEFINITIONS`, and
`AB_REFERENCE_CLASSIC_FIXTURES` enable hash-pinned native property/fixture checks.
`AB_REFERENCE_CLASSIC_BODIES` additionally requires `AB_REFERENCE_LEVEL_ONE` for
the initial-body comparison; `AB_REFERENCE_GAME_ANGLE` checks angle normalization.
`AB_REFERENCE_CLASSIC_STEPS` also checks eight complete steps from a fresh native
level-1 load: all 23 body states, ordered contact manifolds/materials and fixture
bounds match. Captured sling poses are explicit replay inputs; the leading bird's
fat proxy and full animation/callback effects remain outside this comparison.

The first-party slingshot module matches 4,096 original-Lua launch-velocity
cases, retaining the original binary32 operation order and missing-bird result.
It also matches 4,096 active move-to-sling updates, including 968 completion
boundaries and 11,096 ordered position/rotation/body-type calls. The arc uses
an authored evaluator for Lua's double-precision sine path, separate from the
physics `sinf` approximation. Another 4,096 initialization cases match saved-Y
selection, current-Y restoration, jump flag/timer, reset phase and arc height,
plus 8,192 initial transform calls. Reload-delay gating, live frame timing, hats
and actual queue/callback effects remain open. Optional inputs `AB_REFERENCE_SLINGSHOT`
and `AB_REFERENCE_SLING_TRANSFER`, `AB_REFERENCE_SLING_START` enable these
comparisons in `ab_slingshot_tests`.
`ab_slingshot_transfer_body` applies the measured transform/type sequence to a
live C-world body, including game angle normalization. A combined replay now
matches twelve original update frames through transfer completion and holding:
26 physics steps, all 23 body states, ordered contacts and every fixture bound, including
the leading bird's fat proxy. Both history slots and interpolated poses also
match. `AB_REFERENCE_CLASSIC_FRAMES` enables this check. Sling vectors, the
initial partial timer and physics enable are supplied inputs; full queue policy,
callbacks, scene rendering and subsequent input remain open.
`ab_body_pose_capture` samples signed rotation/position and promotes massive
birds to continuous-collision bullets at the native history gate.
`ab_body_pose_interpolate` preserves native position/shortest-angle arithmetic;
`ab_slingshot_hold_body` applies the measured transform/velocity reset order.
Velocity assignment and point/origin impulse primitives match 4,096 direct
native GameLua operations, including 818 wakes, static/kinematic guards,
subnormal speeds and signed-zero angular cases. `AB_REFERENCE_BODY_VELOCITY`
enables this comparison. Full launch callbacks and queue state remain open.
Angular velocity assignment additionally matches 4,096 original GameLua calls,
including 1,072 wakes; `AB_REFERENCE_BODY_ANGULAR` enables that comparison.

`ab_frame_clock_advance` reproduces the timer and accumulator arithmetic in 108
original GameLua update calls. Time advances by unscaled elapsed seconds even
when physics is disabled. Physics uses a separately scaled accumulator and
repeated binary32 timestep subtraction. It rejects work beyond a supplied step
budget without dropping time. `AB_REFERENCE_FRAME_CLOCK` enables the native
comparison. The arithmetic plan leaves callback ordering, complete pause policy,
pose-history synchronization and frame presentation to subsequent integration.
`ab_angle_from_components` supplies the original Android `atan2f` numeric result
without a platform math call. It matches 65,536 component pairs, including signed
zero, interval boundaries, the full finite exponent range and rotations produced
by the reference sine/cosine functions. `AB_REFERENCE_ANGLE_COMPONENTS` enables
the comparison. The combined frame replay uses that signed angle when physics
synchronizes a body's rotation back to Lua before the next sling update.

`ab_script_angle_from_components` separately matches 65,536 promoted-double
atan2 results used by Lua; `AB_REFERENCE_SCRIPT_ANGLE` enables that comparison.
`ab_slingshot_drag` matches 4,096 original-Lua stretch passes and 4,471 ordered
audio query/play boundaries. It preserves the radial clamp, the narrower upward
vertical limit, signed angle, wobble state and selection-sound flag. Final stretch
is the rounded band-to-rest distance divided by maximum length and clamped to
zero through one. `AB_REFERENCE_SLING_DRAG` enables the comparison. Pointer/touch
mapping, actual audio dispatch and release/launch integration remain open.

`ab_slingshot_launch_impulse` matches 4,096 original launchBird calls through
their impulse boundary, with default/explicit vectors, both force signs, zero
force overrides and measured first-bird properties. It preserves force negation
and each scale/mass/power multiply without the preview's inverse-mass step.
`AB_REFERENCE_SLING_IMPULSE` enables this comparison. Position/velocity resets,
event hooks, statistics eligibility and applying the impulse remain separate.

`ab_slingshot_launch` and `ab_slingshot_release` additionally match 754,823 words
from 4,096 paired original methods with mutable host services. The comparison
covers 35,860 ordered boundaries, selected-bird changes, live vectors and
position aliases, numeric-zero force, and partial effects after missing-service
errors. Build `ab_android_sling_launch_host`, stage the pinned Class, Vector2,
utils and SlingshotSystem chunks in the usual slingshot directory, then run
`run_android_oracle.cmake` with kind `sling-launch-host`.
`AB_REFERENCE_SLING_LAUNCH_HOST` enables the retained comparison. Original
methods and public dragged-bird accessors execute without instruction/upvalue
patches. Actual statistics storage, shot/cancel effects and runtime scheduling
remain host work; general Lua coercion and function rebinding are outside this
typed route.

`ab_bird_shot` implements the surrounding launch state transition with explicit
host boundaries. It matches 4,096 original-Lua transitions and 59,392 ordered
calls, including event changes to vectors and script changes to bird mass.
`AB_REFERENCE_BIRD_SHOT` enables that comparison. `AB_REFERENCE_LIVE_SHOT`
also checks launch arithmetic and body impulse against one actual first-level
swipe capture. Runtime host adapters, touch-release routing, statistics
eligibility and subsequent flight/destruction remain open.

`AbClassicShots` binds that transition and the ordered outer sling route to
loaded level bodies, canonical visuals and behavior instances. The storage
publisher exposes start impulse, flying identity, flags and final launch
velocity before later services observe them. `classic_shots_live_android`
checks the composed adapter against the retained native swipe's impulse/body
values. Other event, particle and audio effects remain controlled test inputs;
runtime defaults require their real services when reached.

`ab_level_audit APK LEVEL_THEMES_TSV` constructs current render, behavior,
simulation, character and shot owners for each supplied path/theme pair. Each
declared theme is checked against the APK. It uses explicit seed zero and
logical texture IDs, without advancing gameplay or providing dummy services.
The 592-row catalog now constructs all five owners; 18 levels retain missing
visual names. This diagnostic reports construction separately from playability.

`ab_event_manager_*` provides the dense listener/queued/delayed owner used by
runtime notifications. `AB_REFERENCE_EVENT_MANAGER` checks 6,144 original actions
and 2,144 listener boundaries. Build `ab_android_event_manager`, stage the pinned
complete Class, utils and EventManager chunks in its private directory, and run
`run_android_oracle.cmake` with kind `event-manager`. Borrowed payload/listener
lifetimes and explicit storage/depth limits remain caller responsibilities.

The release-threshold and backwards-statistic predicates additionally match
4,096 original calls, including equality cancellation and distinct eagle/special
mode truth tests. `AB_REFERENCE_SLING_ROUTING` enables that comparison.
`ab_slingshot_cancel_drag` and `ab_slingshot_reset_band` additionally match 4,096
original cancellations and 6,144 ordered position/audio-query boundaries, with
`AB_REFERENCE_SLING_CANCEL`. Runtime callback dispatch, executed audio and
touch-release mapping remain separate work.

`AB_REFERENCE_CLASSIC_FLIGHT`, alongside the classic loader/body/initial-frame
inputs, compares twelve pre-impact flight frames and 36 physics steps. The
source-data loader, settled world, measured stretch, launch impulse and clock
match all 23 body states, ordered contacts and fixture bounds. Queued-bird poses
and initial flight clock are inputs; bird fat bounds are excluded. This does
not establish queue animation, pose-history or collision/destruction parity.

`AB_REFERENCE_CLASSIC_IMPACT` extends that replay through 24 frames and 72
physics steps. `ab_museum_impact_contact` composes ordinary damage, immediate/
deferred velocity, contact disabling and damage score in first-party C. The
replay's 31 callbacks and four removals reproduce body/contact/bounds and
logical strength/velocity/score states, ending at 9,380. Captured initial logical
scalars, queue poses/types and initial clock are inputs. Bird fat bounds,
pose histories, general removal-table mutation and script/presentation effects
remain open; this is not a complete playable controller.

`AB_REFERENCE_CLASSIC_RELOAD` adds the initial partial reload timer and checks
the next bird's timer/fired/fill/ready/index transitions. Source start-number
selection, transfer and held motion generate its poses from frame 9 onward,
while the same impact/score comparison still passes. Earlier idle jumps and
the other queued bird's poses remain inputs.

`AB_REFERENCE_CLASSIC_IDLE` generates both queued birds' poses for all 24 frames
and checks their logical positions/angles and jump state. It uses explicit
initial states plus an inferred initial phase and three inferred jump draws;
the original global RNG state/order remains open. Body/contact/impact/score
checks retain the same limits as the impact corpus.

`AB_REFERENCE_CLASSIC_RETRY` separately compares 38 retry frames and 114 steps,
with five removals and score 11,380. Source-data creation and settling use the
observed proxy identifiers as initial conditions. Initial logical state,
sling/clock values and queue poses/types remain inputs.

`AB_REFERENCE_CLASSIC_RESULTS` extends that controlled projection to all 96
frames and 288 steps, with completion on frame 37, bird retirement on frame 42
and bonuses on frames 73/79 through score 31,380. Body/contact/bounds, logical
damage/score and retirement timers match. Queue poses, retry IDs, initial
logical/clock/sling state, ordinary controller configuration and camera identity
remain inputs. Full queue/controller effects, result presentation and runtime
integration remain open.

Completion initialization now derives its bonus/ending timers and remaining
index, selects subsequent birds from source start numbers, and generates the
held bird's cancel/hold position after completion. Earlier queue poses and the
other idle bird's poses still use captured inputs in this projection.

`AB_REFERENCE_CLASSIC_RESULTS_QUEUE` further generates both queued birds for
all 96 frames. It checks full jump state, reload state, both history slots and
rendered poses as outputs. Captured initial queue state/positions and 25 inferred
random boundary values are inputs; ten discarded values are unobserved valid
choices. Global RNG, object creation history and full presentation remain open.

The screen/world/physics coordinate helpers match 4,096 native sets covering
six point conversions, six delta conversions and one scalar scale conversion.
`AB_REFERENCE_COORDINATES` enables that comparison. Camera positioning, aspect
handling and device input routing remain integration work.

The camera viewport helper matches 4,096 original CameraSystem extent/shake
calculations and a running-camera capture. `AB_REFERENCE_CAMERA_VIEWPORT` and
`AB_REFERENCE_LIVE_CAMERA_VIEWPORT` enable those checks. Movement, limit
enforcement, shake generation and scene rendering remain separate.

`AB_REFERENCE_CAMERA_TOUCH`, together with `AB_REFERENCE_APK`, checks the original
touch FSM and composed camera over 8,192 commands. It compares pre-limit targets,
shared taps, drag/zoom state, selected builtin tweens, sequence state and published
viewport fields. Original filtering, logical input delivery and gameplay
scheduling remain supplied boundaries. The loaded level preview accepts one
desktop pointer through this recovered handler.

`AB_REFERENCE_MENU_POINTER` compares ordinary menu left-button/wheel filtering
over 4,096 original cases and all 5,567 callbacks. It checks routing, field
presence and all-touch copy/clear membership. Menu callbacks are supplied;
hover, extra buttons and copied-table traversal order remain separate.

`AB_REFERENCE_INTEGER_ORDER` checks 4,096 fresh integer tables and two original
deep copies each, including array/hash capacities and all 393,120 entry positions.
`AB_REFERENCE_CAMERA_TOUCH_FILTERED` with the APK checks the extended 8,192-command
menu/copy/touch/camera composition. Raw-table order and actual menu callbacks
remain supplied inputs; the runtime executes first-party C only.

`AB_REFERENCE_UI_FRAME_GEOMETRY` checks 4,096 original ancestor position/scale
queries and 32,768 invisible-button rectangle hits. It preserves separate
position/scroll and cumulative-scale queries. UI routing and layout remain
separate.

`AB_REFERENCE_UI_POINTER` compares base frame traversal across 4,096 native
dispatches and 2,956 controlled child callbacks, including nil versus false
primary replies, typed payloads, truthiness and local coordinates. Callback
mutations, actual widget handling and full UI integration remain separate.

`AB_REFERENCE_UI_IMAGE_HIT` compares 4,096 original ImageButton geometries,
40,960 hit queries and 65,536 native strict rectangle queries. Rectangle centers,
dimensions and angle are checked as float words, with explicit caller sprite
bounds/pivots and padding. Use `cmake/run_android_ui_image_hit.cmake` with
`AB_REFERENCE_UI_IMAGE_HIT_CHUNKS` for the three private, hash-pinned original
methods. Resource selection, button state and final UI presentation remain
separate.

`AB_REFERENCE_UI_IMAGE_BUTTON` compares 8,192 continuing original widget commands,
all typed replies, click/clear flags and 3,713 ordered hit/audio/child callbacks.
The five private methods are staged by `cmake/run_android_ui_image_button.cmake`
using `AB_REFERENCE_UI_IMAGE_BUTTON_CHUNKS`. Geometry, audio execution, constructors,
animation and complete menu scheduling remain separate boundaries.

`AB_REFERENCE_UI_BUTTON_STATE` compares 8,192 canonical widget pointer commands,
complete typed flags/replies and all 5,184 callback snapshots. Build
`ab_android_ui_button_state`, then use `run_android_ui_image_button.cmake` with
`AB_UI_BUTTON_STATE=ON` and the same five pinned method chunks. All six raw value
kinds are supported, including a single identity-only function token. Flags are
reseeded every seven commands; intervening state is retained. Actual geometry,
sound execution, callback mutation and complete menu scheduling remain separate.

`AB_REFERENCE_UI_IMAGE_ANIMATION` compares 8,192 original hover/pressed animation
commands and scale factors, including all 8,419 image/position/hit callbacks.
`cmake/run_android_ui_image_animation.cmake` stages six private methods from
`AB_REFERENCE_UI_IMAGE_ANIMATION_CHUNKS`. It checks original float state, temporary
screen position and clear ordering; real child effects and drawing remain separate.

`ab_ui_widget` composes concrete Frame/Image/ImageButton nodes using caller-owned
stable child storage. It connects lifecycle, child updates, canonical pointer
flags, hit geometry and rendering. Subclasses supply actual C methods and invoke
the corresponding base entry points. `ab_ui_widget_resources` binds localization,
sprite metadata, synchronized textures and the audio bank. The Apple-only
`ui_widget_apk_metal` check loads the pinned APK, resolves its dynamic asset
requirements, clicks prepared logo/play-button instances and captures Metal output
without opening a window or playing device audio. This is an integration fixture;
the complete MainMenu class and startup application remain open.

`ab_dynamic_asset_requirements_open` owns the decoded requirement data and preserves
table aliases. Its restricted constructor reader accepts local literal-table
lookups and assignments, with no global access, arithmetic, calls or control flow.
The terminal `dynamic.addreq` registration is validated rather than executed.
The borrowed lookup result supplies the actual ordered groups to Android asset
loading. Handler lifetimes and scene transitions are separate responsibilities.

`ab_ui_bg_box` is the concrete Frame subclass used by menu panels. It composes
ordinary resource sprites, preserves source component mutation/query/draw order
and supports natural or scaled borders, anchors and colored centers. Colored
centers require an owned solid-white GPU texture for the untextured fill boundary.
The prepared APK check includes the six MainMenuSlider panel components; loading
the actual `common` requirement supplies their POPUPS sheet. Original whole-panel
pixel parity remains separate.

`ab_ui_scaling` subscribes to the actual EventManager and obtains live logical
screen dimensions, scaling flags and optional template size through its host.
The initial Android profile remains fixed across resolution events; disabled
scaling suppresses event-driven recalculation. Direct update still selects one
when scaling is disabled. Widget assignment and scaled sprite bounds/pivots use
the current scale. The application must bind its real state and resolution event
ID; the integration test supplies explicit controlled environment inputs.

`ab_ui_main_menu_slider` composes the actual panel, Scaling and widget owners.
Its source-derived constructor, entry/layout/update methods, visibility and input
gates run as C class methods. State events use the canonical widget identity in
`AB_DATA_OBJECT`; static resource serialization rejects that runtime-only kind.
EventManager notifications run immediately, including during queued dispatch.
The event table is a synchronous borrowed view; receivers copy any fields needed
afterward. `ab_ui_widget_lifecycle` dispatches derived methods for a scene root.
The APK integration reads the real event definitions, opens this slider, lays out
an About button and routes About/Play input into the audio bank before offscreen
Metal capture. Placement and child selection remain fixture inputs; complete
MainMenu construction and application scheduling remain open.

`AB_REFERENCE_UI_FRAME_DRAW` checks 4,096 original frame/tagged draws and all
6,144 callbacks, including method lookup, defaults, visibility, tag equality and
six transform words. `cmake/run_android_ui_frame_draw.cmake` stages the two
private methods from `AB_REFERENCE_UI_FRAME_DRAW_CHUNKS`. Children are controlled
callbacks; real widget drawing and full frame composition remain separate.

`AB_REFERENCE_UI_IMAGE_DRAW` checks 4,096 original Image.drawSelf cases, all
4,671 state/sprite callbacks and early culling. The private selected method is
staged by `cmake/run_android_ui_image_draw.cmake` from
`AB_REFERENCE_UI_IMAGE_DRAW_CHUNKS`. Numeric zero-scale NaNs/infinities, alpha
arity, reset order and floored sprite dimensions are included. Resource/GPU
execution, child composition and final pixels remain separate.
The checker constructs canonical Image state and projects it through
`ab_ui_image_state_draw_view` before comparing the native commands.

`AB_REFERENCE_SPRITE_SIZED` checks 4,096 original explicitly-sized Sprite image
rectangles and UVs across all anchors. Build `ab_android_sprite_sized` and use
`run_android_oracle.cmake` with kind `sprite-sized`; no scripts are staged.
`AB_REFERENCE_UI_IMAGE_RENDER` checks 4,096 composed UI image paths, native
render-state writes, sized Sprite/Image rectangles and GL batch output. Build
`ab_android_ui_image_render` and stage the usual Image.drawSelf method using
`run_android_ui_image_draw.cmake` with `AB_UI_IMAGE_RENDER=ON`. Actual GameLua
and Sprite/GL methods execute; resource binding and shader selection are
prepared. No GPU context or pixels are captured by this corpus.

`AB_REFERENCE_STRING_MAP` checks 4,096 continuing string-only dictionary
mutations, complete slot/link layouts, 262,144 lookups and native next order.
Build `ab_android_string_map` and use `run_android_oracle.cmake` with kind
`string-map`. No scripts are staged. Fixture keys remain rooted, including
empty/embedded-NUL and stride-boundary lengths. Version2 also erases alternate
entries during active iteration. GC dead keys and mixed key types remain open.
`AB_REFERENCE_STRING_MAP_GC` adds explicit original full collections and dead-key
tags, including collections during erase/next. Build `ab_android_string_map_gc`
and use kind `string-map-gc`. Automatic collection is stopped; scheduling remains
a separate input. Fixture strings are rooted to keep object identity/bytes alive;
rooting does not prevent retained nil keys from receiving dead tags.

`AB_REFERENCE_UI_MENU_MANAGER` compares 4,096 original menu lifecycle/update/draw
commands, 32 constructors, every 80-word callback snapshot and 72-word final
state. Build `ab_android_ui_menu_manager`; stage three complete pinned original
chunks with `run_android_ui_menu_manager.cmake` and
`AB_REFERENCE_UI_MENU_MANAGER_ROOT` pointing to the private analysis root.
Version2 adds a dense four-channel transition prefix. Actual Class.new and
controller methods run; input delegation, node/dynamic/event/render methods and
collection phases are explicit providers. Original allocation/collection
scheduling, remaining constructor fields and actual screen integration remain open.

`AB_REFERENCE_UI_MENU_LINKS` adds builtin link registration/lookup and result
routing, comparing the complete three-map state (92 words) and every 100-word
callback snapshot. Build `ab_android_ui_menu_links` and use the same staging
script with `AB_UI_MENU_LINKS=ON`. Version3 preserves the Version2 controller
domain and adds absent link instances, replacement and missing-link rejection.
Generic/custom link records and event/frame navigation remain separate.

`AB_REFERENCE_UI_MENU_NAVIGATION` adds original scene/frame events composed with
actual Frame add/remove/pop methods. Build `ab_android_ui_menu_navigation` and
stage with `AB_UI_MENU_NAVIGATION=ON`; the same script also pins/stages Frame.lua.
Version4 compares all 144 state words and 152 words per callback, including
canonical child order/lookup, entered/parent state and installed removal flags.
Subclass lifecycle methods and event delivery remain providers. Duplicate child
insertion assertion prefixes are excluded from this valid canonical graph domain.

`AB_REFERENCE_UI_MENU_INPUT` adds independent input order and actual original
delegatePointerEvent routing. Build `ab_android_ui_menu_input` and stage with
`AB_UI_MENU_INPUT=ON`. Version5 compares 151 state words and 159 words per
callback, including six-kind primary replies, unused auxiliary replies, missing
current roots and duplicate channels. Node pointer methods remain providers;
full click/hover/key/touch scheduling is separate.

`AB_REFERENCE_UI_MENU_KEYS` adds the original installed delegateKeyEvents body,
called directly and through normal menu updates. Build `ab_android_ui_menu_keys`
and stage with `AB_UI_MENU_KEYS=ON`. Version6 uses the same 151/159-word layout
and adds phase/label/primary records for key methods. Sparse authored 72-key
fixtures feed original code; native code chooses its own mapping, order and
gates. Windows/OSX/Roku, other devices, editor/control and all emitted labels are
covered. Platform input and widget key bodies remain provider boundaries.

`AB_REFERENCE_UI_MENU_FRAME_KEYS` composes the menu key body with original
Frame.onKeyEvent on two prepared base roots and custom leaf nodes. Build
`ab_android_ui_menu_frame_keys`; stage with `AB_UI_MENU_FRAME_KEYS=ON`. Version7
records every base/leaf key call and all three returned values, alongside full
controller/graph state. Base-child insertion is excluded to keep the prepared
one-level graph acyclic; actual subclass bodies and full scenes remain separate.

`AB_REFERENCE_MAIN_MENU_THEME` checks the installed original main-menu theme
layout, exact camera results and typed water prefixes at all eight callbacks.
Build `ab_android_main_menu_theme`; use the same full-chunk staging script with
`AB_MAIN_MENU_THEME=ON`. The script pins LuaBase/Class/Frame and main-menu.lua.
Settings/resource/camera/theme/effect providers are controlled; no menu constructor
or final rendered scene is covered by this corpus.

`AB_REFERENCE_MAIN_MENU_SETTINGS` checks five original settings methods, their
raw getter/default behavior and stored writes, including a no-argument zoom clear.
Build `ab_android_main_menu_settings`; stage the full settings wrapper with
`AB_MAIN_MENU_SETTINGS=ON`. Each case compares eleven typed getter/stored values.
Fixture-only theme lookup records cover absent/false/table/zero values; persistent
settings ownership and actual resource integration remain separate.

With `AB_REFERENCE_APK`, `main_menu_resources_apk` checks static APK theme/SPRT
composition and missing-sprite prefixes. `AB_REFERENCE_MAIN_MENU_RESOURCES` adds
the original-script comparison using actual settings methods and themes_new.
Build `ab_android_main_menu_resources`; stage with `AB_MAIN_MENU_RESOURCES=ON`.
Native bounds and application are prepared providers; C decodes its theme data
and sprite dimensions independently. Merged catalog, startup selection and GPU
rendering remain separate.

`AB_REFERENCE_THEME_LAYER` checks native ThemeLayer horizontal transforms.
Build `ab_android_theme_layer`, then use `run_android_oracle.cmake` with kind
`theme-layer`. `AB_REFERENCE_THEME_LAYER_BASE` checks actual native base-layer,
DrawUtils and ordinary-Sprite image output and render-state writes. Build
`ab_android_theme_layer_base` and run with kind `theme-layer-base`. Neither probe
needs staged Lua. Cameras, layers and ordinary sprites are prepared layouts;
original object reference counting, arithmetic and image dispatch execute in
the pinned library. Resource lookup policy, constructors, composite sprites,
overlays, GPU submission and final pixels remain separate.

`AB_REFERENCE_THEME_LAYER_CONSTRUCT` checks ordinary ThemeLayer constructors;
build `ab_android_theme_layer_construct`, run with kind `theme-layer-construct`.
Prepared resource cache entries feed actual native metadata getters. Numeric
strings and composite metadata are outside the C constructor's current scope.
`AB_REFERENCE_THEME_LAYER_COMPOSITE` adds theme-specific composite entry drawing;
build `ab_android_theme_layer_composite`, run with kind `theme-layer-composite`.
Its V2 trace checks offsets/order, ignored entry transform flags and actual native
DrawUtils/Sprite image output.

With `AB_REFERENCE_APK`, `theme_catalog_apk` decodes the complete classic theme
chunk. `AB_REFERENCE_THEME_CATALOG` compares every typed key/value against the
original graph. Build `ab_android_lua_data`; run
`cmake/run_android_theme_catalog.cmake` with `AB_REFERENCE_THEME_CHUNK` pointing
to the private decoded scripts/themes.lua chunk and the usual explicit Android
oracle/trace arguments. The stager verifies its hash. The C checker reads the APK
directly, validates graph membership and compares unordered native tables.
Additional theme files, merged lookup/load policy and final scene remain separate.

`AB_REFERENCE_THEME_LAYER_GPU` enables `theme_layer_image_batch_android`.
Build `ab_android_theme_layer_gpu`, run with kind `theme-layer-gpu`. Despite
the historical option name, this V3 probe records CPU GL_Image/RenderBatcher
geometry, culling and per-vertex tint buffers; it does not execute a GPU.
The explicit shader token bypasses shader selection. Four-point normalization
and culling compose with original base/composite/Sprite output; final rendering
and shader/color policy remain separate.

`AB_REFERENCE_COMPOSITE_BOUNDS` checks original rotated/scaled integer composite
bounds. Build `ab_android_composite_bounds`, run with kind `composite-bounds`.
Prepared native part/reference layouts execute the original bounds method;
file mapping and resource ownership are separate. `AB_REFERENCE_THEME_BINDINGS`
with `AB_REFERENCE_APK` compares explicitly selected theme1 SPRT/PVR/COMP data
and computed bounds against a private live-cache read. Its enabled flag check
uses the actual byte, excluding surrounding allocator padding. Repeated capture
requires verifying the current process, library mapping and GameLua pointer;
the private debugger script detaches/resumes in finally. Keep the emulator muted.

`AB_REFERENCE_THEME_LAYER_CONSTRUCT_COMPOSITE` checks the same constructor using
actual original composite metadata getters, including full int32 values and empty
sentinels. Build `ab_android_theme_layer_construct_composite`, run with kind
`theme-layer-construct-composite`. C uses resolved integer metrics with no extra
pivot rounding. Native resource ownership/lookup remains a prepared boundary.

`AB_REFERENCE_THEME_BACKGROUND` checks original background traversal with no
spotlight instance, actual GameLua RGB packing and final alpha reset. Build
`ab_android_theme_background`, run with kind `theme-background`. Prepared layer
providers record order/filter/color/alpha while actual composition and color
methods execute. Actual layer bodies, spotlight and complete pixels are separate.

`AB_REFERENCE_GAME_RECTANGLE` checks original GameLua.drawRect fill arguments.
Build `ab_android_game_rectangle`, run with kind `game-rectangle`. Actual native
conversion/packing executes with prepared context state/fill providers; C plans
the same integer arguments and state-reset request. This comparison excludes
full reset-state contents, fill rasterization and GPU pixels.

`AB_REFERENCE_THEME_FOREGROUND` checks original ThemeComposition foreground
math, ground fill and layer traversal. Build `ab_android_theme_foreground`, run
with kind `theme-foreground`. Prepared layer virtual tables retain original RTTI
metadata for actual native base/non-base dynamic_cast selection. Original camera
getters and GameLua rectangle code execute. C resolves the subtype outcome and
fill/reset providers explicitly; full reset contents and GPU pixels are separate.

`AB_REFERENCE_THEME_COLORS` checks original ThemeComposition color construction.
Build `ab_android_theme_colors`, run with kind `theme-colors`. Actual constructor
code reads prepared numeric RGB tables with empty layer vectors; C interprets
static data without a VM, preserving reciprocal multiplication and optional tint
defaults. The existing APK catalog check validates all 31 classic theme color
schemas. Layer allocation/ownership and final rendered color remain separate.

`ab_composite_resources_open` retains explicitly selected KA3D COMP data, resolves
ordinary parts against a borrowed sprite set and computes cached native bounds.
The live theme1 binding check also exercises this owner and its exact/short
allocation and missing-part cleanup. Resource/archive/pixel dependencies remain
explicit; find/enumerate allocate and decode nothing. RVIO binding and native
group load order require separate recovery.

`ab_theme_layers_open` owns static base-layer bindings, colors and ordered
background/foreground handles. The graph, sprite set and optional composite
owner remain borrowed. `ab_theme_layers_draw_base` uses cached bindings through
the verified placement/pivot paths, emitting ordinary images in native order
without frame-time allocation, decoding, I/O or name lookup. Missing base
sprites emit nothing. Dynamic special subtypes and overlays remain unsupported.

`AB_REFERENCE_THEME_SCENE` with `AB_REFERENCE_APK` checks the complete prepared
theme1 constructor/base-draw graph over 256 cases. Build `ab_android_theme_scene`
and use `cmake/run_android_theme_scene.cmake` with `AB_REFERENCE_THEME_CHUNK`
and `AB_REFERENCE_THEME_BINDING_DATA`. The stager pins both private inputs;
the binding data derives from the selected live cache read. Actual native
constructors, resource lookup, composition, layer bodies and CPU image/batch
methods execute. The C checker independently reads theme/SPRT/COMP/PVR data
from the APK, then compares constructed fields, ordered images/ground fills,
selected state, clipping and triangles. Prepared cache/shader/batch boundaries
exclude native loading policy, full state ownership and GPU pixels.

`AB_REFERENCE_IMAGE_MATERIAL` with `AB_REFERENCE_APK` checks native default
point-image shader selection, conditional query order and the ten pinned
shader/include inputs. Build `ab_android_image_material`, run with kind
`image-material`. Original format-alpha and shader-cache methods run with a
prepared resource loader and preallocated CPU batch. The C command adapter
accepts resolved alpha presence; it does not execute or copy original shaders.

`AB_REFERENCE_THEME_IMAGE_FORMATS` with `AB_REFERENCE_THEME_BINDINGS` and
`AB_REFERENCE_APK` checks fourteen selected live theme1 image dimensions and
native SurfaceFormat values against cached PVR alpha metadata. The selected
read retains getter instructions and resumes the muted original process.
PNG native texture-format selection, shader overrides and GPU pixels remain
separate. PVR alpha inspection validates the supported container layout and
payload size without allocating or decoding pixels.

`AB_REFERENCE_FILL_RECTANGLE` compares 4,096 original GL_Context fills after an
actual GameLua identity-state reset. Build `ab_android_fill_rectangle`, run with
kind `fill-rectangle`. Original color conversion and empty-batch addition execute
with prepared cached vertex-color shader objects. C emits a shared clip-space
command through an explicit white-texture boundary; arbitrary state and GPU
color packing/pixels remain separate.

`AB_REFERENCE_RUBBERBAND` compares 4,096 original GameLua/Sprite image-vertex
outputs. Build `ab_android_rubberband` and run the ordinary Android oracle stager
with kind `rubberband`. Original resource/sheet lookups run against prepared
trees; the image boundary records geometry and UVs. Group selection, full sling
composition and GPU execution remain separate.

`AB_REFERENCE_RUBBERBAND_WORLD` extends this comparison through the original
Android script's camera projection in 4,096 cases. Build
`ab_android_rubberband_world` and use `run_android_rubberband_world.cmake` with
`AB_REFERENCE_RUBBERBAND_WORLD_CHUNK` pointing to the private selected original
prototype. The stager pins that chunk before running it with controlled camera
getters and the prepared native sprite route. The checker compares all five
projected arguments, camera/draw order and final vertices/UVs. C exposes
`ab_rubberband_project` and `ab_rubberband_world_vertices`; neither loads Lua nor
allocates resources.

`AB_REFERENCE_SLINGSHOT_DRAW` compares 4,096 states through all three original
back/holder/front script methods and the base class's sprite refresh/getters.
Build `ab_android_sling_draw`, then use `run_android_slingshot.cmake` with kind
`sling-draw` and the existing pinned `AB_REFERENCE_SLINGSHOT_CHUNKS`. The fixture
records ordered state/sprite/band/extras boundaries with controlled camera,
holder pivot and drag/disabled state. `ab_slingshot_draw_plan` exposes the
matching fixed-size C plan. Native resource drawing, extras and complete scene
composition are separate gates.

`AB_REFERENCE_SLINGSHOT_RECTANGLE` checks 4,096 original default Resources/Sprite
draws through GL_Image rectangle CPU batching. Build `ab_android_sling_rectangle`
and use the ordinary stager with kind `sling-rectangle`. Prepared resource trees,
render state, shader and bounded batch storage isolate anchored geometry/culls.
`ab_slingshot_sprite_quad` exposes the corresponding C path.

`AB_REFERENCE_SLINGSHOT_POSE` checks 4,096 `calculateRubberBand` script calls
composed with 8,192 original CoordinateTransforms calls. Build
`ab_android_sling_pose` and use the pinned slingshot stager with kind `sling-pose`.
Controlled level-start/band vectors, angle/distance upvalues and bird fields
exercise radius/width precedence, promoted trig and band-width clamps.
`ab_slingshot_render_pose` implements that numeric C boundary; live scheduling
and full scene ownership remain separate.

`AB_REFERENCE_SLINGSHOT_START_POSE` checks 4,096 original initial-vector updates.
Build `ab_android_sling_start_pose` and use the slingshot stager with kind
`sling-start-pose`. Controlled back-sprite queries and optional source objects
check arithmetic, query order and nil-object preservation.

`AB_REFERENCE_SLING_DELTA` checks 4,096 original delta-based displacement updates
and resets against `ab_slingshot_delta_update`/`ab_slingshot_delta_reset`. Build
`ab_android_sling_delta` and use the slingshot stager with kind `sling-delta`.
Actual native screen-distance conversion executes with prepared camera fields;
sensitivity and shoot range are resolved inputs. Numeric states/conversions and
disabled preservation match. Original Lua identity conditions are retained in
the trace; C storage ownership and callback mutation remain separate.

`AB_REFERENCE_GAME_SLING_MOUSE` checks 4,160 original ordinary mouse input
passes against `ab_game_sling_mouse_plan`. Build `ab_android_game_sling_mouse`,
then use `run_android_game_sling_mouse.cmake` with the private selected original
GameUpdate chunk in `AB_REFERENCE_GAME_UPDATE` and the usual slingshot chunks.
The original instruction stream is unchanged; empty debug upvalue names are
restored for a controlled sling binding. Original point conversions execute.
A checked sentinel error at the following sling-update boundary stops execution
after the input phase. Prefix effects, behavior/query results and non-input
callbacks are controlled. The comparison checks retained vectors, five samples,
presence/index and all 2,635 ordered call snapshots; actual release effects,
callback mutation and complete gameplay remain separate.

`AB_REFERENCE_GAME_UPDATE_TAIL` checks `ab_game_update_tail` across 4,096 original
ordinary GameUpdate executions and 101,239 ordered callback/state snapshots.
Build `ab_android_game_update_tail` and reuse the selected-game-update stager
with `AB_ANDROID_ORACLE_KIND=game-update-tail`. This variant reaches the original
normal return; it uses no sentinel stop. Three stable object identities and
their native traversal order are prepared, along with inactive custom hooks,
joints, grenades and flying-bird specialties. Controlled callbacks change flags,
darkness, cursor, script visibility and elapsed-time presence; the C executor
retains those effects at the original phase. Actual controller bodies and
dynamic membership/order remain separate.

`AB_REFERENCE_IMAGE_RECTANGLE_MATERIAL` with `AB_REFERENCE_APK` checks original
rectangle default shader selection, including opaque-texture fades and alpha
boundaries around one. Build `ab_android_image_material_rectangle`, then run
the ordinary stager with kind `image-material-rectangle`. Its separate C command
adapter is `ab_image_rectangle_command`; point-image policy remains unchanged.

`AB_REFERENCE_THEME_SCENE_FILL` with `AB_REFERENCE_APK` extends the theme1 graph
through actual context ground-fill CPU batches. Build `ab_android_theme_scene_fill`
and use the theme-scene stager with kind `theme-scene-fill`. The shared checker
accepts V1 image-only and V2 image/fill corpora. Independent prepared image and
fill batch lifetimes exclude native resource/GPU ownership from this comparison.

On Apple, `ab_theme_capture APK NEW_OUTPUT.ppm [2] [rgb565]` prepares the direct
APK level/theme preview, advances 128 fixed updates and captures through the
standalone Metal boundary. Default output is 480-by-320 RGBA8; `2` doubles each
extent, and `rgb565` requests an actual supported RGB565 attachment. The output
file must be new. Common remains the interactive preview backend. This tool
isolates logical-resolution pixels for comparison; it does not establish native
presentation timing or full gameplay parity. `ab_metal_tests --rgb565` explicitly
checks that optional attachment on the current device.

`ab_theme_capture --input PRIVATE_INPUT NEW_OUTPUT.ppm [2] [rgb565]` reads
the same bounded GPU cache for Metal comparisons. The cache reader preserves
stored float values, rejects malformed dimensions/counts and nonfinite commands,
and borrows image bytes until capture completes.

`ab_definition_tests --apk-camera APK NATIVE_BODIES INITIAL_FRAMES CAMERA_FRAMES
[NEW_PRIVATE_GPU_INPUT]` optionally exports the first replay boundary with the
most visible trail commands. Trajectory recordings export points alone; scene
recordings export the shared ordinary object/point scene. Camera-only recordings
have no point frame to export. The output must be a new file under `Files/`.
It contains decoded APK textures and C-produced commands in the bounded GPU cache
format above, with a black diagnostic background. Metal and GLES2 consume the same
input. Themes/HUD and original APK pixel parity remain separate.

`AB_REFERENCE_CLASSIC_LEVEL_SCENE` checks the actual native Scene counter, ordered
member IDs/classes, membership and pose/scale/visibility at 25 first-level replay
boundaries. It includes explicit starting render-cache/history and queued
controller-pose inputs; it does not replace settled bodies/broadphase state.
The runtime `ab_classic_render_scene_*` owner composes ordinary objects and typed
Lua sprites in that shared order. Resource/level/counter owners remain borrowed;
extra slots stay stable until close, including after scene removal. Lookup and
allocation occur outside ordinary drawing. Dynamic game objects and additional
renderer classes remain separate.

`AB_REFERENCE_SCENE_ID` enables the 4,096-case original counter/numeric-return
comparison. Build `ab_android_scene_id` and run with oracle kind `scene-id`.
`AB_REFERENCE_SCENE_MEMBERSHIP` enables the 12,288-operation original scene
insertion/removal/disposal comparison. Build `ab_android_scene_membership` and run
with kind `scene-membership`. The latter executes real native map/vector and
Scene methods with controlled renderer identities and pinned reference storage;
actual renderer constructor/destructor effects remain separate.

`AB_REFERENCE_LUA_ASSET_RENDERER` checks 4,096 full original GameLua creation
calls against the typed C renderer. Build `ab_android_lua_asset_renderer` and run
with oracle kind `lua-asset-construct`. Original constructors, scene insertion,
numeric return and destruction execute with a controlled ordinary resource lookup.
`ab_android_lua_asset_construct` with kind `lua-asset-construct-smoke` retains the
initial named-field bootstrap separately. Neither adds a VM to the runtime.

`ab_theme_gpu_data APK NEW_PRIVATE_OUTPUT [HIGHSCORE]` exports the current
128-update idle scene and eleven decoded textures for separate GLES2 execution.
The default score-only state emits 49 commands; supplying a highscore adds its
label and digits. The retained comparison's explicit score input emits 63 commands. Keep the
output under `Files/`. Select `AB_ANDROID_PROBE=ab_android_theme_gpu` in the
Android reference build script; it compiles the project backend and shaders
against the pinned platform C ABI. The small `android_headers` declarations
provide no library implementation and do not affect ordinary builds.

`AB_REFERENCE_SLING_GPU_GLES2` and `AB_REFERENCE_SLING_GPU_INPUT` reuse the pinned
native framebuffer/PNG from `AB_REFERENCE_THEME_GPU_NATIVE` and
`AB_REFERENCE_THEME_GPU_PNG`. The `slingshot_gpu_android_apk` retained-capture
check compares all 105,600 RGBA pixels below the HUD, including the 4,675-pixel
sling/bird region, plus the earlier background and ground checks. Regenerate the current capture with
`run_android_theme_gpu.cmake` and `AB_ANDROID_GPU_SCENE=sling`; its input and
complete output hashes are pinned separately from the earlier `theme` scene.
This is an idle pose comparison, with transfer time skipped explicitly.

`AB_REFERENCE_HUD_GPU_GLES2` and `AB_REFERENCE_HUD_GPU_INPUT` retain the score-text
stage before buttons: all 137,600 pixels outside the upper-left button area
match the original, including the 9,000-pixel score region. Regenerate that
retained input with `AB_ANDROID_GPU_SCENE=hud` in the GPU stager.

`AB_REFERENCE_IDLE_GPU_GLES2` and `AB_REFERENCE_IDLE_GPU_INPUT` check the complete
fixed idle frame using the same native framebuffer/PNG. The
`idle_frame_gpu_android_apk` test compares all 153,600 RGBA pixels with no region
excluded. The stager's `AB_ANDROID_GPU_SCENE=idle` verifies a repeated complete
capture. The private highscore is an explicit exporter input, and the static
button layout is derived from APK metadata and checked against live fields.
Active button behavior, temporal gameplay, Metal and presentation remain open.

`cmake/run_android_theme_gpu.cmake` takes `AB_ADB`, `AB_ADB_SERIAL`,
`AB_ANDROID_GPU_EXECUTABLE`, `AB_ANDROID_GPU_INPUT` and a new
`AB_ANDROID_GPU_OUTPUT`. It checks the input hash, completion marker and complete
output hash. It uses a separate pbuffer and loads no original game code.
For retained comparisons, supply `AB_REFERENCE_THEME_GPU_NATIVE`,
`AB_REFERENCE_THEME_GPU_PNG`, `AB_REFERENCE_THEME_GPU_GLES2` and
`AB_REFERENCE_THEME_GPU_INPUT` together with `AB_REFERENCE_APK`. The C checker
compares the complete original PNG/raw capture and the selected theme regions;
it does not claim Metal, presentation or full-frame parity.

`AB_REFERENCE_UI_BUTTON_DRAW` compares the installed original button/image/frame
draw graph across 4,096 cases and 12,935 callbacks. The seven private methods are
staged by `cmake/run_android_ui_button_draw.cmake` from
`AB_REFERENCE_UI_BUTTON_DRAW_CHUNKS`. Self/child hover transforms and ordered
state/sprite/child calls are checked; class installation, actual resources/GPU
execution and final pixels remain separate.

The same corpus also checks `ab_ui_image_button_state_draw` against every
resolved route and native callback, without a new fixture or native capture.

`AB_REFERENCE_GAME_HUD` checks 4,096 original ordinary GameHud draws against
`ab_game_hud_plan`, including all 34,816 ordered calls and width-history words.
Build `ab_android_game_hud_draw`, then use `run_android_game_hud_draw.cmake` with
the private full GameHud chunk in `AB_REFERENCE_GAME_HUD` and Class chunk in
`AB_REFERENCE_GAME_HUD_CLASS`. Those staging inputs are separate from the host
test's trace path. Native metrics, labels, scale and world/child boundaries are
controlled; this corpus does not execute fonts, GPU rendering, ads, eagle mode,
powerup scores or floating scores.

`AB_REFERENCE_UI_IMAGE_RESOURCES` checks 8,192 original Image/ImageButton resource
commands and 13,474 lookups, including selected state during each callback.
`cmake/run_android_ui_image_resources.cmake` stages six private methods from
`AB_REFERENCE_UI_IMAGE_RESOURCES_CHUNKS`. Localization maps and sprite geometry
are controlled; actual APK provider policy and constructors remain separate.

`AB_REFERENCE_UI_LOCALIZATION` checks 8,192 original localization helper calls,
group-cache state and 13,399 ordered lookups. The private method is staged by
`cmake/run_android_ui_localization.cmake` using `AB_REFERENCE_UI_LOCALIZATION_CHUNKS`.
Platform/profile dependencies are prepared and text results are controlled;
native catalog loading, locale policy and bootstrap effects remain separate.

`AB_REFERENCE_UI_TEXT` checks 4,096 original native UI text calls and all 5,840
glyph callbacks and context states. Build `ab_android_ui_text`, then run
`cmake/run_android_oracle.cmake` with `AB_ANDROID_ORACLE_KIND=ui-text`. The native
LuaState/Table adapters and wrapper execute with controlled current bitmap-font
metadata and empty resource maps; no original script staging is needed.
Finite nonzero scales, valid anchors and the unclipped/uncolored route are
checked. Clipping, color effects, constructors and final pixels remain open.

`AB_REFERENCE_UI_TEXT_IMAGE` checks the same original native text path through
normal Sprite/Image submission and GLES2 rectangle batching. Build
`ab_android_ui_text_image` and use `AB_ANDROID_ORACLE_KIND=ui-text-image` with
the common Android runner. All 5,840 image callbacks, 2,923 emitted quads,
2,917 culls and vertex/depth/UV words match the C font draw adapter. The Image
boundary supplies a prepared shader token and matching batch storage; this
does not exercise shader selection or the GPU.

`AB_REFERENCE_UI_TEXT_VISIBILITY` checks 8,192 original vertical visibility
decisions, including nonfinite inputs and signed scales/metrics. Build
`ab_android_ui_text_visibility` and use kind `ui-text-visibility`.
`AB_REFERENCE_UI_TEXT_CLIP` checks 4,096 prepared clipped-line traversals,
8,190 line records, 2,863 native leaf draws and 5,706 glyph callbacks/state.
Build `ab_android_ui_text_clip` and use kind `ui-text-clip`. A context getter
classifies the unmodified internal caller to count leaf draws independently.
Font/resource state and lines are prepared; nested clipping, color effects,
line creation and real provider ordering remain open.

`AB_REFERENCE_FONT_WRAP` checks original GameLua clipText through native font
measurement and UTF conversion, across 36 authored inputs and 4,096 calls.
Build `ab_android_ui_text_wrap`; use kind `ui-text-wrap`. C line slices, exact
UTF-8 output and widest-line bits match all 6,648 lines. Font/resource state is
prepared; NaN and unsafe nonprogressing tiny-width cases are excluded.
`AB_REFERENCE_UTF8_ENCODE` checks 65,536 native mode-2 encoding decisions and
4,096 whole-string conversions. Build `ab_android_utf8_encode`; use kind
`utf8-encode`. These probes use no original script staging.

`AB_REFERENCE_UI_INIT` checks 4,096 original Class.new/Frame.init/Text.init
calls, all 20 supported typed fields, child/lookup reset and 3,746 ordered
boundaries. Build `ab_android_ui_init`; stage the three hash-pinned selected
prototypes with `run_android_ui_init.cmake` and
`AB_REFERENCE_UI_INIT_CHUNKS`. Default object names and font/resource callbacks
are controlled. Callable font arguments and general class inheritance remain
outside this corpus; no interpreter or original prototype enters the runtime.

`AB_REFERENCE_UI_TEXT_LINES` checks Text.clip with native wrapping/UTF/font
measurement and original Frame cumulative scale plus Class/Frame/Text
constructors: 4,096 calls, 6,295 line objects and 37,468 ordered boundaries.
Build `ab_android_ui_text_lines`; use the same constructor staging runner with
`AB_UI_INIT_INCLUDE_TEXT_LINES=ON`, adding the pinned Frame scale and Text clip
prototypes. Font selection/default names are controlled and missing resource
groups return identifiers. C comparison composes first-party decoding, wrapping,
encoding, scale queries and constructors without an interpreter. The shared
native wrapping fixture also reproduces the prior wrapping corpus unchanged.

`AB_REFERENCE_UI_FONT` checks 4,096 continuing original Text init/setFont/layout
commands, including function identities, getter fallback/retention and
non-callable layout errors. Build `ab_android_ui_font`; use constructor staging
with `AB_UI_INIT_INCLUDE_FONT=ON` to add pinned Text setter/layout prototypes.
The same `ab_ui_init_tests` checker accepts constructor and font corpora. Three
controlled native closures require zero arguments and return all supported typed
values; their execution never enters the C runtime. Font selection/measurement
are controlled, and custom callable metatables/error unwinding remain open.

`AB_REFERENCE_UI_VALUE_TEXT` checks 65,536 original plain tostring conversions
and all output bytes, including raw numeric patterns and embedded-zero text.
Build `ab_android_ui_value_text` and use kind `ui-value-text`; no script staging
is needed. The native builtin and numeric converter execute unchanged. C uses
first-party value handling, the C library's finite %.9g conversion and explicit
nonfinite spelling, requiring C numeric locale and nearest rounding. Object
addresses and custom tostring methods remain outside this contract.

`AB_REFERENCE_UI_TEXT_METRICS` checks 12,288 original width/height/leading
results and 28,614 ordered boundaries with native tostring/UTF/font measurement.
Build `ab_android_ui_text_metrics`; use constructor staging with
`AB_UI_INIT_INCLUDE_TEXT_METRICS=ON` to add the pinned width/height/leading
prototypes and shared line setup. `ab_ui_text_lines_tests` accepts both line
composition and metric corpora. Current font maps are prepared, selection is
controlled and localization is the native missing-group identity behavior.

`AB_REFERENCE_UI_TEXT_BOUNDS` checks 4,096 original corrections/bounds with
native font measurement, 17,650 ordered boundaries and 3,020 child queries.
Build `ab_android_ui_text_bounds`; use constructor staging with
`AB_UI_INIT_INCLUDE_TEXT_BOUNDS=ON` to add the pinned correction/checkBounds
prototypes. Version 2 targets exact corners and neighboring outside floats,
alongside centers/ordinary points and all supported child reply types. Separate
leading values verify both leaf queries. Font/group maps and dense child replies
are prepared; no font selection or stored getter invocation is permitted.

`AB_REFERENCE_UI_FRAME_TREE` checks 4,096 continuing original Frame child,
lookup, parent, reset and lifecycle commands plus all 1,609 ordered callbacks.
Build `ab_android_ui_frame_tree`; stage eleven hash-pinned selected prototypes
with `run_android_ui_frame_tree.cmake` and
`AB_REFERENCE_UI_FRAME_TREE_CHUNKS`. Lifecycle prototypes restore empty debug
upvalue names to allow the public setupvalue API to bind their original Frame
counter slot; instructions/constants/properties remain original. Version 2 adds
dense shared parent arrays and full callback traversal. Subclass callbacks are
controlled and snapshots include their before-call state. C uses borrowed arrays
with stable text names; general Lua maps/metatables and assert prefixes remain
outside this contract.

`AB_REFERENCE_UI_IMAGE_STATE` checks 4,096 full original Image/ImageButton
constructor graphs, all 35 typed widget fields, 6,934 callback state snapshots
and 683 inherited-entry attempts. Build `ab_android_ui_image_state`; stage the
five complete pinned class chunks using `run_android_ui_image_state.cmake` and
`AB_REFERENCE_UI_IMAGE_STATE_ROOT`. Original Class.new/create, Lua default,
inheritance and dynamic setters execute. Four dependency requests are verified
prepared noops. Fresh names are defaulted to capture self before resource calls.
Native empty padding is verified before logical labeling; C borrows a fresh
prepared object and does not reproduce allocator timing. The earlier image
resource checker now compares both typed and resolved C state models against
the same native corpus, including every callback prefix.

Bitmap-font UTF-32 range measurement matches 4,096 native cases, including
missing glyphs, negative dimensions and empty ranges. `AB_REFERENCE_FONT_METRICS`
enables the comparison. Text decoding and glyph submission remain separate.
`AB_REFERENCE_FONT_LAYOUT` checks another 4,096 original layouts and all 2,842
glyph callback positions and temporary renderer rotation origins. Missing glyphs
affect alignment width but do not advance the draw cursor.
`AB_REFERENCE_FONT_IMAGE` checks the normal original glyph-to-Sprite-to-Image
route for those 4,096 layouts: local rectangle origins/sizes, UV passthrough
and rotation origins. Complete native transform/effect/pixel comparison remains
open. `ab_font_draw_unrotated` appends glyph quads to the shared draw list with
an explicit caller affine transform.

`ab_bird_jump_frame` matches 4,096 original idle-jump updates and their ordered
random/position/rotation boundaries. It covers completed-level jumps, roll,
rebound and restoration. `AB_REFERENCE_BIRD_JUMP` enables the comparison. Random
generation, idle eligibility, hats and live queue integration remain separate.

`ab_slingshot_reload_gate` matches 4,096 original reload updates, including
next-bird lookup, nil-versus-false handling, fired/fill flags and camera-reset
state. `AB_REFERENCE_SLING_RELOAD` enables that comparison. Queue selection,
transfer orchestration and launch-cue execution belong to the controller.

The level completion, remaining-goal and before-ending checks match 4,096
original cases, including callback order, mutable state and Lua truthiness.
`AB_REFERENCE_LEVEL_FLOW` enables this comparison. The remaining-bird bonus
prefix matches another 4,096 cases and, with explicit completion/queue inputs,
the captured awards at frames 73 and 79 through score 31,380.
`AB_REFERENCE_LEVEL_BONUS` and `AB_REFERENCE_LIVE_BONUS` enable those checks.
The ending-delay suffix matches 4,096 original calls and ordered win-query/
event requests; `AB_REFERENCE_LEVEL_ENDING` enables that check. The 96-frame
projection predicts an ending request on frame 94 and, using this capture's
next-update dispatch boundary, the controller shutdown on frame 95.
The complete ending controller and result presentation remain open.

`ab_level_complete_begin` matches 4,096 original initialization transitions and
25,598 ordered boundary calls, including mutable callbacks, nil/true tests,
remaining-index selection, camera/audio requests and the final level hook.
`AB_REFERENCE_LEVEL_COMPLETE` enables that comparison. Actual camera, audio,
event and script adapters remain runtime integration work.

`ab_bird_retirement_frame` matches 4,096 original ordinary removal cases,
including missing timers, strict speed thresholds, water fallback, camera
identity and out-of-bounds removal. `AB_REFERENCE_BIRD_RETIREMENT` enables this
comparison. Specialties, mutable host lookups and destruction effects remain open.

`ab_collision_velocity_offer` matches deferred selection through 4,096 original
native contact-handler calls, including equal-speed and overflow cases.
`AB_REFERENCE_GAME_CONTACT` enables the comparison. The flush helper follows
the observed post-step sequence and reuses the validated body setter.
`ab_break_score` and `ab_damage_score` match 4,096 original scoring pairs.
A present merged override selects the base definition's value, which can be
absent; damage score is binary32 `floor(damage) * 10`. `AB_REFERENCE_SCORE`
enables that comparison. Complete callback/destruction effects remain open.

Static tables also retain their constructor allocation hints. A first-party
string-table order pass matches 39,030 entries across 4,096 native cases,
including embedded zero bytes and hash collisions. It supports unique insertions
into an already sized hash part, with explicit scratch storage and no allocation.
All 23 level-1 constructor keys match original Lua execution; after special
ground handling their order matches observed body creation. All 79,568 world
keys and allocation hints across the full 655-file level-related catalog also match original
Lua execution. Non-nil replacements preserve their initial key slots; nil
assignments, array keys, and runtime mutation remain outside this pass.
Actual object creation/gameplay in other levels remains unverified.
A separate growing-table pass matches another 39,030 entries across 4,096 native
cases, including every tested rehash boundary. It reproduces all 23 names in
the running first level's object table. Creation and runtime traversal have
different orders; both use explicit caller-owned scratch and no allocation.

The legacy non-bird collision calculation matches 4,096 original-Lua cases:
relative linear momentum, defence/strength changes, ground/static and damage
flags, 309 dead-block insertions and 12,288 ordered boundary calls. The C API
returns strength/death updates and the floored damage payload. Runtime callers
can compose it with the ordinary impact module. Complete joint, script,
destruction and presentation effects remain open. The optional
`AB_REFERENCE_BLOCK_DAMAGE` corpus is checked by `ab_damage_tests`; original Lua
and bytecode execute only inside the separate Android oracle.
The separate bird-impact helper also matches 4,096 original-Lua cases, including
1,275 lethal/contact-disabling hits, 642 immediate and 633 deferred velocities,
and 8,834 boundary calls. Joint force is measured before the damage multiplier;
the collision callback receives the multiplied force. The API accepts resolved
material/modifier inputs. `AB_REFERENCE_BIRD_DAMAGE` enables its comparison;
material lookup and live game integration remain open. The enclosing dispatcher
now matches another 4,096 cases: both single-bird orders, bird/eagle pairs,
speed ties, strength/return data and 6,493 ordered calls with argument types.
The single-bird callback's measured argument shift/nils are preserved by
`ab_museum_callback_contact`; the two-bird branch selects greater squared speed
(second on a tie), then computes that object's momentum length divided by ten.
`AB_REFERENCE_BIRD_ROUTE` enables this corpus. Glue and full callback effects
remain open.
Optional inputs are `AB_REFERENCE_STRING_ORDER`, `AB_REFERENCE_LEVEL_ONE`, and
`AB_REFERENCE_LEVEL_ORDER`. Growing-table checks use `AB_REFERENCE_STRING_GROWTH`
and `AB_REFERENCE_CREATED_LEVEL_ORDER` (the latter also needs the level input).
The full-catalog check additionally uses `AB_REFERENCE_LEVEL_CONSTRUCTORS`,
`AB_REFERENCE_LEVEL_CONSTRUCTOR_MANIFEST`, and `AB_REFERENCE_ALL_LEVEL_ORDERS`.

`ab_save_data PRIVATE_SAVE_KEY_FILE ENCRYPTED_SAVE` decrypts a captured settings
or high-score file and exports its literal tables as typed JSON. The save key
differs from the resource key and remains a private, 32-byte local input.
The reader does not execute scripts. It supports nested tables, named and
bracketed keys, list fields, finite decimal numbers, booleans, and escaped byte
strings within a combined 32 MiB allocation budget. Decimal conversion requires
the C numeric locale and round-to-nearest floating-point mode.
`ab_save_data --verify-encryption PRIVATE_SAVE_KEY_FILE ENCRYPTED_SAVE` checks
that first-party encryption reproduces every captured ciphertext byte. Both
captured files pass; their 200 typed data nodes also match execution inside the
original APK's private reference Lua engine. Save editing, progression rules,
and runtime persistence remain pending.

`AB_REFERENCE_WORLD_CLEAR` checks 4,096 original `b2World::ClearForces` calls
with `ab_world_tests --clear-forces`. Build `ab_android_world_clear` and run the
ordinary Android stager with kind `world-clear`. The isolated raw ABI records
contain bounded list links and force words; only that constructor-independent
method executes. Native and C checks preserve all other body words/state.
`ab_world_clear_forces` also runs at the recovered per-step and post-loop game
frame boundaries, independently of the world's automatic-clear setting.
`AbGameFrameControls.before_step` precedes each step; `after_step` follows its
explicit clear and precedes history. Enabled updates clear once more, including
zero-step updates, then run `after_steps` before interpolation. The callbacks
must supply the remaining recovered controller effects.

`AB_REFERENCE_OBJECT_FRICTION` checks 4,096 original per-object air/relative
friction calls with `ab_object_friction_tests`. Build
`ab_android_object_friction`, then use the ordinary Android stager with kind
`object-friction`. A native Lua table supplies the three optional numeric
definition fields; bounded body records expose actual force/wake arithmetic.
`ab_object_friction_apply` consumes resolved numbers without a Lua VM or lookup.
The elapsed argument has no effect. Complete per-step preparation remains open.

`AB_REFERENCE_OBJECT_FORCES` and `AB_REFERENCE_OBJECT_FORCES_RELATIVE` compare
4,096 original force-adder calls each with `ab_object_forces_tests`. Build
`ab_android_object_forces` / `ab_android_object_forces_relative` and use the
ordinary stager with kinds `object-forces` / `object-forces-relative`.
Controlled native object tables supply mass/axes/target/disable values; the
relative variant also supplies angle and relative force. Numeric intervals
are absent, so no sprite/audio/impulse branch executes. The matching C API
uses first-party binary64 polar arithmetic through `ab_script_polar`.

`ab_game_physics_before_step` / `ab_game_physics_after_steps` bind directly to
the frame driver's callbacks with `AbGamePhysicsPrepare` as context. Borrow
resolved object settings in native-map order; set `water_seconds` from the
frame's scaled physics duration. Water receives that duration on each tick;
the script-step boundary receives the fixed step. The preparation gate is
separate from body active/awake state. Runtime lookup/order/gate binding and
the external hook bodies are caller work; no original Lua executes.

`AB_REFERENCE_OBJECT_FORCES_INTERVAL` compares 4,096 timed force-adder calls.
Build `ab_android_object_forces_interval` and stage kind `object-forces-interval`.
Actual cached Lua state and `setSprite` participate, with no attached renderable
or sound configuration. `ab_object_force_interval_plan` exposes timer/pulse and
sprite requests; `AbObjectForces.impulse` selects center-impulse application.
Actual sprite/audio dispatch and per-object interval scheduling are separate.

`AB_REFERENCE_OBJECT_FORCES_AUDIO` adds original pulse sound/resource/mixer
coverage. Build `ab_android_object_forces_audio` and stage kind
`object-forces-audio`. Silent metadata and a raw AudioOutput forwarding record
reach an in-memory mixer; no device output or sample rendering is used. Compare
the selected clip, gain, voice state and next RNG value. The force checker also
runs `ab_object_force_step` against every case in all four corpora.

`ab_object_force_sound_plan` resolves named/array requests with explicit shared
RNG state. `ab_object_force_step` orders gravity, interval metadata, resource
callbacks, timer publication and the final force/impulse. Install per-object
interval/sound/RNG/resource bindings on `AbGamePhysicsObject` to use that route
through the preparation driver. Callbacks must perform actual resource effects;
unknown property/coercion or traversal policy is not supplied by this controller.

The initial `ab_physics` library implements circle and convex-polygon mass,
bounds, point containment, axis-aligned box construction, circle-circle,
polygon-circle, and polygon-polygon contacts, plus world-space contact manifolds.
Contact-free island stepping also implements forces, gravity scaling, damping,
translation/rotation clamps, synchronized transforms, and island-wide sleep.
All fields of 8,190 bodies across 4,096 native Android cases match, including
1,304 sleepers. Discrete stepping with supplied contact manifolds combines force
integration, warm-start scaling, constraint initialization, velocity and position
passes, impulse storage, and sleeping without frame allocation. Another 4,096
native cases match all 12,287 body states and 8,190 manifolds, including 810
sleepers. Joints and gameplay policy remain pending.
The dynamic AABB tree supports bounded pool growth, proxy reuse, balanced
insertion/removal, movement skips, origin shifting, and overlap queries without
query allocation. All 4,096 consecutive states and 78,915 ordered query results
match the native tree. Broadphase move/pair buffering also matches 4,096 native
states and 23,880 ordered notifications, including duplicate moves, removal
tombstones, canonical pair sorting, and duplicate suppression. Tree ray
traversal remains pending; fixed-scene contact management is integrated below.
Body initialization from explicit definitions, body/joint eligibility, and signed
fixture collision filters match another 4,096 native cases. Definitions retain
supplied velocities for every body type and give dynamic bodies unit mass before
fixtures. The game's additional collision policy remains under recovery.
Persistent contact material mixing and state refresh match 4,096 native cases
and 2,700 callback records, including retained feature impulses, wake transitions,
sensor overlap branches, and callback changes to the enabled flag. Solid geometry
and sensor overlap use explicit evaluator callbacks shared with the world step.
Circle/polygon fixture proxy creation, synchronization, and removal match 4,096
native operations, including combined endpoint bounds, fat bounds, proxy reuse,
inactive skips, and broadphase state summaries.
Discrete world-island traversal with a supplied contact graph matches 4,096
native cases: 2,331 selected islands, 14,336 body states, and 12,264 contacts.
It preserves body/contact-list order, seed gates, sensor skips, and static-body
reuse. Post-solve impulse reporting matches 8,190 island reports and 910 world
reports, including reduced point counts, world contact IDs, and synchronized body
state before sleeping.
Convex distance/overlap queries also match 4,096 native cases and 9,806 support
iterations, including cached simplices, radius adjustments, witness points,
zero/subnormal inputs, and overlap-tolerance boundaries. Queries use at most
eight support vertices and twenty iterations, with no allocation.
The discrete world connects ordered fixture attachment and mass,
proxy discovery, persistent contact filtering/update/removal, island solving,
fixture synchronization, timestep ratios, and force clearing. Initial states and
4,096 consecutive steps in 16 controlled native scenes match, including circles,
boxes, compound fixtures, sensors, signed filters, zero steps, and changing step
durations. Contact storage is bounded and caller-owned. Joints and gameplay
policy remain open.
The same consecutive scenes also match all 6,365 begin/end/pre-solve/post-solve
events, including event order and active manifold/impulse payloads.
Sweep interpolation and separating-axis queries match 4,096 native states and
20,480 support/fixed-index queries. The time-of-impact query matches another
4,096 outcomes and fractions, plus all 3,811 search and 3,046 root iterations,
including native failure outcomes and large-angle normalization. Continuous
position correction also matches 22,510 native passes across 4,096 cases,
including selected-body mass/inertia and aliasing. Continuous islands match
12,287 body states, 8,190 saved manifolds, and 8,190 report snapshots across
another 4,096 cases. They preserve forces, sleep state, and saved manifold
impulses. Continuous world stepping matches another 4,096 consecutive native
steps and 10,068 collision callbacks, including fast bullet bodies, aligned
sweeps, impact selection/cache state, neighbor expansion, and fixture updates.
The world uses bounded 64-body/32-contact impact islands and caller storage;
substepping remains unsupported. Inactive native cache storage is excluded from
comparison because it contains reused memory; every valid cached fraction and
all impact counts, flags, sweep endpoints, and body states are compared.
Another 4,096 consecutive steps match when pre-solve callbacks disable cached
contacts, including 3,463 rejected callbacks and all 11,063 ordered events.
Those comparisons check sweep rollback and the resulting subsequent steps.
Body repositioning, activation, type and fixed-rotation changes, and fixture
filter/sensor changes also match 4,096 operation/step pairs: 2,560 requested
changes plus 1,536 skips. Checks cover immediate states, following steps,
active fixture/proxy fields, and all 7,342 ordered collision events.
Bounded body/fixture creation and removal match another 4,096 operation/step
pairs: 512 body creations/removals, 768 fixture creations, 192 explicit fixture
removals, 10,460 collision callbacks and 576 implicit fixture-destruction
notifications. Checks include mass changes, world/fixture list order, compound
bodies, inactive proxies, slot reuse, and subsequent continuous steps. Reserve
body/fixture capacities at world creation; runtime operations allocate no memory.
Slots remain stable until removal and may then be reused. Joints remain open.
The GameLua collision eligibility layer matches 4,096 native decisions across
all twelve collider types, including group overrides, masks, missing metadata,
ordered matrix entries and per-object enabled flags. Classic rule initialization
and symmetric pair changes also match 4,096 native setups/updates, including
preserved entries and rule overwrite order. The initialized matrix matches the
running level. Game contact handlers remain separate recovery work.
Polygon construction also preserves the target's eight-input limit, point
welding, hull ordering, normal/centroid arithmetic, and degenerate box fallback;
4,096 construction cases match native output bits. The public builder rejects
nonfinite arguments or arithmetic and supports input/output aliasing.
Circle and polygon ray queries match another 8,192 native cases, including
hit fractions, normal bits, and preserved output on misses.
Explicit body-mass overrides match 4,096 native cases for mass/inertia,
center shifts, and velocity corrections, with lock/type and fixed-rotation rules.
Fixture mass aggregation matches another 4,096 cases, including zero through
four fixtures, density skips, center corrections, and static/kinematic sweeps.
Saved contact impulses and the friction/normal velocity solver each match
4,096 controlled native cases, including coupled two-point solving and up to
ten iterations. Velocity constraint initialization matches another 4,096 cases,
including nonzero rotations supplied from measured Android trigonometry.
Single-point position correction matches another 4,096 cases, including the
target's contact slop and correction limits. Complete ordered position passes
match every body's state after 22,510 Android reference iterations across
4,096 cases. They recompute transforms using first-party rotation math, whose
sine/cosine bits match 65,536 extended Android samples in its documented finite
range. Larger arguments remain unsupported explicitly.
The complete stepped world remains under recovery. Optional comparisons execute
the pinned ARMv7 library's audited machine-code functions under the installed
QEMU tool. On the local host, 4,096 circle and 4,096 polygon
cases match the original output bits, including 2,048 box constructions.
Another 12,288 contact cases match every local and world manifold field,
including preserved impulses, inactive fields, clipped feature IDs, signed
zeros, and both reference-face orientations. This validates the tested geometry
cases; gameplay integration remains pending.

To enable those comparisons with installed Clang, LLD, and `qemu-system-arm`, add
this option when configuring the existing build:

```sh
-DAB_REFERENCE_ARM_LIBRARY="$PWD/Files/AngryBirdsC/target/lib/armeabi-v7a/libAngryBirdsClassic.so"
```

The optional probe verifies input hashes before execution, stays separate
from the runtime, and writes its trace inside the build directory. A separate
Android probe runs the same corpus with real platform imports; its recorded
30,441,357-byte trace includes a 25,669,936-byte prefix matching the isolated
probe byte for byte and additional complete position-pass cases. Startup, main
menu, episode selection, first-level gameplay/results, retry/pause, and process
restart captures have also been obtained from the unmodified
APK in a private reference AVD. See [reference setup](android-reference.md)
for the environment, limitations, and offline probe commands. These captures and
component comparisons do not establish complete gameplay parity.

All project implementation code, including physics, resource decoding, audio,
and offline tools, must be first-party. Do not fetch, vendor, or link external
implementation libraries. Installed development tools, the C standard library,
and isolated native platform APIs are allowed as described in
[AGENTS.md](../AGENTS.md).

Follow [AGENTS.md](../AGENTS.md) for scope and evidence requirements and
[STYLE.md](../STYLE.md) for C style. [ROADMAP.md](../ROADMAP.md) tracks active
implementation and validation work. Keep generated data, build products,
captures, and analysis output under `Files/AngryBirdsC/`.

The owned shot checker also exercises the ordinary sling queue and mouse
effects. Optionally set `AB_REFERENCE_SLING_WOBBLE` alongside
`AB_REFERENCE_CLASSIC_SHOT_EFFECTS` to compare the original full spring method.
Build `ab_android_sling_wobble`, stage the unchanged standard slingshot chunks,
and run oracle kind `sling-wobble`. This 64-case reference uses original reset/
stretch setup and read-only scalar capture. It does not establish full-frame
or all-level playability.

The explicit native-window clip check opens a brief silent window and checks its
actual backing-pixel viewport and clip order; it is excluded from unattended
CTest runs:

```sh
Files/build/ab_common_clip_tests
```
