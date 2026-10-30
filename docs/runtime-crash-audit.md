# Runtime crash and catalog audit

Target: the supplied Android 5.2.0 APK, SHA-256
`b524b6b4b0ad999f46828546dee1bb047d6e00f724285a2e8c9f4b34a8bee7ad`.
ARMv7 library SHA-256:
`ada9bc1a1469c30a9bf906c4293df3ff1a75485b40ddbfb146debbc41f14d38c`.
The following records describe implementation checks on 2026-10-29.
Complete native playthrough and visual parity remain unverified.

The later `crash-level-settle-180-20261029-04.tsv` inventory records 581 of
590 requests surviving 180 ordinary frames. All 15 birthday hat scenes and all
30 Birdday! 5 UGC scenes pass. Its remaining rows are seven registered special
scripts and two vial-removal callbacks. That inventory predates the vial effects
and mode reward hooks below; subsequent focused checks do not establish a new
whole-catalog total.

The original three-frame inventory constructed 565 of 590 catalog requests.
Extending each request to 180 frames exposed 41 additional failures, leaving
524 requests that survived six simulated seconds. After the shared callback
and camera fixes, 563 survive that longer check. The retained private inventory
is `crash-level-settle-180-20261029-02.tsv` under the analysis input directory.
An inventory test succeeds when it records every request, even when individual
levels fail; inspect the rows, not only CTest's exit status.

That inventory's failures are 15 birthday hat construction hooks, ten
registered special level scripts and two reached vial-removal callbacks
(`BlackBird_31` and `BlackBird_43`). This inventory predates the connected
Short Fuse ability; a settled level is not a completed level or ability replay.

## Shared failures corrected

The application now supplies the explosion owner's deferred joint-break and
connected-joint traversal services. The latter follows the live named metadata
order and retains repeated requests. Ordinary block collisions also receive
their own deferred break service. Their queue is consumed by the existing
GameUpdate joint phase. No numeric solver error is converted to success.
The actual Matilda/egg integration scene `LevelP2_80` exercises two ordinary
requests and four requests with the optional connected-radius branch, followed
by real physics and Metal drawing. Unit coverage includes endpoint matching,
ordering, duplicates and queue exhaustion before engine mutation.

Fixed-camera levels `Level77`, `Level45` and `LevelGE_UGC1` can have coincident
camera snaps. Source `SlingshotSystem.defaultAnimateBirdToSlingShot17173` uses
`min(1, max(.25, 1-distance/range))`; its ordered maximum retains .25 for 0/0.
ARM math.max at VA `0x8201bc` confirms the comparison behavior. Preserve that
result without raising host division exceptions. Signed zero and overflowing
ratios have separate unit checks.

`VialEffects.lua` contact prototype 16109 queues a vial when it contacts a goal
and retains `previousCollision` only for actual bird/goal table identities.
Other contacts preserve the previous value. The application now supplies this
callback. Its fixture checks a ground contact, bird alias retention, canonical
pig death queueing and subsequent in-place movement through the retained alias.
Orange transformation/removal and Blue cloning are connected below. Physical
Pink-vial bubbles have a separate implementation and validation gate.

## Birdday! 5 and Short Fuse

The `birdday5UGC` card uses its actual first-click gift action, saved
`UGCEpisodeCardGiftBoxOpened` flag, `piglette` sound and
`particles/giftBoxUnwrapBuff.lua` paper/ribbon effect. A second click enters the
ordinary level screen. The Metal fixture covers all 30 buttons, both pages,
the 100-particle unwrap and gameplay on each page, returning through Pause/Menu.

Short Fuse dispatches `triggerBlackBirdSpecialtyShockwave` from the common
specialty phase. Recovered inputs are
`subsystems/blackbird/BlackBirdAbility.lua` (SHA-256
`ac94e80ba62d95002a11d434e939665a8d51a4a894a8dccd9156d068579fcdd7`)
and `ElectricShockwave.lua` (SHA-256
`2d2a947bb27ba1f2127727f2f5057f60c4df7bc00db1142cdc53e1993422dd83`).
Their charge, force/damage, unnormalized vectors, score, joint requests,
electrocution/invulnerability, callback removal and lightning draw phases use
shared live owners. Idle bird electricity consumes its initial random draw
between the correct bird timer pairs. The native math.rad conversion at ARM VA
`0x81ff48` promotes a float, multiplies by binary64
`0x1.1df46a2529d39p-6`, then rounds to float.

The focused Metal fixture performs an actual mouse launch/manual activation
and a controlled named-contact boundary followed by the ordinary delayed fuse.
It checks the source's two manual specialty events versus one delayed event,
real bird removal, damage, pig electrocution and completion cleanup. Captured
lightning output was inspected. Separate numeric tests cover phase crossings,
radius gates, damage/force policy, RNG consumption and visual state transitions.
These checks do not establish complete native shockwave or vial interaction parity.

Global `applyForce160869` captures the five script arguments unchanged, and
`applyForcesAtPhysicsStep161227` replays that queue at each physics step. The
connected shockwave now uses that shared queue with raw vectors. Explosion and
boost scripts apply their own explicit physics scale; that scale must not be
inserted into the shockwave path. The focused fixture checks an actual queued
vector against its source point, origin and force value.

## Birthday hats and pink bubbles

`level_scripts.lua11476` selects a random hat before its Blues override, creates
the authored hat definition and weld, and registers its shared table on each
bird. The application now connects those bodies, deferred weld removal, collision
release, jump, sling transfer and Eagle queue replacement. The focused Metal
test constructs all 15 episode-7/page-2 levels and advances each for 360 frames
at 60 Hz. Its first level also performs mouse launch/specialty input, and its
second level activates Eagle with the attached hats. These are controlled C
integration checks, not native playthrough captures.

That stronger launch test reached the previously unbound `BUBBLES` branch.
Recovered `scripts_common/bubbles.lua` SHA-256 is
`d3cb1b524bdb25598b8c5aa7dc3b622a9354e06ea05fedcd53ee12c4e47e27e7`.
Prototypes 275, 3017, 3505 and 4872 specify candidate selection, antigravity,
per-object timing and late bubble drawing. `default_conf.lua` SHA-256 is
`4f58e7b320a97e5920ebcf8c4f3b7d9ecda939b6dde319f287c66b891cc53ae1`;
its globals supply antigravity 1.8, timer 2, break force 100 and velocity limit 10.
The common specialty branch at 125163 starts `pinkTimer` from the preloaded
`sonicBoom.lifeTime`; Museum's updateBird19137 decrements that charge before
starting the actual nearby-object effect. Original particle resource definitions,
sprites, sound, shared RNG, live objects, joint queue and force queue are used.
Ordinary pink bubbles mark existing bodies; Pink Vials use a distinct physical
floating-pig constructor and retained captured-object table.

The birthday test now requires charge, actual bubble registration/drawing,
timer cleanup and bird removal. A 1024x768 Metal capture of the active bubble
was inspected. Numeric tests cover strict candidate/velocity boundaries,
binary32 rounding at the total-speed cutoff, timer crossing versus cleanup,
paused time and invalid numeric input. Complete native pink-bird timing and
visual comparison remain unverified.

## Registered reward levels

`LevelP3_271`, `LevelP4_426` and `LevelH900` now bind their actual level hooks.
The level-script module SHA-256 is
`28ed02c661dc9fd522ce5c3d9501c215170b609c13d520ff5952f86c27394498`.
Boomerang hooks 4663/4815/4965 and 5335/5488/5639 prune the decoration after
achievement, set the completion flag, extend the ending by 3.6, notify the
popup, write the correct setting, and notify that it was shown. Cake hook 6010
retains the Eagle gate, first-collection setting/reward, and repeated collected
event without another popup. The actual boomerang/generic reward constructors
2439/5314 share the existing glow and three process actions. Focused integration
uses the real completion/ending/removal owners, events, settings and Metal draws;
it supplies an exhausted unused-bird counter rather than claiming a native win.
Replay pruning, cake Eagle gating and popup completion pass, including ASan/UBSan.

At the reward checkpoint, all 789 Release tests pass. The preceding bubble/hat
checkpoint passes all 584 ASan/UBSan tests, followed by the new reward fixture
under both sanitizers. The full Windows cross-build also passes. These counts
refer to those checkpoints rather than subsequent changes.

## Circle recreation for specialty physics

New original-library calls execute `GameLua.resizeCircle` at ELF VA `0x32195c`.
It sets radius, calls `GameObject.recreate` (`0x3448b4`), and then publishes
current radius/mass into the existing physical table. Recreation destroys only
the circle fixture and calls `createFixture` (`0x34458c`); the body and joints
remain. The new fixture resets filter/sensor state and preserves the object's
material values. Density setters at `0x344220` update fixtures without an
immediate mass reset; the subsequent recreation recomputes mass.

The C implementation matches all 128 original ARM cases, including signed zero,
static bodies and varied radii/materials. Trace SHA-256 is
`2c1193d417bb2f848838b71b797186e67f49b3fa4379c426672d99b5a4007b43`.
The isolated Android 5.1.1/API-22 ARM reference uses the recorded API-23 kernel,
silent host backend, and unmodified target library; no JNI/game launch is used.
This is an operation trace, not a rendered native playthrough. Additional C
checks retain real welds, velocities, table aliases and cached creation mass,
remove existing contacts, and preserve post-physics specialty state while
refreshing its rolling-radius projection. The complete vial factories and
their combined physics traces remain open.

## Orange bird and potion callbacks

`scripts_common/puffer_bird.lua`, SHA-256
`91ede4ef123db6f08d30480ef408e8cdf951f079b9a52527849c4c16f34116ba`,
supplies the manual ability, first-contact delay, inflation, inflated lifetime,
direction changes, shrinking and final retirement. Callbacks use unscaled time,
live object-update lists, shared random state and the native-matched circle
fixture recreation. Native friction/restitution/density change without replacing
the Lua creation fields. Bird retirement and transformed-pig destruction retain
their different source paths. The Level925 integration exercises mouse input and
an actual first collision; the focused ASan/UBSan check and Windows build pass.

`VialEffects.lua`, SHA-256
`990781131637e435a94d3779bbdfbbb81a4c9318ef0cf08eb4cd6e724dc0749e`,
supplies the Orange removal callback 9615, queued goal transformation 7759,
delayed transition 8202 and inflation 8427. The C binding retains canonical
goal identities, copied traversal order, queued closure captures, the source
invulnerability window, sprite mappings, gadget/joint removal and real Puffer
pig bodies. Particle emission uses its captured world position after removal.
The GLOOP_2 effect uses the source duration, rotation, alpha and size phases.
Unreferenced local glow/twinkle drawing closures are not invented as active draws;
the active vial update retains its glow RNG consumption and near-pig defence.
Mode-specific clothing and additional `.scripts.lua` behavior remain open.

The real destruction queue on BlackBird_31 and BlackBird_43 drives the focused
Orange regression. It follows delayed particles/transition, inflated fixture
ownership, shrinking and actual goal/body removal. This exposed stale cached
character timers: script blink/oink/smile/expression writes now update the live
animation projection, with changed timer fields published back after a tick.
Separate unit checks cover those writes, invalid numeric input and publication.
This is controlled C integration evidence, not native potion/playthrough parity.

## Delayed callback table semantics

`scripts_common/delayedCallback.lua`, SHA-256
`010fccb22ba68ad37017301933e58036ecbe5a542336126835b2b2901545b8a0`,
initializes a numeric-key table and consecutive float IDs. Each update makes a
fresh shallow copy, decrements shared timeout records, invokes expired callbacks,
and deletes their keys from both the captured and current tables. New callbacks
wait for a later update. Reinitialization during a callback preserves the old
snapshot; its later deletions apply to the replacement current table. The game
update function remains registered when empty. The first-party owner preallocates
record/snapshot storage and has an explicit consecutive-ID bound before 2^24.

The numeric map matches 8,192 original ARM table mutations and 8,192 fresh
shallow-copy layouts, values and iteration orders. The trace SHA-256 is
`e5f207cd48137ec66174882d53a0512255b7db468ec917ff1640cd5c4d82d13d`.
Cases include array growth/shrink, sparse positive/negative/zero keys, nil writes,
deleted-key reuse and exact float-ID endpoints. C callback-owner tests separately
cover timer crossings, unscaled time, callback append/reset, deletion from the
replacement table, sparse ID reuse, failure retention and reentry rejection.
This trace validates native table behavior, not the whole callback subsystem or
combined native potion timing.

## Mode reward hooks and retained removal data

The EggDefender_classic_8 `_extra` hooks 6459/6556 use the existing loaded-object
and award helpers for BP_BIG_ENGINE_1 / LevelGE_29. BlackBird_15 hooks 6657/6755
use ExtraGoldenEgg_1 / LevelGE_31 and retain their additional current-Eagle
physical cleanup. The common helper owner is reused while keeping those mode
wrappers distinct. Their integration fixture covers fresh construction, Eagle
cleanup without an award, ordinary destruction/reward, and unlocked replay.
Other behavior of the two custom game modes is not established by this check.

Unlocked replay exposed a shared constructor bug: pruning keeps original joint
keys with nil values, while live engine requirements shrink. The joint traversal
workspace now covers the original constructor hash capacity independently of
live engine slots. Unit coverage includes partial pruning and a fully pruned
joint table; no source keys are compacted or surviving traversal reordered.

BlackBird_15's removal hook also exercises particles after native body removal.
Original `gamelogic.lua` getAngle161583 reads `objects.world[name].angle`;
sampling the live native body was incorrect and rejected this valid source
sequence. The particle binding now reads that retained published field, preserving
the source removal ordering and its distinction from the newer native pose.

## Expanded replay checkpoint

At the Orange/mode-hook checkpoint, all 798 Release tests pass in 390.80 seconds,
and all 594 ASan/UBSan tests pass in 1,480.41 seconds. The complete Windows MinGW
cross-build and source formatting also pass. The new seven-scene physics fixture
uses explicit script/particle seeds, real mouse launch/ability input, camera
dragging when needed, ordinary physics/removal phases and periodic real Metal
submissions. It includes repeated white birds on Mighty Hoax LevelP2_80, birthday
abilities, a large Level745 scene and Short Fuse. These are bounded deterministic
C replays, not a native playthrough or proof that every level works.

A separate 590-request audit extends each scene to 900 frames with launches and
abilities. It records seeds, successful shot counts, ability inputs, ending state
and exact failures. The inventory's successful exit only means every row was
recorded. The first run hit its 1,500-second test timeout after recording 541
requests: 522 survived, and 19 reached explicit implementation errors. It is a
partial inventory, not a complete pass. Episode-range selection now allows bounded
shards with unchanged per-request seeds. Later checkpoints follow below.

The Level1045 replay at frame 598, seed `52070209`, exposed a shared death-queue
iterator defect. LLDB locates the error in `ab_block_removal_next`, called by the
ordinary after-step removal pass. Explosion callbacks can append deaths and
rehash a sparsely occupied table to a smaller hash part. Keeping the previous
physical slot then makes it invalid or skips/repeats entries. The cursor now
retains the last key and resumes native `next(table, key)` at its current slot.
It preserves the captured table across global rebinding and still rejects a
genuinely discarded key. A regression forces an eight-to-two-slot shrink, checks
the pending death, and separately verifies invalid-key rejection. This change
follows the full-suite checkpoint above. Its edge regression and the original
Level1045 seed pass under ASan/UBSan through all 900 frames. The retained original
cursor corpus also still matches all 4,096 cases. The named bird-collision service
now dispatches vial callbacks as well as the block service; a focused actual
bird/vial contact test passes under sanitizers. The BlackBird_11 replay now
continues beyond its old collision error and reaches the still-open Blue vial
removal callback instead.

## Destruction spawns and insertion between physics steps

Level1896/1899/1905 and Level932 reach `Spawning_cake.deathSpawn`: ten choices
from the actual `CakeCream` type list. The app binds the existing recovered
removeBlocks spawn continuation to defined native bodies, renderers, GameObject
behaviors, and every fixed-capacity physics/removal adapter. Names remain owned
until level close. Reservation walks the loaded spawn graph, accounts for every
possible descendant, and rejects cycles or capacity overflow before adapters open.
No callback, random choice, rotation, sprite, or physical piece is omitted.

Fresh ModeSystem.createObject2630 and MuseumMode.createObject18509 inspections
confirm direct forwarding to ObjectFunctions.createObject883. Museum load2711
instructions655..668 derive `scaleFactor` from authored `physicsToWorld` (default
100) divided by current `physicsToWorld` (20). The death-spawn continuation uses
that same captured session binding on its retained positions. These are source
observations; a combined native cake destruction capture remains unverified.

Insertion exposed a shared adapter gate that incorrectly prohibited creation
during the unlocked AFTER_STEP removal phase. Simulation now accepts the next
reserved identity during that phase and refreshes the force-preparation prefix
before the next step. Locked worlds and other active simulation phases still
reject registration. Removal metadata permits stable append during its own pass;
recursive removal execution still rejects. A solver/history/publication unit
fixture creates a replacement during AFTER_STEP, preserves its reused body slot,
and continues subsequent steps.

All four actual cake scenes pass a focused ASan/UBSan fixture that destroys their
cakes, checks all generated circles and canonical scripts, submits real Metal
frames, and continues 270 additional frames. The actual definition test separately
checks CakeCream geometry/material/dynamic membership and duplicate-name rejection.
These targeted checks follow the full-suite checkpoint above; they do not establish
native gameplay parity.

## Dynamic goals, Blue vials and electrical aliases

Defined goal-circle construction now supports the actual six pig definitions and
the floating-pig definition. Source ObjectCreation883 retains automatic per-type
float counters, stable names, independent flat face tables, real GameObject
registration, and EID_OBJECT_CREATED notifications. Current levelGoals membership
drives character traversal, including newly created and removed identities.
Constructor capacity is reserved before adapter creation; native bodies still
reject exhaustion rather than silently dropping requested objects.

Museum load2711 counts goals and giftable pigs before constructor traversal.
GiftBoxManager4755/4899 initialize and advance the shared randomizer. Its spawn8794
returns false before probability/settings/RNG when showEditor is false and no
adSystem exists. This application has no ad service; it preserves that source
gate rather than fabricating gift readiness or a claim response. Actual service
gift placement remains outside this local check.

VialEffects Blue effect12313, spin10203, transition and clone9918 use the source's
timers, shared force queue, captured sprite width, original body retention, two
additional goal-circle factories, radius/scale changes, copied effect queues,
new face timers, goals and shared RNG. Captured closures preserve the original
definition while receiving the current clone target. The shared transition7407
does not overwrite damageSprite; Orange's wrapper8202 separately does so.

Native getIntersectingObjects at VA 0x32bac0 and QueryCallback::ReportFixture at
0x33f42c confirm proxy-AABB traversal, static-body inclusion and no shape TestPoint.
The script passes all four offsets as positive .01, producing a degenerate query
at (x+.01,y+.01). Its predicate tests absence from objects.dynamic, independently
of current body type. The C implementation preserves those details.

BlackBirdMode3915 transfers the same electrocution table identity and timers to
each clone. A bounded pool retains old tables while any canonical physical object
still references them, including removed objects. The source can start a new
effect without rebinding the clones' old table. The regression explicitly checks
those aliases and later mutable values, actual object-created events, seven
dynamic pig factories, subsequent physics and Metal.

ASan/UBSan focused checks pass for the four cake scenes, contact dispatch,
shockwave, Orange vials, dynamic factories and all three controlled Blue scenes
(BlackBird_9, BlackBird_16, BlackBird_43). The Blue fixture continues 360 frames,
checks fixtures/scripts and original-body retention, and submits real Metal draws.
Its 1024x768 clone capture was inspected after centering the fixture camera.
The Windows cross-build also passes at this checkpoint. These are C integration
and static-binary observations; combined native vial timing and images remain
unverified.

The complete late-episode inventory records all 257 requests: 240 survive 900
frames, 12 reach vial callback gaps, and five fail at Golden Egg minigame GUI
constructors (SOUNDBOARD1, RADIO, KEYBOARD, SEQUENCER, ACCORDION). Every Birdday! 5
request survives that bounded replay. A later 45-level Short Fuse shard, after
Blue and gift-RNG changes, reaches three Pink callbacks and one composite-sprite
rendering gate; the other 41 survive. Neither inventory is a full playthrough.

## Pink vial capture and composite objects

Pink effect14966 immediately applies shared transition7407, retains a waiting
effect when the goal is already puffing/inBubble, then schedules closure15487
after 1.3. The closure plays its actual potion sound before checking membership.
Bubbles.transformIntoBubble1523 scores/removes the old pig through standalone
removePig, constructs BLACKBIRD_FLOATING_PIG, transfers electrocution aliases,
adds the new goal, and captures eligible nearby objects in current world order.
The source ignores affectOnlyOne metadata; reservation accounts for all goals.
Its floating sprite selection ignores the mapping scale and instead derives scale
from the original floating definition's sprite bounds and spriteRadius.

Captured objects keep their physical-table identities, offsets and native hash
traversal. Construction allocates their exact bounded retained table once;
updates allocate nothing. The source disables masks, stops velocity, consumes
the shared angular-velocity RNG draw, and defers each connected joint. Native
GameObject::setFilterMask at VA 0x344360 walks fixtures newest first, preserves
category/group and narrows the mask to u16. The update3925 uses common force3017,
positions surviving contents, deletes missing members, and queues bubble death
after the timer has crossed. removeFloatingPig4558 clears retained flags and
restores live masks to 0xffff and velocity before losing the floating body.

The first actual Pink check exposed the same composite gate as the later
BlackBird_11 replay. Native GameObjectRenderer::draw at 0x2cdfac, especially its
ordinary context at 0x2ce1ec, delegates to SpriteAsset::draw at 0x2d4be0. That asset
dispatches ordinary sprites or CompoSprite::draw (0x58639c) with pivot anchors.
Scene objects now retain the resolved composite/part selection, use the existing
native-tested transformed-part drawing path, and preserve object membership,
pose, ordering, culling and command rollback. No part is replaced with flat art.
The 1024x768 controlled Pink capture was inspected. Combined native gameplay
timing, interaction and visual fidelity remain unverified.

The next full Release checkpoint ran 802 tests: 801 passed, and the preexisting
force-property regression caught definition assignment incorrectly rerunning
native constructor effects for a direct circle. A separate construction marker
now distinguishes a defined factory from a later copied script definition.
The direct-circle regression and original 23-object force-property comparison
pass under ASan/UBSan after this change. The 801/802 result is not recorded as a
clean checkpoint; the full suites are rerun separately.

All five controlled Pink-vial scenes now pass under ASan/UBSan: BlackBird_30,
BlackBird_40, BlackBird_44, FlockFavorites23 and FlockFavorites24. Each moves only
an authored vial to its nearest authored pig before ordinary destruction, then
checks scored old-pig removal, real floating factories, canonical captured table
aliases, disabled/restored masks, real Metal draws and 480 subsequent frames.
Alternating scenes exercise natural expiry and early ordinary destruction.
The focused native-scene resource regression also passes with composite drawing
and partial-command rollback. Original combined native frames remain unverified.
The original BlackBird_11 seed now survives all 900 replay frames after the
composite fix. Full macOS and Windows cross-builds pass at this checkpoint.

The new early/middle replay shards record all 333 requests in episodes 1..7.
Every row survives 900 ordinary 30-Hz frames. Their retained seeds drive 1,293
mouse launches and 1,292 ability inputs, with no loading/runtime error row.
The late 257-request shard must be rerun against the newer mixed-vial factories.
These bounded C replays still do not establish complete wins or native parity.

At the Pink/composite checkpoint, all 599 configured ASan/UBSan tests pass in
1,737.52 seconds. This includes the full existing numeric/physics/resource suite,
actual launch replays, all five Pink scenes, Blue/Orange effects and dynamic
factories. Subsequent soundboard/canister changes require their own validation;
that complete checkpoint does not cover code added afterward.

## Golden Egg soundboard and canister follow-up

The source SOUNDBOARD1 prepare override6966 marks the real golden-egg row opened,
notifies EID_GE_LEVEL_STARTED, constructs Soundboard and requests an actual
Loader whose retained completion target is that widget. Its closure7412 loads
GOLDEN_EGGS and BACKGROUNDS. The app now handles that GUI route without creating
a dummy physics level. The pinned soundboard module SHA-256 is
`1e7b1518b0b5eb880a7c4dbdd5ceeb0e2cbfacc8a5b2e9d191d4aa25d861ce2f`.
Its root instruction window31..182 supplies all 16 original button descriptors;
the restricted data reader does not execute script functions.

Soundboard clicked5424/selectClip6177 preserve the started/stretch/launch/bird/
specialty sequence and group-specific sound selection/stop behavior. Source
goldenEggStarAchieved66265 emits the first-time popup before changing the actual
highscore, saves both stores and reports current golden-egg stars. StarPopup3671
uses BIG_STAR_2, first-time glow and repeated-award alpha .5; it shares the
existing three showRewardPopup94726 actions. The actual C Loader/input gate,
puzzle, one-time star, sound toggle, resizing, Metal capture and Back to Golden
Eggs pass the focused Release fixture. Its star capture was inspected. Radio,
Keyboard, Sequencer and Accordion remain separate GUI recovery tasks.

The mixed-vial fixture reached rocketCanisterRemoved, which removes the name from
the retained active registry. Fresh RocketCanister.lua SHA-256 is
`bcf180d78ebcfcf8a549c1fdffcede90fc3fb4e7c8e5bf6a771b5b3764ab5fb7`.
BlackBirdMode init640 registers ROCKET_CANISTER at inclusive force10. Bird hook5362
selects the object opposite the shot bird; block hook5697 tests first then second.
Start1161 initializes the seven-field rocketInfo only when nil and registers the
actual game-update function. Update679 checks expiry before advancing, emits from
the strict particle-buffer boundary183, and queues raw mass-scaled thrust.
utils.rotatePointAroundOrigin11901 preserves rounded Lua cos/sin and separate
float products; retained published angle and physical position supply its inputs.

The canister regression passes Release checks for the immediately lower and exact
force thresholds, non-resetting start, particle equality/crossing, queued thrust,
timer equality/crossing, real removal, retained metadata, Metal and 180 subsequent
frames. The Windows cross-build passes after connecting these callbacks. Combined
native canister trajectories and visual parity remain unverified.

The six focused ASan/UBSan checks for shockwave, Blue, Pink, mixed vials,
soundboard and canister pass in 213.26 seconds. Mixed-vial checks use the real
BlackBird_43 Blue/Orange pair in three activation orders; Orange discards the
remaining queue in the original source, so a clone is not required in every order.

## Large clone reservations and later level loading

The next 120-request replay shard (episodes 8..10) had 119 surviving 900-frame
rows. BlackBird_22 failed during construction: Pink's all-goal reservation can
reach the explicit 2,048-object limit, exceeding two unrelated adapter budgets.
The corrected bird-collision allowance is 4 MiB: the measured 64-bit layout needs
3,474,672 bytes for 2,048 object rows, 618 definition adapters and the owner.
This changes admission, not the actual number of allocated level rows.

GameUpdate now sizes its retained otherBirds entry generations by possible bird
identities rather than pig-factory capacity. Each queued bird can retire its own
identity and three Blues children; two numeric split entries and the two Eagle
identities are reserved separately. Fresh generation objects and old aliases are
preserved. The exact BlackBird_22 seed `520a0207` now loads and survives 900 frames,
four launches and four ability inputs. It is also included in the longer physics
replay test. All 30 Birdday! 5 levels survive their separate 900-frame replay.

These checks do not establish complete wins, all possible input sequences or
native gameplay parity. Full suites and the remaining GUI routes are tracked
separately from these bounded replay results.

The completed later shard covers 137 requests in episodes 11..14: 133 survive
900 frames with 450 launches and 450 ability inputs. Its four rejected routes
are RADIO, KEYBOARD, SEQUENCER and ACCORDION. Together with the earlier shards
and repaired BlackBird_22, the retained catalog has successful bounded rows for
586 of 590 requests. The Keyboard implementation below postdates that shard.

## Keyboard and recording integration

Golden Egg GUI scenes now share Loader transport, opened/played state, asset
ownership, star/highscore persistence and Back routing. The soundboard regression
still passes after this refactor. Keyboard uses the verified original module
SHA-256 `7599c736b1f07c67aaaa69da81694d76d5ae15917e2e589aaaa3074bafe8e873`.
Restricted literal windows recover the eight key descriptors and twelve-note
song. Its native hash traversal determines shadow/key insertion order. Methods
2852, 4062, 6770 and 7085 supply press/swipe reentry, fixed hit rectangles,
responsive layout, the five-note puzzle and one-note-per-update autoplay.
Level-script prepare8107/closure8548 use the same actual Golden Egg Loader route
and GOLDEN_EGGS/BACKGROUNDS ownership as the soundboard.

The focused Keyboard check passes on Metal: wrong notes, swipe reentry without
held-note repetition, active-art pulse, sliding note history, autoplay with excess
elapsed time, exactly one saved star, sound toggle, resize and Back. Its 1024x768
C capture was inspected. Original combined native scene/timing parity remains
unverified. Radio, Sequencer and Accordion remain recovery tasks.

The preview now accepts `--record`, `--record half` and optional `--audio web`
through the existing pinned Common recording API. The native audio callback tees
its exact stereo output into Common's bounded queue; lifecycle changes quiesce
the device before allocating or freeing that queue. Unit checks cover parser
combinations, exact copied audio, variable callbacks, overflow, restart and
startup cleanup. Common owns the codec, MP4 writer and asynchronous capture.
The default destination remains private under Files/; files are never overwritten.

Live macOS checks record the actual game with half-size normal PCM, full-size AAC,
a simultaneous screenshot and SIGINT finalization. Each exits successfully.
Native AVFoundation readers decode video and nonzero audio from every result:
480x320/0.791741 seconds, 960x640/0.797398 seconds and
480x320/1.036599 seconds respectively. The Windows cross-build passes; Windows
runtime recording has not been exercised here.

The recording/Keyboard checkpoint passes all 808 Release tests in 495.23 seconds.
The preceding large-clone/canister physics checkpoint passes all 602 ASan/UBSan
tests in 1,974.12 seconds, including the extended BlackBird_22 replay. Neither
full-suite result includes the subsequent Sequencer/UI changes below.

Sequencer's pinned module SHA-256 is
`ca200bbcd3bafef273cb8078e2b39a00cc94c28b0828b190bfc75e33d65faae0`.
Prepare8678/closure9121 load GOLDEN_EGGS, INGAME_PIGS_OLD and BACKGROUNDS. Literal
windows within init2335 and updatePigFaces9746 recover the original pig/state
images. The restricted reader now permits a borrowed child-prototype view;
parameters/upvalues are never imported, and all selected-register reads must be
defined inside the opcode-restricted window. Whole closed-constructor readers
retain their stricter function-shape check.

TiledImage.lua SHA-256 is
`494c72442c0385771308d56e25e4157cdc1774ae88e81d2cc03d05284cb00b86`.
Its draw1066 emits row-major natural sprites after resetting render geometry,
ignores frame/parent scaling for the tiles, then draws ordinary Frame children.
Unit checks cover fractional/empty loops, actual pivots, alpha/direct mode,
refreshed bounds, complete child transforms and partial-command failure prefixes.

ImageButton hit1917 also accepts Image-derived receivers, with absent/false
padding selecting an empty table. It queries actual sprite bounds/pivot rather
than mutable draw dimensions, and returns before resource/angle access on zero
cumulative scale. The adapter now exposes that boundary for Sequencer's custom
bird slider. Tests cover those cases. An older toggle-setter failure fixture
needed its initial image restored to A, so the injected B failure remains at
the intended state-change boundary rather than the earlier resource hit query.

default_conf instruction59 derives tapRadius as `(15 * screenWidth) / 480` at
initialization; ordinary resizing preserves it. Sequencer retains its source
strict drag threshold, delayed first beat, one-column-per-update timer, face
changes, tempo-dependent bird motion and all-24-pigs/tempo >= .98 star puzzle.
The focused Metal interaction test passes press/drag, play/stop, all toggles,
one saved star, continued rhythm after winning, sound, resize and Back. Initial
and star-popup captures were inspected. The Windows cross-build passes.
Original combined native scene/timing parity remains unverified. The Sequencer
checkpoint passes all 810 Release tests in 464.09 seconds and nine focused
ASan/UBSan tests in 39.82 seconds. The Windows cross-build also passes. These
results precede Radio, Accordion and the shared GUI audio-fixture correction.

## Remaining Golden Egg GUI routes

Radio's pinned module SHA-256 is
`35b9f73ae548e9212398c77375c43531c1627d556c89937e29bea094f8c176d9`.
Prepare7542/closure7977 use GOLDEN_EGGS/BACKGROUNDS. The implementation preserves
its untuned entry, annular wheel hit test, shortest angular delta, four-unit
station spin, channel music lifetime, simultaneous buttons and secret station.
An isolated query in the pinned ARMv7 runtime establishes numeric constructor
traversal `2, 3, 1, 4, 3.5`; strict nearest-station comparison makes this order
observable at ties. The same query verifies the float phase/modulo arithmetic
used in the source's motion. This is numeric evidence, not a combined native
Radio capture. Its Metal interaction check passes all four stations, wheel
dragging, the secret station, one saved star, bird motion, audio toggle, resize,
Back and reentry. Escape during pending GUI loading now waits for active input.

Accordion's pinned module SHA-256 is
`b021ef2018148a983579adc3124a56d26b09e57c9289933e2f0a8ae8df1dbe5e`.
Prepare9276/closure9719 use GOLDEN_EGGS/BACKGROUNDS. The closed constructor window
recovers its five original note names. Methods2205/2636/5321/7847/9425/10921/11110
supply handle input, spring return, scaled layout, the three-second overextension
threshold, break art/audio, pig toggle, directional stereo tracks and source
volume updates. Desktop held-button state supplies stationary hold events across
the handle's per-update dragging reset. Compression tests move through the actual
left-edge input constraint rather than teleporting across it. The focused Metal
check passes all five notes, directional gain changes, pig toggle, release,
mute/unmute, resize, timed ripping, post-audio star persistence and Back/reentry.
The initial 1024x768 C capture was inspected; native whole-scene parity remains
unverified.

The Golden Egg interaction fixture now starts the reopened mixer and advances
actual samples, matching application audio setup. Radio verifies one live music
voice after each station transition. Starting the mixer in the shared resolution
fixture also affected layout tests that intentionally do not mix audio, exhausting
their queued click voices; live startup is therefore limited to the interaction
fixture. Eleven focused ASan/UBSan tests pass in 76.56 seconds before that final
fixture isolation. The Windows cross-build passes. The renewed full-suite and
catalog results are tracked after these changes.

The isolated-fixture checkpoint passes all 812 Release tests in 548.90 seconds,
including both full menu/gameplay integrations and the extended physics replay.
The later option-parser change separately passes Release and ASan/UBSan checks:
options are accepted on either side of the APK, with explicit `--` termination
for positional text beginning with a dash. A live run using `--inf APK --record
half`, bounded by `--frames 30`, exits successfully. Native AVFoundation decoding
reads its 1024x768 half-size Retina video, 0.329843-second duration and 7,104 nonzero
audio words. The Windows application and parser-test cross-build also pass.
These checks do not establish complete level wins or native whole-game parity.

## Desktop audio quality and duplicate effects

The user reports simultaneous duplicate effects during destruction and Mighty
Eagle use. Desktop playback now coalesces identical non-looping resource/gain/track
requests inside one application frame. Group selection and game callbacks still
execute; different clips, gains, tracks, later frames, stopped voices and loops
retain independent playback. The frame window uses unsigned mixer-handle
distances, including wrap. This is an explicit desktop presentation policy,
separate from native callback-count fidelity.

The pinned APK audio-header inventory contains 16-kHz mono material, one 16-kHz
stereo BeachAmbience file, and one 24-kHz powerup_awarded file. The desktop now
mixes in stereo at 48 kHz and uses a first-party normalized, 64-tap Blackman-windowed
sinc interpolator for integer ratios two/three. Source PCM is unchanged; integer
phases preserve its samples. Native equal-rate mixer arithmetic remains the
default research path. The device and recording use the configured rate, and
the output queue retains its previous 64-ms target duration.

Tests cover actual wood-destruction and Eagle clips, interleaved gain/track
requests, later/stopped retriggers, independent stereo channels, high-frequency
signals, loop boundaries, finite duration and chunk invariance. All original
decoded registration comparisons still pass. The initial audio checkpoint passes
813 Release tests in 353.69 seconds and seven focused ASan/UBSan checks in 5.75
seconds. The final frame-window refinement additionally passes the actual-APK
bank check under ASan/UBSan. A clean portable build passes all 387 tests; the
Windows cross-build also passes.

A live bounded macOS recording decodes as 48,000-Hz stereo float PCM with a
1024x768 half-size Retina video, 0.563488-second duration and 43,828 nonzero audio
words. The native output reports no underruns. Whole-scene native audio/timing
parity remains unverified; neither interpolation nor coalescing is claimed as
recovered Android mixer behavior.

The final stopped-mixer guard and fixture portability edits pass 20 focused
Release checks and three audio ASan/UBSan checks. The final Windows cross-build
also passes. A separate `--audio web` recording decodes at 48,000 Hz with two
channels, 1024x768 video and 46,848 nonzero audio words; native output again
reports zero underrun frames.
