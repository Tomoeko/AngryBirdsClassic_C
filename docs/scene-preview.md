# Sprite scene preview

Load level one's theme, ordinary sprites and opening camera sequence directly from the
pinned APK:

```sh
Files/build/angrybirds-preview --frames 128 --fixed-step \
    --capture Files/level-one.ppm --level-one \
    Files/com.rovio.angrybirds_5.2.0-12200520.apk
```

The level, catalog, object/theme atlases, composite definitions and sequence data
use first-party readers. The camera runs its recovered default sequence; object
poses remain at their loaded state. Three background layers, the ground fill and
two foreground layers use the verified native CPU paths. Theme1 resource choices
are explicit. The first bird occupies its derived idle sling pose, with transfer
time skipped. APK font/localization data supplies score text, and three static
HUD buttons use recovered layout and draw paths. Physics updates, active sling
input, character animation, full UI and gameplay scheduling remain open.
Theme PVR formats select opaque or alpha rendering; object
premultiplication and clamped linear sampling remain explicit preview choices.
The fixed 480x320 idle comparison matches every RGBA pixel on the reference
Android GLES2 driver. Metal sampling and native presentation remain open.

By default camera elapsed time comes from the host monotonic clock with the
recovered application delta cap. `--fixed-step` instead supplies binary32 1/30
per preview update for repeatable captures; it applies only to `--level-one` and
does not claim native presentation timing. With `--frames N`, capture saves the
last frame. Without a frame limit, it saves the first frame and keeps the window
open. Textures and draw storage are prepared before presentation.

In the loaded level preview, left-drag supplies one logical touch to the recovered
camera handler. Tap return, double-tap zoom and release velocity use that handler.
Focus loss clears the diagnostic gesture. This desktop adapter supplies filtered
input directly; original menu filtering, multi-touch delivery and gameplay input
are separate recovery tasks. Object poses remain fixed.

Render ordinary object sprites from a captured scene with the pinned APK:

```sh
Files/build/angrybirds-preview --frames 2 --capture Files/scene.ppm \
    --scene Files/scene.txt Files/com.rovio.angrybirds_5.2.0-12200520.apk
```

The scene input is an authored diagnostic format. It supplies object poses,
camera state, selected sprites and blend choices explicitly. It does not load
a playable level or reconstruct menus, backgrounds, trails, effects or animation
selection. A black diagnostic background leaves omitted layers visible.

Whitespace separates tokens. The header is `AB_SPRITE_SCENE_V1`, followed by
an item count from 1 through 1,024 and four hexadecimal binary32 words:
camera left, top, horizontal zoom and vertical zoom.

Each item supplies these fields, in order:

1. Exact APK SPRT entry path and sprite identifier.
2. Signed decimal integer layer, unsigned decimal collider, visibility (0/1),
   special-animation flag (0/1), and decimal blend enum.
3. Five hexadecimal binary32 words: interpolated physics X, physics Y, angle
   in radians, horizontal object scale and vertical object scale.

Blend enums are opaque=0, premultiplied=1, straight=2, additive=3, multiply=4.
Items enter in insertion order; preparation sorts ascending layers and retains
that order within each layer. Hidden objects and collider 7 are skipped.
Special animation wrappers reject explicitly. Pose-only frame changes can use
the same prepared ordering through `ab_sprite_scene_draw`.

The preview loads at most 16 PNG/PVR atlases with 128 MiB of retained decoded
assets. It prepares textures and fixed draw storage before presentation and
uses the recovered object-state and rectangle-to-clip geometry. Atlas paths
select the resource variant explicitly; full resource selection and target
frame parity remain open. Keep captures and scene inputs under `Files/`.

Preview APK menu components:

```sh
Files/build/angrybirds-preview --main-menu \
    Files/com.rovio.angrybirds_5.2.0-12200520.apk
```

This graph uses the actual logo/play images, MainMenuSlider, options factory,
ToggleButton and ToggleIndicator with an explicit offline Android component
profile. The interactive menu selects the largest regular UI and font profile
(1024x768), opens a 1024x768 window, and lays out at the native backing-pixel size.
It fills the actual window/fullscreen aspect ratio; resizing retains decoded
resources and recalculates source Scaling, positions, episode anchors and camera.
`--window-size 1280x720` chooses a different initial window size. The 480x320
profile remains an explicit research fixture in the component tests.

Play opens all 14 episode cards. Their arrows, scrolling and Back controls route
through the owned UI graph. The source theme1 layers and menu birds draw behind
it. The options panel opens/closes; sound and notification controls cycle their
state images. Episode requests now run through the C Loader and real theme
resources into basic level-selection screens. Their arrows, saved pages and Back
work. Level one's intro request now runs through its own Loader operation and
opens the recovered pack1 story, with source timing, scrolling and Skip. Its
natural ending writes encrypted watched state. The preview restores its owned
settings from `Files/AngryBirdsC/userstate/` and checkpoints both settings files
on clean exit; malformed existing files reject without automatic replacement.
Gameplay events remain retained pending level preparation. This preview profile
does not migrate Android app saves or implement the original global save scheduler.
Specialized episode roots and the loading overlay remain unfinished. F controls
native fullscreen. Startup, credits, effects, root services and gameplay remain unfinished. There is no audio device
output. Controlled input and headless Metal checks cover these components;
whole-screen native parity and manual window input remain unverified. KA3D/RVIO UI composites support positive axis-aligned context scale, including
transformed parts. Rotated/direct parent context states reject explicitly.
