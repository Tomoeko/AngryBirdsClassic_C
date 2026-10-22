# C source style

This guide is adapted from `PS3_Menu_C/STYLE.md` for the Angry Birds Android
5.2.0 reimplementation. It applies to first-party runtime code, tools, tests,
and shaders. Preserve recovered game behavior, resource names, simulation
timing, draw order, and platform portability.

## Reference and precedence

Use C11 and this project's `.clang-format`: four-space indentation, an
88-column target, attached braces, and no compressed one-line functions or
conditionals. The development constraints in `AGENTS.md` take precedence.

The Linux kernel C style guide and OpenBSD style guide are references for
focused functions, shallow nesting, clear declarations, descriptive helpers,
and centralized cleanup. Their formatting conventions do not replace this
project's settings. Use the C11 specification and SEI CERT C guidance when
language behavior or safety is in question. These are reading references,
not permission to copy external implementation code or add dependencies.

Use the installed ClangFormat for source formatting. The `ab-check-format`
target is available when CMake detects ClangFormat during configuration.

## Functions and control flow

- Give each function one recognizable job. An orchestrator should show phases
  in order; move a phase with its own state or policy into a named helper.
  Do not create helpers solely to shorten a file or hide one obvious expression.
- Prefer guard clauses for invalid inputs and inactive states. When branching
  requires several nested levels, extract a cohesive operation or reconsider
  the state representation. A linear table or switch may remain long when
  splitting it would obscure the mapping.
- Name helpers for their work, such as `update_sling_drag`, `step_world`, or
  `apply_contact_damage`. Avoid `handle_stuff` and `do_more`. Keep parameter
  lists narrow; group state only when it has a real lifetime or invariant.
- Use one cleanup path when several exits release the same resources. Name
  the label for its action, such as `release_level`. Direct returns are fine
  before resources are acquired. Make ownership transfer explicit.
- Use `const` for borrowed input that is not modified, `size_t` for sizes and
  indices, fixed-width integers for file formats, and `bool` for true/false
  results. Use named constants for format bounds and flags. Keep source
  resource identifiers unchanged when they are lookup keys.

For example, keep presentation phases visible without embedding the entire
runtime in one function:

```c
static void present_frame(GameRuntime *game, float elapsed_seconds) {
    update_camera(game, elapsed_seconds);
    update_animations(game, elapsed_seconds);
    sync_audio(game);
    build_draw_list(game);
}
```

This example illustrates source layout. It does not establish the original
game's update order or authorize variable-step physics. Actual ordering and
simulation timing must follow recorded evidence and remain stable in refactors.

## Data, ownership, and safety

- State who owns memory, file handles, decoded assets, audio objects, and GPU
  objects at each API boundary. Borrowed pointers must not outlive their owner.
  Release each owned resource on every success and failure path.
- Check input lengths before indexing, pointer arithmetic, multiplication,
  or allocation. Prefer subtraction-based bounds checks so the check itself
  cannot overflow. Validate resource counts against destination capacities.
- Check allocation, conversion, I/O, and decoding results. Do not use
  `assert()` to validate untrusted files or command-line input. Treat malformed
  APK/ZIP structures, Android resources, native binary metadata, level data,
  images, fonts, and audio streams as ordinary errors.
- Copy complete arrays with `memcpy` only when source and destination have
  matching element types and dimensions. Otherwise convert elements in a
  clearly named loop. A loop is appropriate when each element needs distinct
  work; nesting is not itself a defect.
- Keep native platform interop behind C interfaces, Metal calls inside its
  backend, and GLES2 calls inside its backend. Author shared decoding and
  gameplay support here; do not add external implementation libraries.

A bounded copy makes its preconditions and zero-length case explicit:

```c
static bool copy_span(uint8_t *destination, size_t capacity, size_t offset,
                      const uint8_t *source, size_t length) {
    if (!destination || (length && !source) || offset > capacity ||
        length > capacity - offset) {
        return false;
    }
    if (length) {
        memcpy(destination + offset, source, length);
    }
    return true;
}
```

For multiple owned resources, keep failure cleanup together:

```c
static bool read_payload(FILE *file, size_t length, size_t max_length,
                         uint8_t **output) {
    uint8_t *payload = NULL;
    bool okay = false;

    if (!file || !output) {
        return false;
    }
    *output = NULL;
    if (length == 0 || length > max_length) {
        return false;
    }
    payload = malloc(length);
    if (!payload) {
        goto release_payload;
    }
    if (fread(payload, 1, length, file) != length) {
        goto release_payload;
    }

    *output = payload; /* Ownership passes to the caller. */
    payload = NULL;
    okay = true;

release_payload:
    free(payload);
    return okay;
}
```

Here `file` remains borrowed; its caller closes it. Real code should supply
a project size limit and report why the operation failed. Validate array
bounds, allocation arithmetic, integer conversion, and ownership explicitly.

## Comments and names

Write comments for constraints, non-obvious reasons, lifetimes, provenance,
and fidelity limitations. Do not narrate the next statement. Explain why a
ratio, bit, threshold, delay, or ordering matters. When behavior comes from
APK resources or native binary analysis, identify the input and evidence
record; do not imply original-game fidelity from a C test alone.

```c
/* Preserve the atlas identifier; level data uses it as a lookup key. */
sprite->resource_id = atlas_entry->resource_id;
```

Prefer names that state units and roles: `elapsed_seconds`, `simulation_tick`,
`angle_radians`, `byte_count`, `source_offset`, `owned_texture`, and
`borrowed_level`. Distinguish screen pixels from world units and file offsets
from virtual addresses. Avoid changing public identifiers merely for taste.
Use a private `static` helper for logic local to one translation unit; add a
narrow internal declaration only when modules genuinely share an operation.

## Simulation, rendering, and performance

- Preserve evidence-backed physics step order, numeric precision, contact
  processing, damage rules, scoring, and random behavior. Do not change a
  constant or solver rule to make a screenshot look plausible. Record missing
  evidence and divergence instead.
- Keep scene traversal and draw order shared between Metal and core GLES2.
  Preserve clipping, blending, texture sampling, camera behavior, and timing.
- Avoid allocation, decoding, shader compilation, synchronous readback, and
  redundant state changes in ordinary frame rendering. Reuse buffers and
  textures. Keep shaders multiline and readable like the C source.
- Refactor hot paths with the same output before optimizing them. Measure
  startup, menus, level loading, sling dragging, active simulation, destruction,
  and result screens on representative scenes once they exist. Report the
  platform, backend, scene, frame time, and memory with performance claims.
  Keep the GLES2 core path correct without extensions.
- Use focused tests for bounds, decoding failures, state transitions, and
  reproducible gameplay traces. Compare against the pinned APK where evidence
  exists; retain native fidelity as unverified where it does not.
