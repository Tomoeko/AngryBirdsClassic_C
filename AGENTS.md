# Development guide

## Purpose

Reimplement Angry Birds Android 5.2.0 in readable, portable, first-party C.
The sole target input is the locally supplied APK:

```text
Files/com.rovio.angrybirds_5.2.0-12200520.apk
```

Its SHA-256 is
`b524b6b4b0ad999f46828546dee1bb047d6e00f724285a2e8c9f4b34a8bee7ad`.
Paths beginning with `Files/` are relative to the repository root. Verify the manifest, version, ABI inventory, and native
libraries against this exact file before making target claims.

The supplied APK, verified analysis of its binaries and resources, and captures
from that same build are the source of truth. Recover resource interpretation,
menus, level loading, physics, collision and damage rules, bird abilities,
animation timing, draw order, shaders, input, audio, scoring, and persistence
explicitly. Do not ship dummy gameplay, procedural replacement art, replacement
fonts, invented sounds, or guessed timing as recovered behavior. Unknown or
inaccessible behavior remains an open recovery task.

Chrome, desktop 5.0.1, Seasons, and other versions are outside the current scope.
Do not carry forward their code, assumptions, imported data, or parity claims.
The deprecated implementation is not a source or fidelity oracle.

## First-party implementation only

- Author the runtime, tools, tests, shaders, resource readers, physics, image
  decoding, and audio support in this project. Do not download, vendor, copy,
  link, or add third-party implementation libraries, single-header libraries,
  engines or package dependencies. This applies to offline
  tools and tests as well as the shipped runtime.
- The user authorizes `Tomoeko/Console_Common` as the shared first-party `Common`
  submodule. Use its GLES2/Metal and native window/fullscreen boundaries, keeping
  recovered Angry Birds scene and numeric behavior in this repository. Pin the
  submodule commit; initialize it explicitly before an offline build. This
  exception does not authorize other implementation dependencies. The user also
  authorizes adapting the build workflow and concise README structure from
  `Tomoeko/GameCube_Menu_C`.
- Do not restore Box2D, miniaudio, stb, Lua, or any other external implementation
  from the deprecated tree. Recover needed behavior from the target APK and
  implement it here. Do not substitute another upstream physics version for
  the target's observed behavior.
- Do not use FetchContent, ExternalProject, package bootstrap scripts, network
  downloads, or dependency managers to obtain source. Configuration, builds,
  and tests must work offline with the installed toolchain.
- Installed compilers, build tools, debuggers, disassemblers, and formatters may
  be used for development. The C standard library and native operating-system
  graphics, windowing, input, audio, and filesystem APIs are platform boundaries;
  keep their use isolated. This does not authorize general-purpose external
  runtime or tooling libraries.
- Treat original APK code and assets as private research inputs. Do not commit
  extracted scripts, decompiler output, native binaries, or copied original
  implementation code. Write our own C implementation with recorded evidence.
  Third-party research, documentation, and implementation studies are permitted
  by the user. Treat them as research aids, verify target behavior against the
  pinned APK, and write our own implementation. Never copy them exactly or
  vendor their source; research permission does not permit adding dependencies.
- If a format, codec, script behavior, or service cannot yet be supported under
  these constraints, record the gap and implement first-party support later.
  Do not introduce a dependency or silently replace the behavior.

## Source quality and organization

- Follow `STYLE.md` for source layout, helper design, comments, and review.
- Never author minified code, including shaders, tables, examples, scripts,
  and configuration. Use descriptive names and ordinary multiline formatting.
- Prefer C11, focused modules, explicit ownership, and narrow interfaces.
  Keep resource decoding, game state, physics, animation, input, audio,
  persistence, platform integration, and graphics backends separate.
- Keep the runtime self-contained in first-party C. Isolate any required Apple
  framework and Objective-C runtime interop at the Apple platform boundary.
- Use four-space indentation and the supplied `.clang-format`. Keep functions
  understandable and comments focused on behavior, constraints, or provenance.
- Check allocation sizes, file bounds, and malformed resource data before use.
  Pair every allocation, file, audio object, and GPU object with a clear release
  path. Avoid hidden global mutable state and undocumented fallbacks.
- Keep renderer-facing data independent of graphics API types. Share scene
  traversal, draw ordering, and presentation decisions across backends.
- Keep authored source and bundled shaders readable. Do not commit original
  APK resources, keys, captures, save data, or generated assets.

## Graphics compatibility and performance

- Treat core OpenGL ES 2.0 and GLSL ES 1.00 as the compatibility floor. Do not
  require ES 3 features or optional extensions for correctness. Guard optional
  fast paths and preserve a core ES 2.0 path.
- Build a separate Metal backend behind the same rendering contract. Keep
  Apple-specific source and framework dependencies conditional so that the
  GLES2 build remains portable.
- Use the highest supplied artwork/font profile across supported platforms.
  Regular Android UI tops out at 1024x768; splash and theme groups have their own
  resource policies. Render at the native drawable pixel size and actual window
  or fullscreen aspect ratio, with recovered responsive layout and correctly
  mapped input. Keep explicit lower-resolution research fixtures separate from
  application defaults. Do not decode or reload assets during ordinary resizing.
- Reuse textures, shaders, buffers, and render targets. Avoid per-frame
  allocation, shader compilation, texture decoding, synchronous readback,
  and redundant state changes in normal rendering.
- Preserve sprite order, clipping, blending, texture sampling, camera behavior,
  and animation semantics. Batch only when visible output keeps its order.
  Keep resolution and effect costs explicit. Measure frame time and memory
  on representative scenes before claiming an optimization is effective.

## Fidelity and evidence

The pinned APK and retained captures from its build are the fidelity oracles.
Record the APK hash, ABI, library hash, function address or resource path,
capture dimensions, device or emulator configuration, frame range, and comparison
method as appropriate. Keep file offsets and virtual addresses distinct.
Separate observed behavior, inferred behavior, and unknown behavior.

C tests establish implementation behavior; they do not by themselves establish
native fidelity. Keep fidelity explicitly unverified without a corresponding
native capture or independently validated trace. Do not mark an entire subsystem
complete on the basis of a smoke test or a plausible-looking screenshot.

Preserve resource identifiers exactly where they are lookup keys, including
original spelling errors. Keep logical game coordinates, framebuffer size,
displayed aspect ratio, and input coordinates distinct. Derive simulation step,
numeric precision, physics constants, and timing from evidence. Keep replay input
and random seeds explicit so divergences can be reproduced.

Document unavailable online services and any local behavior separately. Do not
invent service responses or describe them as equivalent to the original build.

## Recovery priorities

Use `ROADMAP.md` to track evidence and implementation gates. Future code recovery,
functional behavior, and validation take priority over expanding documentation.
The user has authorized proceeding through the roadmap to completion. Continue
implementation and validation without stopping at phase boundaries. Record real
blockers and unverified fidelity honestly; never mark a gate complete without
its evidence.

Keep all ignored inputs, extracted resources, analysis databases, generated
assets, captures, user state, logs, and build products under the repository's
`Files/` directory, normally `Files/AngryBirdsC/`. Preserve the supplied APK
unchanged. Keep each research job below 256 MiB of retained working artifacts;
use bounded extraction and compact evidence rather than duplicating the APK or
retaining repeated full memory or GPU dumps. Prune superseded generated output
after validation without deleting original inputs or unique evidence.

An isolated Android reference platform is needed when no device is available.
Official binary SDK tools and emulator system images are development tools, not
project implementation dependencies. Keep their installation and AVD state in
`Files/AngryBirdsC/android-reference/`, separate from per-job research output.
Allow up to 8 GiB for that reference installation, pin package versions and
checksums, disable snapshots, and remove compressed downloads after verified
extraction. This exception is necessary for the authorized native capture work;
the 256 MiB limit still applies to each ordinary research job's retained output.
Do not add SDK downloads to project configuration or builds, or copy SDK
implementation source into project code.

## Privacy and documentation

Before sharing or publishing, inspect source, logs, manifests, and generated
metadata for home directory paths, account names, device identifiers, private
content, and extracted assets. Use relative project paths and logical input
names in public examples.

Keep user-facing build and status instructions in `README.md`. Document only
implemented features as available; keep planned work in `ROADMAP.md` and future
feature evidence and fidelity gaps under `docs/`.
