# Target evidence ledger

All APK paths below refer to the unchanged repository input
`Files/com.rovio.angrybirds_5.2.0-12200520.apk`. No other game version is an
implementation oracle. Raw analysis, keys, decoded resources, and generated
inventories remain private under `Files/AngryBirdsC/`.

## Confirmed target identity

| Item | Observation |
| --- | --- |
| APK SHA-256 | `b524b6b4b0ad999f46828546dee1bb047d6e00f724285a2e8c9f4b34a8bee7ad` |
| APK bytes | 51,653,019 |
| Package / version / code | `com.rovio.angrybirds` / `5.2.0` / `12200520` |
| Android API range in manifest | Minimum 9; target 22 |
| Required graphics feature | `glEsVersion=0x00020000` |
| Launcher | `com.rovio.fusion.App`, MAIN / LAUNCHER intent filter |
| Declared components | 18 activities, 2 services, 3 receivers |
| ZIP entries | 3,481: 798 DEFLATE and 2,683 stored |
| Total decoded entry bytes | 236,017,954 |

The first-party `ab_apk` tool independently verifies these fields and all ZIP
entry CRCs. Its inventory matches an independent installed-tool extraction for
every path, compressed and decoded size, method, CRC-32, and decoded SHA-256.
That comparison validates resource access, not original-game behavior.

## Resource inventory

The complete path and hash inventory is private at
`Files/AngryBirdsC/apk-inventory-c.json` and is reproducible with
`ab_apk inventory`. Counts by extension cover the whole APK, including Android
resources and embedded service SDK data:

| Extension | Entries | Observed interpretation |
| --- | ---: | --- |
| `.lua` | 1,529 | All recover to the target's Lua 5.1 bytecode header |
| `.png` | 786 | All first-party decoded RGBA8 buffers match independent ImageMagick hashes |
| `.mp3` | 347 | First-party MPEG-2 Layer III decoding matches every complete native PCM hash, format and sample count |
| `.dat` | 332 | Sampled font data begins `KA3D` / `FONT`; detailed parsing pending |
| `.pvr` | 243 | Sampled textures use PVR version 3; pixel variants require inventory |
| `.xml` | 157 | Includes Android binary XML and other resources |
| `.fx`, `.vs`, `.ps` | 47 | Original GLES2 shader resources; retained only in the private APK |
| `.ttf` | 5 | Font resources; interpretation remains pending |
| `.so` | 2 | ARM and ARMv7 native game libraries |

The archive contains 655 entries under `assets/data/levels/`, 506 under
`assets/data/scripts/`, 198 under `assets/data/scripts_common/`, 18 under
`assets/data/blocks/`, 536 under `assets/data/images/`, 83 under
`assets/data/fonts/`, and 345 under `assets/data/audio/`. These are file counts,
not verified playable-level or feature counts.

The manifest names billing, advertising, social, push-notification, update,
and video components. Their presence does not establish current server
availability or authorize invented local service responses. Java-level startup
and integration behavior still require DEX analysis.

## Native analysis target and address conventions

| Library entry | SHA-256 |
| --- | --- |
| `lib/armeabi/libAngryBirdsClassic.so` | `f5de1d3b3e7bacf64887ae3f27a16eecd8bc9b68e96fb4b106260117fb922744` |
| `lib/armeabi-v7a/libAngryBirdsClassic.so` | `ada9bc1a1469c30a9bf906c4293df3ff1a75485b40ddbfb146debbc41f14d38c` |

Initial analysis uses the ARMv7 ELF32 little-endian library. Its ELF load
addresses start at zero. The current Binary Ninja ELF view starts at `0x10000`;
addresses in the following table are Binary Ninja analysis virtual addresses,
not file offsets or runtime relocated addresses. Resolve symbols by their full
name when reopening the target. For example, `FileUtil::decrypt` is ELF symbol
value `0x33f6ac` and analysis address `0x34f6ac`.

| Analysis address | Symbol / observation |
| --- | --- |
| `0x2d5e5c` | `getStaticKey()`: constructs the 32-byte resource key; bytes remain private |
| `0x34f6ac` | `FileUtil::decrypt`: selects the shared AES wrapper |
| `0x4e4390` | `LuaFileUtil::decrypt`: selects the same wrapper |
| `0x4e4850` | `LuaFileUtil::loadBundleFile`: reads, decrypts, and decompresses a bundle asset |
| `0x4e4698` | `LuaFileUtil::decompress`: dispatches through the original archive abstraction |
| `0x7a94f0` | `util::AES::Impl` constructor: key size and padding selection |
| `0x7aa3c0` | `util::AES::decrypt`: 16-byte block check and final-byte padding count |
| `0x91c0b4` | `AES::Decrypt`: CBC chaining with a zero first-block IV in mode 1 |
| `0x7dd910` | `io::detail::ZipFileLZMA::open`: dispatches to the LZMA stream reader |

JNI exports include `JNI_OnLoad` and the Fusion renderer's native init,
update, input, pause/resume, resize, and deinit methods. `GameLua` and `b2World`
symbols identify gameplay and physics boundaries for further investigation.
Their names do not establish a recovered update order or physics version.

## Resource recovery observations

The resource-key reader obtains all 32 bytes from verified data operands in the
pinned library's `getStaticKey` constructor (ELF `0x2c5e5c`). Its field locations
and ARM immediate encoding are checked against a private native key reference;
key bytes remain in the supplied input. The reader verifies the exact library
identity and executes no native instructions. `ab_apk` recovery/inventory/search
commands can now obtain the key automatically; a private key-file override
remains available for research. Direct level-1 and block-catalog recovery retain
their recorded decoded hashes. The reusable packed-resource reader bounds its
encrypted, decrypted and unpacked payload buffers. No original native code or
key material is embedded in the project implementation.

The sampled Lua resource chain is encrypted APK entry, AES-256-CBC with a zero
IV, a nine-byte `89 4c 5a 4d 41 0d 0a 1a 0a` wrapper, LZMA-alone properties and
known decoded size, then Lua bytecode. The target AES padding wrapper accepts
a final count from 0 through 16 and does not check the preceding padding bytes.
The first-party decoder preserves that observed input policy.

The C AES implementation passes AES-128/192/256 block vectors and a multiblock
CBC vector, including in-place decoding. Its substitution boxes are derived
from finite-field arithmetic; no external implementation tables or library
code are vendored. C LZMA and single-file 7z support pass synthetic valid,
truncated, invalid-property, size-limit, and checksum-failure checks.

| Recovered entry | Decoded bytes | Decoded SHA-256 |
| --- | ---: | --- |
| `assets/data/levels/pack1/Level1.lua` | 3,276 | `5f9d3c385be421a95f18ceb038d9d7d7d71eeaa982b000b83856f70d22adf8db` |
| `assets/data/scripts_common/gamelogic.lua` | 165,374 | `5b89882fe2a922185b3aa321b16eefb41b51116c75e57dc0ebd885e7eceb6487` |
| `assets/data/scripts/options.lua` | 1,061 | `876192688bde1e99659eb9ff533aa7e66a4ed4190d627d1423ec46154587476a` |

All 1,529 C recovery sizes and SHA-256 values match independent installed-tool
decryption and decompression. There are 1,528 custom LZMA wrappers and one 7z
wrapper (`options.lua`), totaling 12,988,374 decoded bytes. Private inventories
are `Files/AngryBirdsC/lua-inventory-c.json` and
`Files/AngryBirdsC/lua-inventory-reference.json`. The Lua headers encode version
5.1, little endian, four-byte int, size, instruction, and number fields, and
non-integral numbers. The first-party structural reader parses 8,559 prototypes,
1,559,025 instruction words, and 515,470 constants across all chunks. These are
structure checks, not proofs of script behavior or operand validity.
The decrypted `assets/config.json` also uses the supported 7z container; its
configuration semantics remain pending.

The static constructor pass recovers all 655 level-related chunks, each with no nested
functions. It handles bounded constant and register propagation, table creation,
assignments, known table lookups, number negation, list constructors, and final
returns. Calls, branches, loops, and unresolved external globals produce errors.
The pass also supports finite binary32 addition, subtraction, multiplication,
and division, with no implicit numeric conversion or division by zero. Explicit
prerequisite chunks can supply globals; imported table graphs remain read-only,
local assignments override imported names, and known nil values retain their
meaning. Focused checks cover import ownership, nil/ordinary overrides,
mutation rejection, arithmetic rounding, signed zero, and allocation failures.
It is an offline recovery pass, not a general-purpose Lua runtime. The private
level coverage inventory is `Files/AngryBirdsC/level-inventory-c.json`.

Seventeen of 18 block-data chunks now recover. `blockBases.lua` uses
`collider_types` from `materials.lua`. `itemsReactive.lua` still reads unresolved
globals `defaultBuffParticleAmountLimits` and `defaultFeatherParticleAmountLimits`.
A first-party exact-constant search across all 1,529 Lua chunks finds the former
only at its read in `itemsReactive.lua`; this establishes no default value.
Its initialization and the complete block-loading order remain open.

`pack1/Level1.lua` yields original global fields including `world`,
`worldGravity`, `physicsToWorld`, camera data, theme identifiers, object counts,
and joint and particle tables. Its JSON research export retains key types and
exact numeric bits. Original Lua table iteration and application of these
fields to the game's native interfaces still require recovery.

## Image recovery

The PNG reader decodes all 786 entries to 92,596,704 RGBA8 bytes in total.
Independent installed ImageMagick decoding matches every pixel-buffer SHA-256
and size exactly. The private evidence inventories are
`Files/AngryBirdsC/png-inventory-c.json` and
`Files/AngryBirdsC/png-inventory-reference.json`. Original sample and alpha
values are preserved; no alpha-association or color-profile conversion occurs.

Observed PNG variants are 478 RGBA, 277 indexed, 25 grayscale-alpha, and 6 RGB;
all use eight-bit samples and no interlacing. The PNG reader also supports
eight-bit grayscale and transparency keys. Tests exercise every row filter,
palette bounds, truncation, CRC errors, and decoded limits. Android nine-patch
and color-profile metadata are not interpreted as layout or color transforms.
The [W3C PNG specification](https://www.w3.org/TR/png-3/) is the format reference.

PVR inventory finds 231 version-3 files and 12 legacy version-2 files. Version-3
formats are 194 RGBA4444, 2 RGB565, 4 RGBA8888, and 31 ETC1. Legacy files use
the RGBA4444 format code; two omit channel masks. All original v3 files have
zero metadata bytes and one surface, face, depth slice, and mip level.

The first-party PVR reader decodes all 243 entries. Every RGBA8 hash and dimension
matches independent installed PVRTexTool conversion; total decoded bytes are
376,146,040, processed one texture at a time without retaining the full set.
The private inventories are `Files/AngryBirdsC/pvr-inventory-c.json` and
`Files/AngryBirdsC/pvr-inventory-reference.json`. RGB565 uses nearest normalized
eight-bit conversion; an initial bit-replication discrepancy was detected and
corrected by the independent comparison. Native channel-mask data at analysis
address `0xb2a118` confirms the RGBA4444 masks; `0xb2a010` confirms RGB565 masks.
Neither resource conversion nor these comparisons establish GPU sampling or
blending parity.

The [PVR format specification](https://docs.imgtec.com/specifications/pvr-file-format-specification/html/topics/pvr-header-format.html)
and [Khronos ETC1 specification](https://registry.khronos.org/OpenGL/extensions/OES/OES_compressed_ETC1_RGB8_texture.txt)
are format references. Decoder implementations and bounds checks are authored
here; no codec source or library is imported.

## Sprite metadata recovery

All 237 KA3D/SPRT files contain version 1 sprite metadata: a texture reference,
a signed 16-bit count, then named regions with six signed 16-bit fields.
The container tags and lengths are big endian; strings have a signed 16-bit
byte length followed by their encoded bytes. All identifiers in these sheets
are ASCII. The first-party bounded DAT and sprite readers reproduce every
record in the independent parse: 5,089 regions in total. Private evidence is
`Files/AngryBirdsC/sprite-inventory-c.json`,
`Files/AngryBirdsC/sprite-inventory-research.json`, and
`Files/AngryBirdsC/analysis/sprite-loader-native.json`.

Native loader `sub_5ac65c` reads the six region fields at analysis addresses
`0x5ac85c` through `0x5ac89c`, then calls `SpriteSheet::createSprite` at
`0x5ac8c4`. `DataInputStream::readShort` at `0x7dd6c4` sign-extends its
big-endian result. The original `REWARD_VIDEO_BUTTON_ICON` in the 480x320
button sheet has Y = -1; `PIGLETTE_PARTICLE_1` in the base block sheet has
zero width and height. Both are preserved without clamping or dropping them.
Native named-region insertion replaces an existing definition; lookup in the
first-party representation likewise returns the last definition. The target
files contain no duplicate names within a sheet.

Tests check signed extrema, zero-area data, borrowed strings, replacement
lookup, skipped unknown chunks, versions, allocation limits, trailing bytes,
duplicate SPRT chunks, and truncations with adjusted outer lengths. Texture
sampling, coordinate transforms, draw order, and font layout remain open.

## Bitmap font recovery

The first-party UTF-8 converter matches 65,536 native UTFConverter mode-2 steps
and 4,096 original toUTF32string calls (8,765 emitted values). ASCII consumes
one byte. Prefixes C0..F7 select two through four bytes; the target masks later
bytes without checking continuation bits, overlong encodings, surrogates or
Unicode range. Invalid/truncated prefixes consume one byte and preserve the
code output; empty input consumes zero. Whole-string conversion skips failed
bytes without replacement characters. The C boundary uses caller-owned bounded
storage. Native methods execute in the isolated reference with controlled byte
spans; no interpreter is involved. Provenance is `analysis/utf8-native.json`.


All 37 KA3D/FONT files use version 1 and contain 12,577 glyph records in total.
Each header stores a texture identifier, signed leading and tracking, and a
signed glyph count. Each glyph has an unsigned 16-bit identifier and five signed
16-bit fields: X, Y, width, height, and Y pivot. Every exported record matches
the independent parse. Private inventories are
`Files/AngryBirdsC/font-inventory-c.json` and
`Files/AngryBirdsC/font-inventory-research.json`; native evidence is
`Files/AngryBirdsC/analysis/font-loader-native.json`.

`BitmapFont::loadDAT` is at analysis address `0x5b360c`. Assembly after the
version-1 glyph read applies `uxth`, preserving identifiers above `0x7fff`
as positive values. The loader also accepts version 2 with 32-bit identifiers;
first-party tests cover this layout, but no target file uses it. Four Japanese
font files store height -2 for the U+3000 space glyph; the reader retains it.
The original leading/tracking getters at `0x5b0a68` and `0x5b0a70` confirm the
header field order. Ascent is the maximum of zero and all Y pivots; descent is
the maximum of zero and all height-minus-pivot values. Native construction
initializes both to zero, and `getHeight` at `0x5b0a48` returns their sum.

Tests cover both versions, unsigned identifiers, negative dimensions, metrics,
allocation bounds, invalid versions/counts/string lengths, adjusted-length
truncations, trailing data, and duplicate FONT chunks. Unicode decoding,
text anchoring and draw-time layout remain open.

Native UTF-32 range width/height methods at analysis 0x5b15f0 and 0x5b11f8
match 4,096 controlled cases, including 2,495 empty ranges and 1,862 negative
widths. The entire empty string returns zero. Otherwise start/count clamp to
the available range, signed glyph widths accumulate and tracking * (count - 1)
is added even when the range is empty. Missing glyph width is leading / 3,
with signed division toward zero: assembly at 0x5b15cc calls virtual slot 11,
which resolves to getLeading, rather than getHeight. Measured height is the
greatest of zero and known signed glyph heights. No newline policy is applied.
The original range methods execute with bounded read-only font/tree/string
metadata and original virtual methods; no GL context is needed. C rejects
out-of-range int32 arithmetic and invalid storage. Unicode decoding and complete
layout remain open in this measurement comparison. Provenance is
`analysis/font-metrics-native.json`.

The native UTF-32 drawString at analysis 0x5b16e8 separately matches 4,096
controlled layouts, all 2,842 glyph callbacks and restored context rotation origins.
Top/center/bottom alignment uses ascent/descent; horizontal center halves the
signed measured width with arithmetic shift before float conversion. Each known
glyph advances by (signed width + tracking) * scale_x; absent glyphs neither
draw nor advance. Sprite pivot getters add 0.5 then truncate, including negative
pivots. The original method subtracts unscaled rounded pivots to derive temporary
context rotation origin from the original text position/origin, then restores that
origin after drawing. The bounded C layout exposes these callback fields without
allocating. This comparison uses the original custom-glyph callback boundary;
actual Sprite/Image submission, Unicode decoding and final pixels remain open.
Provenance is `analysis/font-layout-native.json`.

The normal native font-to-Sprite-to-Image path separately matches the same
4,096 layouts and 2,842 glyphs. Native Sprite.drawScaled applies rounded pivot
anchors and signed scaled dimensions before the controlled Image callback.
Every local rectangle origin, width/height, rotation origin and controlled UV
component matches. Atlas UVs are controlled inputs in this corpus. A first-party
unrotated adapter appends glyph quads to the shared draw list. The Apple
preview renders the APK's 480x320 FONT_SCORE and decoded PVR atlas with authored
text `31380`; its 480x320 RGBA8 hash is
`c17f43bf0d7fdfac90c5b00d8d2c76ebbf992cecff9ae11ce654be3f3734fe73`.
The retained preview is a visual inspection artifact, not a target screen or
Android pixel comparison. Full native image transforms/effects/Unicode remain
open. Provenance is `analysis/font-image-native.json`.

## Localization recovery

The four KA3D/TEXT catalogs contain 29 locale groups, 510 identifier entries,
and 4,576 translations, including 726 empty strings. Version 1 TEXT payloads
contain LDAT (locale list), LIDS (identifier list), and one TXGP per locale in
the same order. Counts and byte-string lengths are signed big-endian 16-bit
fields. Translation strings retain their original UTF-8 bytes; JSON diagnostics
use the project's byte-preserving escapes. Every source byte matches the
independent parse. Private evidence is
`Files/AngryBirdsC/text-inventory-c.json`,
`Files/AngryBirdsC/text-inventory-research.json`, and
`Files/AngryBirdsC/analysis/text-loader-native.json`.

`TextGroupSet::loadLocaleCodes` at analysis address `0x5bd598` reads LDAT;
`loadTextGroup` at `0x5be0a4` reads LIDS and selects TXGP by the locale index.
The native text loader checks that LIDS precedes TXGP. First-party parsing
checks every group, nested chunk bound, list count, string length, allocation
budget, and group count. Synthetic tests distinguish a present empty value
from a missing key and retain multi-byte text unchanged.

An additional isolated Android comparison passes the four original DAT files
directly to the native TextGroupSet parser through a bounded read-only JNI file
adapter. All 29 locale lists and 4,576 ordered native map entries match the C
decoder byte for byte, including all 726 empty translations. Native TextGroup
and Resources lookups match 4,808 query triplets, including every existing key,
empty/missing identifiers, repeated misses, UTF-8 and embedded zero bytes.
Unknown groups and identifiers return the identifier; existing empty values
remain empty. The lookup-only Resources snapshot uses the actual native parsed
group sets and an explicitly selected locale. Original missing-ID caching adds
174 map entries during the trace; the C value-only provider does not reproduce
that internal allocation/cache. Native getTextGroup analysis distinguishes an
unloaded locale from one absent in the data; neither path silently picks a
fallback. The prepared C provider reports an unavailable locale as failure.
The corpus SHA-256 is
`cdb7eae8e02c786f41ec089a5b8074493ad6abf1011103819fee2867d1fbfdd9`;
private provenance is `analysis/text-native/native.json`.

The C provider composes with the recovered localization group/cache helper.
Android GameLua locale selection also matches 4,096 original calls with the
platform's default locale string supplied through controlled JNI methods. It
normalizes any lowercase `ja` prefix to `ja_JP` before exact matching against
available BASIC locales. Unmatched inputs select `en_EN`, even when absent from
the available list. Cases include 1,401 Japanese prefixes, 520 empty locale
lists and 1,718 absent fallback results. Native platform string conversion and
the selection method execute; Java Locale.toString values and available-list
subsets are controlled. The corpus SHA-256 is
`e74366b77a9db953288301cfc9679f953c4906c220af9a9514cb5601d5a115ec`;
provenance is `analysis/text-native/locale-native.json`. Live platform mapping,
lazy loading, native missing-ID cache allocation, formatting, UI bootstrap and
actual rendered text remain open recovery tasks.

## Composite layout recovery

There are 44 KA3D/COMP version-2 files and eight RVIO/COMP version-1 files.
Together they define 842 composites and 7,954 ordered sprite parts. KA3D parts
store a sprite identifier and signed 16-bit position; the native add operation
uses unit scales and zero rotation. RVIO stores a part name, separate sprite
identifier, position, three big-endian binary32 fields, and two flip flags.
The first-party reader preserves the numeric bits without applying transforms.
All records match the independent parse. No target composite contains a
KA3D version-2 auxiliary point; the native loader reads and discards such
records, and the first-party reader retains them for inspection.

Native `SheetLoaderDAT::loadCompositeSet` is at analysis address `0x5ace40`;
the KA3D part reads are at `0x5ad03c`, `0x5ad08c`, and `0x5ad0a0`. RVIO reads
the float fields at `0x5ad888`, `0x5ad894`, and `0x5ad8a4`, then the flip flags
at `0x5ad8b4` and `0x5ad8bc`. Simple-part initialization is at `0x597a74`.
Private evidence is `Files/AngryBirdsC/composite-inventory-c.json`,
`Files/AngryBirdsC/composite-inventory-research.json`, and
`Files/AngryBirdsC/analysis/composite-loader-native.json`.

Tests exercise both root formats, KA3D versions 1/2, raw float bits, signed
positions, auxiliary records, empty sets, malformed nested counts and strings,
allocation budgets, adjusted-length truncation, trailing data, and duplicate
COMP chunks. Native scale/rotation/flip application and rendered composition
remain unverified; resource record order alone does not establish draw parity.

## Sprite geometry foundation

`GameObjectRenderer::drawSprite` (ELF `0x2cd920`) and the following
`Sprite::draw` matrix path (`0x5a4700`) match 4,096 original calls, with all
four transformed XYZ vertices compared bit for bit. Controlled signed sprite
dimensions/pivots, nine placement floats and UVs enter the original functions;
an authored Image virtual callback records their output without graphics.
The implementation preserves pivot rounding, both identity products in matrix
multiplication, signed zero, and native Y-product/X-product accumulation.
This covers the explicit-matrix path; ordinary object state assembly and scene
traversal remain open. Private provenance is `analysis/object-sprite-native.json`.

The ordinary `GameObjectRenderer::draw` route (ELF `0x2cdfac`) also matches
4,096 calls through the original SpriteAsset and anchored Sprite methods to a
controlled Image callback. All local rectangle, rotation matrix, translation,
scale, pivot, opacity and angle words match. Its translation is the negated
camera origin plus twenty times interpolated physics position, divided by
object scale; the later image scale multiplies zoom by that object scale.
These divisions and later multiplications cannot be canceled without changing
rounding. Provenance is `analysis/object-render-native.json`.

Camera traversal at ELF `0x2d43a4` visits an ascending integer-key layer map and
each layer's insertion vector. A bounded retained read verifies all 28 renderer
identifiers across layers zero, two and four, with the original process resumed
in a finally block. This is a retained membership/order check, without native
draw callbacks or layer-removal/reinsertion coverage. The authored scene adapter
prepares stable ordering outside pose-only frame updates, then composes ordinary
object state and image rectangles into Common clip-coordinate commands.

The ordinary scene/visual adapters also accept a stable resource provider backed
by the active registry and synchronized texture cache. Construction can borrow
sprite dimensions from those resources without loading a separate sprite set.
All 22 retained Level 1 selections bind; missing assignments remain empty until
explicit reassignment. Texture loss and composite rejection preserve scene
bindings. Debug and Release Common Metal captures match the fixed-atlas control
across 153,600 pixels before and after cache retirement/reload, using the authored
castle camera's 19 visible draws. This checks resource composition and GPU image
selection, without establishing original-frame Metal fidelity or native sprite
ownership. The private record is `analysis/input/resource-scene-integration.json`.

Theme graphics transitions use the original `loadThemeGraphics` prototype and
`Set.difference` from common utilities as isolated native oracles. All 282,624
output words match across 4,096 dense-array/string cases, including duplicate
values, editor appends, shared tables and 158 controlled callback failures.
Both differences precede callbacks; old-only groups release before current is
assigned, then new-only groups load. All 31 authored theme lists also transition
through real CPU resources at 480x320, with repeated selections and final empty
source/registry state checked. The record is `analysis/input/theme-assets-native.json`.
Sparse/hash-keyed lists, theme-name selection and complete level-start scheduling
remain outside this boundary.

The scene preview reads 16 ordinary object poses/selections and camera fields
from the same retained observation, loads two APK atlases, and presents the
visible subset on Metal. Its black diagnostic background omits backgrounds,
menus, trails and effects. The 960-by-640 capture is a backend/resource/geometry
integration check; it has no native final-frame pixel comparison. Debug default
checks pass (99), new native comparisons pass in Debug and Release, and the
Windows preview cross-compiles with project warnings treated as errors.

`GL_Image::draw`'s rectangle overload (ELF `0x5cd34c`) matches 4,096 original
cases: 1,542 submitted draws, including 505 rotated and 586 direct-coordinate
draws. The oracle supplies bounded context/state and already-active batch
storage, executes original transform/culling/batching code, and records six
vertices plus UVs without a GL driver. Every coordinate, depth constant,
triangle/UV order and rejection decision matches. Rotated and unrotated paths
have different edge-inclusion rules. Viewport offsets affect the dimension
subtraction, with no additional offset applied to these coordinates.
Common's GLES2/Metal shaders can receive recovered clip coordinates directly;
the resource font preview uses the unrotated rectangle route. This does not
establish final-pixel, effect, scissor or complete scene parity. Private
provenance is `analysis/image-rectangle-native.json`.

The Apple window presents fixed drawable dimensions without taking keyboard
focus in its explicit background check. Debug/sanitized and Release runs
complete eight textured frames and verify controlled native mouse down, drag
and up at top-left coordinates, then emit CLOSE once after native closure.
Key events are mapped but not covered by this input check. This validates
platform plumbing, not target aspect/input or gameplay policy. Normal
presentation submits asynchronously; an explicit finish waits at the check
boundary. Bounded offscreen capture is used by the exact RGBA8/BGRA8 GPU pixel
tests. Capture/window integration does not establish native game-frame parity.

`CoordinateTransforms` methods at analysis 0x3720f0 and 0x372a9c..0x373284
(ELF addresses subtract 0x10000) match 4,096 controlled native sets. The C
helpers cover all six screen/world/physics directions for points and deltas,
plus screenDistanceToPhysicsScale. Physics conversion multiplies by binary32
0.05 or twenty; screen conversion subtracts top-left before zoom multiplication
and divides before adding in the inverse. Deltas transform zero and the endpoint
separately, then subtract. LLVM confirms this order, including translation
cancellation at large offsets. The scalar method uses only horizontal zoom,
then multiplies by 0.05. Native functions read a controlled camera's top-left
and two zoom components through their actual getters. No graphics or Lua state
is involved. All 102,400 output words match, including 12,343 zero components,
signed zeros, subnormals and negative zoom. Nonfinite/overflowing arithmetic
and zero inverse zoom are rejected by the C boundary. Camera setup, viewport,
input mapping and integrated draw parity remain open. Provenance is
`analysis/coordinates-native.json`.

CameraSystem's left/top/right/bottom, width/height and rendering-left/top
methods (prototype offsets 4707, 4846, 4986, 5125, 5950, 5850, 6049 and 6150)
match 4,096 original sets. The unchanged selected functions execute in the
original Lua implementation with controlled center, dimensions, shake and zoom.
Each side independently uses (pixel_dimension * 0.5) / zoom; world dimensions
divide the original dimension by zoom. The rendering origin subtracts shake.
The multiply/divide order matters for subnormal dimensions. The C viewport also
matches all four native camera fields in a later retained process observation
given its actual Lua center/dimensions/shake and native zoom. That scoped read
resolves the known CameraNative setter through the shared Lua globals and its
checked three-light-userdata dispatcher binding, then reads only the native
camera's four scalar fields. It resumes the target in a finally block. This
is a current-state check, not a field retroactively captured in the 96-frame
trace. The native zoom setter at analysis 0x2e3e34 copies its single input to
both zoom components. Movement, limit enforcement, shake generation and
integrated scene rendering remain open. Provenance is
`analysis/camera-viewport-native.json`.

The default CameraMover now matches 128 original command sequences of 64
operations: 1,137 resets, 981 position targets, 1,123 zoom targets, 948 target
clears, 3,161 updates and 842 standalone cubic calls. All 16 numeric/presence
state words match after every command, including reset-preserved start fields,
the immediate zoom setter's distinct `startZoom` field, upper-only timer clamps,
negative elapsed time and continued evaluation after completion. The isolated
native probe controls Vector2 construction/identity multiplication. Two selected
stripped prototypes receive empty debug upvalue names solely to permit public
upvalue binding; their executable properties, constants and instructions are
unchanged. Custom tweens, original vector identity/metatables, controller order,
limits, shake and final game-frame behavior remain separate. Debug ASan/UBSan,
Release, all 115 default tests, formatting and Windows compilation pass.
Provenance is `analysis/camera-mover-native.json`.

The default CameraLimitEnforcer matches 4,096 original cases: eight initialized
limit fields, snap-line projection, typed partial zoom clamps, hard position
correction and elastic input results, all 22 output words per case. Initialization
uses the first and last snap extents, including the original
`screenHeight/farZoom-bottomBorder` top-border expression. Hard correction checks
extents twice and uses a midpoint when opposite edges remain broken; the upper
edge is optional and the bottom edge is always checked. Its `originalX`/`originalY`
comparisons read globals; a whole-APK search for `originalX` finds only this read.
Default input enforcement clamps supplied zoom before position. An earlier probe
bound its zoom method to the neighboring final-values no-op; the corrected
prototype 2171 and regenerated corpus supersede that result. Horizontal input resistance retains
the original squared-distance rational function and separate arithmetic. The
native probe executes the original extents, clamp, projection and squared-length
helpers; numeric-table copying and single-snap fallback are controlled boundaries.
The extended corpus includes signed zero, infinities and quiet/signaling NaNs as
supplied zoom. Input enforcement clamps them before position arithmetic; direct
hard-position calls with zero/nonfinite supplied zoom are excluded explicitly.
Debug ASan/UBSan, Release, formatting and Windows compilation
pass. Zoom-only subclasses, fit helpers, snap setup, controller order, shake and
game-frame parity remain open. Provenance is `analysis/camera-limits-native.json`.

The composed default camera now matches 8,192 original commands across 64
continuous cases. The native oracle runs original sequence delegation, snap
selection, GoToTarget initialization/update, input limits, mover/default cubic,
viewport getters and the complete `updateCamera` method together. C loads the
actual Default/Restart sequence resources directly from the pinned APK. Each
command compares 46 words covering mover and sequence state, enforcement/input
flags, automatic presence, initialized limits, native camera publication and
done-callback state. Cases include interruption, restart, partial input targets,
zoom clamping and supplied shake. Automatic targets replace resolved input;
missing snap zoom retains current zoom. The closest-values wrapper drops its
extra force-zoom argument, so automatic validation constrains position only.
Construction/class assembly, flat numeric-table copying, vector construction and
identity multiplication, resolved input, callback recording and native camera
leaves are controlled boundaries. Generic position actions have source/unit
checks; they are outside this native corpus. Input handlers, custom/velocity
tweens, object following, shake generation, complete startup order and final
frame parity remain open. Debug ASan/UBSan, Release, Windows compilation,
formatting and all 119 default checks pass. Provenance is
`analysis/camera-system-native.json`.

Camera shake matches 8,192 original add/remove/update and externally supplied
amplitude commands, including presence, numeric fields, X/Y offsets and resulting
48-bit RNG state. The original `math.random`, `abs` and `floor` execute in the
native oracle with Android's actual random state. An update crossing duration
zero still shakes; the next update clears it. Positive existing amplitude
persists, add/remove wait until update to change amplitude/offsets, and fractional
amplitudes below one consume two draws despite zero offsets. C keeps the shared
RNG explicit. Finite numeric/nil state and active absolute magnitude below 2^30
are supported; type-error effects, seed/consumer order, explosion scheduling and
full-frame parity remain open. Debug ASan/UBSan, Release, Windows compilation,
formatting and all 120 default tests pass. Provenance is
`analysis/camera-shake-native.json`.

The default five-sample DragHandler matches 8,192 original commands, including
21 state words, returned target/release/average values and native coordinate-call
count. Original table insertion/removal and native screen-to-world distance run
in the oracle. Tests retain large-camera-offset cancellation and oldest-to-newest
averaging order. Release clears start/anchor presence while preserving current,
old and velocity-buffer state; an empty initialized average returns quiet NaNs
with bits `7fc00000` on both axes. The expanded corpus retains zero and negative
finite elapsed times, infinities and all 64 ordered pairs of selected signaling/
quiet NaN, infinity and signed-zero values. ARM add/divide NaN priority, quieting,
sign and payload match; status flags are outside the contract. Touch positions,
coordinate results and targets remain finite. The earlier finite-only corpus is
retained. Edited buffer limits, touch filtering/order, the touch state machine
and event integration remain open.
Debug ASan/UBSan, Release, Windows compilation, formatting and all 123 default
checks pass. Provenance is `analysis/camera-drag-native.json`.

Builtin camera tween selection now matches another 8,192 original mover
commands. Numeric state and both selected function identities match after
switches during movement, target changes, clears and reset. Reset preserves
the selections. Missing movement defaults to cubic in/out; missing zoom inherits
the resolved movement function. Direct evaluation matches 338 cubic-in/out,
294 cubic-out and 337 touch-zoom end-function cases. The end closure returns
`b+c` without using time/duration, including unused NaNs. Resolved C camera
targets carry these selections. Arbitrary named sequence tweens,
the touch state machine and native event mapping remain open. Debug/Release
camera and composed-preview checks, Windows compilation, formatting and all 122
default checks pass. Provenance is `analysis/camera-tweens-builtins-native.json`.

The expanded tween corpus adds cosine-in/out switching and evaluation while
retaining reset and mid-motion selection checks. All 8,192 commands match,
including 246 cubic-in/out, 230 cubic-out, 242 end and 251 cosine queries. Cosine
retains `-c/2*(cos(float_pi*time/duration)-1)+b` with separate float operations.
The first-party script sine/cosine boundary separately matches 131,072 native
input pairs across a dense grid, exponent-distributed inputs and quadrant edges.
It uses shared split-pi reduction and independently authored binary64 polynomials,
then rounds to float. Angles are bounded to [-1024,1024]; the corpus does not
exhaust every float input. The earlier three-function corpus remains retained.
Debug/Release relevant regressions, Windows compilation, formatting and all 123
default checks pass. Provenance is `analysis/camera-tweens-native.json` and
`analysis/script-trig-native.json`.

Camera snap/zoom selection matches 4,096 original query sets. Closest and
directional selection retain Euclidean operation order, first-entry ties,
strict horizontal eligibility and closest fallback. The vertical zero product
is preserved because an overflowing vertical difference can make the original
dot unordered. Relative zoom remains `1-(current-far)/(close-far)` without
clamping, including equal-limit NaN/infinity results. A truthy `snapZoom` field
suppresses selection and returns current zoom; otherwise velocity sign chooses
close/far, and zero/unordered velocity uses the strict half threshold. Original
query and vector methods execute together in the native probe, with numeric
state construction and flat-copy boundaries controlled. Debug/Release,
Windows compilation, formatting and all 124 default checks pass. Touch state,
toggle actions, filtering and complete event/frame integration remain open.
Provenance is `analysis/camera-selection-native.json`.

The touch camera FSM now matches 8,192 original commands across 64 viewport,
snap and limit configurations. Comparisons cover 53 handler/target/tween words
and the composed camera's 46 state words after each command, plus all eight
Boolean input-gate combinations. The original whole touch handler, drag handler
and simple FSM run with selected original camera methods and actual native
distance conversion. Class allocation/inheritance, filtered touches and traversal
order, initial camera state, game-state flags and flat table copies are controlled
boundaries. Empty debug upvalue names allow observation of the shared tap count;
original instructions, constants and executable properties remain unchanged.

The recovered gate requires enabled input, no dragged bird, and either specialty
availability or the released-after-specialty flag. Exactly two taps queue a
half-second cubic-out toggle during idle construction; a delayed single tap
queues a cosine bird return. Pinch reversal restarts its ratio from the previous
distance/current zoom, and coincident touches can supply NaN before input clamping.
The original FSM packing drops interaction when targets are nil, including
entry into drag/zoom. Missing touch IDs, zero/negative time, non-game zoom snapping,
input disabling and Default/Restart sequences are included. Filtering, platform
delivery, other camera handlers, complete gameplay scheduling and final pixels
remain open. Debug ASan/UBSan, 27 relevant Release checks, all 125 default checks,
formatting and Windows compilation pass. Provenance is
`analysis/camera-touch-native.json`.

Fresh integer-key table ordering separately matches 393,120 entry comparisons
across 4,096 original constructions and two deep copies of each. It preserves
strict array-density selection, float-bit hash remainder, descending physical
rehash and array-before-hash traversal. In 3,570 copies the order changes. The
caller-owned C scratch supports unique, exact binary32 integer IDs within signed
2^24; deletions, mixed/arbitrary float keys and constructor hints are excluded.
Provenance is `analysis/integer-order-native.json`.

An extended 8,192-command comparison now composes original menu reset/click
filtering, both real touch copies, touch FSM and camera graph. The C path matches
the same 99 state/target words per command while reproducing the copied touch
order. It covers zero/negative/sparse IDs, consumed pointer events, disabled menu
input and the previous camera cases. The loaded preview uses the prepared
two-copy snapshot helper. Raw source-table traversal order, menu callback results,
logical cursor/event flags and game-state flags are supplied boundaries; native
platform delivery, actual UI callbacks and full gameplay/frame integration remain
open. Provenance is `analysis/camera-touch-filtered-native.json`.

The ordinary MenuManager left-button/wheel filter matches 4,096 original cases
and 5,567 ordered callbacks. Each active press/hold/release/wheel event stops at
its first consuming menu, in supplied input order. Absent versus consumed false
key fields remain distinct; wheel resets to present false. The independent event
results are ANDed: any consumed event clears all filtered touches, while no
active events copies touches when input is allowed. The oracle executes original
reset, pointer routing, click filtering and recursive deepCopy, with supplied
menu callbacks and hover/extra buttons disabled. It checks actual filtered-table
count and each supplied touch's membership/coordinate bits. Copied-table traversal
order, hover, extra buttons, key delegation, menu mutation and full update/input
integration remain open. Provenance is `analysis/menu-pointer-native.json`.

The shared Metal preview now composes direct APK level-one geometry, initial
sprites, three decoded atlases, museum snap fitting and the default camera
sequence. A headless integration check matches the native completed camera
endpoint, including the cubic's rounded X, and emits three initial bird bindings.
Live Metal captures show the ordinary castle and bird views. Explicit fixed-step
capture supplies 1/30; default timing uses the host monotonic clock/application
cap. Poses remain at their loaded state, and blending/sampling are explicit
diagnostic selections. Background, ground, UI, sling setup and gameplay updates
are absent. The Windows GLES2 executable cross-compiles; macOS Common supports
Metal and rejects GLES2. Native Windows/GLES2 rendering and original full-frame
comparison remain open. Provenance is `analysis/classic-preview-native.json`.

UI frame geometry matches 4,096 original ancestor chains and 32,768 original
InvisibleButton hit queries. Screen position adds ancestors from the immediate
parent upward and subtracts parent scroll only when both coordinates are present.
It applies neither scale nor angle; cumulative scale is a separate self-first
product. Rectangle tests include edges and retain signed scales, absent-scale
fallback, zero scales and separate half-width subtraction. Varied exponents and
mantissas exercise float rounding; tiny signed dimensions exercise gradual
underflow. Parent/scroll lookup and arithmetic run as original selected methods.
The C API bounds chain traversal and requires finite used arithmetic; layout,
pointer flags, UI callbacks and complete UI/frame
presentation remain open. Provenance is `analysis/ui-frame-geometry-native.json`.

Base Frame pointer traversal matches 4,096 original dispatches and 2,956 child
callbacks, including 591 Boolean-false primary replies that stop traversal.
Children are visited last to first with visible/active gates, and receive the
input point minus the frame position. A non-nil primary returns all three reply
values; it stops traversal even when false, while outer pointer consumption can
still treat it as false. The typed C reply preserves absent, Boolean, numeric
bits, borrowed text and logical object identity. Empty text and zero/nonfinite
numbers retain original truthiness. Self visibility/activity are not read by
this base method. Menu/child callbacks, dense child arrays and object tokens are
controlled; mutation, button state/hit behavior and complete UI integration
remain open. Provenance is `analysis/ui-pointer-native.json`.

ImageButton hit geometry matches 4,096 original cases, all 40,960 point queries
and all five intermediate rectangle words. Its self-to-root cumulative scales
are made absolute, then applied to the explicit sprite bounds, pivot and padding.
Self position is used without ancestor position or scroll. Original Lua rotation
of opposite corners determines the rounded midpoint; the native rectangle query
uses separate float sine/cosine and excludes edges. Another 65,536 direct native
rectangle queries match, including large supported angles, signed zero, negative
dimensions and points near edges. The runtime uses authored C numeric modules;
original methods and native code execute only in the private oracle. Resource
selection, widget state, malformed-input behavior and final UI pixels remain
open. Provenance is `analysis/ui-image-hit-native.json`.

ImageButton pointer state matches 8,192 original commands over 64 continuing
instances, including 1,924 ordered hit tests, 353 audio calls and 1,436 child
callbacks. The handle/wrapper preserve hover priority, iPad cancellation at four
touches, false versus absent enabled/reply values, nil-primary fallback and the
distinct flag/event resets. A missing image or a blocked activation gate retains
an existing press; a tested activation that fails clears it. Visibility gates
press start and hold feedback but does not independently block an already-started
release. Hover and activation replies set deferred clearing except exact CLICK;
child replies do not set that flag in the parent wrapper. Hit, audio and child
callbacks are controlled nonmutating boundaries. Constructors, hover animation,
real UI hierarchy/resource binding, audio execution and final menu integration
remain open. Provenance is `analysis/ui-image-button-native.json`.

ImageButton animation matches 8,192 continuing original commands: 6,400 updates,
896 mouse enters, 896 mouse leaves and 8,192 resulting scale factors. All 6,400
image-update, 1,776 position and 243 hit-test callbacks match in order, arguments,
temporary position and click/clear flags. The image boundary runs before deferred
clearing. Touch scaling queries screen position even without a qualifying press;
held enabled hits suppress hover transitions while changing scale toward -1.
NONE recovers toward zero only when touch scaling is enabled. Enter/leave counts
remain unclamped floats, including negative values and rounding at 2^24. Separate
6.66-rate hover transitions preserve their clamping and state changes. Constructor
policy, actual image/child update effects, drawing and live menu integration remain
open. Provenance is `analysis/ui-image-animation-native.json`.

Base Frame drawing matches 4,096 original calls and 6,144 child callbacks,
including 2,015 tag-filtered callbacks. Forward child order checks visibility
without active or self gates. Tag equality preserves false, empty text, signed
zero, unequal NaN and actual shared object identity. Numeric defaults preserve
present zero; frame position, scale and angle compose separately while the local
factor passes unchanged. Default, custom and empty method names match. Child
drawing remains a controlled boundary; hierarchy mutation, widget effects,
clipping and final pixels remain open. Provenance is `analysis/ui-frame-draw-native.json`.

Image.drawSelf matches 4,096 original cases, 4,298 render-state calls, 373 sprite
calls and 171 early culls. All callback float words match, including 148 nonfinite
state arguments from zero scale. Numeric defaults, local-factor arithmetic,
optional/fallback pivots, floored positions/sizes, strict visibility and alpha
arity/reset are preserved. Script culling applies only at zero angle and retains
the anchor fast path and signed bounds; its early return skips state reset.
Absent/invisible images still reset state. The C result is a bounded ordered
command plan with explicit borrowed sprite identity. Actual sprite/resource/GPU
execution, button/child draw composition and final pixels remain open. Provenance
is `analysis/ui-image-draw-native.json`.

The installed ImageButton draw route composes seven original button/image/frame/
linear-tween methods over 4,096 cases. All 4,330 state, 359 sprite and 8,246 child
callbacks match in order and float words. Self drawing multiplies parent scales
by hoverScale while fixing its local factor to one; child traversal keeps parent
scales and passes hoverScale as its factor. The incoming sixth numeric factor is
discarded, including supplied NaNs/infinities. A culled self still traverses
children (98 sampled cases). The C adapter emits the image plan before child
callbacks without allocating. A selected live read verifies every instruction,
constant and prototype property of ui.ImageButton.draw/drawSelf/hoverScale against
the compared APK functions. Current Android globals select hover scaling false
and touch scaling true; static configuration and the class's installation gate
agree. The process resumes without a tracer and remains muted. Broader bootstrap,
actual resource/GPU execution, mutable hierarchy behavior and final UI pixels
remain open. Provenance is `analysis/ui-button-draw-native.json` and
`analysis/ui/live-ui-flags.json`.

Image and ImageButton resource setters match 8,192 continuing original commands
and 13,474 lookup callbacks. Original localization fallback, bounds/pivot refresh,
partial size, raw angle, enabled truthiness and raw enabled/disabled identifiers
match. Every callback also records the current image, dimensions, resource/rotation
pivots and enabled value, preserving intermediate assignment order. Clearing an
image retains geometry; refresh copies its resource pivot to rotation pivots while
keeping draw-pivot overrides. Button image changes retain repeated refreshes and
leave the current image when disabled without a disabled image. Localization and
sprite providers are controlled; constructors, actual APK provider/group policy,
error unwinding and full UI integration remain open. Provenance is
`analysis/ui-image-resources-native.json`.

The localization group helper matches 8,192 original commands and all 13,399
lookups. It preserves cached-first search, ordered profiles, skipped cached
groups, duplicate queries, empty successful results and unconditional final
TEXTS_BASIC fallback. Empty identifiers go directly to BASIC; fallback does not
update cache. Group-cache presence/value and state during every lookup match.
The two original requireFile calls use prepared no-op dependencies in the oracle;
C receives an already prepared source. A separate original native comparison
now checks catalog parsing and prepared text lookups; automatic locale loading
and bootstrap dependency effects remain open. Provenance is
`analysis/ui-localization-native.json`.

The unclipped, uncolored native UI text path matches 4,096 original calls,
2,925 visible draws, 1,107 alpha overrides and all 5,840 ordered glyph callbacks.
Checks include every render-context word at each glyph and after each call.
The original LuaState/Table adapters, GameLua wrapper/internal method,
Resources/UTF conversion and BitmapFont placement execute. Current font/glyph
metadata is controlled; empty resource maps retain that font and treat text as
literal. This isolates draw semantics from the separately compared catalogs.
Three numeric parent arguments ignore the third scale until both scales are
present. Final scale uses parent times (self times factor); normalized text
position is floored after division. Both rotation pivots must be numeric;
widget angle is not added. Numeric alpha below one overrides context alpha and
restores one after drawing. Other alpha leaves context alpha unchanged. Render
transforms persist after the call. Visible accepts Lua truthiness, including
zero and empty strings. Finite nonzero scales, valid anchors and explicit font
selection boundaries are the supported C domain. Clipped/colored text, invalid
argument exceptions, constructors, child integration and final pixels remain
open. Corpus SHA-256:
`3ce9047f9e07c354c83f133485f21782c03dcfe7fb403ac5888f960f67671fb1`;
private provenance is `analysis/ui/text-draw-native.json`. Read-only live checks
also verify the installed `ui.Text.draw` bytecode, constants and prototype
properties against the original native-route wrapper, including its subsequent
Frame.draw child call. The reference process resumed with audio still muted.

The same native UI text route through ordinary font/Sprite/Image submission and
GLES2 rectangle batching also matches 4,096 calls and 5,840 image callbacks.
All 2,923 emitted quads and 2,917 culls match, including 759 rotated and 1,126
direct-coordinate quads. Every local rectangle, context field, controlled atlas
UV and six-vertex position/depth/UV batch record matches the composed C path.
The original GL_Image and RenderBatcher use a prepared shader token and matching
preallocated batch state; shader selection and GPU execution are bypassed.
The allocation-free C font adapter emits the shared renderer's clip commands,
preserving glyph order, pivots, temporary rotation origins and native culling.
The Common/Metal font preview now uses this adapter. Its two-frame 960x640
capture has zero differing bytes against the prior Common preview; this is a C
preview regression check, not an Android pixel comparison. Native Windows/GLES2
execution, real shader/resource selection and full UI pixels remain open.
Corpus SHA-256:
`72e6df649cf34b1807b49c4fb16cfa47964eaf6a135025d127e73e3c0b8e4f1f`;
provenance is `analysis/ui/text-image-native.json`.

The clipped-text vertical visibility helper independently matches 8,192 native
decisions, including 2,923 cases with active nonfinite inputs. Only vertical
position, vertical scale, font ascent/descent, vertical anchor and screen height
matter. Bounds retain signed scales without reordering; top equal to height
rejects and bottom equal to zero accepts. NaN comparisons preserve the original
unordered behavior. The corpus SHA-256 is
`b7f016b3c17d864b7b52132b787c539e4d0d903c9d36d2d93b26bd5b3a9c394e`;
provenance is `analysis/ui/text-visibility-native.json`.

Prepared clipped-line traversal matches another 4,096 native calls with 8,190
line records, all 2,863 native leaf draws and 5,706 glyph callbacks. A read-only
context getter classifies the original internal draw callsite to count leaf
draws independently, including empty or unrenderable text. Native traversal
ignores a line's visible flag (1,433 drawn lines are marked hidden) and uses the
root group. It tests vertical bounds using root scale before multiplying line
scales. Position preserves parent+(root+line)*factor arithmetic, then supplied
position/anchors bypass a second line-position addition. Root font, alpha,
pivots, color and text are unused in this branch. Per-glyph context and final
state match, including continued alpha state across lines. The C geometry phase
borrows prepared lines and current font metrics; ordered localization/font
provider effects, nested clipping, color effects, line breaking and constructor
integration remain open. Corpus SHA-256:
`4f5f40dfa7dfa092b94f5037cc2458d90a80f7f49136fe1952a911503e6e02fa`;
provenance is `analysis/ui/text-clip-native.json`.

Native clipText line selection matches 4,096 cases across 36 authored strings:
6,648 lines, 1,413 empty lines, all 9,356 output UTF-8 bytes and every widest-line
float bit. The C implementation borrows decoded codepoints and emits bounded
line slices without Lua or allocation. Candidates must be strictly narrower
than the limit; LF, space, hyphen and U+200B delimit candidates, with hyphens
retained. Tests include repeated/leading/trailing whitespace, hard breaks,
character splits, embedded zero, CJK/emoji and malformed UTF-8 normalization.
Nonpositive widths produce one empty line; positive-width empty text produces
none. Font/resource metadata is controlled. NaN and nonprogressing tiny-width
splits remain explicit C failures; the oracle avoids those unsafe cases.
Corpus SHA-256:
`5a03c03ae67198c768e40af0a49e1baf09fd4abdec20e4673d5a0f5f4cbe0ead`;
provenance is `analysis/ui/text-wrap-native.json`. Script line-object creation,
actual provider selection and complete UI integration remain open.

The target UTF-8 encoder matches 65,536 native code/capacity cases and 4,096
whole-string conversions, including all 23,712 resulting bytes. Mode 2 retains
surrogates and values through 0x1fffff; 0x200000..0x7fffffff fails. High-bit input
values follow the original signed comparison and emit their low byte. Whole
conversion skips rejected values; capacity failures preserve prior output
counts. This supplies exact wrapped-string bytes above. Corpus SHA-256:
`6a07f49a90cab14fbded6026d111db44affbd87be0e084d204f45906251caca9`;
provenance is `analysis/ui/utf8-encode-native.json`. This is the target's legacy
conversion behavior; no native allocator or original converter ships.

Plain Frame/Text initialization matches 4,096 original constructor calls,
including original Class.new and direct reinitialization: 2,048 Text calls,
701 retained existing fonts and 3,746 ordered default-name, font-selection,
localization and measurement boundaries. Compare all 20 supported typed fields,
including signed zero/nonfinite number bits, false, empty strings and logical
object identities. Frame creates separate empty children/lookup tables; the C
model resets their logical counts. Text unconditionally resets scales to one,
visibility/floorCoordinates to true and fontLeadingScale to one. Existing
truthy fonts and getFontFunction fields survive this non-callable-font branch.
Empty groups skip localization; unavailable font setting writes numeric width
zero. Arbitrary argument properties are not copied by Class.new or these
constructors. Original tostring(self) is a controlled logical-name boundary,
and resource callbacks deliberately return typed fixtures. Callable font
arguments, broader metatable inheritance, owned child destruction and full UI
composition remain open. Corpus SHA-256:
`7a870aa7fa43f3a5b108d66e144e493e483d1afa96bfd5b405a02f6fd28c8725`;
provenance is `analysis/ui/init-native.json`.

Text.clip composition matches another 4,096 original calls, producing 6,295
fresh line objects and 37,468 ordered boundaries. Original Frame cumulative
scale, Text.clip, Class.new, Frame.init and Text.init execute with actual native
GameLua line breaking, UTF conversion and font measurement. The C comparison
composes its corresponding first-party functions and compares every created
line's fields and UTF-8 bytes, root block height/widest-line bits and every
boundary argument/result. Coverage includes up to three ancestors, signed
scales, nil/false/zero/truthy own-scale arguments, absent/false/signed/zero spacing,
five anchors per axis and all 36 wrapping strings. Root group is used for
wrapping, while fresh lines default to BASIC; line scales are one and the
supplied height argument remains absent. Repeated Y addition and multiplication
order are retained. The default-name and set-font boundaries are controlled;
the prepared missing-group localization returns the original identifier.
Current font uses the same seven-glyph fixture as the independent wrapping
corpus. No live provider selection, script allocation, final UI pixel or full
gameplay equivalence is claimed. The C contract requires finite arithmetic and
nonzero selected X scale. Corpus SHA-256:
`d69b5112fdc93e11cb14d15560de9d61603a3765b108e66ef1b54c7300a35ac9`;
provenance is `analysis/ui/text-lines-native.json`.

Callable font behavior matches 4,096 continuing original Text.init/setFont/layout
commands across 64 objects: all 20 supported state fields, 1,647 zero-argument
getter calls and 2,543 ordered getter/font/measurement boundaries. The value
model now retains logical function identities separately from object identities.
Initialization stores/calls a function argument before its usual font fallback;
62 false/absent getter results fall back to the supplied function itself.
setFont and layout store the getter result without that fallback. Assigning a
non-function font preserves an older getter, so later layout may replace the
new font. A non-absent false/plain non-callable getter attempts a native call;
127 original errors match the C rejection flag without changing fields. Getter
results cover all six supported types, including signed zero/nonfinite numeric
bits and other function identities. Actual getter execution, font selection and
empty-text measurement are controlled boundaries. Custom callable metatables,
error strings/unwinding and native font selection remain open. Corpus SHA-256:
`9d7200dd5969f93b313133e960303fd6e1f7e8bb5992179f472bbe1cd2a00459`;
provenance is `analysis/ui/font-native.json`.

Plain tostring conversion matches 65,536 native calls, including 65,153 raw
binary32 numbers and all 896,879 output bytes. Native numeric conversion promotes
float to double and uses nine significant digits. The C implementation uses
the permitted C library for finite formatting and explicit nan/inf spellings:
NaN signs/payloads do not affect text, infinities retain their sign and negative
zero remains -0. Coverage includes random raw float patterns, signaling/quiet
NaNs, normal/subnormal limits and finite extremes, plus nil, Booleans and exact
borrowed text with embedded zero. Insufficient numeric storage preserves bytes
and output. Opaque object/function address names and custom __tostring remain
separate boundaries; C numeric locale and nearest rounding are preconditions.
Corpus SHA-256:
`c5e1836411f5ca7bdb0d313ca9e54b219053d239377af04b9366d68ecced0883`;
provenance is `analysis/ui/value-text-native.json`.

Text width/height/font-leading queries match 12,288 original results and all
28,614 ordered font/localization/metric boundaries. Original selected methods
execute with the native builtin tostring and native Resources/BitmapFont UTF
measurement. C composes its value formatter and font measurement with explicit
resource boundaries. Width selects the font before its empty-text positive-zero
return; nonempty inputs localize even an empty group, then measure and scale.
Height multiplies a truthy clipped line count by the builtin font-leading
result, then by scaleY. Stored width/widestLine/textBlockHeight and the font
getter are ignored. Coverage includes absent/false/default/signed/zero scales,
signed metrics/tracking, six clipped value forms, 0..8 dense line counts, all
36 wrapping strings and raw numeric/Boolean/nil text. Every callback string
byte and numeric bit matches. The current seven-glyph font is prepared, font
selection is controlled and missing groups retain identifiers. C arithmetic is
finite; custom methods/metatables and live resource selection remain open.
Corpus SHA-256:
`c867776ee5c81b464b66405803c269a9f6c6a12f966b36d0b42e2e93dddf8dbd`;
provenance is `analysis/ui/text-metrics-native.json`.

Text correction/bounds match 4,096 original cases and 17,650 ordered boundaries,
including 2,510 stored-width increases, 378 leaf hits, 2,275 clipped hits,
650 nil exhaustions and 3,020 child calls. The corpus targets all four corners
(521 leaf queries), adjacent outside float values, centers and ordinary points;
252 corners are inside, while reversed/signed extents retain their rejection.
Raw text always localizes/measures first, even when empty or clipped. Stored
width grows only on strict comparison. Leaves query current leading, then
correction queries it again; independently changed fixture leading values prove
the two calls and their order. Geometry uses raw unscaled X/Y/width/leading.
Clipped children receive the unchanged point forwards; truthy replies become
Boolean true and exhaustion returns nil. Self/child visibility, font selection,
stored font getter, scales and fontLeadingScale are ignored. Actual native
Resources/BitmapFont measurement and original selected methods execute; current
font/localization and child reply boundaries are prepared. C ownership, custom
metatables, nonfinite geometry and a full UI scene remain open. Corpus SHA-256:
`4f9427108cd6cf9b695e0b85ae20dbbdea307398b9bd0149f6bbea1bdb03575a`;
provenance is `analysis/ui/text-bounds-native.json`.

Frame hierarchy/lifecycle matches 4,096 continuing original commands and 1,609
ordered child entry/exit/layout callbacks. Across 32 eight-node graphs, compare
all dense child orders, every lookup child/index, entered/parent values and the
shared binary32 entryCounts counter after every command. Version 2 first fills
two parent arrays with all eight nodes, then exercises full traversal, shifted
indices, removal from an older parent, pop/removeSelf, clear and reinitialization
before randomized continuing commands. Names remain stable text. Removal/reset
preserves parent fields; adding to another array does not remove old membership.
The corpus includes 22,731 stale-parent and 2,673 multiple-membership snapshots.
Callbacks capture current parent lookup/count, own count, entered value and
counter before applying controlled subclass entered changes. Base entry/exit
uses the original bound Frame upvalue and updates the counter before delegation;
repeated exits and float precision boundaries are retained. C uses borrowed,
caller-owned nodes/arrays and derives lookup indices from dense order without
a Lua map/VM. Invalid capacity/index/name/duplicate input is rejected separately;
original malformed-table/assert prefixes, external table aliases, renamed
attached nodes, recursive subclass behavior and full UI ownership remain open.
Corpus SHA-256:
`ffd4c0b04f8cc2a07bd4d95eb8223a23bc28880022f0f8cc1c40d0ec55bc2247`;
provenance is `analysis/ui/frame-tree-native.json`.

Image/ImageButton constructor composition matches 4,096 cases using the full
original Lua base, Class, Frame, Image and ImageButton chunks, including actual
Class.new inheritance/dispatch, lua.default, image setters and resource refresh.
Compare all 35 supported typed widget fields, frame counts/entered values,
shared counter and 6,934 complete callback snapshots. Fresh objects use defaulted
names so the name boundary can retain their new self reference; reinitialization
also covers explicit names. Image resets angle/alpha/floor/rotation pivots, while
an absent/false image argument preserves old image/resource pivots. Button image
dispatch occurs before enabled becomes true, so old enabled state affects repeat
refreshes. Later enabledImage/disabledImage assignments retain exactly the
arguments, even when they differ from the current image. False scale-on-touch
and activation values survive; click sound uses truthy fallback. Native default
padding is checked empty/distinct before private logical labeling (801 cases);
C borrows a fresh caller-prepared empty object, without allocator equivalence.
Another 683 inherited-entry attempts match, including 503 errors after click/
hover resets, retained hover counts/deferred flags and unchanged failed counters.
The typed resource state also matches the existing 8,192-command native corpus
and all 13,474 prefix callbacks alongside the resolved draw/resource model.
Default names, localization/sprite geometry and dependency noops are controlled;
live providers, generic table allocation, custom subclasses and full widget/
render/pointer integration remain open. Constructor corpus SHA-256:
`60a697eedffa98410d044fd3b8f7d29d4b0414e6b6f0bb2741c12616d6656b3d`;
provenance is `analysis/ui/image-state-native.json`.

Canonical Frame/Image fields now project directly into the verified numeric draw
views. The image draw checker consumes typed fields for all 4,096 existing native
cases; the button checker compares canonical and resolved routes for all 4,096
graphs and 12,935 callbacks. Strict Boolean visibility, absent image/alpha, false
pivot defaults, numeric zero truthiness and enabled-only hover reads are retained.
Hidden/absent images leave unused identifier/size/pivot fields unread while still
composing transforms and resetting render state. Drawable identifiers are text;
used numeric fields must be finite. Projection failures preserve output and no
resource or state mutation occurs. Debug/Release comparisons and Windows cross
compilation pass; full scene ownership, GPU execution and target pixels remain
open. Provenance is `analysis/ui/widget-draw-native.json`.

Canonical ImageButton pointer state matches another 8,192 original commands,
3,694 hit queries, 131 audio calls and 1,359 child callbacks. The corpus seeds
all six supported value kinds into visible/image/activation/click/deferred
fields and into replies and sounds. Click/deferred fields are reseeded every
seven commands, then continue between commands. All reply values, complete
typed final flags and both flags during every callback match. There are 1,882
non-Boolean click and 4,388 non-Boolean deferred-flag snapshots after commands.
The shared numeric pointer implementation reports actual original assignments
to the canonical state immediately; it preserves absent/false/opaque values
when no assignment occurred. Failed hit/audio/child calls retain the observed
state prefix and preserve the reply. Hit/audio providers and child geometry
are controlled; live resource geometry, real sound effects and menu ownership
remain open. The earlier native probe was rebuilt and reproduced its unchanged
8,192-command hash. Debug's 166 checks, focused Release comparisons, formatting
and Windows cross compilation pass. New corpus SHA-256:
`6badfc6f93810d9b567b6b923424d7d7ffb67ef0185ca4f29df14a3cfdc562ca`;
provenance is `analysis/ui/widget-pointer-native.json`.

The explicitly-sized Sprite.draw route matches 4,096 original image-boundary
rectangles and UVs across all 20 anchor combinations, signed region dimensions,
signed/zero requested sizes and integer SPRT pivots. Anchor offsets use original
region dimensions and rounded pivots without scaling them by the requested size;
the full atlas region remains selected. Cases include 1,956 negative requested
widths and 1,939 negative requested heights. Corpus SHA-256:
`27ab2056884615265c0bae5fde1764cd17fac31a7bc6713a3717d28a11f03ee2`;
provenance is `analysis/ui/sprite-sized-native.json`.

The UI image renderer then composes canonical Image state and the verified draw
plan into shared backend-neutral clip commands. Another 4,096 original paths run
Image.drawSelf, actual GameLua.setRenderState, actual sized Sprite.draw and actual
GL_Image/RenderBatcher rectangle emission. All script command words, 491 sprite
image calls, 219 emitted batch quads, 129 script culls and complete final 24-word
render states match. State at resource lookup matches the native image prefix;
seven-argument calls preserve alpha and eight-argument calls replace it. Direct
vertices survive state resets. Native triangles retain their fixed z value
(zero for direct vertices, otherwise binary32 0.001); the shared 2D command
stores x/y and UVs, with depth policy separate. Resource bindings/UV fixture,
shader token and blend/color policy are prepared. This establishes geometry,
culling, state and alpha composition; GPU sampling, effects, zero-scale
nonfinite results, live resource policy and full UI scene pixels remain open.
All 170 Debug checks, four focused Release checks, formatting and Windows cross
compilation pass. Rebuilding the earlier image probe reproduces its unchanged
hash. Composition corpus SHA-256:
`5ceb7db67a88b58d7831adea8f1dd52bd14c4e4b9b919399a0a064eef9c95b01`;
provenance is `analysis/ui/image-render-native.json`.

A first-party mutable string map matches 4,096 continuing native raw table
commands across 16 instances: 90,177 physical node snapshots, 262,144 lookups
and 32,547 next entries. Its retained nil keys, collision links, last-free cursor
and capacities match, including 119 growths, 20 shrinks, 20 observed same-size
rehashes, 674 nil writes to previously unreserved keys and 399 erasures during
active iteration. A present zero handle models a live false value independently
of absence. The C owner allocates bounded nodes/rehash workspace once and borrows
pinned string bytes. Native keys are rooted separately; this initial corpus
observes no collection traversal. This is a data structure with opaque value handles; no Lua VM or script
execution is added to the runtime. Mixed/numeric keys, collected keys and menu
controller integration remain open. All 172 Debug checks, two focused Release
checks, formatting and Windows cross compilation pass. Corpus SHA-256:
`0fca087fbb83deec991b7a36e6d1d72b03f6abba9126560e76748e0aec573173`;
provenance is `analysis/ui/menu-manager/string-map-native.json`.

An explicit collection phase matches another 4,096 native string-map commands,
85,717 node snapshots, 262,144 lookups and 29,165 next entries. The native full
collector marks retained nil keys as dead even when their strings are rooted.
Lookup skips those tags; free-slot search still treats the slots as used, while
new-key assignment or rehash removes the tag. All 3,368 observed dead-key
snapshots match, including 283 standalone collections and 332 collections during
active erase/next traversal. Native automatic collection is stopped so these
phases are explicit inputs. The C phase reproduces dictionary metadata effects;
it does not add a VM, memory reclamation or original collection scheduling.
Pinned string lifetime and mixed-key limitations remain. All 174 Debug checks,
focused Release comparisons, formatting and Windows cross compilation pass.
Corpus SHA-256:
`d829c75d6867b739ab34f3dc89bb44ce2bd1b002a851adcde0943bfef6e9f3dc`;
provenance is `analysis/ui/menu-manager/string-map-gc-native.json`.

A bounded MenuManager owner matches 4,096 original controller commands and 32
constructors over the supported fields. The oracle executes complete original
Lua base/Class/MenuManager chunks, including actual Class.new and captured
dynamic helpers. All 5,724 callback snapshots, 157 rejected operations and final
72-word states match: raw flags, dimensions, duplicate ordered channels, node
visibility, current/queued map slots/links/dead tags and empty link-map layout.
Dense four-channel cases cover queued preference, direct replacement without an
old exit, cancellation, nil writes, normalized entry backgrounds, raw draw RGB,
resize layout/update ordering, pause/resume and unconditional visibility=true.
There are 66 queued updates, 45 disabled updates, 190 resize updates and 2,821
explicit collections at callback boundaries. Queue changes suppress input reset
and delegation for that update. C owns bounded map storage and borrows nodes,
frames, names and order arrays; no mutation allocates. Node methods, dynamic
assets, event/render/input providers and collection phases are controlled.
Malformed nonobject roots, remaining constructor fields, link/event/pop/input
routing, real widget methods, allocation/collection scheduling and scene pixels
remain open. All 175 Debug checks, two focused Release checks, formatting and
Windows cross compilation pass. Version2 corpus SHA-256:
`840cbb22f26980e4b1cfeae85409968612589092af02c1a1498879fbf679f3b8`;
provenance is `analysis/ui/menu-manager/controller-native.json`.

Builtin link registration, replacement, lookup and result routing match another
4,096 commands, 5,598 callback snapshots and 280 rejected operations. Every final
92-word state includes all three dictionaries. A link with an absent instance
still exists: routing it immediately deactivates its channel, while a missing
link rejects before root changes. Empty names, default/explicit channels and
interleaved lifecycle/update commands are covered. C stores borrowed target
identities with explicit presence, without allocating original link tables.
Generic/custom records, table identities, event/frame navigation and actual
screens remain open. All 176 Debug checks, three focused Release checks,
formatting and Windows cross compilation pass. Version3 corpus SHA-256:
`6d024215708d735d78d36a45889420e4bc9dc6fefe399c33e416fc481cd87083`;
provenance is `analysis/ui/menu-manager/links-native.json`.

Menu scene/frame navigation composes the canonical Frame graph with owned menu
maps. Another 4,096 original commands match 6,063 callback snapshots, 405 rejected
operations and every 144-word final state. The oracle installs complete original
MenuManager and Frame classes; actual addChild/removeChild/popChild and the
assigned removeFromFrame closure run with controlled subclass lifecycle methods.
Snapshots include entered values, child order, lookup identities/indices, stale
parents and callback installation. Push notifies before installing removal and
attaching the child. An entered root runs child entry/layout, then root layout;
an unentered root does neither. Pop chooses a queued root first. The installed
removal closure emits a plain pop event with no channel, even for overlay frames.
Nil-instance pushes notify before rejection, while missing links reject first.
Scene changes assert target presence before resetting back delegation, then route.
Canonical unique names are required: duplicate native insertions assert after
mutating the array, outside the bounded graph contract. Subclass bodies, event
delivery, other input and scene pixels remain open. All 178 Debug checks, five
focused Release checks, formatting and Windows cross compilation pass. Corpus
SHA-256: `202544e38282c29733196c9092864cf08bbe6ce4e12b9b1ba74fe1f650c35505`;
provenance is `analysis/ui/menu-manager/navigation-native.json`.

The menu owner now keeps input order independently of draw/update order and
routes pointer events through current roots. Within another 4,096-command
controller/frame corpus, all 7,513 callback snapshots and 296 rejected operations
match. There are 536 original pointer dispatches and 1,025 node callbacks across
seven event names. Nil and false continue; numeric zero, empty text, object and
function primary values consume. Truthy auxiliary results do not consume. Six
duplicate channels, empty orders, absent roots, queued/current distinction and
direct dispatch with input disabled or invisible nodes are covered. The corpus
also composes two-child push/pop and attachment to a queued root while an older
root retains the same child. The 151-word state includes independent input order.
Actual widget pointer bodies, click/hover/key/touch scheduling and full scene
integration remain open. All 179 Debug checks, six focused Release checks,
formatting and Windows cross compilation pass. Corpus SHA-256:
`a5136da9cc0094a1161ca3ce1456e55e74da9d9bbbeab7c00861d58fdce7d023`;
provenance is `analysis/ui/menu-manager/input-native.json`.

Menu keyboard routing matches 3,483 original key callbacks across all 72 emitted
labels and three phases, within another 4,096-command corpus. Every one of the
10,521 complete callback snapshots and 290 rejected operations matches. The
oracle runs the original installed delegateKeyEvents body for direct calls and
normal menu updates. Four release keys route through independent input order;
desktop/editor and Roku ordinary events target the current root. Both input
permission flags and a current root gate the entire delegate. Desktop control
suppresses alphanumeric input outside the editor and maps V to PASTE in all
three phases. Roku sends held UP/DOWN first, maps replay/gaming buttons and
prefers press over simultaneous release. Fixed native map orders are prepared
once in C using bounded string data ordering; no allocation or VM runs during
dispatch. Android/other, Windows, OSX, Roku, editor/control combinations and all
six supported primary kinds are covered. Platform key collection and actual
widget key bodies remain providers; full scene/input scheduling remains open.
All 180 Debug checks, seven focused Release checks, formatting and Windows
cross compilation pass. Corpus SHA-256:
`0a1eda41d412eeaed30ca8f89d1ca67cb8eb0d3e10a863b8710834ef25ca23fc`;
provenance is `analysis/ui/menu-manager/keys-native.json`.

Base Frame key traversal now composes with the menu key router and canonical
child graph. Another 4,096 original commands match 15,905 complete callback
snapshots, including 3,221 base Frame calls, 2,425 custom leaf calls and every
three-value Frame return. Children are visited last to first with visible/active
gates. A false primary stops Frame traversal and preserves auxiliary replies,
then allows outer menu release routing to continue; nil continues within Frame.
All 116 observed false Frame returns, 1,583 absent returns and every supported
truthy kind match. Original Frame.onKeyEvent runs on two base roots with custom
leaves, including two-child order, queued/current roots, hidden leaves, stale
parent links and collections at call/return boundaries. This prepared one-level
graph excludes base nodes as children to remain acyclic. Actual subclass key
bodies, full hierarchies and platform scheduling remain open. All 182 Debug
checks, nine focused Release checks, formatting and Windows cross compilation
pass. Corpus SHA-256:
`2ba343003901752f0152bc7b18cfe18c1a296f8ae223d41435b5b9466e6a409d`;
provenance is `analysis/ui/menu-manager/frame-keys-native.json`.

Main-menu theme layout matches 4,096 original calls and all 32,768 settings,
resource, camera, theme and effect callbacks. The full installed original
main-menu chunk computes every result; settings/resource/camera providers are
controlled. C preserves binary32 operation order, fractional zoom and signed
sprite-height/offset geometry over logical heights 240 through 2160, including
the strict >320 extra offset. All six supported water flag/level/state kinds
match: truthy levels retain their complete raw values; absent/false levels
default to 20, and disabled water clears the field. Water changes after theme/
effect selection and before final camera placement. Provider-failure prefixes
are checked in C. Real settings policy remains separate: this layout first
clears stored zoom, then the real getter supplies its default. No MainMenu
constructor, background rendering, birds or final scene pixels are claimed.
All 184 Debug checks, focused Release checks, formatting and Windows cross
compilation pass. Corpus SHA-256:
`b9ec3eede3635d8720a0688f47c19596fa40a1e26d98dac6f2b37ccb56cf5490`;
provenance is `analysis/ui/main-menu-theme-native.json`.

Main-menu settings getters/setters match 4,096 original cases and all 45,056
typed getter/stored values. The complete original settings wrapper executes its
zoom/theme getters and setters plus low-quality getter. All six supported raw
kinds are covered; truthy values keep their types, including zero and empty
text. Zoom defaults to binary32 `0x3fea3d71`, quality to false. A saved theme
survives only when its selected record is truthy; otherwise a truthy default or
literal `theme1` is returned without checking that fallback's existence. Native
records cover absence, false, tables and numeric zero. Every no-argument zoom
setter clears storage and the following getter returns its default. C resource
lookup remains an explicit boundary; persistent graph/save scheduling, malformed
NaN keys and the menu resource bridge remain open. All 186 Debug checks, focused
Release checks, formatting and Windows cross compilation pass. Corpus SHA-256:
`1d1355b5c5122a72da8298a72336d6dea707276f001e31844a149abcdbeba181`;
provenance is `analysis/ui/main-menu-settings-native.json`.

The menu resource bridge decodes APK themes_new tables through the static C
reader and uses explicitly selected 480x320/1024x768 SPRT metadata. It matches
4,096 original-script compositions, exact camera/water results and all 16,384
camera/theme/effect application callbacks. The oracle runs actual original
settings methods and theme data; sprite bounds remain prepared 58x58/104x111
values. C reads those dimensions independently from the APK. Saved zoom clears
before the default getter, saved/missing/non-text theme keys follow recovered
selection, and water clears between effects selection and camera placement.
The permissive getter also selects an existing settings record; actual native
ThemeSystem validation is a controlled boundary here. This is the explicit
themes_new subset, with no claim for merged theme catalog, live startup theme
choice, background/GPU pixels or full menu scene. All 201 Debug checks pass,
including newly enabled APK checks, alongside focused Release checks, formatting
and Windows cross compilation. Corpus SHA-256:
`b6cefb4845ae54d591d146e59a4474cabf43eb2b9b07576199beeaa045546a95`;
provenance is `analysis/ui/main-menu-resources-native.json`.

ThemeLayer's forward/inverse horizontal transforms match the original ARM
library over 8,192 calls: all 7,997 finite outputs match bit for bit; C rejects
195 nonfinite inverse results and preserves the caller's output. The native
camera separately transforms screen center and full-width distance, retaining
camera-translation rounding. Vertical camera fields are unused. Original
coordinate objects retain their prepared reference counts after every call.
Corpus SHA-256:
`c0aad115f96011a7ad5c6e032fb28a0e0d3abd01f813dca1c22e1b67814bac27`.

Prepared base layers then execute actual native ThemeLayer, DrawUtils and Sprite
methods over 4,096 cases. C matches all 9,005 image quads, repeated UV/tint values,
per-draw and final render-state words, including 249 empty layers and up to 72
draws per layer. This preserves separate float and double remainders, animation
and parallax arithmetic, fractional repeat starts and signed scale/offset order.
The ordinary-Sprite pivot getter adds 0.5 before truncating, including negative
pivots; the C theme-sprite path preserves that rule and native matrix zero
products. Prepared resources return an explicit ordinary Sprite; the image
virtual call records output without running a GPU. Constructor/lookup policy,
composite sprites, overlays, composition and final background pixels remain
open. Debug sanitizer, Release comparisons and Windows cross compilation pass;
native Windows execution remains unverified. Corpus SHA-256:
`65c910ffdae00412454c1effadb3816ececab78c5a5ad7ed36739d0fa81d3f74`;
provenance for both scopes is `analysis/ui/theme-layer-native.json`.

The static ordinary ThemeLayer constructor also matches 4,096 original native
constructions with 3,510 present and 586 missing sprites. Prepared resource
cache entries supply ordinary Sprite metadata; original native getters execute.
All recovered fields match: dimensions, biased pivots, parallax/scale defaults,
boolean-only repeat selection, color defaults, named `v` animation speed and
tuple[6] assigned to both x offset and phase. C borrows decoded table/name data
and explicitly resolved metadata without allocating. Numeric-string coercion is
explicitly unsupported; other tested nonnumeric kinds follow native defaults.
Composite constructor bounds, resource ownership and overlays remain open.
Corpus SHA-256:
`be6eff66785309874c1af663f7ba3f4b68209b00f8e9751bd5e68ade2c53caab`;
provenance is `analysis/ui/theme-layer-construct-native.json`.

A second base-draw corpus exercises actual native composite fallback, stored
entry traversal and ordinary Sprite drawing. C matches 17,621 image quads with
their UVs/tints and all per-draw/final state words over 4,096 cases, including
1,006 empty layers and up to 308 images per layer. This theme-specific route adds
part x/y offsets and ignores entry scale, rotation and flip fields. Entry strings
retain their prepared reference counts. C resolves borrowed ordinary metadata
through explicit providers; actual loading/bounds, overlays, GPU submission and
final menu/background pixels remain open. Corpus SHA-256:
`e1cc8a4d2840cb7e5a8fa6aa71b33bf4a0d1fd04615efd8b3e5e8775b0a1fbac`;
provenance is `analysis/ui/theme-layer-composite-native.json`.

The first-party static data reader now directly decodes
`assets/data/scripts/themes.lua` from the APK. Its complete 32-global graph
matches all 3,663 typed native keys/values in 454 tables, independently of table
enumeration order. This covers 31 classic themes and 214 background/foreground
layer records, including theme1. Original bytecode runs only inside the private
native oracle; C executes no scripts. This does not yet validate additional theme
files, merged resource lookup/load policy or live startup selection. Source
SHA-256 is `86df75677d013e4e8f600a32815091421da3aeaa8cf269902528d9f5f7811754`;
native graph SHA-256:
`a33e1c650bb34534dadade061d37004d75168644246469c6c03de4752bba1165`.
Provenance is `analysis/ui/theme-catalog-native.json`.

The theme composite corpus also extends through the original per-vertex-tint
GL_Image and RenderBatcher methods, capturing CPU batch output without a GPU.
C matches all 17,621 upstream image calls, 11,852 emitted clip quads and 5,769
culls over 4,096 cases. Every emitted triangle position, UV and tint word
matches. The point route normalizes already-transformed vertices by viewport
extents; it does not reapply current context translation/scale/direct mode.
Shared C geometry preserves four-point bounds and edge inclusivity. z=0 theme
sprites are the tested domain; shader selection, actual color/alpha rendering,
GPU pixels and complete scenes remain open. Corpus SHA-256:
`84b7d73bac22763a9f939a381c208883ec60a101f08c4ae1cef56ef884d3bcf7`;
provenance is `analysis/ui/theme-layer-image-batch-native.json`.
All 211 Debug sanitizer checks and ten focused Release theme checks pass.
Theme checkers and the preview cross-compile for Windows; native Windows
execution and final target pixels remain unverified. Formatting and diff checks
pass. Rebuilding the shared ordinary-sprite oracle and the catalog stager
reproduces both previously pinned traces byte for byte.

Original CompoSprite bounds now match over 4,096 native cases containing 16,380
parts, 12,286 enabled. Native code normalizes rotation axes, multiplies both
stored scale vectors, composes sprite pivot translation and part offset, then
truncates each transformed corner to int32 before collecting dimensions/pivots.
C preserves operation order, disabled-field non-use, modulo32 output arithmetic
and the width/height1, INT32_MIN+1 pivot sentinel for empty/all-disabled input.
Out-of-range/nonfinite corner inputs remain explicitly unsupported. The corpus
uses prepared native entries with verified reference-count preservation; source
file mapping/ownership is separate. SHA-256:
`45f7d618dadb40c17a6e5eb21179e06e7a4254b9e54f88a19581257ebe7dcff0`;
provenance is `analysis/ui/composite-bounds-native.json`.

A bounded read of the unmodified running APK's Resources cache also confirms all
five theme1 layer bindings: three ordinary sprites, two composites and eleven
ordered parts. C directly reads four themes_android/THEME_01 SPRTs and their PVR
atlases, plus THEME_01_COMPOSPRITES.dat. It matches every selected region/pivot,
UV, part offset/scale/factor/rotation/enabled field and both computed composite
bounds (480x202, pivot240/202; 446x65, pivot220/63). Only the enabled flag's byte
is semantic; its surrounding allocator padding is ignored. The selected read
totals 4,605 bytes, invokes no original functions, writes no original memory and
detaches/resumes in a finally block. The process remains untraced and muted.
This validates these explicit loaded bindings, with no claim for live main-menu
startup theme selection, complete resource policy, layer lifetime or final pixels.
Record SHA-256:
`513e712bcc3d5844f803d0632b49fe547361b3813dec61ebc937bad4bf933a41`;
provenance is `analysis/ui/theme-bindings-native.json`.
All 214 Debug sanitizer checks and 13 focused Release theme/bounds checks pass.
Bounds/live checkers and the preview cross-compile for Windows; native Windows
execution remains unverified. Formatting and diff checks pass.

ThemeLayer construction also accepts explicit native integer getter metrics for
composites. Another 4,096 original constructor calls match with 3,510 present and
586 missing composites, including full int32 dimensions/pivots and empty bounds
sentinels. C converts getter integers directly to float and does not repeat the
ordinary Sprite getter's biased pivot rounding. The ordinary metadata wrapper
still passes its existing corpus. Lookup/cache policy and object ownership remain
prepared boundaries. Corpus SHA-256:
`443a3df44f653b1159ba9ede94cb620c68242b99aded1d90c01859674afa6ae0`;
provenance is `analysis/ui/theme-layer-construct-composite-native.json`.

The no-spotlight ThemeComposition background path matches 8,192 original calls
and 32,761 layer callbacks. The original GameLua RGB setter executes, clamping
finite channels to0..255, truncating and packing opaque ARGB before traversal.
Stored layer order, repeated handles, unchanged 32-bit filters and alpha values
seen at each boundary match. Completed traversal resets alpha to1 even when the
layer vector is empty. C callback failures retain earlier color/state writes;
these safeguard prefixes are C contracts, not native error-return comparisons.
The oracle uses prepared layer methods and no spotlight instance; actual layer
bodies, resource ownership, spotlight rendering and complete scene pixels remain
separate. Corpus SHA-256:
`53e4db44f3964deedcd16c048136e60a91145a5a64e5ddb00155fbf5a52036d8`;
provenance is `analysis/ui/theme-background-native.json`.
All 217 Debug sanitizer checks and 16 focused Release theme/bounds checks pass.
Background checker and preview cross-compile for Windows; native execution
remains unverified. Formatting and diff checks pass.

GameLua rectangle planning matches all 8,192 original context-fill calls.
RGBA multiplies by255 and truncates to integers before unmasked ARGB shifts;
unlike background RGB packing, these channels are not clamped. Left/top and
right-left/bottom-top truncate separately. Native VCVT.s32.f32 instructions
establish truncation despite misleading rounding labels in decompiled output.
Both state-respect flag values match whether the native state getter runs before
fill; the final Context::Blend argument is0. C produces an explicit plan,
without mutating context state or drawing. Nonfinite/out-of-int32 inputs are
unsupported and preserve output. Full reset-state contents, actual fill
rasterization and GPU pixels remain separate. Corpus SHA-256:
`22e88c22544e0dfce8747f17429f1078ceedcd5ce083ca3cfda03f4662934f18`;
provenance is `analysis/ui/game-rectangle-native.json`.
All 18 focused Debug/Release theme, bounds and rectangle checks pass; the last
full regression before rectangle planning passed all 217 checks. The rectangle
checker and preview cross-compile for Windows. Formatting and diff checks pass.

Foreground composition matches 4,096 original calls, all 4,096 ground rectangle
fills and 16,380 layer callbacks, including 1,820 first-base-layer metric cases.
The oracle preserves original class RTTI metadata in prepared virtual tables,
so the original dynamic_cast selects ThemeLayer versus non-base defaults; only
layer bodies are controlled. Original camera getters, ground-edge arithmetic,
floorf and GameLua.drawRect execute. C truncates height-pivot.y before scaling,
preserves camera multiply/subtract order, floors the ground edge, composes the
rectangle plan and dispatches layers after fill with unchanged filters. Alpha
resets to1 after traversal, including empty lists. Unused camera x/zoom.x and
first metrics remain unused. The C fill provider applies the alpha portion of
the requested reset for this comparison; full RenderState2D reset contents,
actual fill/layer bodies, resource ownership and GPU pixels remain separate.
Corpus SHA-256:
`a40917190be83fe65331a0ac719c98e8f12244d69d127e486c351262aecf7677`;
provenance is `analysis/ui/theme-foreground-native.json`.
All 221 Debug sanitizer checks and 20 focused Release theme/bounds/rectangle
checks pass. Foreground checker and preview cross-compile for Windows; native
execution remains unverified. Formatting and diff checks pass.

ThemeComposition color construction matches 8,192 original native constructors
with empty layer vectors. Background RGB stays raw, ground RGB multiplies by
the binary32 reciprocal word0x3b808081, and a table-valued backgroundTintColor
supplies raw RGB while other input kinds use255. C preserves signed zeros,
finite wide/fractional values and all six nil/false/true/number/string/table tint
cases. The APK catalog checker also accepts this schema for all 31 classic
themes. Mandatory numeric channel coercion beyond finite NUMBER fields remains
unsupported. The original constructor runs only in the private oracle; C reads
the static data graph without a VM or allocation. Empty native vectors are
checked to own no storage; layer construction/lifetime and rendered color policy
remain separate. Corpus SHA-256:
`78c2737f496a9996115ebdc02e449f17af75e588cb19ae2ddaf3efa632a37788`;
provenance is `analysis/ui/theme-colors-native.json`.
All 223 Debug sanitizer checks and 22 focused Release theme/bounds/rectangle
checks pass. Color/catalog checkers and preview cross-compile for Windows;
native execution remains unverified. Formatting and diff checks pass.

The bounded composite resource owner now retains explicitly selected KA3D
payloads, parsed records, cached ordinary sprite bindings and computed bounds.
The existing live theme1 comparison runs through this owner and still matches
both composites, all eleven ordered parts and their bounds/UVs. The selected
owner retains 1,689 bytes of logical allocation; its sprite/archive/pixel
dependencies remain borrowed. Exact-budget reopening succeeds; a one-byte-short
budget and missing part bindings fail with a NULL output and full cleanup under
sanitizers. Enumeration and lookup return stable views without per-frame
allocation, archive reads or decoding. RVIO binding is explicitly unsupported,
and last-file/last-definition lookup is an explicit ordered-set policy rather
than recovered native group ordering. Focused Debug/Release checks, formatting
and Windows cross compilation pass; the full 223-test checkpoint precedes this
owner extension. Provenance is `analysis/ui/composite-resources-native.json`.

The static theme-layer owner retains ordered bindings/handles and color data,
borrowing the static graph, sprite set and optional composite owner. Theme1
retains 968 logical bytes for three background and two foreground layers.
Consecutive table-valued integer slots start at1 and stop at the first other
value. Ordinary sprites take precedence over composites; missing names retain
zero metrics and draw nothing without touching state. Recognized special
subtypes are explicitly unsupported. Exact/short budgets, malformed tables,
missing sprites, an ignored suffix after a non-table slot and callback-failure
prefixes pass under sanitizers. Cached drawing visits ordinary bindings and
composite parts without frame-time name lookup, allocation, decoding or I/O.

The owned theme1 graph now matches 256 native construction/drawing cases from
the complete classic theme chunk and a prepared resource cache containing the
selected live metadata. Original ThemeComposition/ThemeLayer constructors,
Resources lookup, background/foreground composition, layer bodies, DrawUtils,
Sprite, GL_Image and RenderBatcher CPU methods execute in the private oracle.
All five constructed definitions and nine color fields match. The direct-APK C
path matches 22,335 ordered images, 256 ground rectangles, 7,522 clipped CPU
quads and 14,813 culls, including positions, UVs, raw per-vertex tints, selected
geometry/alpha state and emitted triangle order. Cases vary both camera axes,
independent zooms, time, filters and viewport size, including a one-pixel extent.
The original foreground reset executes; only its resolved geometry/alpha fields
are compared here. Native batch buffers/state and a shader token are prepared
to isolate CPU output. No GPU executes, and no pixel parity is claimed.
The fourteen ordinary/two composite cache entries come from the bounded live
read, not recovery of native group loading or variant selection. Spotlight,
dynamic subtypes/overlays, shader/color policy and complete scene pixels remain
open. Corpus SHA-256:
`e5237dd187a1a010a66073b5a6d2541ab33a43b4206a2198dccd66c44aa65f95`;
provenance is `analysis/ui/theme-scene-native.json`.

All 224 Debug sanitizer checks, 25 focused Release checks, formatting and Windows
cross compilation pass at the complete theme-graph checkpoint. Native Windows
execution and complete gameplay remain unverified.

Default GL_Image point-shader selection now matches 4,096 original calls over
34 prepared SurfaceFormat values and varying viewports, rectangles, alpha and
per-vertex colors. Original SurfaceFormat::hasAlpha and GL_Context shader-cache
methods execute; the prepared loader records their actual requested resource
paths. All 3,611 culled calls avoid format/path/shader queries. The 224 opaque
and 261 alpha-bearing visible calls select `2d-sprite` and `2d-sprite-alpha`
respectively. The checker pins the ten original FX/shader/include resources.
Their default variants sample without vertex-color modulation; the alpha
variant uses premultiplied blending and applies ALPHA_FACTOR to every channel.
The independently authored command adapter uses explicit resolved alpha presence,
retains native clip geometry and passes render alpha only for the alpha variant.
Culled commands leave their output untouched and do not validate unused material
fields. Shader overrides/effects, native loading/compilation, device precision
and GPU pixels remain separate. Corpus SHA-256:
`c0ee9ed846fdc46477ff89be7bdf9548ff5b1eb65eb45a4b98046f14c7faff20`.

A second bounded live read corroborates the atlas flags for fourteen theme1
ordinary sprite/part bindings: two use opaque ETC1 and twelve use RGBA4444 with
alpha. They reference two distinct native textures, 512-by-1024 and 489-by-277.
The selected format getter reads offset0x20, confirmed by its retained ARM
instructions and the original texture constructor. All dimensions, initial and
current format values match the direct APK PVR interpretation. The selected read
uses 6,453 bytes, detaches/resumes the original process and verifies mute remains
enabled. C inspects validated PVR layout once at atlas open, caches alpha presence
and uses it in the complete theme1 CPU command comparison. PNG native texture
format selection remains UNKNOWN. Live format corpus SHA-256:
`4ea79a3a4780a0d874ad753fe60a5cdb23b0ffc967209c5d62e679927df5f6a7`;
provenance for both checks is `analysis/ui/image-material-native.json`.

All 227 Debug sanitizer checks and 52 focused Release theme/resource checks pass
at the image-material checkpoint. The final command initialization also passes
focused Debug/Release checks and Windows cross compilation. Formatting passes;
native Windows execution and GPU pixel parity remain open.

The reset-state context fill now matches 4,096 original GL_Context.fillRect2D
calls, following an actual GameLua identity-state reset into the prepared
context. Original geometry, SurfaceFormat color conversion and RenderBatcher
addition execute. All 3,008 culls, 589 opaque and 499 alpha batches match exact
triangle positions and raw per-vertex colors, including negative extents and
large signed coordinates. Context::Blend0 automatically selects alpha when
packed alpha is not255;1 forces alpha and2 forces opaque under this reset state.
The selected vertex-color alpha resource specifies straight-alpha blending,
distinct from the premultiplied default alpha sprite path.
The C command adapter preserves normalized channel construction and uses an
explicit caller-owned white-texture boundary for the native untextured fill.
The native z=0.001 is checked in the corpus and omitted from the depth-disabled
shared 2D command boundary. Arbitrary render state, GPU color packing and final
rasterization remain separate. Corpus SHA-256:
`748f4f4fb04132bd0c32a1c594dc3fb041e095c9af7f54d10435fffcd82ee0d3`.

The Android rubberband path now matches 4,096 original `GameLua.drawRubberband`
and `Sprite.draw` calls. Prepared Resources/SpriteSheet trees and original
reference-counted tokens provide one known sprite; the image callback records
vertices and UVs without executing a GPU. All 3,621 image submissions and 475
zero-length returns match, including 139 cases with unused nonfinite widths.
The corpus covers signed/zero widths, squared-length underflow, nonzero
subnormal squared lengths and squared-length overflow. C preserves the original
arithmetic order and identity-transform zero products, allocates nothing, and
retains caller outputs on unsupported geometry. Repeating the native fixture
reproduces its hash:
`e62bde016ef6bd70aaf2a3f2f7a53457d354ed620c4f9dc0dc38a1b01e4be958`.
This gate covers screen-space image geometry. Actual sling resource selection,
full draw order and GPU pixels remain separate.

The selected Android `SlingshotSystem.drawRubberBandLine` prototype at decoded
file offset 8659 now executes before the original GameLua/Sprite path in another
4,096 cases. Controlled camera getters supply left, top and scalar zoom; all
16,384 camera/draw calls have the measured order, and all five projected float
arguments and 3,648 image submissions match. The 448 zero-length returns retain
caller vertices. Cases include 2,044 negative and 216 zero zooms, cancellation
and large camera offsets. C subtracts camera translation before scaling and
scales width independently; it does not switch to the script's PC branch on a
desktop host. The repeated native trace has SHA-256
`49ae7858b3cf1772c35b27bf7d2916499a69d93f12a19c2b503b0aa4195be1b4`.
Debug ASan/UBSan and Release comparisons pass; the runtime and checker also
compile with MinGW. This checks supplied camera values and geometry, without
claiming live camera scheduling, actual sling textures or GPU parity.

The base sling's three script draw methods now match 4,096 states and 12,288
part calls. All 20,480 render-state, sprite, band and extras boundaries match
their float arguments and order. Original `refreshSprites` and getters choose
the back, holder, normal front or scope front identifiers; controlled inputs
cover every disabled/dragged/scope combination and signed zeros. The holder
draws before its band while resting, but after its band with the supplied angle
and sprite pivot while dragged, followed by state restoration. The disabled
closure boolean is controlled fixture state. This runs original Lua privately;
the C runtime builds a fixed, allocation-free plan. Original renderer calls and
the extras body remain separate boundaries. Debug ASan/UBSan and Release pass;
the runtime/checker compile under MinGW. Repeated trace SHA-256:
`894e4b4a5c0a39ca38fe3f9a7ef2bb1a85e3b1b107edb12919f56d4bca3af03a`.

Default pivot-anchored `Resources.drawSprite` now runs through original sprite
lookup, Sprite anchors and GL_Image rectangle CPU batching in 4,096 cases. All
1,104 submitted quads, including 638 rotated draws, match culling, six triangle
vertices, depth and UVs. A prepared shader avoids GPU selection/flushes; resource
trees, render state and batch storage are controlled. `ab_slingshot_sprite_quad`
composes the corresponding C geometry, without allocation. Repeated trace:
`34822d501e6d61703ac4409c524ef19585c70ecc3fedc44a5de903ce9e93ee14`.

The sling render-pose calculation also matches 4,096 original script calls and
8,192 original coordinate transforms. The input position is the level-start
vector, distinct from the sling-base vector. Physics positions multiply by
twenty; the bird-size adjustment uses the separate supplied `physicsToWorld`
scalar. Boxes and missing radii select half-width, including boxes that also
have a radius. Other birds use radius. All 2,048 bird adjustments, 1,536
half-width selections and distance-based width clamps match, including signed
zeros and the original promoted sine/cosine path. Script state and object fields
are prepared oracle inputs. Debug ASan/UBSan and Release pass, and MinGW builds
both modules/checkers. Repeated pose trace:
`da7cb734f198018d3382b63f1d419cdd2b9aee393cc0d784712931729f722d3c`.
Live scheduling and full sling variants remain open. Private read-only live-state
provenance is `analysis/ui/sling-live-state.json`.

Initial sling vectors match another 4,096 original `setPositionByStartObject`
calls: 2,048 objects, 1,024 supplied radii and 4,096 ordered back-sprite
pivot/bounds queries. Nil objects preserve all vectors. The unused pivot-X and
sprite-width results are NaN in the oracle; the calculation uses only pivot-Y
and height. C preserves subtraction/multiplication/addition order. Repeated trace:
`5d225deac57e27516c37e15f9e2ebed59eed30720a2fdd3bd863a0ea6986757e`.

Default rectangle-image shaders also match 4,096 original calls: 3,369 culls,
152 opaque and 575 alpha shader choices. All 34 native format codes and eight
explicit alpha boundaries are covered, including adjacent binary32 values on
both sides of one. A rectangle chooses the alpha shader for a texture with
alpha or a render alpha below one; the point-image path retains its separately
measured texture-only decision. The checker also pins all twelve original
shader-resource hashes. This tests selection and C command policy, with prepared
shader/cache/batch boundaries. Repeated rectangle-material trace:
`bec6c2cbcc4a41d319db67cfaec51e890e8972c4fbb5452fd29ed416b7436160`.

The complete theme1 graph also runs through actual context ground fills. All
222 opaque ground CPU quads and 34 ground culls match within the original
256-case graph, alongside the 22,335 images and five constructed layers.
Independent prepared batch storage keeps image and untextured-fill ownership
boundaries explicit; original GPU flushing/shaders do not execute. Rebuilding
the native fixture reproduces the same corpus hash after masking nonsemantic
bool padding. Corpus SHA-256:
`64ad3ef2217717c384ae4cfd3c17270673f103831797e6420456aa91fc9cf524`;
provenance for these two checks is `analysis/ui/fill-rectangle-native.json`.

The level-one preview owns the selected theme1 and default sling resources.
Startup caches nineteen theme and four sling region/UV/material bindings, with
nine decoded textures including the solid fill. Frame traversal allocates
nothing, decodes nothing and performs no archive reads or name lookup. The
default endpoint emits 40 commands; the prepared camera drag emits 42. All four
ordinary object commands and three initial bird bindings remain. The first bird
uses the recovered idle sling endpoint; other poses remain loaded snapshots.
Back sprite/band precede objects; holder/band follow objects; ground and
foreground precede the front sprite. This agrees with gamelogic prototype
141561 under the selected full-quality, non-UGC branch. The preview explicitly
skips transfer time. Active gameplay, animations, particles, overlays, settings
and complete root-controller behavior remain separate.

The current nine-texture/40-command scene also executes through our GLES2
backend on the reference Android driver. All 4,675 RGBA pixels at x=[140,195),
y=[180,265), covering the sling and seated bird, match the original framebuffer
exactly. The earlier 65,450 background and 19,200 ground pixels still match.
The expanded check compares every RGBA byte in all 105,600 pixels below y=100,
including the other visible birds, ground and foreground; every pixel matches.
All twelve selected idle pose words also match the read-only live observation,
derived from loaded bird and sprite metadata. Repeating the GPU run reproduces
the complete output. Input SHA-256:
`aca537e6360c9cffc027fd5c48c82102d2d9879e80519866226564440a1ec14f`;
GLES2 capture SHA-256:
`8529248e69b4f692156b31fada1cd5aa185eb922e6c2b8f2b1fdd54f3dcbc0cc`.
The 480-by-320 Metal capture matches 3,699 of those 4,675 sling-region pixels;
17 pixels differ by more than ten channel values, with maximum error 166.
Sampling/edge behavior and presentation remain open; no full-frame claim is made.
The complete Debug ASan/UBSan suite passes 242 checks after the integration;
the focused Release checks and Windows cross-build also pass.

Before the sling addition, Common's live Metal capture and standalone Metal capture produced identical
RGB at 960-by-640 after 128 fixed updates. The diagnostic `ab_theme_capture` also
renders at 480-by-320 without a window. Standalone GLES2/Metal commands now accept
already normalized clip coordinates; mixed pixel/clip Metal commands reproduce
the same RGBA8/BGRA8 test pixels. GLES2 command-state recording and both ES1.00
shader precision variants pass. Optional Metal RGB565 capture also passes
packing, clear conversion, opaque-alpha and straight-alpha blend checks on the
current GPU. Public backend support remains conditional on device capabilities.

The captured Android SurfaceView is 480-by-320 with format 4, independently
identified as [RGB565 by Android's PixelFormat reference](https://developer.android.com/reference/android/graphics/PixelFormat#RGB_565).
The retained screen is rotated exactly -90 degrees for comparison, without
resampling. Rendering directly into Metal RGB565 matches 62,132 of 65,450 selected
background pixels and 18,496 of 19,200 ground pixels; errors reach one
quantization step. This attachment is a diagnostic option.

A bounded debugger capture now reads the original framebuffer immediately after
the APK's own `GL_Context.capture` calls `glReadPixels`. The native PNG and the
614,400 captured RGBA bytes match exactly after row orientation is corrected.
The original capture flag and filename are restored, diagnostic memory is
released, and the app resumes muted. No original binary files are changed.
Current GL reports RGB8, no alpha, depth24, stencil8, zero samples and dithering
enabled. Its EGL configuration reports RGB565 with native visual ID 4. The
recorded SwiftShader reference reports 23 fraction bits for all three fragment
float precision classes. These observations are scoped to this emulator.

Against those raw framebuffer pixels, Metal RGBA8 matches 62,573 of 65,450
selected background pixels and 17,412 of 19,200 ground pixels exactly. Maximum
channel errors are three and two respectively. The original camera's published
translation and zoom agree with the C preview; the small logical-center
rounding difference cancels during viewport publication.

The same cached scene now executes through the first-party GLES2 backend on the
isolated Android driver. All 65,450 selected background and 19,200 ground RGBA
pixels match the original framebuffer exactly. The standalone program loads
eight decoded textures and 35 prepared commands; it compiles only project C,
project shaders and narrow declarations of the platform C/EGL/GLES APIs. It
loads no original game code. A completion marker includes the private input's
SHA-256, and a repeated run reproduces the complete captured output. The
`theme_gpu_android_apk` check pins the APK and all private inputs, compares the
original PNG against every raw RGBA byte, and compares both selected regions.
It checks retained captures; regenerating GPU output requires the reference
driver. Metal sampling/rasterization and omitted full-frame content stay open.

Nearest normalized conversion of the original RGB8 pixels into RGB565, followed
by nearest normalized expansion, matches 84,649 of 84,650 selected screen pixels.
The remaining pixel at (211,239) differs by one in each channel and is stable
across a repeated screen capture. This identifies the dominant presentation
conversion but leaves an explicit residual. Native sampler/rasterization,
presentation details, omitted gameplay/UI and full-frame parity remain open.
Private captures, masks, metrics and provenance are recorded in
`analysis/ui/theme-preview-native.json` and `theme-native-gl-method.json`.

Museum camera bootstrap now loads `birdCameraData.ipad` and
`castleCameraData.ipad` from static level data, as the Android CameraSystem
explicitly does. Its fit computes both rounded edges in 1024-by-768 authored
space, subtracts them for horizontal coverage, derives zoom from logical width
and preserves the authored bottom edge. Direct loading of level one from the
APK matches both current native snap positions/zooms and all eight initialized
limit fields exactly at 480 by 320. The selected read resumes the original
process in a finally block and verifies it remains muted. Current mover state
and start-sequence timing are separate from target snap values. Debug ASan/UBSan,
Release, all 117 default tests, formatting and Windows compilation pass. Broader
level/profile coverage, sequence/controller order and final-frame parity remain
open. Provenance is `analysis/camera-bootstrap-native.json`.

The first-party camera sequence manager matches 8,192 original commands and
numeric states, with 1,340 controlled delegate boundaries, 20 custom-condition
queries and 544 completion callbacks. Original duration/no-moving/specialty
closures execute in the isolated probe; action effects and callback mutations
are controlled boundaries. Checks include one stage per update, strict duration
comparisons, discarded overshoot, input interruption before time addition,
completion callback before reset and preservation of `stageTimer` through reset.
The static reader loads both original level-start/restart tables directly from
the APK into six prepared mutable C stages without executing Lua. Debug
ASan/UBSan, Release, all 118 default tests, formatting and Windows compilation
pass. Actual camera action effects, parameter merging, arbitrary script
conditions and integrated CameraSystem startup remain open. Provenance is
`analysis/camera-sequence-native.json`.


`Sprite` construction at analysis address `0x5b44dc` normalizes atlas endpoints
against texture dimensions without a half-texel inset. Unrotated coordinate
order is top-left, top-right, bottom-left, bottom-right. Signed width and height
loads are confirmed by `ldrsh` in the getters at `0x5b4978` / `0x5b4980` and
the draw branches. The first-party quad builder preserves negative source
coordinates, negative dimensions, and zero-area quads.

Anchor names and numeric fields come from `Anchor::fromString` at `0x595f88`:
vertical TOP=0, VCENTER=1, BOTTOM=2, BASELINE=3, VPIVOT=4; horizontal LEFT=0,
HCENTER=1, RIGHT=2, HPIVOT=3. Sprite baseline and vertical-pivot branches both
use the rounded Y pivot. Center branches arithmetic-shift the signed dimension
before scale multiplication. Pivot getters at `0x5b4988` and `0x5b49a0` add
binary32 0.5 and convert toward zero, including for negative pivots. This differs
from symmetric nearest rounding and is preserved.

The affine branch in `Sprite::draw` at `0x5b4700` accumulates the Y product,
then the X product, then translation. The shared draw module retains this
operation order with contraction disabled, requires binary32 floats, and builds
commands into a fixed allocation without reordering. Tests include negative
odd dimensions, anchor/scale order, cancellation-sensitive affine arithmetic,
in-place transforms, non-finite inputs, and list capacity/lifetime. Private
native evidence is `Files/AngryBirdsC/analysis/sprite-geometry-native.json`.
These code-backed checks do not establish frame presentation fidelity. Camera,
clip state, blending, shader behavior, and native captures remain open.

The owned atlas cache now shares decoded PNG/PVR images across an explicit SPRT
set, resolves texture basenames in each metadata directory, and retains stable
image indices without frame-time decoding or allocation. A bounded read of the
running pinned ARMv7 game records all 22 visible ordinary level-object sprite
bindings. Direct APK loading matches their image paths, signed regions, pivot
words and all eight UV words exactly in Debug and Release. The red bird's current
base sprite and a separate current yell observation both use 46-by-45 regions;
the base sheet is 1020 by 1021. Older frozen physics corpora contain 56-by-52 bird
size inputs whose producing state remains unknown; circle geometry ignores
those inputs, and the corpora remain unchanged. Cache ownership, shared-image
lookup, order, PNG/PVR decoding, budget and partial-failure checks pass under
ASan/UBSan. All 114 default tests and formatting pass; Windows compilation passes.
Sampler state and final game-frame comparison remain open. Provenance is
`analysis/sprite-bindings-native.json`. Later resource validation now decodes all
seven APK WebP images entirely in C, matching 20,217,952 original RGBA bytes,
and matches the captured live Android CPU state across 28 source maps and
1,495 bindings. Both backends still need complete resource integration. Evidence:
`analysis/input/webp-decode-native.json` and `resources-live-native.json`.

The classic scene owner now binds the APK-loaded level's initial sprite names,
resolved catalog layers and rendered physics poses to that atlas. All 22
ordinary renderer names, creation/layer order, colliders and unit constructor
scales match the running original. Current expression names from the capture
are controlled setter inputs, not initialization-time evidence. Ground has a
basic collision body and no ordinary renderer; its stable scene-index placeholder
is unbound and invisible. Explicit sprite/scale/visibility changes and pose sync
feed the shared draw list without allocation. Exact retained-budget limits,
lookup failures, append rollback and hiding a removed body pass in Debug and
Release; Windows compilation and all 114 default checks pass. Full controller
scheduling, dynamic scene membership, special renderers and final frame parity
remain open. Provenance is `analysis/classic-scene-native.json`.

## Delta-based sling displacement

Original SlingshotSystem update/reset prototypes 7382 and 7783 match C numeric
states across 4,096 cases, including every signed-zero input combination,
positive/negative horizontal zoom, sensitivity and independent update/reset
enablement. The pinned full Slingshot/Vector2/utils chunks and native
CoordinateTransforms screen-distance method execute. All 4,096 conversion
results match; camera translation and vertical zoom are unused. Enabled updates
subtract converted deltas times sensitivity before clamping length to shoot
range plus binary32 3.2. Disabled updates return nil and preserve state. Enabled
returns alias the retained vector; enabled reset replaces it with positive zero.

The C API owns only values, leaving table identity, resolved truthiness and
callback effects to callers. Native callbacks record original intermediate
states, but mutation during those boundaries is outside the C contract. Corpus
SHA-256 is `0e53dc7f30d7b2ff2019d310738f129f1d36b3e923c1f9b4367635c844ceea1d`.
Debug sanitizer and Release comparisons pass; Windows compilation passes.
Full touch selection, pointer routing and launch scheduling remain open.

## Ordinary mouse sling input

The running pinned APK reports gesture, delta-based and accelerometer control
false, with bird readiness and release-after-specialty true. Original
GameUpdate prototype 110400 executes unchanged through its ordinary mouse input
phase in 4,160 cases. Its prefix uses controlled non-input callbacks and the
original native screen-to-physics/world transforms. A controlled sentinel error
at the following SlingshotSystem.gameUpdate boundary stops the pass; its exact
marker is checked. This is not a complete original game update.

The C plan matches every retained position, five-entry drag sample/presence/index,
resolved dragged state and all 2,635 ordered boundary snapshots. Cases include
all 4,096 Boolean combinations, 64 exact radius-boundary scenarios and the
ten-call press/hold/release path. Selection retains strict radius comparison,
zoom/width scaling and binary32 ordering. Drag sampling retains its 0.017-second
threshold and five-slot wrap; release clears dragStarted before its bird query.
Repeated execution is identical. Debug sanitizer/Release checks and Windows
compilation pass. Corpus SHA-256 is
`d7a0dc85a86a5e1bbb37c0581bd88513a82b492d98581873aab0f6adfcf65942`;
private provenance is `analysis/input/game-sling-mouse-native.json`.

Behavior skipInput, resource queries and sling boundaries are prepared. Actual
stretch/release effects, callback mutation, flying-bird specialties, gesture or
delta modes, later input housekeeping and full gameplay integration remain open.

## Ordinary post-input game update

The original GameUpdate prototype also runs through normal return in 4,096
ordinary-tail cases, with three stable objects and controlled controller
boundaries. C matches all 101,239 ordered callback/state snapshots, final
controls, optional eagle darkness/time, cursor history and object-script flags.
Sling update precedes wobble; object animation/rocket/bird updates precede
conditional boundary/effect work. Camera and boomerang updates precede character
animations, floating scores and score updates. Conditional feather/level effects
precede block scripts and a second object-script pass. Cursor history copies
before graphics effects and drawing; skip flags clear and optional elapsed time
advances afterward. Signed-zero arithmetic is retained.

Controlled callbacks mutate flags, darkness, cursor, script visibility and time
presence. The C executor reads them at the original stages and retains partial
progress on callback failure. Original object traversal order is a supplied
boundary, not recovered by this executor. No flying-bird special branch,
grenades, custom per-object hooks or joints run in this corpus. Real controller
bodies, changing membership/order and full frame integration remain open.
Repeated native execution is identical; Debug sanitizer and Release comparisons
pass, and Windows compilation passes. Corpus SHA-256 is
`a259b8591964e574cd9990ea69f8e060693cd62b11c641fbddb9446c308f3ce6`;
private provenance is `analysis/input/game-update-tail-native.json`.

## Physics frame force clearing

The original `b2World::ClearForces` at ELF `0x8d1a70` matches C in 4,096
controlled calls over every four-body membership mask and randomized list
order. All 8,192 live bodies receive positive-zero force/torque; removed slots
retain their input bits, including NaNs, infinities and signed zero. The native
probe checks every other raw body word for preservation; the C checker checks
complete body records, including sleeping/type/active state. Clearing is valid
while locked. Invalid C storage rejects before mutation. Repeated native
execution is identical, SHA-256
`6cb59331e51f2cbdef01243169f388d2dc6b31328edbbabe1297aeaf76008cba`.

A bounded live capture of the original `GameLua::update` records 34 call-site
boundaries across zero-step, three-step and disabled updates. Enabled updates
clear forces after every world step/glue pass and before block removal/history,
then clear again after the loop, even with no steps. Lua force-function cleanup
follows that final clear; root Lua update follows interpolation. Disabled
updates skip those physics phases and still reach root update. The observer
controls elapsed time and one physics-enable byte, restoring the byte after
each call, then detaches and resumes the muted reference. It does not patch
original instructions or single-step. Private records are
`analysis/input/frame-phase-live.json` and `world-clear-native.json`.

The C frame driver now clears at these boundaries and exposes preparation and
post-loop callbacks. Focused cases verify force carry/clearing, history timing,
zero/disabled paths and partial failure. All 253 Debug sanitizer tests and 13
focused Release comparisons pass; Windows cross-compilation passes. Native
force preparation, glue, deferred work and complete outer-frame integration
remain separate gates.

## Per-object friction preparation

`GameLua::updateFriction` at ELF `0x317f94` matches first-party C in 4,096
original-library calls. Nonzero `airResistance` takes priority and scales both
velocity components by its negation. Otherwise the optional relative X/Y
coefficients are transformed using the body's rotation matrix determinant;
their velocity projection applies only when negative. Force application at the
center preserves the original intermediate operations and signed-zero torque.
Dynamic bodies wake when a force is applied, including zero air force; static
and kinematic bodies retain their state. The native elapsed argument is unused.

The corpus covers optional numeric/missing fields, normalized and arbitrary
rotation values, singular matrices, signed zero, existing forces and sleeping
bodies. C matches 689 force changes and 241 wake transitions; all other body
state is preserved. Repeated execution is identical, SHA-256
`777c781d84d06a080e00497666443aebbc68b33a62e2aae6bfdc2720b05f61a4`.
Debug sanitizer/Release checks and Windows compilation pass. The C API accepts
resolved definition values; runtime table lookup/coercion, force-adders, water
and whole preparation integration remain open. Private provenance is
`analysis/input/object-friction-native.json`.

## Continuous object force preparation

`GameLua::updateForceAdder` at ELF `0x322e10` matches C in two 4,096-case
corpora. Gravity-disabled objects first receive negated gravity times the
engine body's mass, even when `disableForce` is Boolean true. Other truthy
disable values do not suppress configured forces. The ordinary continuous
route uses the object-table mass and optional X/Y values. Relative force instead
uses the body's stored angle, multiplying binary64 cosine/sine by the promoted
force before float rounding; it ignores object mass and X/Y settings. The C
trigonometric arc is explicitly bounded to [-1024,1024].

Velocity limits suppress an axis at/above its absolute limit only when that
force accelerates it. A nonzero vector admitted before limiting still wakes a
dynamic body when both axes are subsequently suppressed. Both force requests
apply at center with the native intermediate/signed-zero torque arithmetic.
Static/kinematic bodies retain all state. The ordinary corpus matches 860
force changes, 459 wakes and 107 disabled gravity cancellations; the relative
corpus matches 1,103 changes, 587 wakes and 107 disabled cancellations. Exact
limit edges, adjacent floats, both force signs and signed zero are included.

Repeated captures are identical. Ordinary SHA-256 is
`fdaf13b2c2c1ac7d079ea0a221d74f6305071838b69206c902968d21a670d98e`;
relative SHA-256 is
`d9c92f19e2b15c1981555f819ae0f208c77f2f8bb7218aff572d5142be5b3449`.
Debug sanitizer/Release comparisons and Windows compilation pass. Interval and
resource phases have separate comparisons below; runtime field/coercion binding
remains open. Private provenance is
`analysis/input/object-forces-native.json`.

The C preparation adapter now runs queued-force hooks, then each enabled
object's friction and force-adder in supplied native-map order,
then water and script-step hooks. It can be installed directly on the frame
driver; post-loop cleanup is a second callback. The original live capture
passes float 0.1 to water three times while each world step receives 1/30.
Assembly traces water's argument to the scaled frame duration, so the adapter
keeps it separate from the fixed step. Focused composition checks exercise
actual force integration over two world steps, late object gates, world-gravity
reads, zero/disabled updates and partial failure. Actual native map/gate/table
binding, queued-force/water/script bodies and a complete native composition
comparison remain separate gates.

## Timed object force pulses

The original force-adder also matches C across 4,096 numeric-interval cases,
with 1,583 pulses, 91 sprite restorations and 407 velocity changes. Enabled
timers subtract the fixed step from a present timer or the initial interval.
At/below zero, one interval is added, the current sprite becomes original and
the applied sprite is requested. The configured vector then applies as a center
impulse, with the existing velocity limits and gravity-cancellation order.
Otherwise a present original sprite is requested below the strict binary64
`interval * 0.8` threshold. That reference remains present, so restoration can
be requested repeatedly. Boolean disable preserves timer/sprite state while
gravity cancellation continues. Zero/negative intervals do not create a
catch-up loop. C edge cases also cover repeated transitions and partial input
rejection; invalid plans preserve output.

The native fixture uses actual cached Lua tables and a controlled object map.
`GameLua::setSprite` executes and updates the shared world table, with the
attached renderable explicitly absent; actual renderer effects are outside
this corpus. Sound fields are absent. No audio output or device is created.
The trace SHA-256 is
`daf9c7b663763e38c6d1bb9f7db49ee52ca7f528bf71d5b9a75fb9c492c83def`;
repeat execution is identical. Debug sanitizer/Release comparisons and Windows
compilation pass. A separate sound/RNG comparison follows; complete per-object
scheduling remains open. Private provenance is
`analysis/input/object-force-interval-native.json`.

The preceding preparation checkpoint passes all 258 Debug sanitizer tests
(37.74 seconds), ten focused Release tests and Windows compilation. The new
interval route has separate focused checks; that checkpoint does not claim a
complete native preparation or playable gameplay comparison.

## Force pulse resources and composed update

A fourth 4,096-call original force-adder corpus covers pulse sound selection,
actual Resources/AudioOutput forwarding and pending mixer voices. It matches
984 requests, 318 integer-remainder RNG draws and 662 queued voices (315 first,
198 second and 149 third clips). Numeric sound values pass the original string
predicate; the selected numeric name is absent from the controlled clip map,
so 322 requests produce the original missing-clip result without a voice.
Named sounds consume no draw; arrays use `lrand48() % count`, rather than Lua
math.random's float scaling. A missing numeric gain defaults to one. Present
gains retain their float bits. Requests are nonlooping on track zero with unit
speed and the other native play parameters zero.

The fixture constructs silent clip metadata and an in-memory mixer. A bounded
AudioOutput forwarding record points to that mixer; no device output is
constructed, no mix occurs and no samples are played. The checker compares
the next native random value and complete supported pending voice metadata,
alongside force, velocity, timer and sprite state. Repeat execution is identical,
SHA-256 `4cbf04825406728035bc4aba7df84812dc4d49de49e66f2ccf5edd30127a8d03`.

`ab_object_force_step` composes gravity cancellation, timer arithmetic, sprite
and sound requests, timer publication and continuous force/center impulse.
Its complete result matches all 16,384 native cases from the four corpora.
Resource callbacks see sprite state before timer publication; later force and
sound values are read at their original phases while the initial disable
decision remains captured. Focused cases verify those observations and retained
partial state on callback failure. The preparation driver now uses this
controller for each gated object. Resource binding remains caller-owned.

The checkpoint passes all 260 Debug sanitizer tests (37.99 seconds), twelve
focused Release tests and Windows compilation. Runtime property/coercion,
map/order, renderer/device binding and complete scene/RNG-consumer order remain
open. Private provenance is `analysis/input/object-force-sound-native.json`.

A separate read-only snapshot records 23 original level-one object-map entries
and their cached Lua force properties, using 20,837 selected bytes. Their native
force-adder gates are all false, independently of awake/active body state. The
two queued static birds retain positive cached Lua mass while their engine mass
is zero. Constructor instructions gate preparation on Lua truthiness of the
six force/relative/air/friction fields, and the native setter writes the separate
GameObject flag only when a body exists. Initial property binding must preserve
those distinctions. The snapshot does not establish later mutations or other
levels. It is retained as `analysis/input/force-properties-live.json`.

Initial binding now reads the resolved catalog into a bounded `AbClassicForces`
owner. Its rows follow length-aware lexicographic name order, separate from Lua
world-table iteration. The level loader retains creation-time mass before queue
changes; the owner borrows canonical gravity flags and prepared sprite names.
The six-field preparation gate preserves numeric-zero truthiness. Sound arrays
resolve contiguous numeric indices independently of constructor entry order.
No Lua, effects or RNG draws execute during initialization.

The enclosing native preparation branch also visits objects whose gravity flag
is false even when their force-adder gate is off. Analysis addresses `0x3348a8`
through `0x3348b0` test the force byte; `0x334a60` through `0x334a6c` call the
gravity getter and branch to friction/force preparation when it returns zero.
The C driver preserves this OR condition and reads the canonical gravity flag
at each visit. A focused regression checks both canonical and resolved flags.

All 618 pinned catalog definitions initialize, including 25 force gates, one
numeric interval and three sound choices. The loaded level matches all 23 native
map names, cached masses, gates, gravity flags and selected resolved numeric force
fields. The normalized private snapshot has SHA-256
`f4a86c909ad3ceda4bd185d870d1510f5b2a69744fac4cf7e056b61f287c23e5`;
the original read-only JSON has SHA-256
`de5d2060916b33fe737465d447be5ce4bd1f464233131740ffed9614d0f79bfe`.
Focused C checks cover allocation limits, duplicate/embedded-zero names, cached
mass after queuing, changing gravity flags and source-indexed interval resources
with shared RNG. The latter is a binding check using an APK definition on an
existing body, not another native gameplay capture. Initial numeric strings and
noncontiguous sound tables reject explicitly. Later table mutation/coercion,
membership changes, actual renderer/audio effects, water and full frame behavior
remain open.

## Queued physics forces

The original gamelogic prototypes 160869 (capture), 161153 (clear) and 161227
(per-step replay) match C in 4,096 sequences and 14,816 ordered applications.
The five captured arguments retain their original float bits across zero to
three passes, with object membership changing between passes and applications.
The closure checks for nil: a false-valued object entry still reaches the native
force boundary. Clearing drops every request before an empty replay, then a new
append/replay works independently. Selected prototypes and their instructions
remain unchanged; original Lua runs only inside the Android oracle. Native
repetition is identical, SHA-256
`1ede6ed6636267aeab57300f4c38c154ac83b97f392ab1ebbabc1e3c3b67a84c`.

The direct original `GameLua::applyForce` boundary at ELF `0x3128f0` matches
another 4,096 C cases for accumulated force, world-point torque, wake and sleep
state. All other raw native body words remain unchanged. The corpus covers all
three body types, signed zero, arbitrary force/application points, existing force
and torque, and both awake states. Repetition is identical, SHA-256
`9af755bd0c5cdca65825c1bf37e63ddac881ab7c30756d0772e52d4fe4acf31f`.

The C queue uses caller-owned bounded storage, borrowed names and no per-frame
allocation. Loaded-level binding uses the force owner's initial name map and
current body slots; missing/removed names skip and static bodies ignore force.
Focused frame checks retain requests during disabled updates and clear them
after enabled zero-step updates. Partial callback failures preserve prior work.
Queue membership remains stable during a replay; arbitrary table mutation,
numeric/string coercions, object insertion and complete gameplay remain open.
Private provenance is `analysis/input/force-queue-native.json` and
`force-point-native.json`.

The APK-backed twelve-frame comparison now initializes this force owner and
runs its preparation and post-loop callbacks. All 23 body states, contacts,
bounds, both pose histories and interpolation still match through 26 physics
steps and sling transfer/holding. This level's captured force gates are off;
the comparison does not establish other levels' force/water behavior. The
checkpoint passes all 268 Debug sanitizer tests (45.62 seconds), 17 focused
Release checks and Windows compilation. The muted reference is detached and
running; ongoing source changes remain uncommitted.

## Water-effect configuration

The original `WaterEffect` constructor at ELF `0x2e8dc8` matches C across
4,096 finite typed configurations. A numeric top-level `waterLevel`, including
signed zero, takes precedence over nested `water` and clears freezing/kill-level
options. A nested table enables water even without a numeric level; missing or
non-number levels become zero. Freezing requires a Boolean true, while a numeric
`killLevel` enables the kill threshold. The oracle runs the native constructor
and original LuaState/LuaTable field accessors. Disabled/absent numeric words are
uninitialized natively and normalized only when their getters are unused.
Repeated execution is identical, SHA-256
`aabb4a4b7dced2a2c517569e73517d1748008991948b011ebcc276817f66d0ac`.

All 655 decoded static level-related chunks pass the C reader; none directly declares
enabled water. This does not establish the state after menu/theme/script
assignment. Numeric-string coercion rejects explicitly; buoyancy, drag, splashes,
kill effects and runtime binding remain open. Debug sanitizer edge/native/level
checks, three focused Release checks and Windows compilation pass. Private
provenance is `analysis/input/water-effect-native.json`.

## Block events and script physics steps

The original BlockScriptManager per-object/global event prototypes 1447 and
1682 match C across 4,096 cases and 5,270 handler calls. Three stable names and
two attached script stand-ins cover six insertion permutations, absent/false
handlers, nil/false world pruning, repeated global traversals and direct object
dispatch. C composes the independently recovered growing string-map order.
Arguments retain number/nil/string values and arity. Each present handler gets
a fresh raw-definition lookup; earlier callbacks can replace later handlers,
definitions and world membership. Pruned objects stay absent from later global
passes. Selected instructions/constants remain unchanged; one empty debug
upvalue name is restored solely for the native public binding API. Repeated
execution is identical, SHA-256
`383f288cf3c493777a3150037a93d3a581fd3787dccdb3047f950f0eecffc9e9`.

The original gamelogic updatePhysicsStep prototype 56438 also composes with
those native methods in 4,096 cases and 7,003 recorded level/block calls. The
optional level hook receives elapsed time alone, then the current block event
name is read and dispatched with that same numeric argument. Hook mutations
affect the following traversal; handler changes to the event-name field apply
to later traversals while the current event remains captured. C matches every
call and final membership/handler/definition/event state. Repetition is
identical, SHA-256
`8a2544f870141fb5f4d4d9413e417d5822615f405c72483e3396cb50aa7ac14e`.

The checkpoint passes all 274 Debug sanitizer tests (49.97 seconds), three
focused Release checks and Windows compilation. Actual level/block handler
bodies, insertion/reinsertion and full frame binding remain separate gates.
Private provenance is `analysis/input/block-event-native.json` and
`physics-script-native.json`.

The original whole BlockScriptManager module now matches C across 4,096
registration/application/clear/reapply cases and 46,086 callbacks: 32,760
applicability tests, 2,774 initialization calls, 7,454 applications and 3,098
disposals. Two script identities and three names cover duplicate registrations,
preexisting/duplicate attachments, numeric-zero truthiness, absent/false optional
methods and catalog rebinding during callbacks. Definitions remain cached during
one application. Initialization membership is set before the callback; attachments
precede apply, and tracking follows it. Clear retains registration and object
attachments. All callback snapshots, both state snapshots and growing string-map
orders match; the largest observed attachment list contains 24 entries.

The native initialized set uses script-table pointer keys. Its order is read
before clear and supplied as an explicit C disposal permutation. This comparison
does not recover that pointer-key hashing/allocation policy. Eight empty debug
upvalue names are restored for native public table binding; original module
instructions/constants remain unchanged. Repetition is identical, SHA-256
`074854b17ae49c7a735533312650d9841ba6f5f2d412101deef9fd5fd7bb8387`.
Debug sanitizer native/edge checks, five focused Release checks and Windows
compilation pass. Renaming, insertion after pruning, actual script bodies and
complete level binding remain open. Private provenance is
`analysis/input/block-script-native.json`.

The shipped FlameEffect, AmbientAnimations and AmbientParticles canApply
prototypes match another 12,288 original calls over 4,096 configurations. Both
the raw definition's components container and its selected child must be tables;
Booleans, numeric zero and the string "table" do not qualify. Selected prototypes
are unchanged. Native repetition is identical, SHA-256
`3f5d8d1955bea03a0efed919f78fedc8f470e9a0066e18ec6de9f18e553a85f2`.
All 618 raw catalog definitions pass the C reader: two flame, eight ambient
animation and five ambient particle definitions qualify, with maximum child
table entry counts of four, one and one. Plain-table applicability, borrowed
child views and invalid-storage checks pass Debug sanitizer/Release comparisons
and Windows compilation. Component state/update/draw bodies, overridden type
functions/metatables and full runtime binding remain open. Private provenance is
`analysis/input/block-component-native.json`.

The original FlameEffect update (prototype 329), with the original clampAngle
helper (8867), matches 4,096 resolved C updates. The corpus covers zero-to-eight
sprites, one-period timer carry, out-of-range nil sprites, nil/false/numeric
previous angles, velocity thresholds, jitter and shortest-angle smoothing.
Every call consumes one shared unit-random draw, including zero elapsed time.
Sprite-specific offsets precede default and positive-zero fallbacks. Unused
scale values, including NaN payloads, are retained. State and the following
random result match exactly. Original instructions/constants are unchanged.
Repeated native execution is identical, SHA-256
`abb39292ea488ba5dc4f09005dd751109c88047eb8c63a1ad45474c2796d9ace`.
Debug sanitizer/Release edge and native comparisons and Windows compilation
pass. Component construction, callback registration, draw and complete runtime
binding remain separate gates. Private provenance is
`analysis/input/flame-effect-native.json`.

The original FlameEffect draw prototype 1192 and utils rotation prototype
11901 match 4,096 C draw plans. Nil/false world entries remove the callback;
numeric zero remains present. Scale the offset first, round original double
cosine/sine results to float, rotate with float products, then add script x/y.
The body's angle rotates the offset; the smoothed flame angle rotates the
sprite. Absent sprites stay absent at submission. Signed zeros, cardinal arcs,
subnormal inputs and ignored NaN fields on the removal path match, with no
random draws. Repeat execution is identical, SHA-256
`d81c5154f16de8233c69f14c405f69dd002be54b413e8e75f5816d96b9cfb60a`.
Debug sanitizer/Release comparisons and Windows compilation pass. The renderer
and extra-draw map remain separate boundaries. Private provenance is
`analysis/input/flame-draw-native.json`.

The unchanged whole FlameEffect module also matches 4,096 construction cases
using its real update/draw closure graph. Initial state is observable at update
registration: zero timer, configured/default speed and scale, first sprite and
absent derived fields. Registration precedes getAngle/getVelocity and the
immediate zero-time update. That update consumes one shared random draw before
the extra-draw record is stored at z_order + 1, with the original object and
draw-function identities. Nil/false speed/scale default to one; numeric zero and
unused NaN scales remain values. All initial/final fields and the following
random result match. Repeat execution is identical, SHA-256
`098f0424b1cbca59b8a3a221da030a2269964c067b80a6f8ad190faf2f04ec16`.
Nine focused Release checks, Debug sanitizer comparisons and Windows compilation
pass. Both APK flame definitions pass the bounded C configuration reader.
Callback failure/registration mutation checks cover the C host contract;
those mutation cases are not native fidelity claims. Object update/draw-map
ordering, resource submission and level startup binding remain open. Private
provenance is `analysis/input/flame-apply-native.json`.

The original AmbientAnimations update (1377), with its original start (649)
and advance (987) helpers, matches 4,096 four-update sequences in a single live
object map. These cover one-to-four variants, nil/false/numeric delay,
zero-to-eight sprites, zero/negative elapsed time, repeated selection and
original-sprite restoration. All 16,384 snapshots match: 7,173 starts, 9,924
delay-only calls, 1,994 playing calls, 4,466 completions and 6,172 sprite calls.
Variant selection consumes an integer-boundary draw; an implicit delay consumes
another unit draw. A call crossing the delay only increments its delay timer.
Later calls select ceil(timer * speed); a missing frame clears that variant's
timer and always requests the captured original sprite. Every variant timer,
current selection, delay, sprite assignment and following RNG result matches.

Original instructions/constants are unchanged. Three empty debug names permit
binding the original update's object map and helper closures. The physics query
is true and the single world object remains present throughout this corpus;
multi-object pointer-key order and pruning are separate gates. Repetition is
identical, SHA-256
`15597b564cabf0a20e4e4c1b55c146386d78e393429fb1af76ed0213745d66fb`.
Debug sanitizer/Release edge/native checks and Windows compilation pass. The C
reader also accepts all eight shipped descriptors and 20 sprite entries, with
explicit entry order and independently mutable variant timers. Apply shallow
copies, map ownership/disposal and complete controller scheduling remain open.
Private provenance is `analysis/input/ambient-animation-native.json`.

The original AmbientParticles update (631) matches 4,096 four-update sequences
and 16,486 ordered conversions/emissions in a single live object map. Up to three
descriptors cover nil/false/numeric timers and delays, signed zero, negative time
and ignored NaN object fields for empty lists. Absent timers take an explicit
numeric delay or one unit-random draw; numeric zero remains present. Each call
subtracts elapsed time once and emits at most once per descriptor. Expiry clears
the timer before rounded double-trig offset rotation and world conversion.
Particle name and amount are read after conversion. Every timer, conversion
input, effect/amount/coordinate argument and following RNG result matches.
The native recorder verifies three trailing positive zeros and two false flags.

Instructions/constants are unchanged; one empty debug name enables object-map
binding. Physics is enabled and the object remains present. World conversion
is a controlled affine boundary, so this corpus does not recover its actual
implementation or particle spawning/simulation. Repetition is identical,
SHA-256 `2bbf13f8511b83db88af14d937dac540dd26943357811da1428faf8e63d157b3`.
Nine focused Debug sanitizer/Release checks and Windows compilation pass.
All five shipped descriptors pass the bounded C reader: four explicit half-second
delays and one random delay. Multi-object pointer-key order, application copies,
disposal and complete frame binding remain open. Private provenance is
`analysis/input/ambient-particle-native.json`.

Another 4,096 sequences compose that original particle update with the actual
CoordinateTransforms::physicsToWorldTransform method at ELF 0x362ccc (analysis
0x372ccc). All 16,486 input/output conversions and resulting emissions match.
The C effect uses the recovered x20 point helper by default; a supplied host
override remains available for controlled boundary comparisons. This closes
world-conversion composition for this path. It does not implement particle
spawning, simulation or scheduling. Repetition is identical, SHA-256
`740dd3f9c9eaa3124bf6bd1ab6bf50b5e54832e1f021af44f0a7c461c5eb9e71`.
Three focused Debug sanitizer/Release tests and Windows compilation pass.
Private provenance is `analysis/input/ambient-particle-coordinates-native.json`.

The original BlockScriptManager.update prototype 1913 matches 4,096 cases and
7,892 calls: 3,796 registered-script updates and 4,096 final global events.
Zero-to-six dense registrations include duplicates, nil/false/optional methods,
method replacement, self-disabling, current event-name changes and 839 bounded
appends reached during the same traversal. All callback order, registration and
handler snapshots, event selection and numeric bits match, including unused
NaN time payloads. Instructions/constants are unchanged; one empty debug name
permits registration-list binding. Native global dispatch is a recorder boundary;
C composes its separately validated global dispatcher with one recorder object.
Repeat execution is identical, SHA-256
`30040e8d8ad89fbe032966540bd7e65385218b36aabd3b892d8d6d0b1911778c`.
Debug sanitizer/regression checks, four focused Release checks and Windows
compilation pass. Actual ambient-controller scheduling, registration removal
and complete frame binding remain open. Private provenance is
`analysis/input/block-update-native.json`. The preceding full checkpoint passes
all 289 sanitizer tests in 50.69 seconds.

Original per-object hook add (121392), remove (121898), clear (122197) and has
(122288), with the nested callable-list closure (121778) and original contains/
find_key helpers, match 4,096 cases / 49,152 operations. Three stable function
identities cover lifecycle changes, queries, direct/registered calls and
in-callback addition, removal and clear. All 9,934 calls, 17,702 assertion/call
errors and 1,728 collapsed-alias snapshots match. Numeric argument bits, including
NaN payloads, are retained.

A first addition stays a function; adding to a function creates a two-entry
callable list even for the same identity. Adding an existing list member raises
the original assertion. Removing the last single function clears both bindings;
shrinking a list to one changes the object's property while the registry keeps
the old list alias. Captured dispatch keeps that list and observes bounded list
edits between slots. A first-party adapter executes requested mutations in the
isolated original VM, so original assertion exceptions remain within its native
frames. Selected original instructions/constants are unchanged; three empty
debug names enable registry bindings. No interpreter enters the C runtime.
Repetition is identical, SHA-256
`16792e53b52f10785884b9d729080f3e4e92f2f6afe78830a0f2eeed45660a8c`.
Debug sanitizer/Release edge/native checks and Windows compilation pass.
Multi-object map traversal, global registry replacement, nil/nonfunction inputs
and complete startup/frame scheduling remain open. Private provenance is
`analysis/input/object-update-native.json`.

The ordinary GameUpdate now also matches 4,096 original calls with a nonempty
registered-hook upvalue map: three live object names and one missing world name.
All 6,144 hook calls and 2,048 prunes match, along with every existing tail
callback and state snapshot. Hooks run after eagle-darkness arithmetic and
before object animation/rocket/bird processing; controlled hook mutations to
cursor, bird-fired and darkness establish that placement. C independently
computes the original initial growing string-map order. Missing names drop only
registered bindings. New/reinserted keys during traversal, flying-bird special
paths, grenades, joints and complete runtime ownership remain separate gates.
Repetition is identical, SHA-256
`806ec9cbdb88576d5f9750dafe81af9b534673ad0c9d0abbe5d8de26a7560094`.
Three focused Debug sanitizer/Release comparisons and Windows compilation pass.
Private provenance is `analysis/input/game-update-hooks-native.json`. The
preceding full checkpoint passes 294 sanitizer tests in 51.06 seconds.

The actual GameLua::setSprite method at ELF 0x3224bc, attached-renderer accessors,
RenderableObject/SpriteAsset setters and native composite-cache lookup match
8,192 assignments across 4,096 cases. The script's sprite string is written
before renderer access. Asset name changes and both old bindings clear before
ordinary lookup; ordinary sprites take precedence over composites. Empty names
perform no lookup, and missing names leave empty bindings. Objects without a
renderer retain the new script name while leaving the unattached asset unchanged.
Repeated names still refresh resources. Embedded-zero names preserve their full
length, and composite-cache changes during the ordinary lookup affect fallback.

All 3,272 ordinary lookup snapshots and final names/bindings match. The matching
C paths include 1,636 composite attempts and 818 missing notifications; native
composite lookup executes against prepared typed cache entries. Repetition is
identical, SHA-256
`064f819ea32009d34ea1279b9d5a0ee60bf8048fea6de24f7332cc074cecd0c8`.
Debug sanitizer/Release edge/native checks and Windows compilation pass.
Resolved C state borrows name/resource storage; this does not recover native
allocation/reference-count policy, argument coercion, resource-group loading or
complete scene ownership. Private provenance is
`analysis/input/object-visual-native.json`.

`AbClassicVisuals` now owns canonical sprite fields and cached atlas bindings for
the loaded level's fixed object membership. Names copy into bounded per-object
buffers, preserving embedded zero bytes and avoiding frame allocations. Oversize
or invalid spans reject before state changes. Empty or missing ordinary assets
suppress drawing while retaining requested visibility; restoring a valid asset
restores drawing. Ground retains its script name without an attached renderer.
The shared APK preview uses this owner and synchronizes cached bindings before
ordinary scene submission.

All 22 captured ordinary object names, creation/layer order, colliders and
constructor scales match the existing native scene trace, SHA-256
`0941cae540191c7cda5413f90b4e2fc8dc55143c45e5c583d6d70ce7cddcdef9`.
Captured current sprite selections serve as setter inputs; they are not claimed
to be initialization-time selections. Debug sanitizer and Release scene checks
pass, including caller-buffer reuse, initial name copies, exact allocation
limits, visibility restoration and empty/missing assets. Windows compilation
passes. Composite bindings remain canonical but ordinary scene synchronization
explicitly rejects their submission; special renderers, dynamic membership,
native allocation policy and complete gameplay ownership remain open.

Original GameLua::setPosition (ELF 0x31b888) and setRotation (ELF 0x31ba1c)
match 4,096 controlled calls, split evenly between methods. The native fixture
uses original world/body and LuaState/Table construction, an existing script
object, independent cached poses and an optional attached body. All 1,024
missing-body cases leave state unchanged without using nonfinite request values.
Position assignment changes only script/cached positions; rotation normalizes
its request and changes only script/cached angles. Physics transform/sweep,
nonzero local-center arithmetic, body type and awake state match every recorded
word. This corpus uses no fixtures; fixture/contact transform behavior remains
covered by the separate world-operation comparisons.

Repetition is identical, SHA-256
`e981944c67db00d5c4d8be438530cf7f0730de30a7abe4c805b4dd4c0f751590`.
Debug sanitizer/Release edge and native checks pass; Windows compilation passes.
The owned level now routes component setters and ordered numeric jump events
through these methods. A composition check advances jump, rebound and final
restoration through actual owned bodies and both render-history samples using
the shared C random generator. Name/argument coercion, locked-world assertions
and complete frame/controller scheduling remain open. Private provenance is
`analysis/input/object-transform-native.json`. The preceding full visual-state
checkpoint passes all 298 sanitizer tests in 52.16 seconds.

`AbClassicCharacters` binds the independently compared character controller to
the owned level, canonical visual resources and shared random generator. Bird
yell/blink identifiers come from raw APK definitions; mutable base/blink/smile
names start from the verified damage-name snapshot. Goal expression limits read
raw definition strength and damage1.min. Numeric jumps call the level's ordered
body/history transform adapter. Loss, camera distance, sound choice/playback and
hats remain explicit hosts; missing required operations fail when reached.

The loaded level-one C composition check exercises all three birds and its goal:
blink/yell/base/smile/special selections, exact shared RNG consumption, one
ordered goal audio request and an actual jump/hat boundary. It also checks
uninitialized-timer rejection, exact memory limits and failure at a missing loss
host. Debug sanitizer and Release checks pass, as does Windows compilation.
These are C integration checks over separately native-compared components,
without a new complete native frame comparison. Complete object-property
synchronization, current-bird/flag scheduling, actual audio/distance providers,
dynamic membership and complete gameplay remain open. The full checkpoint now
passes all 300 sanitizer tests in 52.20 seconds.

A bounded live observer now records 69 original GameLua.update object visits
across controlled elapsed values 0, 0.05 and 0.1. All 15 property calls and 54
skips match the C argument plan. The original unchanged updateObjectProperties
callback (prototype 161393) writes all seven selected fields exactly, and skipped
objects preserve them. Position comes from the rendered sample, velocity from
the body and angle from signed atan2 of its rotation. The caller passes sleeping
as inverse current awake. Publication runs when either current or previous awake
is true. The native object loop records its final current awake flag at the end;
circle and box creation explicitly initialize that byte to zero.

The plan also implements the inspected positive-mass boundary test: strict
Y > 20 or X outside float-converted signed integer bounds. The idle live corpus
contains no positive boundary result or sleeping-transition publication. C edge
checks cover those branches, adjacent thresholds, signed zero, unused nonfinite
fields and rounded extreme bounds; these do not expand the native coverage.
The level owner now publishes script pose/velocity/flags without changing body,
rendered pose or either history sample. It owns the old-awake flag, keeps it
through publication and updates it through a separate end-of-object operation.
This preserves the place for intervening native object work.

Normalized live corpus SHA-256 is
`5df5963c020cbb95a40ea9f271a5e921cef1d793d63dac301f369ee14c752928`;
private provenance is `analysis/input/object-properties-native.json`. The
observer read 174,939 selected bytes, detached and resumed the muted reference.
Debug sanitizer/Release edge, native and owner checks pass; Windows compilation
passes. The full checkpoint passes all 302 sanitizer tests in 52.43 seconds,
with subsequent focused checks for owned awake transitions. Remaining native
post-physics work, bounds selection, dynamic lifetime and full frame/controller
scheduling remain open.

The C post-property activity plan now matches 64 controlled original
GameLua.update prefixes, including 48 processed objects and 24 real rolling
accumulator stores. The observer supplies zero elapsed time, isolates one
existing object with current/previous-awake flags and varies velocity, angular
speed and bird/non-bird collider. It restores all temporary body/object words,
seven existing script fields and three globals before the original Lua root
update. Every controlled record repeats identically. Three additional live
updates match the aggregate flags over all 69 object visits.

Movement uses squared linear speed > 9 or signed angular speed > binary32 0.1
without joints, plus the resolved bird-timer and Boolean-bubble branches.
The above-tolerance test promotes the rounded float squared speed to double
before comparing with 0.0005, and excludes birds and bodies with joints.
Processed non-bird circles request rolling accumulation, preserving conditional
negation of negative zero, multiply/divide/multiply order, upper-only volume
clamping and the previous maximum. Controlled cases retain the original mass
and coefficient and use the empty sound name with an initially zero accumulator.
No named playback or multi-object rolling-key sharing is claimed.

Debug sanitizer and Release edge/native comparisons pass, as does Windows
compilation. C edge checks cover joint, bubble, non-nil timer, Boolean-ignore,
negative-zero and overflow branches outside the native corpus. A Boolean ignore
entry suppresses movement even when false; these type branches follow inspected
original instructions. Normalized corpus SHA-256 is
`75bdf15893a86c9e3586753831f6613ec55ac2c5d326f13369c0c69c87fdf95d`.
Private provenance is `analysis/input/object-activity-native.json`. All 304
sanitizer tests pass in 55.34 seconds. Named audio,
shared rolling-map ownership, portal updates, dynamic lifetime and complete
post-physics/frame scheduling remain open.

`AbClassicPostPhysics` now composes property publication and activity over the
fixed ordinary object map in native lexicographic name order. It owns typed
specialty fields, aggregate activity and a reusable per-name rolling map, then
advances each object's old-awake flag at the recovered late phase. Native
rolling names follow ObjectCreation's material-only setter argument; the script
definition override is a separate property. The live capture includes named
box bindings, while its rolling-eligible circles have empty names.

The combined owner matches all seven script fields, aggregate flags and final
old-awake flags over the same 69 native visits. Tests supply captured post-step
body and rendered inputs and preceding script values; this is a post-physics
comparison, without replaying physics or complete original frames. Body and
render-history bytes remain unchanged. The normalized corpus SHA-256 is
`cba9fe4987da29d9dcaad6efe4e46da3d5d0333d0dc5dfcb905b395e83db41cf`.
Private provenance is `analysis/input/classic-post-native.json`.

Separate C checks cover shared-name rolling maxima in both increasing and
decreasing order, lexical sound dispatch, query/play/gain/stop requests, map
clearing, sleeping transitions, non-nil false timers, Boolean-ignore and bubble
types, exact storage limits and host failures. Audio remains an explicit
resource boundary: play requests loop on track two with unit speed and the
three remaining zero arguments. Actual named resource playback has no new
native comparison. Portals, joints, changed membership, custom property
callbacks, full outer-frame/session ownership and playable gameplay remain open.
Debug sanitizer and Release owner/native checks pass; Windows compilation
passes. The full checkpoint passes all 306 sanitizer tests in 56.87 seconds.

`AbClassicSimulation` now owns force preparation, its retained force queue and
post-physics controller under one allocation bound. It runs fixed steps and
history through the previously compared frame driver, clears queued requests
after all enabled steps, interpolates, then publishes object properties and
activity. Water, script-step and after-step removal/deferred-velocity effects
remain required host boundaries. Disabled physics preserves queue and post
state while advancing unscaled time; enabled zero-step frames clear the queue.

The APK-loaded twelve-frame fixture now uses this owner and still matches all
26 physics steps, 23 body states, ordered contacts, fixture bounds, both history
samples and rendered poses. Its water/removal/deferred-velocity host has no
effects; this does not implement those controllers or establish full root-frame
parity. The captured sling vectors, partial initial transfer time and physics
enable remain explicit fixture inputs. C checks add phase order, repeated force
application before one final clear, disabled/zero-step behavior, script-state
publication, exact combined storage bounds, host failure and reentry rejection.
Debug sanitizer and Release checks and Windows compilation pass. All 306
sanitizer tests pass in 57.09 seconds. Root-controller/session scheduling,
particles, joints, dynamic lifetime and playable gameplay remain open.

## Ordinary game HUD draw plan

The original `scripts_common/menus/GameHud.lua` draw method, prototype 44365,
matches the C plan across 4,096 calls and 34,816 ordered boundaries: 13,824 font
selections, 10,752 render states, 3,072 world draws, 3,072 text draws and 4,096
child-frame calls. The original Class and full GameHud chunk execute in the
pinned ARMv7 runtime with controlled metrics, localization, scaling and render
callbacks. All 128 combinations of retina, highscore presence, hidden/shown
scores, prior width, update mode and tutorial state are covered. Positions,
scale operation order, truncated numeric scores, four-argument state arity and
retained width match every recorded word. Repeated native execution is identical;
Debug sanitizer and Release comparisons pass, and Windows compilation passes.

Corpus SHA-256 is
`f394a46ccf01ec16fc772c9048fb14c06e00282acc6e607d5dfbcf408f57d331`;
private provenance is `analysis/ui/game-hud-draw-native.json`. Actual font and
GPU execution, complete child widgets, ads, eagle mode, powerup scores and
floating scores remain open. This establishes a scoped draw plan, not full HUD
or gameplay parity.

The fixed level-one preview now binds the APK's 480x320 FONT_BASIC, English
TEXTS_BASIC labels and HUD_INGAME_1 button atlas. A read-only live Lua snapshot
confirms scale one, zero margins, font name, score state, visible child order
and every selected button position/size word. The three positions are derived
from actual pause-sprite pivots and the recovered GameHud.layout arithmetic;
they are not captured coordinates embedded as implementation constants.
The C score plan composes with the independently verified font and ImageButton
draw adapters, cached resources and shared draw list. The private highscore is
an explicit tool input; the normal preview does not embed saved progression.

First-party Android GLES2 execution of that cached C scene matches all 153,600
RGBA pixels in the original 480x320 framebuffer, without exclusions. Eleven
textures and 63 ordered commands produce the retained comparison; repeated
execution is identical. Input SHA-256 is
`f9d05d1f325a524560640be797bbcd1cbb0425a6cdb686c6f36dcef2eeb2ccc7`;
complete output SHA-256 is
`db4eb82467e33995fc153911522f637453719058fbfd65d93c437500f47431c1`.
The original raw capture and PNG are independently checked byte for byte.
Private records are `analysis/ui/hud-live-state.json`,
`analysis/ui/hud-buttons-live-state.json` and
`analysis/ui/idle-preview-native.json`.

This comparison fixes the camera and idle pose, explicitly skipping transfer
time. Button input, changing scenes, floating scores, other HUD modes, temporal
gameplay and presentation remain open. Metal renders the same cache with
137,595 of 153,600 RGB pixels exact, maximum channel error 166 and 501 pixels
with error above ten; sampling/rasterization differences remain unresolved.

## Initial GLES2 boundary

The authored GLES2 backend accepts native entry points and an already-current
context. It handles basic RGBA8 sprite commands with explicit blend factors,
component modulation, and a separate opacity factor. Programs, vertex buffers,
and texture slots have owned lifetimes; frame submission does not allocate or
decode. Input is validated before a frame is submitted, including all texture
references. Shader variants compile/link with installed glslangValidator for
GLSL ES 1.00, with both mediump UV fallback and optional fragment high precision.
No GL loader library or external renderer source is bundled.

Target `2d-sprite-alpha.fx` selects premultiplied blending. `pp.fx` specifies
ONE / ONE_MINUS_SRC_ALPHA for that path, SRC_ALPHA / ONE_MINUS_SRC_ALPHA for
straight blending, ONE / ONE for additive blending, and DST_COLOR / ZERO for
multiplication. `pp.ps` applies its opacity factor to every channel. These
observed equations and states inform the independently authored shader and
backend; original shader source is not copied into the implementation.
Sampled game font PNGs have RGB at or below alpha in every semi-transparent
pixel, consistent with premultiplied content. Decoders preserve stored channel
bytes; they never multiply them again or unpremultiply them.

Native `GL_Texture::allocate` at analysis address `0x5deecc` sets linear
magnification and normally linear minification without mip levels. Repeat
wrapping remains the GL default for power-of-two textures; non-power-of-two
textures without `GL_OES_texture_npot` use clamp-to-edge. The initial backend
accepts explicit wrapping and rejects NPOT repeat under its core-only contract.
Private upload evidence is `Files/AngryBirdsC/analysis/texture-upload-native.json`.
The [GLES2 specification](https://registry.khronos.org/OpenGL/specs/es/2.0/es_full_spec_2.0.pdf)
is the API reference, not an implementation dependency.

The C command recorder checks failed shader/program/buffer construction,
resource release, failed upload recovery, raw upload pointers, NPOT rules,
prevalidation before drawing, blend factors, retained order, and driver-error
propagation under sanitizers. It does not simulate rasterization. This backend
has not run in a native GLES2 context. Native window/context integration,
clipping, effects, original packed GPU formats (especially RGB565), Metal, and
native image comparisons remain open. RGBA8 decoder equivalence does not prove
packed-format GPU sampling equivalence.

## Initial Metal boundary

The Apple-only `ab_metal` target consumes the same basic draw commands through
a C interface to the Objective-C runtime and native Foundation/Metal APIs.
There is no external renderer library. The backend owns its device reference,
pipelines, samplers, depth state, and textures; platform command submission and
presentation remain caller responsibilities. Render-time parameter bytes are
copied through native encoder methods; no project heap allocations occur in
the render loop. The authored MSL shader uses the same position, UV, modulation,
and opacity contract, with fast math disabled at compilation.

The installed Apple compiler compiles and links the shader. The native GPU test
executes on the local Apple M2 Max and reads a two-by-two RGBA8 render target.
Exact RGBA8 and BGRA8 checks pass for source-pixel orientation and values, opaque,
premultiplied, straight, additive, and multiplicative blending, a half-opacity
case, component tint, clamp/repeat wrapping, and overlapping-command order.
The default-device function returns nil
in this test process; enumeration finds the available GPU. An initial null
depth-state call failed in the driver; the backend now creates and binds an
explicit always-pass state with depth writes disabled. The rerun passes under
address/undefined-behavior diagnostics.

These are synthetic native GPU checks, not Android target comparisons. The
current boundary supports single-sample RGBA8/BGRA8 attachments, RGBA8 uploads
up to 8192 pixels per dimension, and the shared core GLES2 NPOT clamp policy.
Packed GPU formats, clipping/effects, attachment variants beyond that contract,
window presentation, performance measurements, and target frame comparisons
remain open. Apple API signatures and ABI layouts were verified
against the installed SDK headers; no implementation source was imported.

## Isolated ARM geometry comparisons

The first-party freestanding probe runs audited target functions under installed
QEMU 10.2.0 (`virt`, Cortex-A15, 128 MiB), with the FPSCR control bits initialized
to zero: nearest rounding and gradual underflow. It loads the pinned ARMv7 ELF
segments, resolves exported symbols, and applies the target's internal
`R_ARM_RELATIVE` relocations. Undefined function imports abort except for guarded
zero-angle sine/cosine bindings in the initialization experiment below;
unresolved data imports use an unmapped sentinel. Exception vectors terminate
on invalid memory
access; a deliberate unmapped-read probe verifies this guard. No platform
services or original constructors are supplied. CMake verifies
the full library SHA-256 before execution. No original implementation is linked
into the first-party runtime or committed to the project.

The `ab_physics` geometry uses independently authored binary32 formulas with
the observed arithmetic order. A reproducible corpus compares the host C output
to the original ARM machine code, including each result's exact float bits.
All 4,096 circle and 4,096 polygon cases pass for mass, centroid, inertia, AABB,
and point containment. The polygon cases include 2,048 native axis-aligned box
constructions checked against first-party vertices and normals, plus supplied
triangles, hexagons, and octagons. Circle cases include zero, negative zero,
subnormal, and minimum-normal radii. Skin expansion affects polygon bounds;
point containment and mass use its edges.

| Native operation | Analysis virtual address |
| --- | --- |
| Circle point test | `0x8efc08` |
| Circle AABB | `0x8efc6c` |
| Circle mass | `0x8efcc4` |
| Polygon point test | `0x8f0360` |
| Polygon AABB | `0x8f05a4` |
| Polygon mass | `0x8f0684` |
| Axis-aligned box setup | `0x8f092c` |
| Circle-circle contacts | `0x8ec4f8` |
| Polygon-circle contacts | `0x8ec5d8` |
| Polygon-polygon contacts | `0x8ef0e8` |
| Segment clipping | `0x8ec344` |
| World manifold initialization | `0x8ebb60` |
| Polygon hull construction | `0x8f0ab8` |
| Circle ray query | `0x8efdd4` |
| Polygon ray query | `0x8f0424` |
| Body mass override | `0x8dece4` |
| Fixture mass reset | `0x8de8ac` |
| Contact warm start | `0x8f5804` |
| Contact velocity solve | `0x8f5928` |
| Velocity constraint initialization | `0x8f53b4` |
| Position constraint solve | `0x8f5fa0` |

The corpus also compares 4,096 circle-circle, 4,096 polygon-circle, and 4,096
polygon-polygon contact cases. Every word of the 64-byte manifold matches,
including untouched impulse values and inactive fields. Polygon-polygon results
include no contact, one point, and two points, with both reference-face
orientations. The reference-face choice retains the target's binary32 bias
bits `0x3ba3d70b`. Clipping retains endpoint order and the original packed
feature identifiers, swapping their fields when the reference face reverses.

World-space normals, points, and separations match all 12,288 contact cases,
with unused fields preserved. Circle fixtures specifically include coincident
centers and separations below, at, and above the normalization threshold.
This comparison exposed and corrected a signed-zero rotation discrepancy:
native VNMLS adds a rounded product to a negated accumulator. Negating a
completed subtraction gave a different zero sign. The first-party geometry
and contact corpus passes after the correction; decompiler expressions alone
would not have established this behavior. Arm's formal
[VNMLA/VNMLS instruction description](https://support.arm.com/documentation/dui0646/c/The-Cortex-M7-Instruction-Set/Floating-point-instructions/VNMLA--VNMLS-and-VNMUL)
also distinguishes the product and accumulator signs.

All 4,096 additional polygon-construction cases match active vertex ordering,
normals, centroid bits, and counts; the probe separately verifies skin-radius
preservation. The target takes at most the first eight inputs, retains the first
point within its strict squared-distance threshold `0x3ccccccd` (about 0.025),
and selects the rightmost, then lowest, start vertex. Collinear hull candidates
prefer the farther point. Fewer than three input or hull vertices falls back to
the native unit-half-extent box. The first-party builder adds bounded handling
for invalid/nonfinite inputs and supports input/output aliasing. Private native
evidence is `Files/AngryBirdsC/analysis/polygon-build-native.json`.

Another 4,096 circle and 4,096 polygon ray-query cases match hit flags,
normal/fraction bits, and output preservation on misses. The circle query
rejects squared ray lengths below `FLT_EPSILON`; its first root retains
signed-zero fractions. Polygon queries clip against the original edges,
without skin expansion, and require an entering edge. The corpus covers
interior starts, parallel/zero-length rays, tangent/boundary cases, shortened
fraction intervals, and the circle's short-ray threshold. Traversal order and
game callbacks are separate. Private native evidence is
`Files/AngryBirdsC/analysis/raycast-native.json`.

The initial body state implements explicit mass overrides. Another 4,096
comparisons match mass and inverse mass, central inertia and its inverse,
local/current/previous centers, and adjusted linear velocity. Controls, transform,
and angular velocity remain unchanged. A locked world or nondynamic body ignores
the override. Nonpositive mass becomes one. Fixed rotation suppresses inertia;
otherwise the target shifts positive origin inertia to the new center without
clamping the result. The corpus includes zero central inertia and its infinite
reciprocal. Native execution uses controlled numeric body fields and a stand-in
world lock flag, without either class's constructor. Integration and the full
world are still separate recovery work. Evidence is
`Files/AngryBirdsC/analysis/body-mass-native.json`.

Fixture mass aggregation matches another 4,096 native cases. Body type and
fixture count vary independently, including dynamic bodies with zero through
four fixtures. Zero densities skip shape access; their mass fields contain NaN
sentinels. Controlled fixture vtables dispatch to the original circle/polygon
mass functions. Static and kinematic bodies ignore fixtures, move both sweep
centers to the transform position, and copy the current angle to its previous
value. Dynamic bodies retain fixture order, center-shift inertia and velocity,
and preserve angles. These are controlled data layouts, without original
constructors or a stepped world. Evidence is
`Files/AngryBirdsC/analysis/body-reset-native.json`.

Saved contact impulse application matches all velocity fields in another
4,096 controlled native cases. The corpus varies zero through four contacts,
zero through two points, shared bodies, aliased endpoint indices, inverse
masses/inertias, and signed zeros. Contact and point ordering is preserved;
both endpoint velocities are loaded before their updates, matching native
behavior when indices alias. Public count/index errors fail before mutation.
Constraint construction and later solving remain separate recovery tasks.
Evidence is `Files/AngryBirdsC/analysis/warm-start-native.json`.

The friction and normal velocity solve matches 4,096 additional cases with one
through ten iterations: all 12 velocity fields and four saved impulses match
the original instruction results. Native constraint fields other than impulses
are checked for preservation. The target's `g_blockSolve` initial byte is one;
the probe asserts that value before execution. The corpus contains 2,049
single-point cases and 2,047 coupled two-point cases. Final two-point patterns
include 663 with both impulses active, 335 with only the first, 333 with only
the second, and 716 with neither. Signed-zero cases also pass. These constraints
and positive-definite matrices are supplied directly; constraint construction,
position solving, integration, and gameplay remain separate. Evidence is
`Files/AngryBirdsC/analysis/velocity-solver-native.json`.

Velocity constraint initialization matches all 39 scalar fields in 4,096
controlled cases, including preserved impulses and inactive fields. Cases vary
manifold kind, one/two points, radii, mass centers, velocities, inverse masses,
restitution, and coincident points. The coupled matrix condition uses the
target's strict `K11 * K11 < 1000 * determinant` rule; failure reduces the
active point count while preserving previous matrix fields. Restitution bias
requires normal velocity strictly below minus one. The first-party API receives
already computed transforms. This native experiment supplies only positive or
negative zero angles: guarded sine/cosine import bindings return the exact
zero-angle mathematical values and abort for every other input or context.
These bindings do not validate Android's trigonometric implementation. All
other imports still abort. General angles and complete world integration remain
open. Evidence is `Files/AngryBirdsC/analysis/velocity-initialize-native.json`.

The subsequent nonzero-angle experiment uses 4,096 angle/result triples measured
by an authored ARM C tool against Android 5.1.1's actual `libm`. The captured
table is hash-checked before loading; unknown angles and imports outside the
audited initialization context still abort. All 39 fields match another 4,096
initializations. Fourteen shared corpus sections also pass inside Android with
real platform imports and native library constructors, producing a
25,669,936-byte prefix identical to the isolated trace.
See [reference setup](android-reference.md). Supplied
transform support is validated; the full stepped world remains open.

Single contact-point position correction matches another 4,096 cases for both
centers, angles, and the native completion flag. The corpus covers all three
manifold kinds, mass/inertia variants, nonzero rotations, correction and
completion boundaries, coincident circles, and circle-normal lengths around
`FLT_EPSILON`. The target's slop is binary32 `0x3d4ccccd` (0.05), correction
factor `0x3e4ccccd` (0.2), correction lower bound `0xbe4ccccd` (-0.2), and
completion threshold `0xbe19999a` (-0.15). Circle position contacts use the
center midpoint and preserve the unnormalized difference below the normalization
threshold. The API consumes current transforms and updates centers/angles;
callers must recompute transforms before another point. TOI solving and world
integration remain open. Evidence is
`Files/AngryBirdsC/analysis/position-point-native.json`.

The first-party rotation module matches the Android C library's sine/cosine
bits for 4,096 baseline and 65,536 extended samples, covering signed zeros,
subnormals, polynomial/quadrant/reduction boundaries, and finite angles through
the module's documented limit `0x1.921fb4p+28` radians. It evaluates authored
binary64 polynomials and range reduction, without host trigonometric calls.
Larger-angle reduction and floating-point status flags remain outside this
implementation. Native host float and double alternatives each differ on
sampled values. Private numeric evidence and platform-library provenance are in
`Files/AngryBirdsC/analysis/rotation-native.json`.

Complete ordered position passes use that rotation module to recompute both
transforms before each contact point. All body centers, angles, and completion
flags match native Android after every one of 22,510 iterations across 4,096
setups. Cases include zero through four contacts, zero/one/two points, consistent
per-body masses/centers, shared bodies, and aliased endpoint indices. Native
constraints are checked for preservation. The extended Android trace is
30,441,357 bytes and retains the identical shared prefix above. These are
controlled solver buffers, without a constructed world, broadphase, integration,
TOI, joints, or gameplay callbacks. Evidence is
`Files/AngryBirdsC/analysis/position-passes-native.json`.

The optional CTest `physics_native_arm_geometry` records its inputs and original
outputs in `arm-oracle/geometry.txt` inside the ignored build directory. Native
circle assembly evidence is retained in
`Files/AngryBirdsC/analysis/circle-geometry-native.json`; contact evidence is in
`contact-geometry-native.json`, `polygon-contact-native.json`, and
`world-manifold-native.json` in the same private analysis directory.
These checks cover the listed finite geometry cases and explicit floating-point
controls. Broadphase, integration, solver state, Android initialization, and
full gameplay traces remain open. The probe is not an Android emulator image
and does not establish full-game parity.

The probe uses the formal [Arm semihosting interface](https://github.com/ARM-software/abi-aa/blob/main/semihosting/semihosting.rst)
and QEMU's documented [Arm virtual platform](https://www.qemu.org/docs/master/system/arm/virt.html).
Relocations follow the [Arm ELF ABI](https://github.com/ARM-software/abi-aa/blob/main/aaelf32/aaelf32.rst).
Its boot assembly, ELF access, corpus generation, and comparison code are authored
here; no third-party implementation source or runtime dependency is added.

## Captured save data

The reference app produced `highscores.lua` (240 encrypted bytes) and
`settings.lua` (2,432 encrypted bytes) after the first-level capture. Their private
copies remain under `android-reference/saves-after-level-1/`. `GameLua::saveLuaTable`
at analysis address `0x34213c` uses the key member initialized from
`getVariableKey()` (`0x2d6018`), rather than the resource decryption key.
The private key is not embedded in project source or exported metadata.

Both files decrypt with AES-256-CBC, a zero IV, and the target's padding policy.
First-party encryption reproduces all 2,672 original ciphertext bytes exactly.
The high-score table contains `Level1.completed = true`, score/legitimate/low
score values of 31,480, one bird used, and a zero power-up score. These agree
with the recorded completion; wider progression and star rules remain open.

The first-party literal reader recovers 34 settings globals and two high-score
globals. A separately authored Android probe loads the plaintext into the original
library's Lua engine and enumerates typed values. All 177 settings nodes and
23 high-score nodes match, including number bits and string bytes. Another
139-node authored constructor case validates overlapping numeric/list fields,
the 50-field list flush boundary, byte escapes, subnormals, decimal rounding,
and the native compiler's reuse of the first literal zero's sign. This is
reference execution only; no Lua engine is linked into first-party builds.
Private provenance and hashes are in `analysis/save-format-native.json`.

The parser accepts serialized literals only and rejects calls, expressions,
comments, unsupported keys, nonfinite values, excessive nesting, and allocation
budget overruns. It owns a text copy and all resulting tables. Decimal parsing
currently requires the C numeric locale and round-to-nearest mode. Native
`LuaTable::writeValue` uses `%g` for finite numbers, so original save writing can
lose precision. Serialization order, editing, atomic storage, migration, and
full runtime save semantics remain open.

## Native update observations

The reusable game-frame phase now owns clock planning, fixed physics steps,
post-step callbacks, history gating and awake-body interpolation. The eight
existing native level frame/flight/impact/reload/idle/retry/results comparisons
use this runtime phase instead of duplicating its loop in the checkers. They
retain their exact snapshots and restrictions, including the 96-frame result
projection. Controllers, event services and presentation are separate remaining
work; this phase alone is not a complete GameLua update replacement.

The application caller (GameApp update, ELF `0x337674`) compares incoming seconds
with binary32 0.1 and selects that cap for greater/equal or unordered values.
LLVM confirms that the resulting scalar is passed unchanged to GameLua update
at `0x337b64`. The separate application-delta helper therefore caps positive
infinity and NaNs, while retaining negative values and signed zero. The GameLua
clock itself remains unclamped. This caller rule is assembly-backed; the existing
controlled clock corpus exercises the downstream GameLua arithmetic separately.

The authored 48-bit generator matches 65,536 Android draws from 4,096 explicit
32-bit seeds, including all incoming/final state words. Its transition and
31-bit extraction are verified against the reference platform's original
`srand48`/`lrand48` functions. The original game's Lua random function (ELF
`0x8206a4`) matches another 65,536 calls from explicit 48-bit states: 39,592
values, 9,560 empty-interval errors and 16,384 argument-count errors. Every
case consumes exactly one draw, including errors. The output arithmetic uses
binary32 multiplication before binary64 floor/add and a final float rounding;
integer interval width wraps at 32 bits. Controlled boundary cases include
32 unit results equal to one and 36 values above a valid interval's upper bound.
The first-party API preserves these outcomes and keeps generator state explicit.
The original Lua implementation runs only inside the private reference oracle.
Native callers share this platform generator across Lua, force adders, GUIDs
and an AES path. Seed policy and complete consumption order remain open; this
does not retroactively remove the inferred RNG inputs from earlier frame traces.
Private provenance is `analysis/random-native.json`.

A live entry/return capture also compares 16 returned random values and 15
adjacent generator-state pairs in the running APK. One window contains 21
generator advances, with 20 additional consumers unobserved. Their purpose and
interleaving are not assigned to the Lua call. The comparison retains that count
as explicit input, leaving complete shared-consumption recovery open.

Broadphase move/pair buffering now matches 4,096 consecutive native states and
23,880 ordered notifications across 693 pair-update calls. The corpus includes
704 creations, 643 removals, 660 moves, 739 explicit touches, 656 origin shifts,
and an initial empty state. Move capacity grows from 16 to 64 and pair capacity
from 16 to 4,096. Repeated moves remain repeated; removal replaces every matching
move with `-1` without reducing the count. Canonical pairs retain duplicates in
the sorted buffer while callbacks run once per unique pair. The initial dense
case yields 4,032 buffered pairs and 2,016 native notifications for 64 proxies.

Notifications are observed through the original contact manager's filter call,
using distinct controlled dynamic bodies with empty joint/contact lists. The
authored filter records the fixture identifiers and rejects each pair before
native contact construction. This confirms notification order and suppression
directly; it does not establish the game's filtering rules or persistent contacts.
All recorded tree summaries, move buffers, sorted pair buffers, capacities,
counts, and notification sequences match Debug sanitizer and release builds.
First-party limit tests also check that failed collection emits no callbacks,
retains moves, and can be retried after proxy removal.

The broadphase trace is 3,860,217 bytes, SHA-256
`a0f1a07cc7c19d50d74b2975084f4c2d8761eecf5e3eefe262de14cef130559d`.
Private provenance and native function records are in
`analysis/broadphase-native.json` and `analysis/broadphase-functions-native.json`.

The dynamic AABB tree now matches 4,096 consecutive operations against native
Android execution. The corpus includes 997 creations, 933 removals, 1,133 moves
(275 containment skips), 1,032 origin shifts, and an initial empty query. Pool
capacity grows from 16 to 128. Every snapshot compares root, node count,
capacity, free-list head/links, insertion count, all active-node bounds,
parent/child links, heights, and logical payload identity. Across the trace,
488,179 active-node records and 33,309 free-node records match. Uninitialized
free-node bounds/payload/children are excluded; pointers are normalized to
controlled payload identifiers.

Every operation also has a native overlap query. All 78,915 reported payloads
match in order, including early callback termination and the target's traversal
of the second child first. Proxy IDs, sibling cost choices, balancing ties,
fat bounds, displacement expansion, and reuse behavior match in this corpus.
Debug sanitizer and release checks pass. Capacity-limit failures and malformed
bounds are checked separately by first-party tests. The trace is 43,724,958
bytes, SHA-256
`04308184ecfd80baccbaabf755025f91d3284582dcef296b5ff0e54127c32d32`;
private provenance is in `analysis/tree-native.json`, with native function
records in `analysis/tree-functions-native.json`. Ray traversal, bottom-up
rebuild, and persistent world contact management remain open.

The contact-free path of `b2Island::Solve` (`0x8f1ee8`) now has a first-party
implementation. A native Android corpus covers 4,096 islands with zero through
four bodies, all three body types, five timesteps including zero, gravity/force
and torque integration, damping, translation/rotation clamps, local centers,
sleep controls, and zero/nonzero position-iteration counts. All 64 words per
body match across 8,190 bodies, including 1,304 put to sleep. Unmodeled fields
are checked for preservation. Sanitized and release comparisons are retained
privately in `analysis/free-island-native.json`.

The target's default translation limit after library construction is 2, with
squared limit 4; both controls remain explicit parameters because the game may
change them. The rotation limit uses `0x3fc90fdb`, its squared comparison uses
`0x401de9e7`, sleep velocity comparisons use squared constants `0x3c23d70b`
(linear) and `0x3a9fb511` (angular), and the minimum sleep time is 0.25 seconds.
The island clears velocity, force, torque, and sleep time when sleeping, including
static members. It retains forces otherwise. Body selection is supplied by the
caller; the isolated island function does not itself filter awake flags.
Contact integration is validated separately below. World selection, reports,
broadphase, and continuous collision are covered by later experiments. Joints
and gameplay integration remain open.

An additional 4,096 native discrete-island cases combine zero through four
supplied contacts with two through four bodies. They cover all manifold kinds,
one/two points, shared and aliased endpoints, warm-start on/off, time ratios
0/0.5/1/2, one through ten velocity iterations, and zero/ten position iterations.
All 64 body words match across 12,287 bodies, and all 16 manifold words match
across 8,190 contacts, including unchanged geometry/IDs, stored impulses, and
810 sleeping bodies. Debug sanitizer and release comparisons both pass.
The trace is 17,471,753 bytes with SHA-256
`97bc423c717f6de1a61a6be30182a9963bc019ca2361a75d60c0dd05edd39ff5`.
Private provenance is in `analysis/contact-island-native.json`.

The first-party island API uses caller-owned work arrays. It combines the
previously verified solver routines in native order, retains the second saved
manifold impulse when a two-point velocity constraint is reduced to one, and
gates sleep on position convergence. Malformed indices, workspace capacities,
manifold counts, and controls reject before mutation. Numeric/angle contracts
remain explicit; unsupported intermediate math may leave partial updates.
These tests use controlled input manifolds and do not establish game-world
contact selection, persistent world traversal, joint behavior, or gameplay parity.

The assembled circle/polygon world now matches initial attachment and every
body/contact state across 4,096 consecutive discrete native steps, together
with 6,365 ordered collision events. Separate continuous experiments match
4,096 fast-body steps and 10,068 events, then another 4,096 steps and 11,063
events with callbacks rejecting cached contacts. They validate world-list
selection, aligned sweep endpoints/alpha, active cached fractions, impact
counts, bounded 64-body/32-contact islands, rollback, and fixture rediscovery.
Inactive native cache bytes are normalized because they contain reused memory.
Public cache options and reference commands are in
[the Android reference guide](android-reference.md); private provenance is
`analysis/world-continuous-native.json`.

World state-change comparisons add 4,096 operation/step pairs and 7,342
collision events. They cover repositioning, activation, body type, fixed
rotation, fixture filtering and sensors, with all immediate and following
body/contact states and active tight/fat proxy bounds. Body/fixture lifecycle now
matches another
4,096 operation/step pairs, including 512 body creations/removals, 768 fixture
creations, 192 explicit fixture removals, 10,460 collision callbacks and 576
implicit fixture notifications. Compound mass changes, stable slot reuse and
native list/proxy ordering are checked. Joint behavior remains open.
These controlled engine tests do not establish
level setup, launch, damage, scoring, or gameplay parity.

`GameLua::update` is at analysis address `0x333ec0`. A physics-enabled path
uses a binary32 accumulator threshold of `0x3d088889` (approximately 1/30),
calls `applyForcesAtPhysicsStep`, updates per-object forces and water, then
calls `updatePhysicsStep`. Assembly at `0x334944`–`0x334960` supplies
`0x3d088889` as the Lua update argument. Assembly at
`0x33496c`–`0x334984` calls `b2World::Step` with the same binary32 timestep and
10 velocity / 10 position iterations. The accumulator is decremented after
that call. These values come from instructions, not an upstream physics
version. Timer scaling, pause behavior, interpolation, and the complete order
of callbacks are still under investigation; the decompiler's floating-point
register representation requires care.

`ab_classic_level_load` verifies the pinned APK, reads its resource-key fields
as data, decodes the catalog and level with restricted static constructors, and
loads an explicit ordered SPRT set. It creates the stable, owned C world through
`ab_classic_level_open`; normal stepping uses the retained solver/history arrays.
All existing native body/frame/impact/result checks pass through this owner.
Separate direct-APK checks match the initial level-1 bodies and 12 startup frames
without extracted resources, a private-key file or captured sprite sizes.
Ground derives from MuseumMode prototype 2711: initial bounds are +100000 and
-100000; each ordinary object contributes X minus/plus its full stored width.
Circle width is the native -1 sentinel, which is used instead of its radius.
Ground X is the rounded bound sum times 0.5 and Y is 5. The direct-APK check
matches the native ground X bits `41cdf454`; level gravity is read from static
data, with nil/false defaulting to 20. Widths retain unhalved constructor values.
Scale/profile, selected sheets and sling timing are still explicit inputs.
Resource-group policy, complete initialization and gameplay
presentation remain open. Debug ASan/UBSan and Release comparisons pass; Windows
cross-compilation with warnings treated as errors passes. The default suite has
107 passing checks. Private provenance is `analysis/classic-level-runtime.json`.

A separate level-1 restart capture pairs eight original `math.random` calls
from native load-level entry through the last queued bird: two calls with
bounds 5/30 and six with bounds 10/30. Every before/after state is one generator
advance, adjacent calls share the same boundary state, and the first state is
the captured load-entry state. At the final `setObjectBodyStatic` entry (ELF
`0x312b5c`), selected Lua fields record all 23 objects after timer assignment.
The owned loader matches every timer presence/value, start number and
width/height/radius, including circle/box -1 sentinels, and the final RNG state.
A separate retained-table read records bird iteration as RedBird_3, RedBird_4,
RedBird_2 with capacity four and start numbers three, one, two. The C loader
reproduces that order by inserting controllable names in runtime-world order;
it keeps this distinct from creation and launch order. The cached order covers
initial membership; deletion/reinsertion remains open. This validates the
selected prefix from explicit RNG state; process seeding, later load hooks,
animation/sprite initialization and other level paths remain open. Debug
ASan/UBSan, Release and Windows compilation checks pass. Private capture and
projection are `analysis/debugger/startup-object-random-calls.json` and
`startup-objects.txt`.

A further capture follows the same eight draws to a later shared native method
dispatcher return. The generator is unchanged between these selected boundaries.
The earlier boundary contains two bird-table entries; the later snapshot
contains all three, with the order above, and one goal-table entry. The owned
loader now seeds reusable character state and both group orders from APK data,
body poses and generated startup timers. It matches 20 selected fields for every
object (460 values), including Ground's explicit script strength of thirty.
Nil/false flags
are compared as the truth/literal gates used by those fields; generic Lua field
presence is not reproduced by this numeric state. This does not establish sprite
binding or complete initialization. Debug sanitizer and Release comparisons pass,
as does Windows compilation. Private capture/projection are
`analysis/debugger/startup-character-ready.json` and `startup-ready.txt`.

The later snapshot was initially mislabeled as a load return. The shared
dispatcher address, even with the same stack/thread, can belong to a subsequent
method invocation. GameLua.loadLevel (ELF `0x332a60`) reads level tables into
loadedObjects before the later script timer phase. This snapshot establishes
selected partial initialization only; it is not a completed-load boundary.

An unambiguous subsequent GameLua.update entry provides a separate later
snapshot. Pairing every math.random call through that entry records exactly one
additional draw with bounds 1/2. Its Lua caller has lines 1825..1832 and matches
gamelogic getAudioName, prototype 56034. All nine draws form a continuous native
generator chain from the explicit level-read entry state to that update entry.
Timers and the leading bird's pose have also advanced by this later snapshot;
it is not interchangeable with the earlier initialization prefix. This does
not close shared consumption elsewhere in the app or the older inferred replay.
Private capture is `analysis/debugger/startup-character-random-through-update.json`.

Script initialization in gamelogic prototype 67409 calls
`math.randomseed(os.time())`. Original zero-argument `os.time` (ELF `0x770ef4`)
reads signed 32-bit time, rounds it to a float Lua number, and returns nil for
the -1 error sentinel. The seed wrapper (ELF `0x820134`) converts that number
with the target's signed ARM conversion before `srand48`. The first-party
`ab_random48_seed_time` matches 65,536 native conversion/state/first-draw cases,
including float rounding near integer boundaries and saturation when a positive
epoch rounds to 2^31. The -1 path preserves RNG state. One original no-argument
clock result also matches the adjacent platform time readings. Debug sanitizer,
Release and Windows compilation checks pass. Time is explicit; complete app
initialization, other consumers and historical seed values remain open. Private
provenance is `analysis/random-native.json`.

The ordinary bird sprite prefix in gamelogic prototype 137493 now runs in
`ab_bird_animation_update`. All 4,096 original-script cases match timer and
active state, 660 random draws, 59 controlled distance queries, 14 audio calls
and 334 sprite changes. Boundary snapshots verify timer writes before calls and
the final active-flag write afterward, including callback mutations and sprite
presence captured before a distance callback changes the definition. Missing
optional sprites still begin the active interval. Shot, drawing, timer and
literal-true gates remain explicit; distance, sound selection/playback and sprite
binding are caller boundaries. This does not establish complete animation,
spatial-audio or presentation parity. Debug sanitizer and Release comparisons
pass, and Windows compilation passes with warnings treated as errors.

The jump adapter also accepts the native float random wrapper's rare upper
bound plus one. A separate 4,096-case original-script corpus supplies one such
controlled boundary per selected case and matches 6,119 random draws, 3,347
position events and 638 rotation events. This fixes an earlier validation rule
that rejected a possible native value. These are controlled boundary tests,
separate from an actual global-generator stream. Private provenance is
`analysis/bird-animation-native.json`.

The same original character-animation prototype visits all goal expressions
before its separate oink pass, then visits birds. First-party expression and
oink functions match 4,096 original-script cases: 1,094 draws, 372 lose-condition
queries, 701 sprite calls, 683 controlled distance queries and 388 sound calls.
Exact boundary snapshots cover special-expression expiry, damage thresholds,
truthy blink flags, smile priority, strict negative timers, zero/subnormal
boundaries, possible random overshoot and callback mutations. A callback that
freezes an object can suppress its later oink pass. The strength comparison
preserves division-by-zero IEEE results. Geometry, actual sound selection and
sprite binding remain controlled boundaries; this is not a complete scene or
audio comparison. Debug ASan/UBSan, Release and Windows compilation checks pass.
Private provenance is `analysis/goal-animation-native.json`.

`ab_character_animations_update` composes the goal expression pass, goal oink
pass and per-bird sprite/jump work in explicit table orders. A combined original
script probe runs two goals and three birds over 32 frames from each of 64 seeds.
All 2,048 frames match final selected state and the shared generator, including
2,225 original random draws, 1,463 sprite calls, 111 sound calls, 1,595 position
calls and 293 rotations. These tests use the actual original math.random wrapper
and platform generator, with no inferred draws. Input frames vary held bird,
completion, draw/frozen/shot gates, smiling and roll policy. Distance and sound
selection remain controlled; hats, full-world integration and history/coordinate
effects are not covered. The jump helper commits its numeric frame before applying
position/rotation boundaries; intermediate callback state and mutations within
that numeric jump calculation remain outside its contract. Debug sanitizer,
Release and Windows compilation checks pass. Private provenance is
`analysis/character-animation-native.json`.

The range-selection portion of getDamageSpriteNames, prototype 151134, matches
4,096 original cases. It preserves first-match iteration order, strict lower and
inclusive upper bounds, numeric-zero initial strength, IEEE zero-divisor results
and non-nil optional fields including false. Range order and definition/catalog
availability remain explicit inputs to that numeric selector. The initial-object
adapter obtains range order from retained constructor layouts. The APK loader
prepares initial names without applying sprite changes or decoding textures.
All 69 native initial damage/blink/smile fields across 23 objects match; each
nonempty name resolves in the explicitly selected block and pig sheets. The
native snapshot is at the later update entry above, and this comparison covers
only those names, not its changing timers/poses or rendered pixels. Debug
sanitizer and Release checks pass; Windows compilation and all 106 default
checks pass. Private provenance is `analysis/damage-sprite-native.json`.

ObjectCreation prototype 883, instructions 232..237, writes a missing material
strength into the definition before initial sprite selection. The immutable C
adapter now supplies that normalized strength as its divisor. An additional
native probe executes the original constructors through their first sprite-bounds
boundary, then selects names from the normalized catalog. All 15 affected
definitions' strength writes and 60 sprite fields match. The catalog integration
sweep at that stage loads/steps 591 of 592 actual levels with real theme resources and
logical texture IDs; 17 loaded levels have missing visuals and one other level
lacks a required sprite-size binding. These are C integration results. Full
native construction and original-frame/GPU gameplay comparisons remain open.
The record is `analysis/input/initial-sprite-material-native.json`.

Original normal level transitions subsequently confirm absent sprites in
LevelS039, EggDefender_2 and UGC1Level06. Resources width/height getters return
positive zero for missing names; active composites use their cached bounds.
All 12 captured dimension words agree with the C resource adapter. LevelS039
contains 223 live objects, including two PRESSURE_METER circles with zero radius;
their sprites remain absent. Another 4,096 original GameObject circle cases match
all 122,880 fixture, mass, material and metadata words, including explicit/derived
signed zero and underflow. The C catalog sweep now loads/steps and opens ordinary
scene adapters for all 592 levels: 574 have complete bindings and 18 retain absent
visual names. This is component integration, not 592 native gameplay replays.
All 54 Debug/71 Release related checks, Windows compilation, formatting and the
existing Common Metal scene controls pass. Complete loading policy, sprite
ownership, original-frame Metal parity and gameplay remain open. Provenance is
`analysis/input/native-missing-bounds.json`.

`ab_audio_group_name` recovers getAudioName's dense-group selection. All 4,096
original cases match typed result and generator state, including 1,330 draws,
78 empty-group errors that consume a draw, and 964 controlled maximum-rounding
cases that select nil beyond the final variant. Missing groups return the request
unchanged without drawing. Sparse length rules and non-table groups remain
unsupported. The catalog reader recovers audio.lua's independent constructor
without running its platform branch or function definitions. All 46 dense group
names and 196 ordered string variants match the original full script's execution,
with sizes 1..12. The owned loader also reads directly from the pinned APK without
an extracted file or private key file. Exact allocation-limit, corruption and
cleanup checks pass. Sound-format policy, decoding and playback remain open.
Debug sanitizer and Release comparisons, Windows
compilation, all 107 default checks and formatting pass. Private provenance is
`analysis/audio-group-native.json`.

The same bounded catalog loader also recovers all 340 constant createAudio
property tables, in original call order, from loadAudioAssets. All 825 string,
boolean and numeric properties match original Android script execution with a
recording createAudio boundary. The reader returns data without invoking any
script calls. Missing properties stay absent; channel volume/count statements,
native defaults, file resolution and playback are outside this comparison.
Direct-APK and decoded-resource checks pass in Debug sanitizer and Release
builds; Windows compilation, all 108 default checks and formatting pass.
Private provenance is `analysis/audio-assets-native.json`.

Audio definition recovery now matches 4,096 original createAudio calls: 1,072
creations and 3,024 duplicate skips, including a false existing entry. Missing
extensions select MP3 under the Android policy; explicit false selects WAV.
Stream requests and stored name/channel/volume/overwrite/loop metadata preserve
their original types. A read-only capture of the running exact APK confirms
deviceModel android, audioPath audio and MP3 mode enabled. Using the original
loadAudioAssets and createAudio together in the isolated guest, all 340 resolved
paths, stream arguments and stored metadata match the C path from decoded and
direct APK input. Every file exists: 339 MP3 assets and one WAV asset. Debug
sanitizer and Release checks, Windows compilation, all 109 default checks and
formatting pass. The pure metadata boundary does not emulate resource-callback
mutation, numeric path coercion, decoding, mixing or device playback. Private
provenance is `analysis/audio-definition-native.json`.

Playback-request resolution matches all 4,096 original playAudio cases: 2,389
resource calls, 1,366 suppressed nil/empty names and 341 non-table metadata errors.
Truthy stored channel replaces caller channel; channel volume then replaces
caller gain, and truthy stored clip gain replaces both. These gains are not
multiplied. Numeric zero remains truthy; stored false loop leaves the caller's
loop value in effect. Remaining falsy values default to gain one, loop false and
channel zero. The original fifth argument is unused. Debug sanitizer and Release
comparisons, Windows compilation, all 111 default checks and formatting pass.
This is a typed request boundary; native conversions, return values, scheduling,
mixing and device output remain separate. Private provenance is
`analysis/audio-play-native.json`.

The first-party equal-rate PCM16 mixer projection matches 4,096 original one-clip
and 4,096 ordered two-clip AudioMixer/AudioClip cases: all 393,216 binary32
accumulator values and signed-16 outputs. The two-clip corpus uses separate
tracks and checks both source cursor advances. Track gain clamps to [0,1];
clip and master gains remain raw. Gain
multiplies in the original order. Signed samples divide by 32,767; final samples
multiply by 32,767 and truncate with asymmetric signed clipping. Preserve the
original channel mapping: stereo-to-mono repeats the left channel with half
weight for each addition. The reference uses generated PCM and writes memory
buffers without an audio output device. Debug sanitizer and Release comparisons,
Windows compilation, all 112 default checks and formatting pass. This validates
one/two-clip equal-rate arithmetic, with lifetime/order changes, scheduling,
resampling, delay/fades and device output still open. Private provenance is
`analysis/audio-mix-native.json`.

The owned PCM mixer now matches two original 128-by-64 command traces: all 16,384
state transitions and 35,051 signed-16 output samples. Comparisons include active
and pending order, all recorded voice fields/cursors, next handles, gain/limit
state, per-track counts and playing queries. Paused voices still count as
playing. Partial non-looping reads pad silence; a later zero read marks ended,
and removal waits for the following mix boundary. Looping resets at EOF. Ended
voices are removed stably before pending publication; zero-frame mixes still
publish/remove voices. Constructor allocations are bounded together, clip PCM
stays borrowed and mixing allocates no storage. Debug sanitizer and Release
comparisons, Windows compilation, all 113 default checks and formatting pass.
This covers serialized raw-PCM/unit-speed behavior; codec streaming, non-unit
rates, fades/delays and audio-thread/device integration remain open. Private
provenance is `analysis/audio-mixer-native.json`.

Clip-identity pause/stop/gain/query operations now match a third 128-by-64
original mixer trace: 8,192 commands and 5,035 signed-16 output samples. Clip
mutations touch 2,560 matching voices, including 1,202 paused and 688 ended
entries; queries include 535 true and 734 false results. All active/pending
fields, ordering, handles, gain state and PCM words match. Stop/pause/gain visit
every matching clip independently of track, including ended entries awaiting
removal. Paused voices count as playing, ended ones do not. The target exposes
resume by voice handle; the clip action API supports pause and stop.

The isolated original PCM mixer trace repeats byte-identically. SHA-256 is
`9fbf63066d2be28b8b78f1c867e8dde41cbff8c5f9f6d4a8e9f81ea89a2d46e8`;
private provenance is `analysis/input/audio-mixer-clips-native.json`. Debug
sanitizer and Release edge/all-three-native-trace checks pass, as does Windows
compilation. The reference job creates no audio device; emulator master mute
remains enabled.

`AbAudioResources` binds a borrowed, sorted map of already decoded equal-rate
PCM clips. It preserves full name bytes, missing-name requests and shared clip
identities. The rolling host now resolves query/play/gain/stop into the actual
first-party mixer, using loop, track two, unit speed and no fades/delays. C
composition checks cover embedded-zero identifiers, aliases affecting all voices,
two rolling loops, gain refresh without duplicate voices, stop/retirement and
PCM output. Resource lookup follows inspected native Resources branches; these
name-map composition checks do not establish full native Resources parity.
APK audio loading, group lifetime, MP3 decoding, resampling and device output
remain open. All 310 sanitizer tests pass in 56.30 seconds; focused Release
checks, Windows compilation and formatting pass.

The first-party MPEG-2 side-information parser now matches selected original
decoder fields over 2,047 frames and 3,978 channel granules: eighteen mono
collision frames, 1,931 stereo ambience frames and 98 mono 24 kHz award frames.
Comparisons cover main-data backsteps/private bits, part lengths, big values,
scalefactor compression, block/mixed flags, Huffman table selections, gain and
subblock-gain offsets, scalefactor scale and count1 selector. All three captures
repeat byte-identically. They include block types zero through three and no
mixed blocks. Derived region boundaries are retained by the observer but are
outside this parser comparison.

The observer runs the pinned original decoder in an isolated process. It
temporarily wraps that decoder's synthesis callback slots with first-party
read-only observers, forwards every synthesis call to its original function
and restores the slots after each frame. No original instruction bytes change;
no audio device opens. The parser/frontend also walks all 34,824 frame and side
records in the APK's 347 MP3 assets, preserving bounded main-data spans and
stored CRC words. C checks cover truncation, CRC placement, invalid channel/
window/count combinations and unchanged outputs on error. Native malformed
frame repair, CRC validation, reservoir/Huffman/scalefactor work and sample
decoding remain open. Debug sanitizer/Release comparisons, Windows compilation
and formatting pass; all 315 sanitizer tests pass in 56.41 seconds. Private
provenance is `analysis/input/mp3-side-native.json`.

The bounded first-party reservoir joins now match all 335,559 bytes in 2,047
original main-data windows over those same three assets. The observer reads
the original joined buffer after INT123_set_pointer and before its storage is
reused, preserving the native synthesis forwarding above. The C reader walks
the source file, accounts for Xing's exclusion from native frame numbering and
pushes preceding metadata/audio payloads before each observed frame. It retains
the newest 255 payload bytes without per-frame allocation. Every capture
repeats identically.

All 34,824 APK frames also pass the C reader/reservoir pipeline; that asset-wide
check does not expand the native byte comparison. C edges cover full retention,
sliding tails, empty payloads, missing history, invalid storage and unchanged
state/output on insufficient capacity. Native damaged-stream repair remains
unsupported. Debug sanitizer and Release checks, Windows compilation and
formatting pass; all 319 sanitizer tests pass in 57.83 seconds. Private
provenance is `analysis/input/mp3-reservoir-native.json`. CRC verification,
scalefactor/Huffman decoding and PCM remain open.

The MPEG-2 LSF scalefactor stage now matches 239,765 original values and derived
preflags over 8,589 channel granules in 5,121 frames. The real assets contribute
the preceding 3,978 granules. First-party synthetic packets exercise all 512
compression values in long, short and mixed layouts for normal mono and the
second channel of intensity stereo, with deterministic payload bytes. These
include setup frames and the normal first channel of each stereo frame. Every
captured value, native trailing-zero slot and preflag matches the C stage, and
all five traces repeat identically.

The runtime computes partition widths with bounded integer arithmetic and
uses explicit cumulative band endpoints. It preserves original ordering and
22/39/38 returned slots for long/short/mixed blocks, including their zero tails.
C checks cover non-byte-aligned starts, zero-width partitions, insufficient
input/part23 length, invalid metadata and unchanged output on failure. All
34,824 APK frames pass reader/reservoir/scalefactor processing; this wider C
asset check does not expand the native value comparison. Debug sanitizer and
Release checks, Windows compilation and formatting pass. All 325 sanitizer
tests pass in 56.36 seconds. Private provenance is
`analysis/input/mp3-scalefactors-native.json`. Native malformed-frame repair,
Huffman decoding, requantization, stereo transforms and PCM remain open at that
checkpoint.

The Huffman pair/quad primitives now match 20,355 original controlled packets.
These cover every codeword in all 32 pair table IDs and both count1 selectors,
every applicable sign combination, the zero-book aliases 0/4/14, all linbits
widths, isolated extension bits and maximum magnitudes for either or both pair
values. All five native corpora repeat byte for byte. The C checks also move
each packet through all eight bit alignments and reject every shortened packet
without changing its cursor or output.

The original dequantization core at ELF `0x8f92b8` runs inside the isolated
decoder at its first synthesis callback. The observer copies the initialized
granule, uses private scalefactor/spectral storage, temporarily rewinds the
decoder bit cursor, restores cursor/cache words and forwards original synthesis
exactly once. Controlled mono long blocks have zero scalefactors and gain one.
Each observed float must exactly match the original Android `pow(q, 4/3)`
initialization dictionary, including sign; the remaining spectral tail must be
zero. No original instructions or application state are changed. The first-party
reader uses independently constructed breadth-first branch pairs derived from
the format's codeword relationships, with immutable bounds and no allocation.
Original packed trees stay private. Provenance is
`analysis/input/mp3-huffman-native.json`. This primitive comparison leaves full
granule regions/count1 termination, nonzero scalefactor requantization,
short-block reordering, stereo transforms and PCM open.
All 331 sanitizer tests pass in 60.37 seconds; all 22 Release MP3 checks,
Windows compilation and formatting pass. Existing Common submodule compiler
warnings remain outside this decoder change.

Complete quantized spectral reads now match 2,918,592 native values across
5,067 first-party controlled frames at 16, 22.05 and 24 kHz. The corpus covers
every encoded long-region count combination, switched start/stop blocks,
non-mixed short blocks, randomized table/region crossings, full pair capacity,
variable count1 tails and every shortened count1 symbol/sign packet. Native
partial sign prefixes and discarded incomplete tree prefixes are reproduced.
All three traces repeat identically. Each native coefficient is identified by
the exact float dictionary described above; original cursor endpoints and
region boundaries are checked too. The C reader also checks nonzero part
offsets and an explicit preceding scalefactor budget without allocating.

The region resolver additionally matches 405 native side records covering all
128 long region-count combinations and switched block/mixed flags at each of
the three rates. The existing 3,978 real-asset granules now compare derived
region endpoints as well. All 34,824 APK frames pass the combined C frame,
side, reservoir, scalefactor and quantized-spectrum readers; this asset-wide
success is not a native comparison of their full spectra.

Mixed short spectral decoding stays unsupported. A retained synthetic packet
makes the original decoder report `attempted xrpnt overflow` and skip
synthesis. Its initialization appears to retain eight long mixed bands at
MPEG-2 rates; substituting a conventional map would hide the observed
divergence. The reader explicitly rejects that route, while independent side,
region and scalefactor parsing retain their tested mixed support. The isolated
spectral observer rejects mixed short packets before decoding and uses a
guarded private coefficient buffer. Private records are
`analysis/input/mp3-spectrum-native.json` and
`analysis/input/mp3-mixed-native-overflow.json`. Nonzero-scalefactor
requantization, stereo transforms, hybrid/polyphase synthesis, PCM and native
damaged-frame handling remain open.

The quantized spectrum checkpoint passes all 338 sanitizer tests in 60.35
seconds, all 29 Release MP3 checks, Windows compilation and formatting. Both
guarded repeats preserve the same native traces and the private output guard.

Magnitude reconstruction now matches every native binary32 value for unsigned
quantized magnitudes zero through 8,206. A separate first-party packet sweep
sends every magnitude through the original Huffman/dequantization core at unit
gain, retaining the raw float and signed quantized result; all 8,208 packets,
including setup, repeat identically. The runtime computes the correctly rounded
cube root of the integer fourth power with portable wide-integer comparisons,
then compares the exact midpoint between adjacent binary32 encodings. It uses
no host `pow`/`cbrt` calls, extended integer types or imported lookup tables.

The original `INT123_init_layer3_gainpow2` at ELF `0x8fa934` independently
supplies all 378 factors in the complete quarter-step range -46 through 331.
The first-party four-fraction/exponent construction matches every float bit,
with an identical repeated trace. Neither helper allocates or changes output on
invalid input. Optional native checks use the `POWER` Huffman variant and
`AB_REFERENCE_MP3_GAIN`. Private provenance is
`analysis/input/mp3-magnitude-gain-native.json`. These numeric factors do not
establish scalefactor-band application, stereo transforms or PCM synthesis.

Applying the numeric factors now matches 7,646,400 native float values over
13,275 controlled original-core calls. These cover long/start/stop and
non-mixed short blocks at all three rates, every magnitude and global gain
byte, both scalefactor scales/preflags, short-window gains and the native
inverse-sqrt-two mid/side gain adjustment. All four corpora repeat identically.
The native observer prints its explicit scalefactor/preflag arguments before
replaying the core on private guarded storage. The compressed packets encode
zero-width scalefactor partitions; these are unit comparisons of band scaling,
not pristine full-stream captures of encoded nonzero scalefactors. The C
checker reconstructs quantized input from those packets and compares every
requantized float bit. Native pre-emphasis data is independently read at BNE
`0xba3d8c`, as two 22-byte rows.

All 34,824 APK frames now pass the C frame/side/reservoir/scalefactor,
quantized-spectrum and requantization pipeline. This establishes bounded
support for the asset inventory, without claiming native agreement for all
their spectra. Pristine original-stream spectral captures, mixed-short
overflow recovery, stereo transforms and hybrid/polyphase PCM synthesis
remain open. Private provenance is `analysis/input/mp3-requantize-native.json`.

The combined frontend now also matches 67,968 pristine native spectral floats
from 118 real-asset granules, including priming frames. These comprise all 19
granules of the mono collision clip, 66 granules from the stereo ambience and
33 from the 24 kHz power-up clip. All three captures repeat byte for byte.
Entry observations retain the original scalefactor arguments, preflag,
independently computed native part2 bit count and bit cursor, joined reservoir,
region endpoints and normalized gain pointer. Return observations copy all
576 original coefficients before stereo, alias or hybrid processing. The C
checker reads the pinned source through every preceding frame, then compares
each observed field and float. This closes the native bit-count observation
gap as well as checking frontend composition on actual compressed input.

The isolated capture uses a first-party reader for the documented
[GDB all-stop remote protocol](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Overview.html).
Hardware-breakpoint attempts missed prefix calls in this reference environment.
Accepted captures instead use temporary software traps, with each original
instruction word verified restored before execution resumes. Debugger code
does not write decoder data or registers. All traps are removed before detach;
the guest library's file hash remains pinned, the application stays detached
and emulator audio stays muted. Side bytes captured after reservoir joining
can contain overwritten tail bytes; the checker reconstructs that overlap
rather than treating them as the original encoded side record. Private
provenance is `analysis/input/mp3-pristine-native.json`. These selected captures
do not establish every asset's full spectrum or final PCM. Mixed-short overflow
recovery and stereo/hybrid/polyphase synthesis remain open.

The pinned Android initialization selects one channel, 16 bits and 16 kHz.
AudioFlinger independently reports those exact fields for the running APK's
AudioTrack. Keep this Android game profile on both desktop hosts; the original
desktop platform branch is outside the selected target. Its 4,096-frame track
capacity does not establish the game's mixer callback size. The one 24 kHz MP3's
playback handling and downstream platform resampling remain open. Private
provenance is `analysis/audio-profile-native.json`.

The first-party PCM16 RIFF/WAVE reader now handles all three WAV entries in the
pinned APK, including the score loop's trailing CDif metadata and the notification
asset's extended PCM format chunk. Every decoded binary32 sample matches macOS
afconvert's independent LEF32 output: 5,730 score-loop frames and two copies of
the 9,920-frame notification, all mono at 16 kHz. Truncation, chunk ordering,
padding, format consistency, duplicate chunks and output-capacity checks pass.
Debug sanitizer and Release comparisons, Windows compilation, all 108 default
checks and formatting pass. This establishes PCM16 conversion, with MP3,
resampling, mixing and Android playback fidelity still open. Private provenance
is `analysis/wave-native.json`.

The bounded MPEG-2 Layer III reader walks every MP3 resource, including five
leading ID3v2 tags, 20 trailing ID3v1 tags and 338 Xing/Info/LAME records. All
34,824 MPEG frame boundaries match an independent parse. All 347 output formats
and 19,238,996 decoded sample-frame counts match the original bundled decoder,
including encoder delay/padding. There are 345 mono 16 kHz files, one stereo
16 kHz file and one mono 24 kHz file. The isolated reference job hashed all
40,701,012 bytes of original signed-16 PCM without retaining the complete sample
corpus. Debug sanitizer and Release checks, Windows compilation, all 110 default
checks and formatting pass. This reader validates structure and length; it does
not decode compressed samples or validate MPEG/LAME CRCs.

One independent macOS converter comparison produces the same 8,787-sample length
and format but differs by one signed-16 unit in 4,214 samples. Float output from
that converter is already quantized and does not remove the discrepancy. MP3
sample decoding remains open. The original decoder stays in the private oracle;
no decoder source or runtime dependency is imported. Its feed/getformat API was
checked against [the primary API documentation](https://www.mpg123.de/api/group__mpg123__input.shtml).
Private provenance is `analysis/mp3-native.json`.

The classic catalog property/geometry layer and constructor ordering now feed
`ab_level_bodies_create`, with explicit scales, ground position and sprite-bound
access. A paired LLDB entry/return capture records the first eight world steps
after a level-1 restart. The first pre-step snapshot has 23 bodies/fixtures and
zero contacts; its return has 22 contacts. Registers confirm timestep bits
`3d088889` and 10/10 iterations. FPSCR numeric controls select nearest rounding
and gradual underflow. Debugger pauses affect wall time, so these observed tick
indices do not establish normal presentation timing. The emulator remains muted.

All 23 initial fixture shapes/materials and collider/collision/sensor/gravity
metadata match. Defined body fields and tight bounds match for 22 objects.
The leading bird's later sling pose, native allocator/padding bytes and pointer
fields are excluded explicitly. Ground geometry is 2000 by 10 physics units,
zero density, friction 0.8, restitution zero, skin 0.1 and basic collider; its
position is a supplied input while automatic level-bound recovery remains open.
Queue physics sets zero rotation before static type. The runtime loader does
not claim random/timer state, scripts or full gameplay effects.
Private provenance is `analysis/level-body-native.json`; full paired snapshots
remain retained for subsequent replay comparisons.

The growing string-table pass matches 39,030 entries across 4,096 original-Lua
cases, including embedded zero/high bytes, collisions and all tested growth
boundaries. Growth doubles the hash part and reinserts old physical slots in
descending order. This differs from a constructor's preallocated hash part.
Creating level-1 objects in constructor order, then ground, and applying that
growth reproduces all 23 names in the running game's runtime object table.
`ab_level_bodies_order` exposes this traversal with caller-owned scratch, and
the initial-body comparison now applies queue physics in that order. Deletion,
reinsertion and queue timers/animation remain open. Private provenance is
`analysis/string-growth-native.json`.

A second paired capture starts with an empty world after a cold APK process
launch. The C loader and queue setup reproduce all body states across its first
eight complete steps, including ordered contact manifolds, material/TOI fields,
proxy identifiers and fixture bounds. The game collision matrix is active in
this comparison. Each observed leading-bird sling pose is supplied as an
external transform input; its fat bounds are excluded because intermediate
animation moves were not captured. Other fat bounds match. Body allocator
indices, wall-clock profiling values, pointers and padding are excluded. This
establishes the bounded physics sequence, not animation, damage callbacks or
full gameplay. Debug sanitizer and Release comparisons pass. Provenance is
`analysis/level-steps-native.json`.

`GameLua::setRotation` at analysis `0x32ba1c` uses Android `fmodf` by twice
binary32 pi, then adds that same period for negative remainders. The authored
integer-significand remainder and binary32 addition match 65,536 measured
Android-import cases, including signed zero, subnormals and largest finite
magnitudes. Tiny negative values can round to the period itself. Exception and
status flags remain outside this numeric-result contract. This corpus checks
the recovered import sequence; it does not invoke GameLua for every sample.

Frame-clock arithmetic now matches 108 complete original `GameLua::update`
entry/return pairs: twelve frames after a cold level load and 96 controlled
updates in the same loaded level. The controls vary elapsed seconds, numeric
speed factor, physics-enabled byte, initial time and initial accumulator.
Debugger stops perturb wall time; these calls do not measure normal frame rate.
Time receives unscaled elapsed seconds, while only enabled physics adds scaled
seconds to its accumulator. The recovered loop subtracts `0x3d088889` repeatedly;
binary32 `0.1` initially permits two steps, then permits three with the carried
remainder. Missing/non-number speed-factor behavior is established statically,
outside this numeric-factor corpus. The C arithmetic planner has an explicit
step budget and never drops time on overflow. Full callback, pause and pose-history
integration remain open. Provenance is `analysis/frame-clock-native.json`.

`GameLua::setAngularVelocity` at analysis `0x322ad4` leaves static bodies
unchanged, stores angular velocity for kinematic/dynamic bodies and wakes a
sleeping body only when the binary32 squared speed is positive. This underflow
condition matters for subnormals. All 4,096 direct native cases match, including
1,072 wakes, signed zero and previously awake bodies. The existing controlled
single-entry object map supplies the actual GameLua body lookup. Debug sanitizer
and Release comparisons pass; this does not establish full launch/queue behavior.
Provenance is `analysis/body-angular-native.json`.

The original `updateObjectProperties` callback (gamelogic prototype 161393)
stores its supplied angle directly in the object's Lua table. Native update
instructions obtain that argument with `atan2f(body.sine, body.cosine)` before
the subsequent game update call. This signed result differs from reusing the
body's stored wrapped angle: after crossing pi, a sling rotation addition takes
place in the negative half-circle before `setRotation` wraps it. A ten-frame
composition initially exposed a one-ULP difference from omitting that round trip.
The authored component-angle math now matches 65,536 original Android libm
results. Its five polynomial parameters and split-angle constants are measured
from the pinned binary; no external implementation source is included.
Provenance is `analysis/angle-components-native.json`.

With that synchronization, the C loader, queue-body setup, clock, world and
sling body adapter match twelve complete original update frames through dynamic
transfer completion and two held-bird updates. The 26 physics steps reproduce all 23 defined body states,
ordered contacts and every tight/fat fixture bound. No leading-bird bound is
excluded in this comparison. Both sling vectors and the already elapsed initial
partial-update timer are captured inputs, as is the physics-enable state before
each frame. Native resource/pointer/padding/allocator bytes and profiling clocks
remain excluded. Both pose-history slots and interpolated pose fields match for
every object. Histories start with the wrapped angle set during construction;
physics captures subsequently use signed atan2. The history pass marks a massive
bird as a bullet, and holding applies position, linear velocity zero, rotation
zero, then angular velocity zero. Full queue, callbacks and scene presentation
remain open. Debug
sanitizer and Release comparisons pass. Provenance is
`analysis/level-frames-native.json`.

Lua's math.atan2 wrapper at analysis `0x8305d0` promotes both float arguments to
double, calls atan2, then rounds the result back to float. Its result sequence
differs from physics atan2f. The authored addition-identity reduction and degree51
alternating series match 65,536 measured Android import sequences in Debug
sanitizers and Release. Original Lua is not invoked for each of those numeric
samples; a separate stretch oracle executes the original methods and helpers.
This is sampled finite-input evidence, without an exhaustive rounding or
floating-point exception/status claim. Provenance is `analysis/script-angle-native.json`.

Default Slingshot.stretchRubberBand (prototype 28331), its stretchability factor
(29626), and SlingshotSystem.stretchRubberBand (11295) now have a composed C
numeric/state pass. All 4,096 original-Lua cases match band coordinates, angle,
normalized stretch, stored distance, wobble, selection state and 4,471 ordered
audio boundaries. Core radial clamping uses displacement times maximum length
before division by original length; an angle-dependent vertical constraint
narrows the upward region. The system then recomputes rounded band-to-rest
distance and its stretch getter clamps distance/maximum length to [0, 1].
Original vector allocation, length/distance helpers and factor policy execute in
the private oracle; selected scalar UpVals and original rest-vector fields are
controlled inputs. Audio/definition lookups are explicit boundaries. No input
mapping, actual audio dispatch or launch equivalence follows from this test.
Provenance is `analysis/sling-drag-native.json`.

Actual SlingshotSystem.launchBird at prototype 11861 negates selected force
before multiplying physics scale, bird mass and sling power. It normalizes
either an explicit vector or the original getVectorToRestPosition result, then
multiplies force and stretch. No inverse mass appears in this impulse result.
The C calculation matches 4,096 original Lua calls through launchBirdWithImpulse's
position/impulse boundary in Debug sanitizers and Release. Controls include both
force signs, signed zero, zero overrides and 32 parameter sets using -925 force,
0.05 scale and first-bird mass bits 0x4159e6a0. The -925 initialization value
comes from gamelogic updateValues, prototype 66976; active-level/mode override
selection remains caller work. Statistics eligibility is suppressed in this
arithmetic oracle with its explicit eagle flag; only the unconditional shots
boundary executes. The actual event, position/velocity, impulse-application and
queue effects are separate from this arithmetic corpus. Provenance is
`analysis/sling-impulse-native.json`.

The following launchBirdWithImpulse transition (prototype 12600) matches 4,096
original-Lua cases and 59,392 ordered host boundaries. The bird event receives
the original mutable position/impulse vectors, and application rereads the
bird's position after that event. The level hook precedes assigning flyingBird
and looking up its definition hook. Tutorial time resets unconditionally after
the optional hook. Flags/counters precede the optional all-birds event; sprite,
particle and audio requests follow. Object-script launch velocity and final
stored velocity independently reread mass and impulse around the script call.
Controlled event and script mutations verify these distinctions. The C boundary
uses nullable string identifiers and a local active-list count; arbitrary Lua
types, environment mutation and shared-table identity are outside its contract.

The complete launchBird/releaseRubberBandToLaunchBird host route now matches
754,823 words from 4,096 paired original scenarios, including 27,947 launch and
7,913 release service boundaries. Explicit-vector distance is captured before
callbacks; queried direction remains live through the later power lookup.
Statistics can change selected bird metadata before definition lookup. Base
force captures scale and mass before power, while impulse calculation precedes
the launch-position query. Position may alias direction, and callbacks may
mutate both. Total-shot increments are unconditional after the optional backwards
increment. Missing services yield 425 launch and 186 release failures with earlier
effects preserved. Nil/false force uses the current default; numeric zero stays
present. The native repeat is byte-identical. The original root-loaded methods,
Class/Vector2/math helpers and public dragged-bird setter/getter execute unchanged;
only their service environment is controlled. Typed predicates/numbers exclude
general coercion, metatables and function rebinding. Statistics persistence,
actual shot/cancel services and complete input/session scheduling remain open.
All default Debug/Release/Windows builds, 45 strict Debug and 45 Release affected
checks, seven native replay profiles and formatting pass. Private provenance is
`analysis/input/sling-launch-host-native.json`.

The fixed-level `AbClassicShots` owner composes those routes with real owned
position/history setters, velocity reset, explicit-point impulse, live definition
lookup, canonical sprite selection and behavior dispatch. A separate C storage
publisher exposes internal shot assignments before subsequent services execute;
it does not add game callbacks. The final launch vector belongs to the level's
property owner. Flying identity is the physical object, as the actual Museum
behaviors require. Catalog-backed sprite bytes survive adapter teardown. The
retained native swipe matches both impulse and resulting body values through
this adapter. Other services in that integration test are controlled inputs;
statistics persistence, actual events/hooks/particles/audio and the interactive
session remain open. Private provenance is `analysis/input/classic-shots-owned.json`.

An explicit catalog-wide constructor audit found one shared behavior gap:
MuseumBluesTrajectoryRenderer excluded 130 levels. The added C class now matches
22,946 words from 512 original constructor/event cases and 262 mutable boundaries.
The original Class/BehaviorBase/Blues roots execute unchanged. Parent launch
writes flying before adding follow; child init writes it afterward. Specialty
removes follow before setting split. Destroy cleanup requires a truthy cluster
that has not split. Child collision freezes originalCluster/self before two
fresh otherBirds/getObject lookups, preserving four keep-list slots and nil holes.
There are 44 matching failures; the repeat is byte-identical. The typed projection
does not expose allocation identity or sparse Lua length/multiple-return rules.
All default Debug/Release/Windows builds, 55 strict Debug and 55 Release checks,
seven existing native replay profiles and formatting pass. All five current
owners now construct for all 592 catalog levels with actual declared theme
groups and explicit seed zero. Eighteen retain missing visual names. This does
not establish playable sessions or all-level native parity. Private records are
`analysis/input/blues-trajectory-native.json` and
`analysis/input/catalog-runtime-owner-blues-audit.json`.

The first-party EventManager owner matches 207,456 words from 6,144 original
subscription/notification/queue/delay actions and 2,144 listener boundaries.
Original Class/utils/EventManager roots and native base/table libraries execute
unchanged, with controlled listeners and inactive diagnostics. Live dense-list
mutation, payload IDs, protected nested notifications, shared delayed-event
identities and partial failures agree. There are 1,088 failed actions, including
832 boundaries retaining active buffering. The native repeat is exact; Debug
sanitizers, Release, all default platform builds and formatting pass. The retained
comparison uses string IDs. Raw internal-table replacement, sparse sequences,
metatables, GC ownership and original allocator-limit equivalence are outside
this typed owner. Private provenance is `analysis/input/event-manager-native.json`.

A separate bounded read at a real GameLua.update entry identifies the current
ordinary session's two bird-shot listeners. Their original instruction bytes
match AchievementProcessor.eventTriggered (335 instructions, prototype 3180)
and ClassicFlurry.eventTriggered (315 instructions, prototype 879). The same
snapshot has no all-birds-shot or bird-put-to-slingshot listeners, powerups
enabled and the red-special flag false. Inherited callbacks are resolved through
read-only plain-table __index chains. No original calls or process-memory writes
run, and the game is detached/resumed. This records one registration state, not
other modes or complete callback effects. Private provenance is
`analysis/debugger/shot-event-listeners-provenance.json`.

Owned statistics storage now matches the unchanged original SettingsWrapper
root in 1,024 actions and 4,096 subsequent local/device queries (15,424 words).
The reference uses fixture IDs only. Separate current-device installations,
other-device exclusion, malformed root/device values, nil/false distinctions,
binary32 increments and hash-order cancellation sums agree. The repeat is
byte-identical. This covers empty-table string-key growth, finite numeric
addition and bounded owned state; saved constructor hints, collection, numeric
coercion and save-file persistence remain separate. Private provenance is
`analysis/input/settings-statistics-native.json`.

The shot service binding uses that state for outer increments/inner reads and
delivers both shot notification types through the shared EventManager. Stable
payload storage preserves borrowed original vector-table identities. Tests
retain payloads beyond the call, mutate aliased vectors, replace event fields,
reject reentry, and preserve earlier writes after listener failure. The owned
launch/body/visual/behavior comparison uses this binding and still matches the
retained native impulse/body values. Its gameplay listeners and remaining
particle/audio services are controlled inputs, not finished runtime effects.
Five relevant checks pass in Debug with sanitizers and Release, all three
default builds pass, and formatting passes. This is shared-service integration,
not a playable session. Private status is `analysis/input/shot-services-owned.json`.

The two ordinary shot listeners now match 128 original EventManager deliveries
across 64 scenarios, including repeated achievement attempts, first-time status
writes/saves, analytics session caching and the index/score/powerup/level gates.
The full original class, subsystem, settings, manager and handler roots execute
unchanged. Save, powerup, level type/text, unlocked-event observation and the final
analytics request are controlled native host boundaries; no files are saved or
network requests sent. The public level-text query is rebound on the fixture
instance, so this comparison does not cover the original level-text formatter.
The ordinary Android `iOS == false` branch returns before the platform unlock
queue. Other platform/event/mode handlers remain outside this owner. The repeat
is exact; seven checks pass in Debug with strict sanitizers and Release, all
default builds pass and formatting passes. Both actual handlers are also bound
in the owned launch/body/visual/behavior comparison; the resulting Android
impulse/body values still match. Save restoration/persistence, complete powerup,
particle/audio services and the session loop remain open. Private provenance is
`analysis/input/classic-shot-listeners-native.json`.

The owned audio bank now loads all 340 original registrations and binds stable
PCM/metadata to the real mixer. All 339 MP3 bindings match their complete original
PCM hashes/formats; the remaining score-count WAV has the previously verified
5,730 mono 16-kHz frames. Thirty-five registrations request streaming. Playback
currently uses PCM decoded at startup, so this does not recover stream refill
timing. The one 24-kHz clip is retained for inspection and rejects at playback
until its original rate handling is recovered. The initial channel volume/count
policy comes directly from the pinned loadAudioAssets instructions 1682..1751;
the resource/group/play/mixer primitives retain their separate native evidence.
All 340 original registration tables and 825 typed properties were also rechecked
against the retained original corpus. Default sling launch uses getAudioName on
bird_shot (Slingshot prototype 29533), then gain one/non-looping playAudio
(SlingshotSystem prototype 6618). The owned shot/body/visual/behavior comparison
now submits both actual decoded sounds while preserving the Android body words.
Eight relevant checks pass in strict Debug and Release, default platform builds
and formatting pass. These checks open no device output. Native output scheduling,
the remaining definition/particle effects and a playable session remain open.
Private status is `analysis/input/audio-bank-owned.json`.

The shared used-powerup list matches 128 original mark/query/storage-reset
operations and 384 subsequent queries (648 words). The unchanged original
PowerupManager, utils contains/find_key and native string.lower methods run on
controlled dense use-list storage. This covers ASCII case folding, duplicates,
empty names, counts and membership, not the full manager initializer, activation,
purchase or reset callbacks. Its native repeat is exact. The ordinary shot effect
binding separately matches 64 original attachment/red-special gate cases (576
words), including numeric-zero/empty-string potion truthiness and disabled paths
with no manager. Forty attachment paths and 48 red gates succeed. Powered trail
spawners and the active red update-hook service are intentionally unbound in the
native fixture and C binding; the remaining cases establish only their preceding
flags/failure prefixes. The C tests also rebind script.physicsObject while
checking direct canonical field writes. Two additional physical-property slots
support the recovered shot flags. Eleven relevant checks pass in strict Debug
and Release, all default builds, seven retained native replay profiles and
formatting pass. This closes the reached initial ordinary launch services while
leaving powered/red abilities, interactive sling/queue/frame orchestration and
complete sessions open. Private provenance is `analysis/input/powerup-uses-native.json`
and `analysis/input/classic-shot-effects-native.json`.

A real first-level swipe supplied a paired original GameLua::applyImpulse
entry/return snapshot. Composing the C launch arithmetic with explicit-point
body impulse matches both impulse components and the resulting linear velocity.
Every other selected scene field, including the other 22 bodies, contacts,
proxies and pose histories, remained unchanged across that native call.
The rest vector and default force/scale are previously measured inputs; this
capture does not recover touch mapping or a complete flight replay. Debugger
stops perturb wall-clock timing. Debug sanitizers and Release pass both new
comparisons. Runtime host adapters, input release routing, statistics storage
and subsequent flight/destruction remain open. Provenance is
`analysis/bird-shot-native.json`.

Release routing (prototypes 7901/11591) launches only above the stretch
threshold. Equality cancels; signed zero, infinities and NaNs retain the
original numeric comparison behavior. Launch statistics in prototype 11861
exclude backwards increments for literal-true eagle bait and any truthy special
mode, then always increment total shots. A further 4,096 original calls match
all impulse values, 577 backwards/4,096 total increments and their order,
and 1,537 launch/2,559 cancel releases. Original methods execute with controlled
property values and boundary callbacks. Cancel side effects, input mapping and
runtime statistic storage remain separate. Debug sanitizers and Release pass.
Provenance is `analysis/sling-routing-native.json`.

Cancel/reset methods (prototypes 6044/6245) match another 4,096 original
calls and all 6,144 ordered position/audio-query boundaries. A present dragged
bird is positioned at rest, then its reference and selection flag clear. An
absent bird preserves selection. Both paths copy rest to band and reset angle,
distance, wobble and previous distance to positive zero before querying
laser_sight playback. If it is stopped, scope-sound eligibility becomes true;
otherwise that flag is preserved. The oracle uses unchanged original closures,
selected numeric/boolean UpVals and root-vector fields, and controlled host
boundaries. Actual input dispatch, body adapters and audio remain separate.
Provenance is `analysis/sling-cancel-native.json`.

Twelve pre-impact flight frames now compose the source-data loader, previously
validated initial transfer/hold frames, 512 bounded settling steps, measured
stretch, launch impulse and frame clock. All other dynamic bodies must be asleep
before launch. All 23 body states, ordered contacts and fixture bounds match
36 physics steps in Debug sanitizers and Release. Queued-bird poses and initial
flight clock are explicit inputs; bird fat bounds and flight pose histories
are excluded. A radial-limit band can round to a measured distance just below
maximum: this shot's stretch bits are 0x3f7fffff, rather than 1.0. The shared
stretch-measurement helper preserves that distinction and the existing 4,096
original drag comparisons still pass. The 24-frame raw capture continues into
first impact, body removals and scoring; those later frames remain unverified.
Provenance is `analysis/level-flight-native.json`.

The complete 24-frame shot capture also supports an ordinary impact projection.
The C world composes Museum collision dispatch, material multipliers, retained
logical velocities/masses, strength loss, contact disable and legacy immediate
bird velocity, then caller-ordered removal/break score and deferred consumption
after each step. All 72 fixed steps reproduce surviving body state, ordered
contacts, fixture bounds, logical strength/defence/mass/velocity/hasCollided and
score at update boundaries. The C replay dispatches 31 callbacks and removes
two wood blocks, one glass block and a pig, ending at score 9,380. Native callback
counts/order are not separately captured. Source is `src/game/museum_impact.c`;
`AB_REFERENCE_CLASSIC_IMPACT` pins the comparison in Debug sanitizers and Release.

Initial logical scalars, initial flight clock and queued-bird poses/types are
captured inputs. Ordinary script flags/modifier values are supplied; script
dispatch, sprites, audio, particles and achievements are outside the projection.
Bird fat bounds and flight pose histories are excluded. The replay reconstructs
each small dead-block batch with the growing string-table order helper; this
does not establish general retained-table deletion/reinsertion order. Queued
logical mass remains its original value while its body is static. End-of-frame
velocity synchronization precedes held-bird velocity reset; the second bird's
Lua velocity records gravity while its held body returns to zero. The existing
history-capture helper supplies the measured bullet transition. Provenance is
`analysis/level-impact-native.json`.

An extended impact corpus adds the initial partial reload timer and before/
after timer, birdFired, fillInNextBird, birdReady and currentBirdIndex fields.
The C replay selects the next bird by the source level's startNumber, crosses
the timer on frame 9, initializes the transfer from the settled queued body,
composes the validated arc/body operations through dynamic completion on frame
18, then holds the ready bird. Native body poses/type for that bird from frame
9 onward are compared outputs. All earlier 72-step impact, logical-state and
score checks still match. The initial timer is captured; preceding idle jumps
and the other queued bird's poses remain inputs. This does not dispatch transfer
script/audio/event/hats/camera hooks. `AB_REFERENCE_CLASSIC_RELOAD` enables this
separate comparison. Provenance is `analysis/level-impact-reload-native.json`.

The idle extension generates both queued birds for all 24 frames. It compares
their logical x/y/angle, jumpOn, jumpTimer and saved Y as well as the prior
world/impact/score fields. The second bird's first jump phase is inferred as
0x403b0048 from its captured initial/next positions and a later retained terminal
height/speed/phase; the constrained inverse branch has one matching phase. The
other bird's first jump has one matching bounded draw triple, 17/13/2, for its
captured timer, position and roll angle. These are inferred replay inputs, not
captured original global RNG state. Unobserved initial fields that are unused
until jump initialization are zeroed in the test seed. Retained native queue
fields were read while the same target process was briefly stopped, and it was
resumed in a finally block; audio mute remains verified.

The queued static body's equal-position history interpolation can advance its
logical X by one ULP before a jump writes it back to physics. In frame 19 the
Lua angle retains assigned body angle 0x3e36a240 while the rendered angle is
0x3e36a241; the idle projection preserves that observed distinction. The
comparison does not establish general pose-history policy. All queued pose
records are expected outputs rather than per-frame mutations in this extension.
RNG generation/global call order, presentation and complete script/controller
effects remain open. `AB_REFERENCE_CLASSIC_IDLE` enables the comparison.
Provenance is `analysis/level-impact-idle-native.json`.

A separate 96-frame native retry capture includes the complete queued jump
fields and reaches completion on frame 37. Remaining-bird bonuses on frames
73 and 79 take score 11,380 to 21,380 and then 31,380. The original process was
resumed by normal detach; Android master mute was verified before and afterward
with the silent host backend retained. The separate restricted replay below
now compares the longer trace. Raw-capture provenance is
`analysis/level-results-native.json`.

That retry retains different proxy identifiers and 15 different settled
non-bird states. A controlled private experiment creates all bodies from the
same source catalog/level, then either keeps fresh identifiers or applies a
bijection to the constructor-generated tree/proxy IDs before contact discovery.
The latter imports only observed leaf identifiers, with no native positions,
velocities, bounds, contacts or impulses. After 512 fixed settling steps, all
30 defined body words for all 20 non-bird objects match; the fresh-ID arm differs
for 15 objects. Sanitizers pass. This isolates discovery/solver ordering caused
by retained identifiers. Full prior-world allocator/reset lifecycle remains open.
Provenance is
`analysis/retry-proxy-identifiers-native.json`.

The regular comparison adapter now seeds those leaf identifiers before contact
discovery, settles bodies created from source data, and compares the first 38
retry frames. All 114 steps match body state, ordered contacts, fixture bounds
and logical strength/defence/mass/velocity/hasCollided/score. The C replay has
84 callbacks and five removals, ending at 11,380. The generated ground's
`staticGround` material lookup is supplied by its controlled metadata adapter.
The adapter imports no native body poses/bounds/contacts for initial settling;
initial logical scalars, sling/clock values and queue poses/types are inputs.
Native completion decisions and bonuses are not compared by this projection.
Prior-world reset lifecycle, pose histories, bird fat bounds
and full script/presentation effects remain open. `AB_REFERENCE_CLASSIC_RETRY`
enables this separate check. Provenance is `analysis/level-retry-native.json`.

A further restricted projection compares all 96 retry frames and 288 physics
steps. It composes collision damage, five block/pig removals, ordinary bird
retirement, goal completion and remaining-bird bonuses in first-party C.
All defined body/contact/bounds fields, logical damage/score fields and removal
timers match, with bird fat bounds and histories excluded. The C run emits 91
collision callbacks; the capture has no separate per-callback trace to compare
that count. It predicts completion on frame 37, an additional bird removal on
42, bonuses on 73/79 and final score 31,380. Goal metadata comes from the source
catalog. This scene has no joints, specialty timers, bubbles or movement-ignore
table entries. Its movement test follows signed angular speed > 0.1 or squared
linear speed > 9 (LLVM ELF 0x3250a4..0x3250f0), with timeout bypasses held false.
Queued poses/types, initial retry identifiers, logical/clock/sling seeds and
ordinary timer/bonus/camera configuration are explicit inputs. Camera identity
is supplied as false; actual removal establishes it was not this bird at that
boundary, without recovering the full camera policy. The ending-delay helpers
predict an event request on frame 94. Using the capture's observed next-update
dispatch boundary, the projection disables the controller on frame 95 and
compares that flag to the native physics-enable field. No controller flag is
assigned from that field. The resulting free-body sweep and velocity match.
The general event scheduler and mode-exit effects are still open. This is a
state projection, not complete mode-exit, queue, audio, result or persistence
parity. `AB_REFERENCE_CLASSIC_RESULTS` enables the check. Provenance is
`analysis/level-results-replay-native.json`.

The 96-frame projection now also calls the first-party completion initializer.
It derives the 3.5-second bonus timer, remaining index and one-second ending
timer instead of assigning them as state seeds. Source start numbers determine
subsequent bird presence; the retained last-index field is checked against
source data. The existing cancel-drag helper and held-body helper generate the
current bird's post-completion position and velocity, which are compared before
any queue mutation. Earlier poses and the other idle bird remain inputs. The
initial current index/shot metadata, ordinary configuration, absent level hook
and false camera-target identity remain controlled. Audio/event/camera calls
are boundaries; no replacement resources or presented effects are supplied.

An additional 96-frame comparison generates both queued birds throughout the
same retry. It compares their physical poses/types, all captured jump scalars
and flags, reload timer/fired/fill/ready/index, both history slots and rendered
poses before and after every frame, along with the existing physics, damage,
completion, retirement, bonus and handoff fields. All 288 steps still match.
Initial queued logical/physical states and histories are captured inputs;
the initial reload timer is explicit. Fifteen bounded random values are inferred
from resulting jump state. Completed-level jumps discard their first timer and
height draws; ten additional supplied values (zero/five) are valid unobserved
choices, not recovered actual RNG output. The original global RNG remains open.
The history index starts at zero: initial slots are equal, and the final free
held-bird frame uniquely distinguishes the current slot, so this is an inferred
boundary input rather than a captured index. Capture uses the recovered <= two-
tick history gate and weighted interpolation. Position-only jump writes retain
the rendered angle; rotation writes use the assigned wrapped angle. Frames
86..88 distinguish these: the body retains angle 6.2663717 while the next
position-only update uses signed angle -0.01681358 in logical/history/rendered
state. Completed-level jumps, source queue selection and held motion are now
generated; full RNG, creation/reset history, audio, scene rendering and UI mode
effects remain open. `AB_REFERENCE_CLASSIC_RESULTS_QUEUE` enables this separate
check. Provenance is `analysis/level-results-queue-native.json`.

The idle-bird branch of gamelogic `updateCharacterAnimations` (prototype offset
137493, instructions 248..391) matches 4,096 original calls with controlled
bird metadata and random/position/rotation boundaries. The selected prototype
is unchanged and runs only in the original Android Lua implementation. Pig,
blink and sound branches are held inactive; its unused slingshot upvalue is nil.
The C comparison preserves 6,068 random draws, 3,347 positions and 695 rotations,
including timer subtraction, strictly negative restart, completed-level height,
roll gating, the first rebound and the second landing/restoration. An already
negative timer restarts even if `jumpOn` was true. Level completion uses Lua
truthiness; jump/rebound/allow-roll use literal true. Phase-boundary cases use
the adjacent floats around binary32 pi. The shared independently authored
promoted-sine helper now also serves sling transfer; its existing original-Lua
comparisons still pass. RNG generation, idle eligibility, hats and native
position/rotation round trips remain caller responsibilities. Provenance is
`analysis/bird-jump-native.json`.

The default sling reload prefix (Slingshot prototype 17173, instructions 0..27)
matches 4,096 calls to the full original function, with a retained transfer name
and an incomplete fixed transfer available afterward. It subtracts elapsed time
only from a strictly positive timer and queries currentBirdIndex + binary32 one
only when that subtraction reaches <= zero. A non-nil result, including false,
combined with truthy birdFired clears that flag, sets fillInNextBird and clears
the retained launch cue. A nil result disables camera reset. There are 1,650
queries, 543 cue resets and 565 missing-next results. Conditional transfer
position/rotation calls also match the already validated arc helper. Boolean
fired/fill/camera states and controlled next-bird results are oracle inputs;
lookup implementation and complete queue/controller integration remain open.
Provenance is `analysis/sling-reload-native.json`.

Classic `checkLevelComplete`, `checkLevelGoalsDestroyed` and `beforeLevelEnding`
(gamelogic prototypes 149936, 150347 and 104006) match 4,096 original cases.
The original functions run as selected unchanged prototypes in isolated Android;
the C implementation contains no interpreter. The corpus includes nil, false,
true, numeric zero and strings for boolean/presence predicates, signed-zero
timers, boundary timer signs and mutable callbacks. All 1,756 goal queries,
139 notifications and 663 script hooks match. Completion checks prevention and
literal eagle true before querying goals, then reads movement/timeout flags
after that callback. Cleared-goal notification sets its sent flag after the
callback; the before-ending hook sets its called flag before the callback and
subtracts elapsed time afterward on every call. The callback can replace that
timer or clear its called flag. The C boundary rejects unsupported nonfinite
arithmetic. Complete ending orchestration and bonus timing remain open.
Provenance is `analysis/level-flow-native.json`.

The remaining-bird bonus prefix of `updateLevelEnding` (gamelogic prototype
101384, instructions 0..81) matches 4,096 calls to the unchanged full original
function with subsequent delay/event branches held inactive and nonmutating
host callbacks. All 645 queries, 402 score insertions, 131 before-ending requests
and 514 bird-effect sequences match. Queries use the current counter, present
birds include false/true/zero/string keys, only literal eagle true suppresses
the score, and the 0.5-second timer reset also occurs for eagle. Missing birds
leave the negative timer running, causing repeated queries. The oracle checks
audio arguments and floating-score fields, including 0.9-second lifetime.
These are requested boundaries; playback, floating-score presentation and
later ending logic remain separate. Another comparison checks all 58 captured
post-completion score boundaries in frames 38..95. Given completion after frame
37, counter two, two remaining birds, a recovered 3.5-second initial timer and
10,000-point increment, C predicts the awards at frames 73 and 79 and score
31,380. It does not derive completion or the queue in that comparison, nor does
the capture expose the intermediate bonus timer. Provenance is
`analysis/level-bonus-native.json`.

The delay/event suffix of `updateLevelEnding` (prototype 101384, instructions
105..175) matches 4,096 calls to the full original function with the bonus
timer held positive. The C comparison includes the ordinary incomplete-level
timer prefix and skip gate around the independently authored suffix. It checks
movement-delay expiration at <= zero, literal no-wait true, first default-delay
assignment, strictly sub-one-second movement extensions while the counter is
below three, timeout assignment before the win query, and ordered event requests
with the original level/score/boolean completion payload. All 435 win queries,
868 event requests, 291 extensions and 345 initial delays match. Boolean
completion/win values and nonmutating host callbacks are controlled; general
payload types and event dispatch remain separate. Provenance is
`analysis/level-ending-native.json`.

`initLevelComplete` (gamelogic prototype 104369) matches 4,096 original calls and
25,598 ordered boundary snapshots, including 3,285 camera requests and 1,833
level hooks. The original selected prototype is unchanged. Its stripped debug
upvalue names prevent lua_setupvalue from addressing the retained slingshot
slot, so the oracle binds a globally rooted controlled table to only that fresh
closed nil upvalue through checked ARM32 closure/UpVal fields. No prototype or
instruction is changed. Cases exercise callback mutations to bird metadata,
remaining counter, bonus timer, eagle/hook flags, ending timer and completion
flags. C preserves notify/cancel before completion assignment, source counter
selection, nil-versus-present lookup, camera request after the 3.5 timer,
audio resolution/playback before ending timer selection, hook invocation and
the final completing flag. Original gain-one/nonlooping audio arguments are
checked. Runtime adapters remain separate. Provenance is
`analysis/level-complete-native.json`.

The ordinary removal branch of `MuseumMode.updateBird` (prototype 19137)
matches 4,096 original calls, including 2,120 definition queries, 622 squared-
speed queries, 103 camera-target queries and 939 removals. Specialty branches
are inactive. Timer presence, truthy metadata, literal shot true, strict speed
comparison, water fallback and signed-zero timers are covered. Outside bounds
removes directly; negative timers consult camera object identity first. A false
water timer is accepted as the fallback during an active water update, while
the original rejects false in the later inactive numeric comparison. That
invalid state is excluded from the supported C interface. Definition, squared
speed and camera results are controlled; actual destruction and specialties
remain separate. Provenance is `analysis/bird-retirement-native.json`.

The native basicBeginContact handler at analysis 0x329660 (ELF 0x319660)
retains the first deferred bird velocity or a strictly smaller later squared
speed. LLVM assembly confirms separate binary32 y*y + x*x evaluations and a
direct comparison; decompiler integer/VFP types are not authoritative. The C
selection matches 4,096 original-handler calls, including 3,600 callbacks,
3,592 proposals and 2,441 stored-vector changes. Cases include the finite
exponent range, signed zero, subnormal components, ties and squared overflow.
Original Lua/native object construction and cleanup run in the isolated oracle.
The resolved C begin-contact handler now also applies the callback guards,
supplied geometry and disable flags, then deferred velocity and glue responses
in native order. Both this 4,096-case corpus and the separate 4,096-case glue
corpus call that handler and match all observed fields, including 2,104 appended
pairs and 888 absent endpoint pairs. Failure checks retain preceding disable
and velocity effects. World geometry/object lookup, collider-specific routing,
gameplay callbacks and the full listener/session bridge remain open. The new
record is `analysis/input/game-contact-handler-native.json`. Native update assembly
applies proposals after each physics step's removeBlocks call, then clears the
pending flag; the flush helper composes the previously validated body setter.
This corpus does not directly compare full GameLua.update with pending state.
Debug sanitizers and Release pass. Provenance is
`analysis/deferred-velocity-native.json`.

Contact routing data separately matches 512 executions of original
setupCollisionTable and the three symmetric handler setters. All 442,368
post-setup/update tokens agree, including opaque retained entries. Setup leaves
all pre-solve slots and the bird/bubble end handler untouched; that pair's begin
handler is special. Eligibility and routing remain independent. A read-only
capture of all 432 live delegate selections matches native initialization.
The C table stores semantic tokens, without the original delegate ABI or actual
handler effects. Debug/Release, Windows compilation and formatting pass.
Provenance is `analysis/input/game-contact-routes-native.json`.

The basic end-contact forwarding boundary matches another 4,096 original calls:
3,616 two-name onEndContact callbacks after fixture/script-table checks. It does
not consult the begin-disable flag, use geometry, consume responses or change
contact/pending-velocity state. All 36,864 output words and a repeated native
capture agree. Seven focused Debug/Release checks, Windows compilation and
formatting pass. Callback gameplay effects and live listener integration remain
open. Provenance is `analysis/input/game-contact-end-native.json`.

Outer event dispatch matches 4,096 native cases and all 20,480 output words,
covering all 432 ordered collider-pair/stage combinations. The original invokes
223 begin, 283 end and 276 pre-solve callbacks. All stages require both fixture
objects; only begin consults the disable byte. Empty handlers skip, collider
order is retained, and contact/previous-manifold identities pass unchanged.
Script-table checks belong to the selected basic handlers. Nine focused
Debug/Release checks, Windows compilation, formatting and repeated native captures
pass. The C interface uses semantic routing tokens and opaque caller contact
identities; it does not reproduce the C++ delegate ABI. World-listener ownership
and gameplay effects remain open. Provenance is
`analysis/input/game-contact-dispatch-native.json`.

The world-listener adapter composes those original routing/basic-handler
boundaries with actual physics geometry. A combined oracle runs 64 independent
two-circle original-engine worlds for 16 fixed steps, then separates the bodies
with a zero-duration step and destroys the dynamic body. Original GameLua
handlers execute through original Lua state/table/object boundaries; authored
callbacks control the disable, velocity and glue responses. All 73,141 words
match C across 1,088 steps: 22 begin events, 29 end events, 897 post-solve reports,
body/mass/sweep state, contact flags/manifolds/impulses, ordered callback geometry
and retained queue/pending state. The second native capture is identical.
Pending velocities and glue pairs deliberately remain unconsumed here; this
does not compare the complete GameLua update loop. Sixteen focused Debug and
Release checks pass, along with Windows compilation and seven earlier native
world corpora in both builds. The adapter forwards prior filter/post/destruction
contexts, refuses unsafe close/reentry, and stops the world on host failure.
Sensor zero-point geometry and missing special handlers fail explicitly;
gameplay effects and level/session scheduling remain separate work. Provenance
is `analysis/input/game-contact-world-native.json`.

`AbClassicContacts` now binds that listener to the classic loader's stable
fixture userdata and actual names/collider metadata, retaining the existing
level collision filter. The caller supplies script-table membership and pending
velocity storage explicitly. Named responses resolve to those actual objects
and bodies; per-object flushing remains an explicit post-removal controller
operation. C owner tests exercise missing/present script tables, response
retention, static/dynamic flush behavior, end and special callbacks, borrowed
host contexts, unsafe-close rejection and listener restoration. All ten focused
Debug/Release checks, Windows compilation and formatting pass. This establishes
adapter behavior and component composition, rather than a complete native
level/session comparison. Provenance is `analysis/input/classic-contacts.json`.

Classic break/damage scoring matches 4,096 pairs executed by the original
selected functions at gamelogic prototype offsets 131103 and 131408. The
merged definition's non-nil `destroyedScoreInc` selects the base definition's
field, not the merged value. False, numeric zero and strings all count as
present. A missing selected base value returns nil; otherwise a truthy
`levelGoal` selects the goal increment only when no merged override exists.
The corpus includes 3,276 override selections, 1,640 absent base values and
492 goal selections. Damage scoring floors the float input and then multiplies
by binary32 ten, preserving signed zero. Cases span subnormals, signed zero,
integer boundaries and large finite values. Numeric/absent base values are
supported; arbitrary Boolean/string base results and nonfinite numeric inputs
are outside the C contract. Museum mode assigns these classic helpers as its
scoring callbacks. Accumulation, floating scores and destruction remain open.
Provenance is `analysis/score-native.json`.

MuseumCollisions' legacy non-bird function at bytecode prototype offset 2030
has a first-party C calculation. It compares the difference of linear momenta,
scales its length by binary32 0.1, applies ordered strength/defence checks, and
floors the sum passed to the block-collision boundary. Its original vector and
class helpers execute only in the Android oracle, with controlled metadata
getters and recorded boundary callbacks. All 4,096 cases match, including 309
dead-block insertions and 12,288 calls. Ground/static is a name/collider rule,
not the physics body type; `noCollisionDamage` on one object suppresses damage
to its counterpart. Native entry into Lua occurs in basic begin-contact
handling at analysis `0x329660`. This calculation does not establish complete
bird, joint, destruction, score or callback policy. Private provenance is
`analysis/block-damage-native.json`.

The bird helper at MuseumCollisions prototype 280 also has a C calculation.
All 4,096 original-Lua cases match: 1,275 lethal hits disable contact, 642
request immediate legacy velocity changes, 633 return deferred velocity data,
and 8,834 ordered boundary calls retain their numeric payloads. The joint
boundary receives force before the damage multiplier; target defence comes
from its script getter, and its immunity flag is consulted only when the bird
has a script object. Material/modifier lookup is supplied as explicit inputs.
The helper corpus calls it with eight explicit arguments. Private provenance is
`analysis/bird-damage-native.json`.

An additional oracle calls the original enclosing `onCollision` directly with
its seven native boundary arguments, without replacing its upvalues. All 4,096
cases match the primary C dispatch and arithmetic: 1,024 cases per single-bird
order, 2,048 bird/eagle pairs, 711 lethal hits and 6,493 boundary calls. Typed
callback records confirm that single-bird calls forward the incoming point as
the callback normal and nil in the final point slots; the enclosing function
supplies six arguments to an eight-parameter helper. Both-bird calls retain all
four contact values. Reversing the object order does not negate them. Bird pairs
select greater squared speed (second on a tie), multiply that velocity by its
own mass and compute length divided by binary32 ten. They do not run strength
damage or joint breaking. Material lookup is controlled; glue and full callback
effects remain open. Debug sanitizer and Release comparisons pass. Provenance is
`analysis/bird-route-native.json`.

The glue helper at MuseumCollisions prototype 1534 now matches 4,096 executions
of that original zero-upvalue prototype in the original ARMv7 VM. All 142,456
words agree: 228 disable/pair responses, 114 egg grabs and 1,822 ordered
definition/object/velocity boundaries. The grabber guard uses Lua truthiness
before definition lookup; only exact bird/egg numeric colliders proceed. Eggs
write reciprocal fields before the positive-zero velocity request. The glued
flag and response names follow that request, including host name mutations and
plunger aliases with either argument. Earlier glued flags do not suppress a
new call. Repeated native captures agree; fourteen focused Debug/Release checks,
Windows compilation and formatting pass. Controlled C failures retain egg-field
prefixes while preserving output. The parent callback's primary damage and
dead-object gating, actual velocity/queue effects and session scheduling remain
separate work. Provenance is `analysis/input/museum-glue-native.json`.

The enclosing onCollision post-damage glue policy also matches 8,192 original
outer calls and all 491,756 recorded words. The original glue closure remains
unchanged. Two authored primary callbacks supply controlled disable decisions
and dead-map insertions through checked fresh closed upvalues, whose stripped
debug names prevent the target's setupvalue API from addressing those slots.
No original prototype or instruction is changed. All 140 ordered collider pairs
outside the separate two-bird branch are covered. Truthy dead entries produce
7,820 skips; otherwise first glue takes priority over second, selecting 71
helpers and generating nine pairs. Thirty-five selected helpers return false
and clear an earlier disable decision. A skipped helper preserves the existing
disable/pair response, while a selected helper replaces both fields even on a
false/nil result. The helper performs 59 host calls in the recorded suffixes.
Repeated captures match; seventeen focused Debug/Release checks, Windows
compilation and formatting pass. Primary numerical damage is deliberately
controlled in this corpus; full gameplay and live registry/session integration
remain open. Provenance is `analysis/input/museum-glue-route-native.json`.

The native bubbleBeginContact handler at ELF 0x31ae10 matches 4,096 original
executions through real original LuaState/LuaObject/LuaTable boundaries and
controlled fixture/body records. All 260,716 words agree: 2,273 trapped callbacks,
2,728 collision callbacks and 3,413 dead-bubble assignments. Only Boolean true
inBubble preserves the bubble; otherwise strength becomes positive zero,
killSilently becomes true and the original embedded object table enters the dead
map before shot is checked. A truthy shot overwrites pending velocity with current
body velocity times binary32 0.1, rather than selecting a minimum. Capture fields
precede trappedInBubble, and actual world geometry is calculated afterward.
onCollision always receives collider then bubble, the negated fixture-order
normal, the first world point and a fresh empty response table. Its disable,
velocity and glue responses are ignored. Cases reverse fixture order, span
signed-zero/subnormal/large finite velocities and mutate fields, pending state
or body transforms from the trapped callback. Native callback checks confirm
capture-field/dead-map timing. Repeated captures match; sixteen focused
Debug/Release checks, Windows compilation and formatting pass. C uses a typed
state projection; live registry insertion, trapped gameplay effects and special
listener/session wiring remain open. Provenance is
`analysis/input/bubble-contact-native.json`.

CollisionCallbacks' shared damage-state particle helper at prototype 151 matches
4,096 executions of the original zero-upvalue prototype. All 73,728 words agree:
3,413 coordinate conversions, 683 emission requests and 2,730 malformed non-nil
descriptor failures. A nil damageStateChangedParticles skips before position
or host access. Otherwise the raw definition's descriptor is cached, physics
position converts first, then its current name/amount fields are read. Cases
mutate those fields and replace the raw binding during conversion; the original
descriptor remains selected. The emission passes three positive-zero numeric
arguments and two false flags. Malformed Boolean/numeric/string descriptors fail
in the original VM after conversion; those original errors are caught wholly
inside its protected call, rather than raised through authored C callbacks.
Repeated captures match; nineteen focused Debug/Release checks, Windows
compilation and formatting pass. C host-error checks preserve earlier effects
and the default conversion composes the measured x20 helper. Actual emission,
particle simulation and full block/bird callback scheduling remain open.
Provenance is `analysis/input/damage-particles-native.json`.

The full blockCollision orchestrator at CollisionCallbacks prototype 6139 now
matches 4,096 original root-loaded calls with controlled named/script/resource
hosts. All 578,896 words and 33,564 ordered boundary calls agree. Both sensors
return immediately; a second-only sensor runs a swapped recursive invocation
before the outer invocation continues. Definition/block-event handlers reverse
the normal for the second object, while gameObjectScript:onBlockCollision methods
retain both incoming vectors. A current second sensor suppresses only its
object method and final contact sound. Truthy onBlockCollision level-hook results
suppress default score addition, while damage visuals/audio/particles continue.
Changed damageSprite stores the returned blink/smile values into blinkSprite
and smileSprite before sound/particle effects. The C particle host composes the
measured damage-state helper above. Final contact sounds are independent first
and eligible second requests, rather than a fallback choice. The playAudio
binding is cached before getAudioName, including when that query replaces it.
Cases mutate later definition handlers, sensor fields, sprite state, score totals,
thresholds and audio bindings from callbacks. Captured counts include 6,376
script events, 2,865 object methods, 695 particle emissions, 1,367 score appends
and 264 calls to replaced audio bindings. Repeated native captures match;
twenty-one focused Debug/Release checks, Windows compilation and formatting
pass. C host-failure checks preserve preceding callbacks/state and score-total
assignment before bounded append failure. Actual named/script/resource effects,
the gameplay registry/session and the separate bird callback remain open.
Provenance is `analysis/input/block-collision-native.json`.

The bird notification prefix at CollisionCallbacks prototype 489, instructions
0..155, matches 4,096 original root-loaded calls and all 229,140 words. The
original callback runs unchanged; related-hat fixtures cause its normal return
immediately after this prefix. That hat guard and later gameplay are outside
this comparison. There are 3,188 projectile skips, 164 missing required named
bindings and 5,542 ordered boundaries. Level-hook return values are ignored.
Named and block-event second calls negate the normal; game-object methods retain
incoming geometry. Each method requires the opposite object not be a sensor.
Only the second method reverses object-table arguments when that object's
controllable value is truthy. The 544 method calls include 272 with nil point
slots and 94 reversed second-object calls. Later fields and method bindings
reflect earlier callback mutations. Original VM binding failures retain their
preceding callback/state effects. Repeated captures match; twenty-three focused
Debug/Release checks, Windows compilation and formatting pass. Scoring, hats,
collision flags, bird abilities and actual script effects remain open.
Provenance is `analysis/input/bird-notify-native.json`.

Bird callback instructions 156..300 now have a separate C continuation for hats,
damage-score requests, floating-score metadata, per-bird totals, collision-score
appends, slingshot cooldown and optional achievement notification. All 395,864
words match in 4,096 original root-loaded calls. Either related hat suppresses
the remainder. Only the first shot object's hat receives positive-zero defence
and strength; an absent world hat skips both writes and continues. Positive
damage requests a score and floating metadata, using current first-object
position and scaling after the score query. Text uses the measured nine-digit
number conversion. Score accumulation uses the incoming first name, separately
from that object's current name; a missing score entry starts at positive zero.
Cooldown uses current shot/hasCollided and global truthiness after earlier
callbacks, without assigning collision flags. Cases include 1,108 related-hat
skips, 1,728 score appends, 466 cooldowns and 1,491 achievement calls, plus host
mutations of positions, scaling, score entries and later gating fields.

This corpus executes the unchanged original callback with inert notification
hosts. For continuing calls, a controlled missing flying-bird world lookup records
the next boundary, returns nil, and lets the original VM raise its own indexing
error after the authored C host returns. That expected cutoff is normalized only
by the comparison harness; it scopes the comparison before flying-bird handling
without modifying instructions or unwinding errors through authored C frames.
Dense append is a controlled ordinary table-insert boundary. Both captures match;
twenty-five focused Debug/Release checks, Windows compilation and formatting pass.
C failure controls preserve prior hat reset, callbacks and score-total assignment
before bounded append failure. Actual floating-score presentation, live registry
binding and the remaining bird/specialty/effect phases remain open.
Provenance is `analysis/input/bird-prelude-native.json`.

Original GameObject.lua delegateFunctionCall (prototype 1365) and its original
bird, block and destroy wrappers (1641, 1776, 1908) match 4,096 calls and all
574,352 words. The original root loads with a controlled class-factory boundary;
the method prototypes and instructions are unchanged. Original pairs/ipairs
traverse actual string-keyed groups and numeric member tables. C composes the
measured mutable string map with bounded group arrays: visit native group order,
cache each group's identity, and stop its inner sequence at the first nil slot.
Only truthy event bindings call; a truthy non-callable fails after the preceding
methods. Script return values are ignored. Native errors are required to be
call errors raised by original VM operations after authored hosts return.

The corpus has 1,029 bird-wrapper cases, 1,023 block-wrapper cases, 1,022 destroy
cases and 1,022 generic delegates. All 6,804 calls and final binding/member/map
states agree, including 1,390 non-callable failures and 4,543 nil arguments.
First callbacks clear/replace later methods, remove/replace a member, append a
member, remove an existing group, replace an existing group or clear their own
binding. Aliased members, nil gaps, insertion order/hash hints and trailing nil
arguments are covered. The current cached group keeps running after its map
binding is replaced. Repeated native captures match. Separate C composition
controls connect production bird/block collision notifications directly to the
adapters and behavior maps, checking reversed bird object arguments, nil point
slots and block other-object names. This composition control does not establish
a complete original gameplay session. All default Debug/Release/Windows targets
build; 31 Debug and 32 Release focused checks plus formatting pass. Actual
behavior effects, mixed group keys, insertion/reinsertion of group keys during
dispatch, metatables/custom iterators and gameplay registry ownership remain
open. Provenance is `analysis/input/game-behavior-native.json`.

Original GameObject.addBehavior (3268) and BehaviorUtils.createBehavior (337)
now match 2,048 constructed calls and 139,264 words. The real table.insert runs
after a controlled class:new callback. Match 1,537 constructor calls, 511
missing-class failures, 1,698 published empty groups, 768 group replacements
and 765 whole-map rebindings. The append retains its pre-factory group identity
and reads that group's length after factory mutations. Both captures repeat
exactly. All default Debug/Release/Windows builds, 57 Debug and 57 Release
related checks and formatting pass. Bound exhaustion controls preserve earlier
publication and constructor effects. The comparison uses dense groups, one
table-valued constructor result and an inactive original collector. A retained
active-collector run first differs in map iteration at case 1,101; automatic
collection scheduling remains open, with retained nil-key collection exposed
as a separate C map phase. This does not implement BehaviorBase constructors,
catalog class discovery or actual level behavior ownership. Provenance is
`analysis/input/game-behavior-add-native.json`.

The two legacy Museum bird classes now have first-party constructor projections
and trajectory/camera event methods. A complete original Class/BehaviorBase/
Museum-class graph with unchanged merge helpers matches 2,048 constructor/event
cases and 79,624 words. The 256 directed and 1,792 random cases compare 737
service calls, 578 ordered failures, 671 callback mutations, 57 keep-list calls
and eight boomerang sequence requests. Preserve post-ADD flying publication,
fresh physical/script/flying bindings, cleanup order, destroy's retained flying
field and specialty-before-launch getter order. Both captures repeat exactly;
all default Debug/Release/Windows builds, 59 Debug and 59 Release related checks
and formatting pass. Property getters and trajectory/camera resources remain
controlled services. The constructor scope is the observed empty configurable
parameter definitions and absent/false/empty overrides; arbitrary parameter
merging, initializer hooks and custom data metatables are outside it. A nil
keep-list element produces an empty list, and table allocation identity is not
part of the typed resource boundary. Dynamic level registry ownership, trajectory
rendering, boomerang sequence playback and full scheduling remain open. Private
provenance is `analysis/input/museum-bird-behavior-native.json`.

The fixed first-level behavior owner now composes these recovered components.
ObjectCreation.createObject (prototype 883, instructions 1248..1259) supplies
getObjectDefinition(name).behaviors to GameObject.createGameObject. MuseumMode
creates ground separately without that argument. The C owner uses the actual
owned catalog's constructor metadata, the live level definition/property graph,
addBehavior factory/append order and string-map delegate traversal. Initial
script.behaviors keeps a stable address; its groups and concrete instances stay
owned until explicit teardown. Close clears those projections without inventing
onDestroy events. Unknown classes/configurations and missing layouts reject.

Both independent cold first-level captures contain the complete original-created
script graphs. Their normalized initial graphs agree: 23 scripts, six Museum
instances, native group order and 18 name/gameObject/physicsObject fields with
the correct own-script/physical identities. The 604-byte normalized corpus has
SHA-256 `dc10928fe849c8c78d87c3c70c9c69e8b60e6f3ace952099ce0d88544ba3e19d`;
its generator verifies both source hashes and every native field before removing
process addresses. C construction matches it in 36,384 retained bytes. Ownership
controls also check exact memory bounds, failure cleanup, duplicate binding,
reversed authored order, retained earlier callbacks on later failure and fresh
flyingBird identity after service mutation.

The first-level camera replay now receives onLaunch through this actual owned
registry, reaches the real C camera sequence service, then publishes the first
launchVelocity vector. Both 25-boundary camera/world comparisons remain exact.
Trajectory follow-list creation now uses the actual C implementation described
below. Point rendering,
collision/event scheduling, dynamic insertion/removal, body-creation/level-hook
interleaving and a complete interactive app remain open. All default Debug,
Release and Windows builds, 61 strict Debug and 61 Release related checks and
formatting pass. Private provenance is
`analysis/input/classic-level-behaviors-native.json`.

Ordinary trajectory following now matches the complete original TrajectoryRenderer
root and unchanged vector/deep-copy helpers, with the real native physics-to-world
transform. Across 256 independent constructed sequences and 8,192 operations,
all 213,192 words agree: 9,360 track snapshots, 3,099 emitted points, 1,525
nonzero angles and 475 ordered missing-create failures. Both captures repeat
exactly. C preserves duplicate entries, last-match removal and the original
absent-object removal of the final entry. Adding a world-coordinate track still
seeds its position through physics-to-world conversion. Updates use strict
sqrt(1000) spacing, advance the sprite before creation and retain the old position
on creation failure. Ordinary points use layer two. The directed prefix includes
the spacing threshold and both adjacent binary32 values, signed zero, coordinate
flags and custom/default sprite cycles.

The native observer reads the original stripped closure's internal state through
the pinned ARM closure/upvalue layout, copying its table TValue to a reserved
stack slot. Original lua_getupvalue requires absent debug-name metadata; no
original instruction, prototype, closure or upvalue is modified. Renderer
creation is a controlled service that supplies the expected container and
records the completed position/angle/attachment. Numeric scene IDs and renderer
registry/disposal are outside this follow-list proof. C supplies a typed emission
boundary; actual rendering, special trails, puffs, sparse/mixed sprite lists,
nonfinite positions and full-frame/session integration remain open. All default
Debug/Release/Windows builds, 63 strict Debug and 63 Release related checks and
formatting pass. Provenance is `analysis/input/trajectory-follow-native.json`.

The level property owner now publishes raw cached x/y alongside its canonical
pose at initial construction, successful setPosition and the resolved
updateObjectProperties phase. Missing/removed bodies retain their no-op behavior.
Position writes target the owned physical table directly, even after script
physicsObject rebinding. Its fixed field capacity is twelve, covering the
existing dimension/script fields, x/y, raw controllable and first launch vector.
ObjectCreation's non-nil definition/material controllable selection is preserved
without collapsing nil/false or other scalar values; ground omits that assignment.
All 23 initial/ready native controllable fields match the startup capture, by
object name. The retained startup and flight captures enumerate objects in
different orders; this scalar comparison makes no traversal-order claim.

Raw positions mirror the owned canonical pose, and the launch replay now calls
real C follow-list creation through the actual Museum behavior. The standalone
native trajectory proof covers that operation; actual whole-frame trajectory
timing and renderer lifecycle remain open. Both recorded camera/world runs,
all seven earlier replay profiles, 64 strict Debug and 64 Release related checks,
default Debug/Release/Windows builds and formatting pass. Provenance is
`analysis/input/classic-level-physical-fields-native.json`.

The ordinary first-level trajectory path is now compared through actual C
launch, collision and follow-update bindings. A resolved bird-collision host
runs after Museum damage/contact/velocity changes and before generic collided/
score publication. It derives the real world manifold and preserves the
original single-bird point-to-normal/absent-point argument mapping. The owned
GameObject dispatcher reaches the actual Museum method, which removes following,
keeps the flying bird's existing points and publishes flying=false. Ordinary
trajectory updates run after cached position publication. The broader collision
prefix, level/named effects and full outer frame remain separate.

Two fresh game processes now have complete read-only camera/physical/behavior/
trajectory recordings. Each C comparison matches all 25 boundaries, twelve
point positions/angles, 72 physics steps and four removals. The captures have
different timing/launch outcomes: the first matches 40 callbacks and score
10,070; the second matches 30 callbacks and score 9,350. Both remove the follow
entry during update twelve, before another point is emitted. Each comparison
adds 1,079 trajectory/flying-state words to the existing complete camera/world
projection. Initial cached positions, launch elapsed/velocity, queued poses,
logical state and resolved activity/specialty flags remain explicit inputs.

Native renderer IDs are normalized by first appearance for retained point
comparison; this does not compare scene-ID allocation or native renderer-map
iteration. C observes point creation in bounded stable storage and retains the
current bird's points at KEEP. Actual renderer allocation/disposal, GPU trail
drawing, special trails, wider callback effects and the interactive session
remain open. The standalone native emission proof separately covers sprite
selection and layer two. All default Debug/Release/Windows builds, 65 related
strict Debug and Release checks plus the separate cold-repeat check (66 distinct
checks per build), all seven older replay profiles and formatting pass. Private
provenance is `analysis/debugger/level-trajectory-native.json`.

The resolved ordinary LuaAssetRenderer sprite-state branch now has a first-party
C adapter. The complete original native draw method, actual LuaObject/LuaState
wrappers and controlled ordinary SpriteAsset/Sprite/image boundaries match all
184,320 words across 4,096 cases: 3,191 image calls, 905 invisible/empty-asset
skips and 1,601 alpha-enabled draws. Both native captures repeat exactly. Integer
resource pivots, signed rotations, independent axis/object/camera scales, world
positions and alpha values are compared along with all retained render-state
words and image rectangles. Inactive cases carry a NaN position and preserve
render state without reaching geometry. C also checks output preservation and
the inherited direct-vertices flag.

Positions are already in world units: add the negative camera top-left, then
divide each axis by object scale. Multiply object scale by camera zoom. Native
SpriteAsset pivot getters feed both origin bindings and the image's pivot anchor;
the existing rectangle primitive consumes the corresponding C state. Preserve
the angle and useAlpha ? alpha : 1 behavior. This adapter selects imageSize.use
false and drawString false, with ordinary finite geometry and nonzero object
scale. It does not resolve native property coercion, resources or scene IDs.
Constructor analysis also finds imageSize.w/h defaults while the custom-size
draw branch reads width/height; no alias or fix is assumed. Constructor execution,
custom sizes, strings/composites and actual trail GPU submission remain open.
All default Debug/Release/Windows builds, eight strict Debug and eight Release
focused rendering checks and formatting pass. Provenance is
`analysis/input/lua-asset-sprite-native.json`.

Scene.newId at ELF 0x2d52e8 increments one application-wide 32-bit counter. The
first-party counter preserves signed wraparound and the original binary32 numeric
return, including aliases beyond exact float integer range. All 16,384 words in
4,096 native cases match and repeat exactly. The native Lua integer-to-number API
supplies the numeric observer; complete GameLua renderer construction is separate.
Provenance is `analysis/input/scene-id-native.json`.

Complete original GameLua.createLuaAssetRenderer at ELF 0x3213b4 now executes
LuaAssetRenderer/RenderableObject/SpriteAsset/LuaObject construction, scene
registration and numeric return in 4,096 controlled cases. Scene.clear also
executes the actual destructors. Only ordinary resource lookup returns a prepared
Sprite identity; no original code or class/prototype/upvalue is patched. All
237,568 words match the typed C renderer projection and repeat exactly. Default
position/scale/image-size tables are distinct, unrelated container fields remain
unchanged, and native ID/layer conversion covers wraparound, fractions, signed
zero, saturation and nonfinite layers. imageSize has use/w/h defaults with absent
width/height; the C custom-size path rejects explicitly. The script setSprite
function has a C method; no general table/metatable/function VM is introduced.

The owned first-level replay uses this constructor before publishing point
position/angle. Its exported GPU input is byte-identical to the earlier explicit
pose composition. All default Debug/Release/Windows builds, 19 strict Debug and
19 Release focused checks, seven older replay profiles and formatting pass.
Native registry loading, composites, missing-name logging, arbitrary argument
coercion/property lookup, destructor callback order and complete trajectory scene
lifetime remain separate. Provenance is
`analysis/input/lua-asset-renderer-native.json`.

The bounded scene membership component preserves ascending signed layers and
insertion order within each layer. Native Scene constructor, map/vector insertion,
Scene.remove(int/object), GameLua.disposeLuaAssetRenderer(float), and final native
Scene.clear execute in a controlled fixture. All 297,967 words across 12,288
operations match, including duplicate IDs/objects, reference counts, reinsertion,
first-match removal and survivor order. Float disposal truncates and saturates;
NaN selects zero, and infinities select the corresponding signed endpoint. Both
captures repeat exactly. The C projection borrows object identities; actual
renderer constructors/destructors, empty native layer nodes and clear-time
destruction order remain separate. Provenance is
`analysis/input/scene-membership-native.json`.

The ordinary sprite adapter now caches provider-resolved regions, UVs, texture
IDs and explicit image alpha policy, then composes its state through the recovered
image rectangle/material path into shared clip-coordinate commands. Missing names
produce empty assets; unsupported composites and unknown alpha policy reject.
Resource binding happens outside drawing. Both cold first-level trajectory replays
bind actual APK trail resources, retain twelve points and normalized C scene IDs,
and produce 136/134 visible point commands over their 25 boundaries. Explicit
ordinary renderer poses supply scale/visibility; actual constructor/property
lookup, global ID seed and interleaving with other scene members remain open.

A private exported frame contains seven visible point commands and three decoded
textures. First-party Metal and first-party GLES2 each repeat exactly. Their RGB
outputs agree at 153,532 of 153,600 pixels, with 68 differing pixels, maximum channel
error 13 and mean channel error 0.00177734375. There are 106 nonblack Metal pixels.
This isolates trail submission; no original APK GPU pixels or complete gameplay
frame are compared. The backend raster difference remains open. All default
Debug/Release/Windows builds, 17 strict Debug and 17 Release focused checks, seven
older replay profiles and formatting pass. Provenance is
`analysis/input/trail-binding-native.json` and
`analysis/input/trajectory-points-gpu-comparison.json`.

The shared classic render-scene owner now owns initial ordinary renderer state,
stable extra typed Lua renderer slots and one layer-ordered membership sequence.
Application IDs remain borrowed from a counter that outlives scene close. Mutation
updates a parallel dispatch route; ordinary frame drawing visits that cached route
without allocation or resource lookup. Removal detaches membership while keeping
borrowed extra property slots stable until close. Resource loading, script GC and
dynamic game objects remain separate.

A new cold original recording captures complete Scene layer vectors, actual
GameObjectRenderer cached poses/scales/visibility and LuaAssetRenderer properties
at the launch boundary and 24 update returns. All 9,122 comparison words match:
756 member snapshots, initial IDs 1–22, point IDs 23–34, global counter 22–34,
two removals and surviving order. The C simulation also matches 72 steps,
38 collision callbacks, twelve points and score 8,640. The replay supplies 208
words of measured starting render history/current caches because its earlier
settling loop advances physics alone. Queued controller poses remain measured
inputs and execute the recovered pose-reset boundary; settled body transforms
and broadphase state are not replaced. Complete startup/controller scheduling
remains open. The capture reads at most 271 selected camera/scene pages per
snapshot; no original VM calls or behavioral patches occur.

Actual APK atlas checks exercise owner allocation limits and exact storage budget,
extra-slot exhaustion, retained removed slots, layer routing, numeric removal,
resource failure ID effects, draw rollback and absence of draw-time lookups.
All default Debug/Release/Windows builds, 24 strict Debug and 24 Release related
checks, seven older replay profiles and formatting pass. A diagnostic mixed frame
contains 27 commands and three textures. First-party Metal and GLES2 each repeat
exactly; RGB differs at 2,913 of 153,600 pixels, maximum channel error 95 and mean
channel error 0.04119140625. Original GPU pixels, current sprite/animation choice,
themes/HUD, remaining renderer classes and complete trajectory lifetime are not
compared. Provenance is `analysis/debugger/level-scene-native.json` and
`analysis/debugger/scene-owner-gpu-comparison.json`.

Museum ObjectFunctions.getObjectProperty and setObjectProperty in
gamefeatures/objects/ObjectProperties.lua (prototypes 155 and 511) now match
4,096 original root-loaded cases and all 194,572 words. An absent object or
falsy physicsObject returns nil before a definition query. A truthy first
definition result is required before consulting even an existing physical
property. The first truthy physical, freshly queried definition, or material
property wins; false falls through, while numbers including zero and nonfinite
payloads remain truthy. Material fallback caches the materials binding before a
third definition query, then reads that definition's materialName. Names,
fields and function bindings are read at their original boundaries. A replaced
materials binding does not change the cached table, while mutations inside that
cached table remain visible. The setter directly writes the current physical
table, with no definition query or truthiness guard.

The corpus contains 4,604 gets, 3,199 sets, 3,220 definition queries, 318 third
queries and 3,984 original VM indexing failures. It includes 145 materials-binding
replacements, 145 cached-material field mutations and 146 third queries with an
initially absent catalog. Successful numeric returns include ten infinities,
six NaNs and three signed zeros with matching bits. Callback prefixes, mutable
state and results all agree; the oracle rejects errors other than original VM
indexing errors after authored hosts return. Both captures match. Thirty-three
Debug and 34 Release focused checks, Windows compilation and formatting pass.
Separate C controls cover early gates, output preservation, bounded setter
failure and retaining a newly assigned nil key. The C setter changes caller-owned
entry storage without modeling table hash iteration. GameObject/ModeSystem mode
selection, live object binding, metatables and non-data property types remain
separate. Provenance is `analysis/input/object-property-access-native.json`.

The underlying gamelogic.getObject (prototype 149452, five instructions and no
upvalues) now matches 77,824 words across 4,096 original calls, including 420
matching indexing failures. It directly indexes objects.world, without logging
or a fallback. Mixed ordinary keys include numbers, Booleans, strings with NUL
and UTF-8, and table identities; nil and NaN keys miss. Returned scalar/table
values and prior output on failure agree. Both captures match. Default Debug,
Release and Windows builds, 49 Debug and 49 Release related checks and formatting
pass. The combined camera/property/world oracle also invokes this original
helper at all 144 target lookups and preserves every byte of its retained trace.
The C level-camera owner uses that lookup before resolving the flyingBird alias;
named world objects take precedence. Private provenance is
`analysis/input/object-world-lookup-native.json`.

The real definition provider, gamelogic.getObjectDefinition (prototype 149536),
now matches 78,424 words across 4,096 original executions, with 1,329 matching
index/call failures and 50 diagnostics. The complete unchanged zero-upvalue
function executes in the APK's VM. It reads objects.world[name], prints the
exact missing-name bytes and returns nil when absent, otherwise captures
blockTable.blocks before rereading the world entry's definition key. Returned
values include nil and false. Ordinary scalar and table-identity keys retain
original equality: opposite zero signs match, NaNs miss, distinct tables miss.
Names include embedded NUL and UTF-8. Diagnostic callbacks can replace objects
and reject after that replacement; prior effects survive failure. The probe's
rejecting boundary tail-calls the original error function to avoid unwinding
through a freestanding C frame. No original instructions are patched. Both
captures match; default Debug/Release/Windows builds, focused checks and
formatting pass. Live GameObject/mode/session ownership remains separate.
Provenance is `analysis/input/object-definition-lookup-native.json`.

The level owner now stores a bounded property projection for each created body:
physical name/definition/dimensions, a separate script table with cached name,
physicsObject and empty behaviors, and the physical gameObjectScript identity.
The live script-world registry is independently removable. The first-party
getter composes definition lookup and Museum property precedence against the
actual catalog; physical name writes leave the cached script name and world key
unchanged. The separate owned-object prediction entry point reads numeric radius
through this path, then uses the real named physics query. It preserves all
binary32 radius values and rejects unsupported nonnumeric values when reached.
Other physical fields and generic script/behavior ownership are not supplied by
this projection.

A native composition executes the complete unchanged zero-upvalue original
GameObject.init/get (1030/4009), ModeSystem.getObjectProperty (3009),
MuseumMode.getObjectProperty (18612), the whole ObjectFunctions resource, and
getObjectDefinition. All 2,048 cases and 61,440 words match, including 513
original indexing failures and 682 checked diagnostic calls. Cases cover plain
physical/definition/material precedence, arbitrary returned scalar/table values,
nonfinite dimension/radius bits, physical renaming, cached script names,
world erasure and nil physicsObject. Original methods remain unpatched;
diagnostics assert the actual definition caller's prototype, instructions and
PC9. Both captures match. C controls cover bounded writes and separate physical
versus script-world removal. The owned startup getter returns the captured
width/height/radius words for all 23 initial objects. All seven existing native
replay profiles remain unchanged. Default Debug/Release/Windows builds,
53 Debug and 68 Release related checks and formatting pass. Full behavior
registration, dynamic creation, general mode selection and live launch/update
scheduling remain open. Provenance is `analysis/input/level-properties-native.json`.

Bird callback instructions 301..367 now match 4,096 original root-loaded calls
and all 107,568 words. This collisionFunction channel applies when the flying
world object's isMightyBait or isMightyEagle value is truthy. World metadata is
resolved before matching incoming contact names. A matching first/second flying
name requires the opposite object not be a sensor; the call receives
collisionFunction(flyingName, otherIncomingName), without a receiver. Eligible
nil, false, numeric and table bindings all fail as non-callables; an absent
flying world object fails earlier. Script return values are ignored. All 304
functions, state mutations and 1,084 phase failures agree.

The oracle leaves original instructions unchanged. Inert notification hosts,
zero damage, absent hats/cooldown/achievement hooks, and nil shot/controllable
fields isolate this channel. Continuing calls terminate at either the first
later flying-bird definition query returning nil (241 cases), or original
LEN of absent flyingGrenades (2,771 cases). Those later cutoff errors are
normalized by the comparison harness, independently of real phase failures.
The configured intervening branches have no effects; native checks also require
both hasCollided fields remain nil. These controls bound the comparison and do
not establish later sprite/ability/physics behavior. Callback cases change
sensors, flying binding/name, collisionFunction and mighty flags. Both captures
match; 35 Debug and 36 Release focused checks, Windows compilation and formatting
pass. Actual Mighty Bait/Eagle effects, live owner binding, collision flags,
sprites and remaining bird abilities/effects are open. Provenance is
`analysis/input/bird-mighty-native.json`.

A further identical mighty-phase capture checks all 8,737 original caller
instructions through a read-only ARM32 Lua frame guard. Closure/prototype/code
fingerprints and native getstack/getinfo layouts are validated before requiring
the two initial definition PCs (44/48), later definition cutoff (522), and two
collisionFunction call PCs (345/367). This confirms the scoped host locations
without changing original VM state or instructions; the duplicate trace was
pruned after its hash matched.

Bird callback instructions 368..499 now match 4,096 original root-loaded calls
and all 313,856 words. Each eligible shot object sets hasCollided to literal
true before querying its current name's definition. Both sides require the
current second object's sensor be falsy. A non-nil spriteCollision and falsy
dontUpdateDamageSprite select further checks: GRENADE requires current specialty
availability and flying-bird pointer identity with the cached contact object;
other specialties require the second object's frozenSolid be falsy. First-object
sensor/frozen state does not provide those gates. The current world entry is
resolved after definition callbacks, its sprite is assigned, then setSprite
receives current name and sprite. False and zero remain eligible non-nil sprite
values. A definition callback can redirect the world entry while the cached
contact object retains its separate state. Earlier callbacks can suppress or
alter later second-object work.

All 1,194 first and 693 second definition queries, 139 first and 86 second sprite
calls, final typed fields and 577 phase failures agree. Controlled hosts mutate
sensors, shot/update/frozen flags, later definitions, flying identity, specialty
availability, world membership and current names. The read-only original frame
guard requires exact caller instructions for every definition/sprite boundary.
Inert prior notifications, zero damage, empty hats and nil controllable fields
isolate this phase. Later flying definition at PC 522 returns nil in 942 cases;
2,577 other continuing calls reach the unique original LEN at PC 789 with absent
flyingGrenades. Those suffix errors scope the comparison independently of real
phase failures; original instructions remain unchanged. Both captures match;
37 Debug and 38 Release focused checks, Windows compilation and formatting pass.
C controls retain hasCollided before missing-host failure, sprite assignment
before setSprite failure and callback changes before rejection. Actual visual
resource effects, later specialties and full gameplay owner binding remain open.
Provenance is `analysis/input/bird-sprites-native.json`.

Bird callback instructions 500..736 now match 8,192 original root-loaded calls
and all 1,736,048 words. A falsy second sensor and matching current flying name
enable this phase. The definition's specialty is cached before the optional
onFlyingBirdCollided channel. That channel requires a falsy flying collision
flag and truthy definition hook key, prints the key and its current binding,
then resolves the binding again before passing the current flying value, both
incoming names, force, damage and normal. Point arguments are absent. The
second print can replace that binding or clear the flying value; 1,040 named
calls receive nil flying arguments, and 590 use a function changed after its
printed identity. Incoming contact data remains captured across host mutation.

Later decisions retain the cached specialty while reading current fields and
flying/world bindings. Ordinary defaults can clear specialty availability and
assign non-nil collision sprites. New BOMB handling uses a truthy explosionTimer
or binary32 1.5, assigns damageSprite/sprite, then enables availability after the
sprite callback. Numeric collision flags remain distinct from Boolean true for
that timer gate. BUBBLES sets a 1.5 pinkTimer only for falsy inBubble, nil pinkTimer
and collision unequal to Boolean true. All continuing specialties assign
collision=true before checking whether an attached hat's world object exists
and optionally calling releaseHat. Callback changes after those writes remain
visible; no effect is fabricated for a missing required host or world object.

All 3,925 definition queries, 2,090 first and 2,090 second prints, 2,082 named
functions, 1,108 sprite calls, 780 releases and 1,589 phase failures agree.
The read-only frame guard checks 28,459 original caller instructions, including
the two inert prefix definitions per case. Nil shots/controllable fields, nonpositive
damage and inactive mighty metadata isolate this phase; after it, the unique
original LEN of absent flyingGrenades ends continuing calls before later
effects. Only that suffix error is normalized. Named/sprite/hat implementations
are controlled hosts, with deliberately ignored script return values. Repeated
captures match, all default Debug/Release/Windows targets build, and 39 Debug
sanitizer/40 Release focused checks plus formatting pass. Separate C controls preserve
timer/sprite writes before failure, late collision-flag placement, named callback
effects and incoming argument snapshots. Actual ability physics, named/hat/
resource implementations, later bird effects and owned live session binding
remain open. Provenance is `analysis/input/bird-specialty-native.json`.

Bird callback instructions 737..826 now match 4,096 original root-loaded calls
and all 1,040,232 words. Both boomerang checks and both shot/controllable collision
flag assignments use the current second sensor. Definition callbacks can change
later gates, specialty fields and grenade-list state before its length is read.
Only specialty BOOMERANG clears boomerangActive. The backward grenade loop uses
literal Boolean true for the opposite controllable gate, allowing numeric zero,
strings and tables to explode a matching entry. An exploded first incoming name
becomes locally nil; the cached object name and second local name remain intact.
Repeated first-name entries therefore differ from repeated second-name entries.
Earlier explosions remain when a subsequent entry returns from the callback.

All 2,130 definition boundaries, final typed fields, list aliases/mutations and
1,569 original failures agree. There are 3,120 newly assigned explode flags,
including 210 before an early return; 337 list appends and 554 first-definition
sensor mutations exercise live state. A return hook installed through the
original debug API records the original instruction and raw local name tags:
287 returns at PC 819, 260 at PC 825 and 1,980 at PC 973. There are 1,438 successful
calls with nil first local names; the second remains a string in every successful
call. The hook performs only reads after validating the original closure/code
fingerprint and recovered ARM32 frame layout. Each case is also executed with
the hook disabled, requiring identical status, final state and boundary traces.
The isolated VM's hook setting changes only for instrumentation; no original
instructions or input registers are patched.

Inert earlier hosts, empty hats/bird metadata, no flying specialty, zero contact
force/damage and a maximum threshold leave later effects inactive. A controlled
tail damage-sprite query returns the unchanged second sprite. Original function
returns are observed directly; there is no artificial suffix error for successful
calls. Missing definitions/list tables retain their preceding effects on failure.
Both captures match; all default Debug/Release/Windows targets build, with 41
Debug sanitizer and 42 Release focused checks plus formatting passing. C controls
also cover stable incoming snapshots, missing hosts/entries, output preservation
and prior collision flags before a missing list failure. Dense list length and
plain non-nil names scope this component. Explosion physics, arbitrary table
length/metatables, later effects and owned session binding remain open.
Provenance is `analysis/input/bird-projectiles-native.json`.

The final bird damage refresh at instructions 947..973 matches 4,096 original
full callback calls and all 295,768 words. It runs with zero contact damage;
only a truthy second sensor or literal Boolean-true controllable suppresses it.
The query receives the cached second object and current blockTable.blocks.
Later callback mutations of the sensor/controller do not repeat those gates.
Compare the returned sprite with current damageSprite after the query, then
write damageSprite/blinkSprite/smileSprite before calling the original private
damage-particle helper. No setSprite or damage sound is requested by this phase.
Numeric equality treats signed zeros as equal and NaNs as unequal; strings and
table identities retain their own comparison rules.

The oracle keeps earlier phases inert and executes the unchanged original
particle helper from the full callback's upvalue. Controlled coordinate/emission
hosts record its effects; the C continuation composes the existing first-party
damage-state particle implementation. There are 1,860 queries, 507 transforms,
112 emissions, 1,440 matching failures and 954 sprite changes retained before
failure. Cases include 199 NaN-valued sprite returns, 1,118 queries with numeric,
string or table controllers, and mutations of catalogs, descriptors, positions
and later fields. Query PC 959 is validated through the original frame guard.
Original VM errors occur after authored hosts return, without throwing through
authored C frames. C controls also cover missing hosts, preserved callback writes
on failure and inactive gates with absent hosts.

Repeated native captures match. All configured default Debug/Release/Windows
targets build; 531 Debug sanitizer and 598 Release regression tests pass. Native
window-control execution is excluded to avoid opening windows; Windows execution
is not claimed. Formatting and duplicate-public-definition checks pass. Actual
visual presentation, contact sound/particle selection and full owned session
binding remain open. Provenance is `analysis/input/bird-collision-damage-native.json`.

Bird contact effects at instructions 827..946 match 4,096 original full callbacks
and all 351,408 words. A truthy second sensor or locally nil first name suppresses
the phase. Cache birds[first] once; only immovable/staticGround targets with both
velocity fields non-nil derive momentum from original vLength, current mass and
division by ten. Strictly greater contact force or momentum enables target sound,
particle-property lookup, then bird sound. A non-nil target collisionSound can
select truthy damageSound when damage exceeds ten. False collision sounds still
reach getAudioName. Both requests cache playAudio before name resolution and use
false loop/channel two. Target volume is binary32 .7; bird volume reads mass after
earlier callbacks, scales its denominator only when damage exceeds one, then
uses original first-operand min/max comparisons. Cached bird identity survives
replacement of the map binding. Later sensor mutation does not repeat this gate.

All 1,757 audio-name queries, 1,718 audio requests, 767 get("particles") methods,
58 addParticlesNew calls and 869 original failures agree. There are 297 false
sound queries, 508 calls retaining an earlier audio binding after replacement,
401 calls to the replaced binding, and nine positive default-NaN volume words.
Numeric/Boolean particle descriptors remain truthy and pass unchanged; a missing
or non-table get result fails only after the property callback. Original caller
PC checks validate audio-name/audio instruction pairs, particle query 896 and
emission 906. Audio events normalize the two audio-call PCs after those guards;
they retain argument types/bits, binding identity and ordered state snapshots.

The oracle executes unchanged bird instructions, the separately recovered
original utils vLength prototype 9192 and original math library. Earlier score
hosts are inert with valid score tables; nil shot/controllable/flying metadata
and empty hats isolate prior phases. A grenade entry clears the first local name
for absent-name cases. The final damage query returns the unchanged sprite.
All successful cases return normally, without an artificial cutoff. Captured
boundaries provide controlled resource effects; they do not play audio or claim
actual resource presentation. C failure controls preserve previous callbacks,
cached audio selection and incoming argument snapshots. Both captures match;
all default Debug/Release/Windows targets build, with 45 Debug sanitizer and 46
Release focused checks plus formatting passing. Full callback composition,
actual effect/resource implementations and owned live session binding remain
open. Provenance is `analysis/input/bird-effects-native.json`.

The recovered phases now compose through one C bird callback. Its notification
prefix returns borrowed cached object/definition identities for continuation;
later world and definition queries remain fresh where the original requires
them. All notification corpus words still agree after that refactor. The
prelude can read an external canonical specialty-availability binding; the
composer binds that read to sprite/specialty state and restores the prior
internal pointer on every exit. This prevents stale availability copies across
cooldown and subsequent ability work. Incoming contact data is captured once.

C integration controls combine positive damage scoring, shot-hat reset,
cooldown, Mighty dispatch, both shot sprites, Bomb timer/sprite, hat release,
boomerang deactivation, collision flags, both contact sounds and final damage
refresh. Notification callbacks replace the second world/definition bindings:
fresh sprite work reaches the replacement world object, while later damage
uses the cached second object and contact sound uses the cached definition.
Separate controls verify prior writes before a missing later host, a grenade
return suppressing both final phases, a cleared first local name suppressing
only contact effects, projectile skips without continuation state, and restored
internal binding pointers on success/failure.

The composed callback also matches all 351,408 words in the existing original
4,096-call contact-effect projection. Its earlier hosts/phases are inert, as
documented above; this is not yet a coupled native validation of every active
phase in the same call. All default Debug/Release/Windows targets build, with
47 Debug sanitizer and 48 Release focused checks plus formatting passing.
Coupled native coverage follows below. Concrete resource effects and owned live
session binding remain open. Provenance is `analysis/input/bird-collision-composition.json`.

The composer additionally matches 256 unchanged original full callbacks with
multiple phases active. All 34,512 words of final typed object/global state and
ordered boundary-kind traces agree. The eight independently combined fixture
flags cover projectile skips, related hats, second sensors, first/second grenade
entries, a definition function cleared after hat release, absent grenade tables
and existing hasCollided fields. There are 128 projectile skips, 64 related-hat
returns and 40 matching protected VM failures: 16 late missing definition calls
and 24 length errors. Original instructions remain unchanged, and failures occur
after authored C hosts return. No artificial suffix cutoff is used.

Notifications replace both the second world entry and the definition registry
binding. Fresh sprite queries update the replacement world object in 32 cases;
the original cached second object remains distinct. Original contact sounds
retain the cached prefix definition. The trace includes 256 notifications, 384
definition queries, 64 score/floating requests and appends, 32 cooldowns/Mighty
calls/hat releases and 96 sprite calls. Six first-object grenade flags become
true; locally cleared names suppress contact effects while allowing final
second-object damage. Four damage refreshes update the cached second object,
with four coordinate conversions/emissions through the original private helper.
C composes its actual damage-state particle helper at that boundary. There are
four audio-name queries and four audio requests. Each direct original bird host
boundary, plus score append, validates its caller instruction: 1,036 checks.
Particle-helper boundaries validate arguments separately.

Repeated captures match; all default Debug/Release/Windows targets build, with
48 Debug sanitizer and 49 Release focused checks plus formatting passing. These
are controlled host effects with fixed contact values and selected specialties;
the individual phase corpora supply broader value/alias/mutation coverage.
Actual ability/hat/audio implementations, larger live gameplay replay and owned
session/registry lifecycle remain open. Provenance is
`analysis/input/bird-collision-native.json`.

Gamelogic prototype 107147 is the private notification helper captured by
removeBlocks at root PC 523, called at removal PC 233, and also used by the
public onObjectRemoved wrapper. Its 40 instructions now match 4,096 original
calls and all 159,904 words. Cache raw blockTable.blocks[object.definition],
then use its truthy onRemoved key to conditionally call the current truthy
environment binding. A missing/false binding skips; zero and table bindings
remain truthy and raise an original call error. Next read current levelName and
the current level script's optional onRemoveBlock. Both callbacks receive only
the physical object, without a receiver; all script return values are ignored.

The definition callback can change the level name, replace its script registry,
change a later handler or clear the catalog. The cached raw lookup is not repeated
after that callback. Native coverage includes 226 definition handlers, 122 level
handlers, 134 registry replacements, seven level calls after catalog clearing,
50 level calls with changed object definitions, and 16 failures retaining earlier
definition callback effects. Original indexing/call diagnostics classify all
3,487 failures. The standalone oracle retains the original zero-upvalue helper's
complete unmodified instructions. Its independent closure/prototype fingerprint
and recovered frame ABI validate all 348 callback PCs (18 or 38). Returning false
plus a table from controlled functions verifies ignored results. Plain tables
and pure binding/level lookups scope this component; repeated original reads are
retained in C and have additional focused controls.

Both native captures match; all default Debug/Release/Windows targets build, with
50 Debug sanitizer and 51 Release focused checks plus formatting passing. C host
failure checks retain earlier mutations. The enclosing dead-map/removal pass,
actual named/level handler effects and owned object lifecycle remain open.
Provenance is `analysis/input/object-removal-notify-native.json`.

The public onObjectRemoved wrapper, prototype 120975, now has its own complete
39-instruction comparison. All 4,096 original calls match 284,184 words,
including 1,739 successes and 2,357 original partial indexing/call failures.
The oracle checks 1,159 private-notification calls at PC 21, 850 exitedFunction
calls at PC 17 and 78 goal checks at PC 31. Its isolated fresh closed upvalue is
bound to a rooted controlled notification; original instructions, registers,
base library and VM remain unchanged. Callback returns include extra values
to verify the original ignored-result boundaries.

Capture objects.world[name] once. A falsy entry returns; truthy non-table values
fail when their controllable field is read. Controllable ordinary birds return
without clearing anything here. Mighty bait/eagle objects instead call their
current exitedFunction with the cached object as receiver and dt, then return.
For non-controllable objects, notify first and clear current world, current
dynamic and current levelGoals bindings in that order. Callback replacement
of those bindings remains visible. The goal check runs after all three clears;
58 truthy results copy current time/score into lastScoreTime/oldScore, while
20 falsy results preserve them. This path never clears deadBlocks or destroys
a physical body. Complete removeBird has its own cleanup and effects.

Both native captures are byte-identical. The C component owns no registry or
effect implementations, uses plain tables and bounded string-map nil writes,
and retains earlier mutations on failure. All default Debug/Release/Windows
targets build; 98 strict Debug ASan/UBSan and 115 Release affected checks,
formatting and diff checks pass. Actual exit/private/goal effects and owned
session integration remain open. Provenance is
`analysis/input/object-removed-native.json`.

Complete removeBird, gamelogic prototype 148346, has a separate 126-instruction
comparison. All 1,024 original calls match 432,872 words and 5,045 independently
checked callback boundaries, including 334 original partial indexing/call
failures. The corpus crosses script/hook/hat/bubble/sound/effect/particle/camera
and binding-mutation flags, then removes individual services and supplies bad
later table bindings. Original base/getfenv and all instructions run unchanged;
the physical and effect boundaries are controlled here. Its unused third
argument is supplied with different valid values. Both captures are identical.

Optional script:onDestroy precedes definition lookup and the optional named
onBirdRemoved hook. A present related hat receives zero defence/strength. Cache
the current name after these callbacks, then request script removal particles
and camera:onRemoveBird. Current/flying bird comparisons use that cached name
and object identity even after later mutations; clearing flyingBird also clears
specialty availability. Bubble sound uses another definition query and a truthy
override, otherwise bird_destroyed. Nil play-sound defaults true. A truthy
level effect supplies gain before getAudioName/playAudio; the playAudio binding
and its context are cached before name resolution. Seventy native calls keep
the original audio function despite its replacement during that callback.
After physical removal, clear current world/dynamic/birds/otherBirds in order.
Failure retains all earlier callbacks and table mutations; no score is awarded.

The results replay now routes its retiring bird through this C component and
the full physical/joint/teleporter removal function, then unbinds the actual
owned level slot. It retains bird tables across frames and uses the real parsed
definition/material property lookup for particle descriptors. Script receivers,
camera behavior, current/flying identities and effect gain remain explicit
controlled projection inputs; audio and presentation requests have no concrete
effects here. Both existing 96-frame native result profiles still match all
numeric/body/controller outputs, with five ordinary removals and one retiring
bird. The separate ordinary-removal counter still counts only the five blocks/
pigs. All seven existing native replay profiles pass under strict sanitizers.
No new full live-session or coupled native physical-removal capture is claimed.

All default Debug/Release/Windows targets build. One hundred strict Debug
ASan/UBSan and 117 Release affected checks, seven additional Debug native
replays, formatting and diff checks pass. Concrete script/camera/effect/audio
implementations, owned live-session binding and dynamic creation/reuse remain
open. Provenance is `analysis/input/bird-remove-native.json`.

The base LevelEffect methods now replace the replay's supplied removal gain
and nil destroyed-sound response. Original `scripts_common/level_effects/
LevelEffect.lua` prototype 512 returns one without reading its receiver/bird;
prototypes 569, 641 and 710 directly return the supplied definition's collision,
damage and destroyed sound fields. They do not fall back to materials or choose
an audio variant. All 64 original method calls match 576 words, with 43 successes
and 21 original non-table indexing failures. Include nil/false, both zero signs,
quiet NaN payloads, text and table-valued results; unused receiver/object values
also vary. Both captures match. The isolated oracle loads the original complete
class chunk with controlled require/class-container bootstrap, then invokes its
unchanged methods and checks original result counts.

The restricted first-level fixture selects this base-effect input and uses
actual parsed definition fields for destroyed sounds. Its audio-name and
playback services remain controlled. This does not recover factory/constructor
ownership or water-specific overrides; static factory analysis records the
objects.waterLevel/objects.water selection separately. All default Debug/Release/
Windows targets build; 102 strict Debug ASan/UBSan and 119 Release affected
checks, all seven additional Debug native replay profiles, formatting and diff
checks pass. Provenance is `analysis/input/level-effect-base-native.json`.

The bird-following camera's PhysicsQueries.predictBirdImpactPosition dependency
now has a complete numeric/callback implementation. Original prototype 199
contains 115 instructions and three nested helpers at 851/1014/1308. The oracle
loads the unchanged original Class, Vector2 and PhysicsQueries chunks plus the
original vectorCreate/vectorPerpendicular functions. Their metatables, vector
operators, normalization, sqrt, power and constructor behavior run unchanged;
raycast/radius/print boundaries are explicit controlled inputs. All 128 original
calls match 427,336 words and 35,302 checked boundaries. Strict fingerprints
check the nested 22-instruction ray tailcall at PC 19, root radius calls at
58/73 and final print at 109. Original call diagnostics verify all 53 failures.

Capture the origin and optional steps once; nil/false steps select ten. Velocity
and world gravity remain live across callbacks. The initial interval is 1/steps
and its lower bound is 1/(steps*8). Each iteration issues all three rays, with
two intervening radius lookups, then selects the first truthy ray. A hit replaces
the retained point/name and halves the interval without advancing time; misses
advance time and 200 consecutive misses stop the loop. A retained hit can remain
after later misses while returned time continues advancing. Print the total
query count before returning the retained point, name and current time.

The original position helper ignores its radius-offset argument: all three rays
use the same normalized perpendicular unit offset unless the controlled
callbacks change live metadata between them. The radius reads still occur.
Zero velocity has no normalization fallback and can produce NaN coordinates.
The corpus includes 21,134 rays, 4,361 rays with NaN coordinates, 14,093 radius
queries, 75 prints, 49 returned hits and eight zero-query completions. Callback
mutations replace velocity/gravity and initial object position; the origin stays
captured. Clearing later radius/raycast/print functions preserves prior effects
before an original call error. No physical query result is claimed for these
controlled NaN inputs.

Both captures are byte-identical. C uses bounded caller-owned state and an
explicit query budget, with separate controls for budget failure preserving
output and prior callbacks. Non-progressing original inputs are not presented
as completed native runs. Actual world raycasts, bird-following camera update
and owned integration remain open. All default Debug/Release/Windows targets
build; 104 strict Debug ASan/UBSan and 121 Release affected checks, formatting
and diff checks pass. Provenance is
`analysis/input/bird-impact-prediction-native.json`.

The initial removeBlocks traversal (prototype 131514, instructions 0..42)
matches 4,096 original calls and all 406,488 words. An initially nil objects.world
returns before pairs or deadBlocks access. A non-nil false/non-table world can
survive an empty dead table, but fails when indexed. Capture the iteration table
once; read current world/objects bindings per entry, and target the current
deadBlocks binding for deletion. Falsy world values remove the key before the
cached dead value is indexed. Numeric-zero world values remain truthy. Nil
strength skips; nonnumeric strength fails at comparison before rocket fields.
Strength <= 0, non-nil rocket and nil deathTimerFull call shootRocket(object.name),
then delete the current key. False rocket values are non-nil; false timers block
that branch. The ordinary path admits any remaining non-nil numeric strength,
including positive values and NaNs, without a second <= 0 requirement.

The C cursor composes the measured mutable string map, preserving the captured
iteration identity through current-binding changes. Rocket hosts can change
later world/object fields, replace maps, clear bindings or change existing dead
values. All 123 calls and final map/value states agree; eight calls run with the
replacement map bound, 75 names are non-strings and 23 are nil. There are 1,039
ordinary selections (313 positive-strength and 291 NaN-strength), 463 normal
completions and 2,594 matching phase failures. Eighty failures retain earlier
rocket effects. C controls cover host failure before key deletion and cursor
continuation after an enclosing removal deletes its selected key.

The complete original 479-instruction function remains unmodified in the oracle.
A controlled script:onDestroy at PC45 observes the first selected object; the
next definition host at PC48 returns nil and the original VM raises a call error
at the missing breaking-score function. Only this post-selection cutoff is
normalized. It occurs before the function's private upvalue is used. Calls with
no selection return normally or retain genuine index/comparison/pairs failures.
Read-only original frame checks validate all 2,201 rocket/selection/cutoff caller
instructions. No error is thrown through authored C frames. Both captures match;
all default Debug/Release/Windows targets build, with 52 Debug sanitizer and 53
Release focused checks plus formatting passing. Plain pinned string keys,
unchanged captured-table topology except deletion/value replacement and explicit
bounded current-map storage scope this component. Remaining ordinary destruction,
scoring, effects, spawning and owned object/session lifecycle remain open.
Provenance is `analysis/input/block-removal-cursor-native.json`.

Ordinary removal scoring (prototype 131514, instructions 43..159) matches all
624,803 words across 4,096 original executions. Required script:onDestroy gets
only its script receiver. Definition lookup uses the captured dead-map key;
breaking-score lookup uses the cached dead object. Both precede current
levelGoal reads. Truthy goals resolve any non-nil damageSound, including false,
and request volume one, false loop and no channel. The audio function is cached
before name resolution; 112 calls retain that binding through callback changes.

Goal score 5000 requests sprite metadata even with an absent spriteScore; other
goal scores use text. Non-goals use every non-nil spriteScore, including false,
or text when absent. Forward coordinates as raw values. Sprite lifetime is .9
with scaling times one; text lifetime is .6 with scaling times (.25 + score/3000),
preserving each binary32 operation. Numeric text uses the measured nine-digit
format. Coverage includes 228 sprite and 302 text requests, three goal-5000
requests with absent sprites, 192 NaN scales, 76 infinite scales and 20 negative
zero scales. Fifteen absent-score sprite requests reach the callback before a
later arithmetic failure; absent-score text fails before that callback.

Read the current destruction total after the floating callback, then assign the
sum before appending the cached increment. Callback changes to totals, array
bindings, score-table blocks, definitions, coordinates and required functions
remain visible at their original boundaries. There are 306 completions and
3,790 matching failures, including 224 after floating callbacks. C controls
retain total assignment through append failure, earlier callback effects through
host failure and the original entry key/object through source-entry mutation.

The oracle executes the unchanged complete 479-instruction function with its
original table.insert. A controlled script:get at PC164 observes successful
scoring, then an absent g_levelEffect produces an original protected index error.
Only that verified post-score cutoff is normalized; genuine phase call, index,
concatenation, arithmetic and array errors remain failures. All 6,381 authored
callback/cutoff boundaries validate the original caller frame/instruction.
No exception crosses authored C frames. Both captures match; all default Debug,
Release and Windows targets build, with 54 Debug sanitizer and 55 Release focused
checks plus formatting passing. The contract covers plain stable tables and
numeric-or-absent scores/scaling; later removal effects, native body/map removal,
spawning, actual host effects and owned session binding remain open.
Provenance is `analysis/input/block-removal-score-native.json`.

Removal effect preparation (prototype 131514, instructions 160..230) matches
all 696,989 words across 4,096 original phases. Query cached-object particles,
levelEffect:destroyedSound(object, cachedDefinition), then the object's current
particleAmount. Falsy amounts default to twelve; numeric zero remains zero.
Always request random(12,20), even when unused. Cached smokeBuff/flameBuff force
one; cached batBuff uses the returned random value without numeric validation.
There are 4,092 property calls, 1,533 destroyed-sound queries, 732 random draws,
141 successful default amounts, 75 zero amounts, 124 smoke/flame overrides and
68 bat overrides. Twenty-four completions retain cached particles after a
callback replaces their current binding.

The explosion condition is cachedDefinition.specialty == BOMB OR current
deadBlocks[key].specialty == BOMB. A BOMB definition short-circuits the specialty
lookup; a non-BOMB definition still indexes the current entry and can fail there.
Capture the current explosion object and function before optionally resolving
tnt_explodes for a falsy explosionSound. The 215 explosions include 91 selected
by definition alone, 113 by current entry alone, eleven by both and 39 using a
replacement object. Five preserve an earlier function after rebinding, and seven
keep the captured object through a name-query callback that clears the map.

After explosion effects, read the cached object's current killSilently. Falsy
permits any non-nil cached destroyed sound, including false. Resolve its audio
name after caching playAudio, then request .7 volume, false loop and channel
three. There are 257 name queries and 159 audio calls; 37 destroyed-sound queries
receive false and 23 retain earlier sounds through current-binding replacement.
Finally updateDestroyedBlocksValues gets the cached object. Script returns are
ignored. Thirty failures retain earlier explosion effects; 569 phases complete
and 3,527 retain their original failure boundary.

The oracle executes the complete unchanged original function. Controlled inert
scoring reaches this phase and sets the requested current dead-map binding.
At update PC230, read-only validated original frame/register inspection observes
cached particles and amount. Returning to the original function then reaches its
unbound private-notification upvalue at PC233; only that verified post-phase call
error is normalized. All 7,557 phase calls and 12,288 controlled prefix calls
validate their original caller instructions. Pinned API-created values identify
the 569 cached-register results without writing original registers or assuming
string-object layouts. Both captures match. All default Debug/Release/Windows
targets build; 56 Debug sanitizer and 57 Release focused checks plus formatting
pass. C controls retain earlier effects through injected update failure and the
captured key/object through source-entry mutation. Actual destroyedSound/random/
explosion/statistics effects, notifications, particle emission, native removal,
spawning and owned session binding remain open.
Provenance is `analysis/input/block-removal-effects-native.json`.

Removal notifications and particle routing (prototype 131514, instructions
231..273) match all 610,616 words across 4,096 original phases. Notify the cached
object first, then capture its current optional removeFunctions list. Ascending
ipairs stops only at nil; false/zero items fail as non-callables. Each function
gets just the object, with no receiver, and ignores script results. The list
binding can change without replacing the captured iterator; later writes to its
items remain visible. The corpus observes 3,070 notifications and 1,976 removal
functions, including 83 later calls after list rebinding, 123 after later-slot
writes and 147 complete four-function traversals. Two hundred one failures retain
earlier removal-function effects.

Read the cached object's current killSilently after all removal functions.
Truthy skips emission; otherwise query current script:get(useNewParticleTable).
Falsy requests legacy particles with the earlier cached particles/amount.
Truthy captures addParticlesNew before a fresh script:get(particles), indexes
its removal descriptor, then invokes the captured function. Both paths receive
the captured map key and two false flags. There are 1,478 property calls, 230 new
and 127 legacy requests; 84 new requests keep an earlier function through
rebinding, 67 run after its global binding is cleared, and 69 run after a property
callback changes killSilently to true. New requests forward 29 nil and 91 false
descriptors. The phase has 1,370 completions and 2,726 matching failures.

The oracle loads a fresh unchanged original function per case. Its fresh closed
nil notification upvalue receives a globally rooted controlled C host through
validated ARM32 fields; stripped upvalue names reject ordinary setupvalue. No
prototype, instruction or register is changed. Controlled inert earlier phases
set the current silent/property gates at update PC230. Original ipairs and all
subsequent phase instructions run normally. A controlled removeObject at PC288
observes success; absent levelGoals then produces an original protected index
error. Only this verified post-phase cutoff is normalized. All 6,881 phase calls
and 1,370 cutoff calls validate original caller instructions. Both captures
match; all default Debug/Release/Windows targets build, with 58 Debug sanitizer
and 59 Release focused checks plus formatting passing. C controls retain earlier
effects through host failure and capture entry/cached metadata before callbacks
mutate their source storage. Concrete notification/removal functions, particle
creation, native removal, spawning, mixed-key/metatable iteration and owned
session binding remain open.
Provenance is `analysis/input/block-removal-particles-native.json`.

The raw-catalog capture and native-removal boundary (prototype 131514,
instructions 274..288) match 234,932 words across 4,096 original phases. Resolve
blockTable.blocks[object.definition].spawn, then independently resolve deathSpawn,
then cache raw x/y before removeObject(captured map key). The merged scoring
definition does not supply these values. Non-table catalog/definition paths fail
before native removal; a missing removal function fails after the captures.
The 1,861 successful calls retain 928 earlier spawn values, 753 deathSpawn values,
778 x values and 790 y values through callback replacement. Coordinates remain
untyped values; 1,811 calls forward at least one non-number. Nine hundred thirty
three succeed after the callback clears the catalog binding, and the same count
after it clears its own removal binding. There are 2,235 matching failures.

The oracle reuses the validated inert preceding fixture and executes the complete
unchanged original function. At PC288, read-only validated caller/register access
observes cached R12..R15, identified against pinned API-created string/table
values. State after controlled removal mutations is captured before post-phase
cutoff inputs clear blockTable/levelGoals. The original VM then reports a protected
index/empty deathSpawn-loop error; only the verified post-boundary path is
normalized. No real native body removal or spawn execution is claimed. Both
captures match; the shared preceding-particle corpus was rebuilt and recaptured
unchanged. All default Debug/Release/Windows targets build, with 60 Debug sanitizer
and 61 Release focused checks plus formatting passing. C controls also preserve
callback effects on failure and capture the entry before source-storage changes.
Cleanup currently has C controls for ordered partial map clears and post-pig gift
reads; its original full-function replay, native removal, spawning and owned
lifecycle remain open.
Provenance is `analysis/input/block-removal-detach-native.json`.

Post-spawn removal cleanup (prototype 131514, instructions 451..475) matches
303,321 words across 4,096 original phases. Clear the captured key from current
levelGoals, objects.world, objects.dynamic and deadBlocks, in that order. A later
failure retains earlier clears: the corpus has 1,142 failures after a goal clear,
245 after a world clear, 206 after a dynamic clear and 215 after a dead-map clear.
Maps may begin with absent keys. The C implementation composes the measured
mutable string maps for goals/dead objects and bounded plain world/dynamic
tables; hash iteration of the latter and ownership remain separate.

Then current cached-object levelGoal truthiness selects removePig(captured key).
After that callback, current isGiftBox selects manager:collectGiftBox with current
levelName, x and y. There are 1,366 pig calls and 979 gift calls, including 203
with changed level names, 226 with changed coordinates, 140 nil level names and
962 with at least one non-number coordinate. Four hundred seventeen failures
retain earlier pig effects, and 394 complete after the pig callback clears the
objects binding. All script results are ignored.

The original complete function executes unchanged with controlled inert earlier
phases and nil spawn/deathSpawn. At the validated removeObject PC288 boundary,
the fixture installs the current cleanup bindings/fields. The original suffix
then returns normally in all 1,631 successful cases; no post-phase error is
normalized. Its 2,465 failures retain original index/call status. All 4,096 entry
boundaries and 2,345 pig/gift caller instructions/arguments are checked. Both
captures match; all default Debug/Release/Windows targets build, with 61 Debug
sanitizer and 62 Release focused checks plus formatting passing. Concrete pig/gift
effects, spawn execution, real native removal and owned session binding remain
open.
Provenance is `analysis/input/block-removal-cleanup-native.json`.

Grid/death spawning (prototype 131514, instructions 289..450) matches all 128,704
words across 2,048 original phases. Non-nil spawn selects the grid even when
false; deathSpawn uses truthiness. Raw target width/height are independently
queried before layout, although target height is unused. Columns are
max(1,floor(object.width/target.width)); rows are
max(1,ceil(spawn.amount/columns)). Preserve the recovered cell-size, signed
half-grid-offset and binary32 operation order. Loop bounds/layout are cached;
angle, name, position, spawn definition and block-table binding remain current
for each create. Normalize angles through promoted-double fmod and the float
pi/tau boundary; use the separately measured script sine/cosine. Create precedes
the grid rotation lookup and cached-angle call.

Death spawns cache the current types table before random(1,#types), then index
that cached table even if the callback replaces its binding. Generated names use
the captured dead-map key; positions use detached x/y times current scaleFactor.
After create, cache setRotation before random(), then multiply by float two and
current pi. Coverage includes empty and one-to-three-entry dense type lists,
nil chosen definitions forwarded to controlled create hosts, signed zeros,
fractional loop bounds, huge finite angles and callback changes to geometry,
names, definitions, type bindings, scale, pi and required hosts.

There are 1,726 grid and 1,063 death creates, 1,669 grid and 1,039 death rotations,
1,096 type draws and 1,047 unit draws. Result coordinates include 590 NaN,
nine infinite and 165 negative-zero components; 203 cases create grid objects
with zero target width and 197 with maximum-finite-magnitude source angles.
Three hundred forty-six death rotations retain the earlier function after the
random callback replaces it. Ninety-seven failures retain earlier create effects;
the phase has 1,319 completions and 729 matching failures. These are controlled
create/random hosts, not actual object construction or random-source parity
claims; the independent random engine has its own evidence.

The complete original function and standard original math library execute
unchanged with inert preceding fixtures. At removeObject PC288, current position
changes distinguish live grid coordinates from previously cached death-spawn
coordinates. A controlled post-phase levelGoals __newindex at PC452 observes
completion, then clearing objects produces a protected original index error.
Only that verified post-phase cutoff is normalized; the metatable is outside
the compared spawn phase. All 7,640 phase calls and 1,319 cutoff callers are
validated. Both captures match; all default Debug/Release/Windows targets build,
with 63 Debug sanitizer and 64 Release focused checks and formatting passing.
Broader frozen runs pass 551 sanitizer checks and 618 Release checks, excluding
the interactive Common window-controls test. C controls cover layout reuse,
cached positions/type tables/rotation hosts, earlier effects through failure,
instance exhaustion and absent spawns without unused math requirements. The
contract currently covers finite source numbers, counts <= 2^24-1, positive pi
through 512, dense type arrays and bounded disjoint name storage; sparse arrays,
metatables, nonfinite source values, actual creation/rotation and owned full-pass
binding remain open.
Provenance is `analysis/input/block-removal-spawn-native.json`.

The C removeBlocks composer now joins retained traversal, onDestroy/scoring,
effects, notifications/removal functions/particles, detach, spawn and cleanup.
It borrows each phase host rather than copying mutable bindings. Effects and
cleanup share the canonical removal state. Rocket handling stays in traversal;
ordinary metadata remains cached through native-removal boundaries. Nil world
and empty dead tables need no phase hosts. Missing later configuration retains
all earlier phases and entries; absent/falsy spawn paths need no spawn host.

C controls compose the actual first-party behavior dispatcher, raw/level removal
notification helper, breaking-score calculation, script RNG and mutable maps.
They cover a false non-nil rocket flag, goal score 5000 with absent score sprite,
numeric-zero particle amount, a later phase binding installed by a floating-score
callback, cached spawn metadata after catalog replacement during removal,
post-pig gift reads, retained score/behavior/notification effects on particle
failure and nil-world early return. The physical-removal control now invokes the
first-party removal core and physics world, verifying that successful removal
destroys the actual C body and that earlier particle failure preserves it.
Full native gameplay integration remains separate.

The existing unchanged-original 4,096-case cleanup corpus is also compared
through the complete C pass: all 303,321 words still match. Both implementations
use the same intentionally inert preceding fixtures, restore current bindings at
the native-removal boundary and then expose the same cleanup mutations/errors.
The captured iteration map survives cleanup of a different current dead-map
binding. This is a full C composition projected onto the original suffix; it
does not replace a coupled full-game replay. All default Debug/Release/Windows
targets build, with 65 Debug sanitizer and 66 Release focused checks plus
formatting passing. Actual native removal/creation, concrete gameplay effects,
dynamic membership/hash insertion and owned session binding remain open.
Provenance is `analysis/input/block-removal-pass-composition.json`.

The native removeObject physical prefix (entry ELF 0x31d48c, main path through
0x31d688 and its out-of-line branches) matches all 6,144 words across 256 original
calls. Real original GameObject constructors, managed string assignment, RB-node
allocation/erasure and world body destruction execute unchanged. A bootstrap
first verifies one physical deletion and a repeated missing-key call. Controlled
enclosing GameLua fields provide two objects, optional bird-list duplicates and
three rolling-sound cache nodes; no scene attachment, camera animation, joint
records or teleporters are supplied.

Missing physical keys leave world/cache/list unchanged. Bird/eagle colliders
remove only the first bird-list entry whose current physical name matches the
requested key; other collider types leave that list alone. The corpus has six
bird and eleven eagle prunes, all retaining a later duplicate. Nonempty rolling
sound names reset their cache entry to present positive zero, including prior
negative-zero/nonfinite values. There are 117 resets and 41 removed objects with
empty sound names. Among 171 physical removals and 85 missing names, 86 locked
removals still erase the physical object while the engine body remains. The C
core preserves that native locked behavior and never clears its stored body slot.

Assembly also establishes body destruction before optional camera close, then
renderable retain/scene removal/current detach, physical-map erasure/object
destruction and final retained-renderable release. C controls verify this order,
current versus retained handle changes, and balanced references on scene failure;
they do not establish actual native camera/scene effect parity. The composed C
removal pass now uses this core with the real first-party physics world. The
native corpus leaves later joint/teleporter paths empty; those suffixes must
still execute even when the physical key is absent. Both policy captures match;
all default Debug/Release/Windows targets build, with 67 Debug sanitizer and
68 Release focused checks plus formatting passing. Owned membership, camera/
renderable effects and the remaining native suffix stay open.
Provenance is `analysis/input/game-object-remove-native.json`.

The joint-record suffix of the same native removeObject (ELF 0x31d688 through
0x31db78, including out-of-line branches) now has 512 original cases with three
real allocated physical objects, four uniquely named joints and three removal
requests per case. Native constructors, joint creation, body destruction and
record erasure execute unchanged. The corpus matches 61,440 words, including
2,048 complete registry/world snapshots. Of 1,536 requests, 768 remove a physical
object and 768 encounter an absent key. There are 1,706 erased records and 341
destroyed engine joints; all 256 locked cases retain engine bodies and joints.
Creation locks independently produce null-engine immediate records.

Deferred records run before immediate records. A matching first endpoint marks
the current second physical object for delayed destruction, then assigns its
timer from current named metadata. A matching second endpoint only clears and
erases the record: it does not mark the first endpoint. Absent second objects
skip timer lookup. Immediate records only clear metadata and erase; this suffix
does not call engine joint destruction. Survivors keep their order. Repeated
removal requests still run the suffix after an absent physical lookup. The
captured states include 166 changed surviving-object delay/timer states, with
25 negative-zero, 45 infinite and 41 NaN final timer values retained exactly.

Assembly of LuaTable::getTable<std::string> (ELF 0x2f2130) shows that missing
named joint tables throw after the delayed-destruction flag is set, before timer
assignment or record erasure. C controls separately check this duplicate-name
partial failure and skipping missing metadata when the second physical object
is absent; native exception capture is still open. The typed C API borrows the
physical registry and requires the same world as the joint registry. It does
not own objects, coerce numeric strings, or implement the teleporter suffix.
Both native captures match. All default Debug/Release/Windows targets build;
80 Debug sanitizer and 81 Release focused checks plus formatting pass.
Provenance is `analysis/input/game-object-remove-joints-native.json`.

The teleporter suffix (main path ELF 0x31db78..0x31dcf8 and out-of-line RB-node
erasure) matches 33,792 words across 1,024 original cases and 4,096 snapshots.
Real original physical objects, base teleporters, PortalObjectTeleporters and
their map nodes are allocated, constructed and destroyed by native routines.
The portal constructor receives real bodies and a valid native LuaTable;
resource/particle pointers are null because this corpus never updates or plays
portal effects. The native constructor completes its ordinary velocity setup.
Only the portal's retention flag at byte 0x86 is a controlled state input; no
vtable, instruction or register is replaced. Original virtual getters and
destructors execute normally.

Simple teleporters always remove. Portals remain registered while byte 0x86 is
true. The physical object may already have been deleted; both joint and
teleporter suffixes still run. Clear the portal retention flag between requests
and a later missing-physical-key call destroys the teleporter and erases its
entry. Across 3,072 requests there are 1,536 physical deletions, 1,536 absent
physical lookups, 864 teleporter deletions, 480 retained-portal requests and 96
teleporter deletions after missing physical lookups. All 512 locked cases keep
engine bodies while still applying registry cleanup. Every object position has
768 instances each of absent, simple, removable portal and retained portal.

The C ab_game_object_remove function now composes the physical prefix, joint
suffix and teleporter suffix in order. C controls verify destruction before
teleporter map erasure and joint failure before teleporter cleanup, retaining
earlier physical deletion and delayed-destruction flags. The unchanged 512-case
joint corpus also passes through the full C function. Removal-pass controls use
the same function with real world, deferred-record and teleporter state; an
earlier particle failure retains all three. These controls do not establish
native portal effects or an owned playable session. Both captures match;
all default Debug/Release/Windows targets build, with 82 Debug sanitizer and
83 Release focused checks plus formatting passing. Portal update/effect policy,
actual scene/animation effects, exception capture and owned membership remain
open. Provenance is `analysis/input/game-object-remove-teleporters-native.json`.

The complete original removeBlocks prototype now executes with real native
GameLua::removeObject at its ordinary PC288 boundary, without a phase cutoff or
post-phase error normalization. All 512 calls return normally. Two original
heap-allocated physical objects and one original immediate/deferred joint are
constructed per case. The script dead/world/dynamic maps are distinct and
contain one ordinary removal; spawning and explosion/audio paths are absent.
Surrounding script hosts remain controlled inputs, including a deterministic
random result of seventeen and a notification callback bound to the checked
closed upvalue. Original instructions and registers remain unchanged.

The full C pass matches all 27,136 words and 6,848 ordered host boundaries.
Mutations toggle goal status during onDestroy, replace the score total and name
during floating-score creation, flip particle suppression/gift status during
notification, and change gift coordinates/level during pig removal. Final
scores, one score append, surviving physical/joint state, delayed-destruction
flags/timers and all four script map deletions match. Native boundaries check
the original caller PC and argument count; relevant callback arguments are also
checked. The C comparison uses its actual breaking-score helper and physics
world, then the complete physical/joint/teleporter removal orchestrator.

There are 256 goal removals, 320 gift callbacks, 128 legacy and 128 new particle
requests, 256 particle-suppressed removals and 256 changed names. All five joint
kinds and both endpoint directions occur; 48 surviving second endpoints acquire
glue destruction timers. All 256 locked cases preserve the engine body/joint
effects while still erasing physical and game joint records. Both captures
match, and the shared particle fixture also recaptures byte-for-byte identically
to its earlier 4,096-case corpus. All default Debug/Release/Windows targets
build; 83 Debug sanitizer and 84 Release focused checks plus formatting pass.
This establishes the coupled removal path with controlled hosts, not a full
GameLua.update or playable-session replay. Actual notification/particle/audio/
explosion/spawn effects, dynamic membership and owned session binding remain
open. Provenance is `analysis/input/block-removal-pass-native.json`.

Initial level slots now have an owned, bounded removal mask separate from engine
body existence. The mask is borrowed by force preparation and named queued-force
lookup, frame history/interpolation, ordinary post-physics publication/activity/
rolling, character goal/bird passes and scene pose/visual synchronization.
ab_classic_level_forget_object only marks the slot after physical-map removal;
it does not destroy bodies, clear script maps or perform joint/teleporter work.
It retains source indices and cached poses until level close. Explicit insertion
and body-slot reuse remain unsupported. Level transforms/publication on forgotten
slots leave cached values unchanged. A forgotten scene row cannot be made
visible through the ordinary setter.

C integration controls bind the complete verified removal function to a real
level's world and joints. Its object destructor marks the level slot only after
the physical map entry has been erased. After the first physics step, one
ordinary removal destroys its body and one locked-world removal keeps an engine
orphan; later steps and another frame still complete. Queued force targeting the
orphan is skipped, while its engine body remains in the world. Cached histories
and published properties of both removed objects stay unchanged. Independent
scene controls hide both missing and live orphan bodies, including after visual
synchronization; character controls skip removed goal and bird work without
reading invalid delta/bindings or changing timers. These are first-party
integration checks, not a new native update capture after deletion.

All default Debug/Release/Windows targets build. The full frozen check set passes
560 Debug ASan/UBSan tests (halt on error) and 627 Release tests, excluding only
the interactive Common window-control test; formatting and diff checks pass.
Earlier native numeric/resource/scene comparisons remain unchanged. A native
post-delete update comparison, owned physical-registry and scene destruction,
dynamic creation/reinsertion and full session binding remain open.
Provenance is `analysis/input/object-membership-integration.json`.

Two independent live level 1-1 captures now compare post-physics traversal after
actual native physical removal. Stop on the original update/render thread at
GameLua.update (ELF 0x323ec0), supply a bounded native-owned string fixture to
unchanged GameLua.removeObject (0x31d48c), and remove SmallPiglette_7. Both native
physical-map and world-body counts change from 23 to 22. With elapsed input
zero, capture the next original post-physics prefix through its root-update
boundary. Each capture visits exactly the 22 survivors in native name order,
including six original updateObjectProperties calls with checked arguments.
Original Lua property publication runs unchanged. Supplied elapsed data and
diagnostic native calls are explicit inputs; no implementation instructions
are replaced and no single stepping is used.

Each capture normalizes 797 numeric input/output words. The first-party loader
constructs its own 23-object level and post-physics owner, then the complete C
removal function destroys the named pig's body and its physical destructor
unbinds the owned level slot. Feed the retained native body/render/script
inputs to survivors and run the C post phase. Both captures match all seven
script fields, aggregate activity and final old-awake flags. The removed C
slot's cached pose and published properties remain unchanged. Compare native
shape/collider/coefficient and material-only rolling names to the first-party
APK loader, and check that surviving engine/render-history state is untouched.
The independent captures have different live inputs; they are not claimed to
be identical replays of a common initial frame.

This is a physical-map/post-prefix experiment. It leaves the original Lua
world and goal registries outside its scope; reloading that modified reference
hit an invalid object reference. Subsequent captures discard the isolated
process before its Lua root call, and a clean stock reference is relaunched
afterward. An earlier diagnostic allocation also required preserving the raw
ELF symbol's Thumb flag; the rejected attempt precedes physical removal and
is excluded from evidence. Both failures are retained in private logs. The
capture checks native removal and later membership, not camera/renderable
reference counts or complete Lua cleanup. The earlier complete removal-pass
corpus separately covers ordered script cleanup with controlled surrounding
hosts. Full root/session replay and actual scene/resource ownership remain open.

All default Debug/Release/Windows targets build. The affected set passes
96 Debug ASan/UBSan and 113 Release checks, plus both independent-input
comparisons in each configuration, formatting and diff checks. This follows
the full 560/627-check runtime checkpoint above; the later changes add and
refactor reference tests. Provenance is
`analysis/input/classic-removed-post-native.json`.

The existing first-level flight/impact replay now uses the actual composed
ordinary removal pass, including physical-object, joint-record and teleporter
cleanup. Its dead table survives across physics steps instead of being rebuilt
for each removal pass. Stable body tables expose the original parsed definition
and material lookups to the recovered property-access phases. Physical removal
unbinds the corresponding owned level slot by body identity; trace record order
and level source order remain distinct. Retained values and maps are closed on
all replay exits, and the previous world listener is restored after failure too.

Every pass checks physical/world/dynamic/goal membership and that score,
floating-score and required random requests occur once per ordinary removal.
These are C integration invariants; the existing native traces compare body
membership, strength, velocity, contact order, score, reload, idle and results
outputs. All seven profiles still match, including four ordinary removals and
score 9,380 in the 24-frame impact, five removals and score 11,380 on retry, and
five ordinary removals plus the separately projected bird retirement in each
96-frame result sequence ending at 31,380. No new native trace is claimed.

Initial script scalars and remaining captured/inferred inputs are unchanged.
Collision dispatch remains the restricted numeric museum projection. Destroy/
notification, audio, floating presentation and particles use controlled hosts;
the unused random result is supplied explicitly. Unsupported spawning, rockets,
gifts and explosions fail when reached. Actual script/effect ownership, RNG,
retirement cleanup and full gameplay remain open. All default Debug/Release/
Windows builds pass; 34 strict Debug ASan/UBSan and 49 Release affected checks,
seven additional Debug native replays, formatting and diff checks pass. Private
provenance is `analysis/input/level-removal-integration.json`.

Original `scripts_common/Slingshot.lua` prototype 24118 computes launch velocity;
the C result matches both float outputs in 4,096 cases. Original utils prototype
9296 supplies distance in the oracle, with controlled sling properties and
definition force/default inputs. Keep mass multiplication and reciprocal mass
multiplication in their measured order; algebraic cancellation changes rounding.
Prototype 17173's active transfer path also matches 4,096 cases, including 968
completion boundaries and 11,096 ordered position/rotation/type calls. Time is
added before arc evaluation and is not clamped before completion handling.
The final snap subtracts binary32 0.1 from both target coordinates, then requests
dynamic type and zero rotation. Hats, actual camera/audio/event
effects and live timing are not covered.

Transfer setup additionally matches 4,096 original-Lua cases after invoking the
original level reset with unrelated hooks stubbed. Its default phase is binary32
0.75 times pi. A currently jumping bird restores current Y from saved Y and clears
the jump flag; saved Y itself remains unchanged. Other birds use their current Y.
Height is the target/start Y difference times binary32 1.33, and jumpTimer becomes
10000. All metadata and 8,192 initial position/rotation calls match. Reload-delay
gating and complete queue state remain open.

The native Lua `math.sin` wrapper at analysis `0x8300bc` converts float to double,
calls `sin`, and converts its result back to float. Reusing physics `sinf`
initially exposed one transfer mismatch; a separate authored split-pi/Taylor arc
evaluator preserves all measured outputs. Its phase domain is explicitly bounded
to magnitude 1024; the transfer corpus is sampled evidence, not proof for every
finite argument. Physics rotation math is unchanged. Debug sanitizer and Release
launch/transfer comparisons pass. Private provenance is
`analysis/slingshot-native.json`.
The C body adapter composes the measured position/rotation/type calls, with
game-angle normalization at rotation boundaries and completion mass reset.
Its focused static-to-dynamic/body-position checks pass. A native comparison of
the complete composed adapter remains distinct from the checked Lua arithmetic
and previously validated individual physics operations.

Velocity assignment (`GameLua::setVelocity`, analysis `0x322a68`) and impulse
application (`0x322790`) match 4,096 direct native calls: 1,366 assignments,
1,365 origin impulses, 1,365 explicit-point impulses and 818 wakes. The oracle
uses a bounded one-entry GameLua object map and native Lua argument stack; it
executes the original methods without starting game services. Static velocity
sets and nondynamic impulses are no-ops. Positive squared assigned speed wakes
a sleeper, preserving underflow in that test; a dynamic zero impulse also
wakes. Default impulse points use transform origin, independently of center.
Signed-zero cases distinguish native VNMLS's negated-accumulator addition from
negating a completed subtraction. The first-party implementation preserves that
order. Debug sanitizer and Release comparisons pass. Provenance is
`analysis/body-velocity-native.json`; full launch callbacks remain open.

Formal format references used for independently authored support are
[DEFLATE / RFC 1951](https://www.rfc-editor.org/rfc/rfc1951),
[SHA-256 / FIPS 180-4](https://csrc.nist.gov/pubs/fips/180-4/upd1/final),
[AES / FIPS 197](https://csrc.nist.gov/pubs/fips/197/final), and
[CBC / SP 800-38A](https://csrc.nist.gov/pubs/sp/800/38/a/final). The
[7z format description](https://raw.githubusercontent.com/ip7z/7zip/main/DOC/7zFormat.txt)
was used as a container specification; no 7-Zip implementation source was
imported or linked.

Lua 5.1's official
[instruction declarations](https://www.lua.org/source/5.1/lopcodes.h.html) and
[chunk reader](https://www.lua.org/source/5.1/lundump.c.html) were consulted as
format references under the user's research permission. The project reader,
static data pass, and export code are independently authored; no Lua source or
runtime library is bundled.

The MPEG-2 frontend now derives last-nonzero long/short scalefactor bands and
full-rate subband bounds, applies normalized mid/side addition/subtraction and
performs eight alias rotations at each active long-block boundary. The long
bound includes the native eight-line margin; short bounds use positions across
three interleaved windows. Empty spectra retain one active subband. Mid/side
processing shares the greater channel bound. Pure short blocks skip aliasing.
The eight rotation pairs are generated with bounded binary64 arithmetic and
match all sixteen original binary32 coefficients; no decoder table is copied.

Fresh isolated original decoders provide dequantization returns, pre-alias
spectra and post-alias spectra at ELF `0x8f92b8`, `0x8fd324` and `0x8fd4dc`.
All 135,936 stereo/alias float values match bit for bit in 118 granules over
85 original frames: the complete 19-frame mono collision prefix and 33-frame
stereo ambience and mono 24 kHz award prefixes. Captures include priming,
block types zero through three, independent per-channel nonzero bands and
active/shared bounds. Each capture repeats exactly. Tracked observation words
are verified restored before execution resumes, with no decoder-data or
register writes. C comparisons reconstruct the preceding frontend directly
from each encoded source; bounds are calculated by C as well as observed.
All 34,824 APK frames pass this extended frontend, but only the retained
prefixes have native spectral comparisons. Intensity stereo, mixed-short
overflow recovery, hybrid overlap, polyphase synthesis and final PCM remain
open. Private provenance is `analysis/input/mp3-stereo-alias-native.json`.

The first-party long transform separates adjacent-input accumulation, even/odd
nine-point cosine components, paired output formation, window application and
overlap addition. The target associates several even/odd sums differently;
preserving those binary32 operations matters for exact output. Coefficients
are generated once with a bounded binary64 cosine series, then borrowed as
immutable caller-owned data. All 26 measured native coefficient values match,
including nine reciprocal half-cosines. No host trigonometric dependency or
copied decoder coefficient table is used.

The original exported `INT123_dct36` at ELF `0x8fbffc` executes normally on
1,024 first-party frequency, preceding-overlap and arbitrary window inputs.
All 36,864 output/next-overlap float words match C, including signed zeros,
positive/negative basis vectors and varied magnitudes/previous state. Repeated
native captures are identical. The observer verifies the original function's
stride-32 output and adjacent guards, and confirms that preceding overlap is
unchanged. This establishes a controlled long-transform primitive. Window
construction is compared separately; short transforms, composed stream overlap, polyphase
synthesis and PCM remain open. Private provenance is
`analysis/input/mp3-long-native.json`.

All 288 original hybrid-window float values also match first-party generation:
normal/start/short/stop windows for even/odd subbands, including the sign of
zero in inactive start/stop ranges and positive-zero padding after the short
window's twelve used coefficients. Bounded binary64 sine/cosine series,
piecewise transition ranges and the factored cosine denominator generate the
tables without a host trigonometric dependency. The native observer resolves
the window addresses from `INT123_do_layer3`'s PC-relative long-transform
argument setup, after original initialization. Repeated native captures are
identical. These tables and the separate long primitive still need composed
short-transform/state and PCM validation. Private provenance is
`analysis/input/mp3-windows-native.json`.

The short primitive now computes three interleaved six-value transforms and
their overlapping twelve-value windows. It reuses generated cosine/reciprocal
coefficients, preserves the ordering of current-output additions and writes
new next-overlap values directly before the following window accumulates into
them. Six trailing overlap values clear to positive zero. All 36,864 native
output/overlap float values match over the same 1,024 controlled cases used
for the long primitive. The original function at ELF `0x8f847c` runs normally
with guarded own inputs and repeated captures match exactly. Private
provenance is `analysis/input/mp3-short-native.json`.

The composed hybrid stage retains one 576-value overlap vector per channel
and produces a time-slot-major 18-by-32 matrix. The original processes active
subbands in pairs, rounding its bound upward to an even count. Inactive
subbands copy their preceding overlap and clear the following overlap.
The C state uses a single retained vector instead of exposing original
alternating storage banks; all defined values match.

Fresh original-stream observations at ELF `0x8fd4dc` and `0x8fd608` capture
each channel's preceding overlap, post-transform matrix and following overlap.
All 135,936 matrix/next-overlap float words and 67,968 preceding-overlap words
match across 118 granules over 85 frames. The three asset-format prefixes
include priming, all block types, both parities and transitions from one frame
to the next. Each capture repeats identically, with observation instructions
verified restored before continuation. The comparison reconstructs spectra,
bounds, stereo and aliasing directly from each encoded source and carries C
overlap from stream start; no intermediate native arrays are injected.
All 347 APK MP3 assets also pass C hybrid progression, skipping Xing metadata
while retaining its reservoir contribution. This remains selected native
hybrid evidence; intensity stereo, mixed-short overflow recovery, polyphase
synthesis, final PCM and game audio integration remain open. Private
provenance is `analysis/input/mp3-hybrid-native.json`.

The synthesis cosine primitive splits mirrored inputs into sum/difference
halves, applies five generated reciprocal-cosine groups and interleaves the
recursive results. All 31 factors match the original initialized groups.
Its C interface returns 32 contiguous values; the native function stores
indices0..16 in reversed stride-16 order in one buffer and indices16..31 in
another, repeating the center. This storage mapping is checked explicitly.

Fresh native observations at `INT123_dct64` (ELF `0x8f3f08`) match all 10,944
result float words across 342 time slots of the 19-frame mono reference,
including priming. Original input is verified unchanged and both captures
repeat exactly. C uses captured primitive inputs for this comparison; hybrid
composition has a separate encoded-source gate above. All 347 assets also
pass C progression through this transform. All 365 Debug sanitizer checks,
56 Release MP3 checks, Windows compilation and formatting pass. Synthesis
history/window application, integer conversion, trimming and final PCM remain
open. Private provenance is `analysis/input/mp3-synthesis-transform-native.json`.

The complete synthesis stage expands 257 recovered quantized codec-window
parameters into the native 544-float window. The parameters are numerical data
observed at Binary Ninja analysis address `0xb4b878`; window initialization is
`INT123_make_decode_tables` (ELF `0x8f24d8`). First-party C owns two 272-value
history banks and a phase per channel, starting at phase one. It places the
cosine coefficients into observed stride-16 positions, applies the forward,
center and reverse window sums in their original binary32 order, clamps to
signed PCM16 and truncates toward zero. Products and sums remain separate.

Fresh `INT123_synth_1to1` entry/return observations (ELF `0x8f1f14`) cover
342 mono, 1,188 stereo and 594 mono 24 kHz calls. All 544 initialized window
words, 1,155,456 subsequent history words and 67,968 PCM samples match, including
initial priming and all history phases. Each of the three captures repeats
identically. The temporary observation instructions are restored and checked
before continuation; the original guest library's SHA-256 remains unchanged.
Private capture records are `analysis/audio/mp3-synthesis-native-cases.json`.

Complete MP3 decoding now composes the independently checked frontend, stereo,
aliasing, hybrid overlap and synthesis stages. Xing metadata contributes to
the reservoir but does not advance audio history. The mono reference's final
PCM is an exact slice starting at raw sample 1,105: its encoded delay is 576,
establishing the additional 529-sample synthesis delay. Apply that offset and
the encoded final length to LAME-tagged assets; other target assets retain
their full decoded sample span. Every complete PCM hash matches the original
native decoder across all 347 assets: 40,701,012 bytes, 19,238,996 sample frames
and all 34,824 encoded frames. The public decoder returns owned host-order
PCM16 with explicit allocation bounds. Intensity stereo, mixed short blocks,
damaged-stream repair and LAME padding below 529 samples remain unsupported
general routes; none occurs in the pinned asset inventory. Private provenance
is `analysis/input/mp3-pcm-native.json`.

The explicit-path audio loader reads APK MP3 or PCM16 WAVE resources into the
shared owned PCM representation, preserving source sample rate and channels.
It bounds encoded and decoded allocations separately. Focused composition
checks load all three MP3 format families and compare their native PCM hashes,
load the score-loop WAVE, reject missing/oversized resources without publishing
ownership, and borrow a decoded clip through the named mixer binding. These
composition checks establish C ownership and integration; resource registry,
cache/streaming policy, rate conversion and native device behavior remain open.
All 371 Debug sanitizer checks, 74 Release audio checks, Windows compilation,
formatting and the three native synthesis corpus gates pass.

## Joint construction

First-party static data recovery across 640 level files and 15 companion script files finds 9,780 joint
entries in 412 levels and no nonempty `chains` tables. Encoded type counts are
274 type one, 7,375 type two, 1,666 type three, 31 type four and 434 type five.
`GameLua::createJointLua` (ELF `0x31e018`) compares the encoded binary32 type
against one through five. Its branches construct distance, weld, revolute and
prismatic definitions; type five retains deferred glue/timer metadata. These
are resource and call-path observations; complete game construction policy and
glue lifecycle remain open. Private inventory is `analysis/joints/levels.json`,
with counts and field presence in `analysis/joints/inventory-summary.json`.

The C geometry primitive transforms explicit world anchors into each body's
local frame. Distance returns the world-anchor separation length. Weld and
revolute share one world anchor and retain second-minus-first reference angle;
prismatic also rotates the supplied axis into the first local frame without
normalization. The native initializers are ELF `0x8d7464`, `0x8d4e84`,
`0x8d5c84` and `0x8d3de8`, respectively. The interface does not create a world
joint or select game defaults.

Guarded original-function calls match all 90,112 defined float words across
4,096 inputs per initializer, including signed zeros, subnormals, basis
rotations, coincident anchors and varied finite transforms. All original body
words and unassigned definition words remain unchanged; body references are
checked independently. Both captures are identical. The C API defines unused
result fields as positive zero, while the probe verifies native unused fields
are preserved. Subnormal inputs are constructed as integer float bits to keep
setup identical across hosts. Private provenance is
`analysis/input/joint-geometry-native.json`. Constraint solving, motors/limits,
breaking and world joint membership remain open.
All 373 Debug sanitizer checks, both Release joint-geometry checks, Windows
compilation and formatting pass.

The joint matrix helpers retain symmetric storage while reproducing the
original binary32 cofactor/determinant order. The 2D inverse clears unused
angular fields to positive zero. A zero determinant retains its sign through
the native cofactor multiplications; no epsilon threshold is added. Native
`b2Mat33::GetInverse22` and `GetSymInverse33` are ELF `0x8e18b0` and
`0x8e1918`. All 73,728 output words match over 8,192 guarded calls, including
zero, rank-one, near-singular and small diagonal matrices. Inputs and output
guards remain unchanged, and both captures are identical. Nonfinite required
inputs/results reject atomically in the C API. This establishes matrix
inversion, with weld and other joint constraint integration still open.

Weld initialization now composes first-party rotation, rotated local offsets,
constraint matrix construction, rigid or angular-spring inversion and retained
impulse application. Matrix terms and bias/gamma operations retain native
binary32 association. The original constructor and initializer (ELF `0x8d4f10`
and `0x8d4b08`) match all 98,304 captured offset/matrix/gamma/bias/impulse and
velocity words across 4,096 controlled calls. Cases include signed zeros,
zero/nonzero inverse inertia and duration, rigid/spring routes and disabled or
zero/half/unit/double-ratio warm starting. Guards, definitions, source bodies
and preserved joint fields remain unchanged; native cached centers, masses and
indices are checked against their inputs. Both captures are identical. C
failures preserve state and velocities. All 377 Debug sanitizer checks pass;
private provenance is `analysis/input/weld-initialize-native.json`. This remains
initialization evidence: velocity/position iteration, game construction and
world joint membership are open.

Weld velocity solving now carries cached masses/inertias, rotated offsets,
inverse matrix and retained impulses through consecutive rigid or spring
iterations. The spring route solves angular bias/damping before its linear
constraint; the rigid route solves the coupled three-component constraint.
Original `SolveVelocityConstraints` (ELF `0x8d435c`) matches all 491,424 float
words over 4,096 initializations and 16,380 consecutive iterations, including
zero-through-eight iteration cases. Every C state starts from its own
initializer with the same explicit inputs. Original cached joint fields,
source bodies, positions, solver controls and guards remain unchanged; both
captures are identical. All 379 Debug sanitizer checks pass. Private
provenance is `analysis/input/weld-velocity-native.json`. Position solving,
game construction policy and world membership remain open.
The eight Release joint checks, Windows compilation and formatting also pass.

Direct symmetric matrix solves now reproduce the original determinant and
right-hand-side product order, independently of the inverse helpers. Original
`Solve22` and `Solve33` (ELF `0x8e1858` and `0x8e1778`) return vectors through
the verified ARM hidden-result-pointer ABI. All 20,480 vector words match over
8,192 calls using the singular, near-singular and small-diagonal matrix cases
with explicit right-hand sides. Matrices and vectors remain unchanged, output
guards hold and both captures are identical. All 381 Debug sanitizer checks
pass; private provenance is `analysis/input/matrix-solve-native.json`. Joint
position correction and world composition remain open.

Weld position correction now recomputes rotation, offsets and the constraint
matrix at each current body angle, using the direct matrix solves. Original
`SolvePositionConstraints` (ELF `0x8d4794`) matches all 221,502 initialization
and position float words and 16,437 convergence decisions over 4,096 cases.
Cases carry their own C positions through up to eight passes. Dedicated
boundary inputs verify the native linear threshold `0x3d4ccccd` (0.05) and
angular threshold `0x3d0efa36`, using errors measured before correction. Rigid
zero-inertia cases retain angular error; the spring route corrects only linear
position error. Original joint state, velocities, source bodies, solver controls
and guards remain unchanged. Both captures are identical; private provenance is
`analysis/input/weld-position-native.json`. Joint world membership, game creation
policy and distance/revolute/prismatic solvers remain open.

Distance joints now initialize offsets, axis, effective mass, spring gamma/bias
and scaled retained impulse, then carry velocity and position iterations.
Original construction, initialization, velocity and position methods (ELF
`0x8d7530`, `0x8d7104`, `0x8d6bb8`, `0x8d6ed4`) match 617,430 float words
and 16,485 convergence results across 4,096 initializations and 16,485 passes
of each solver. The original initialization normalizes only above 0.05;
position correction uses a separate `2^-23` normalization threshold, clamps
length error to ±0.2 and tests its magnitude strictly below 0.05. Spring joints
skip position correction. These distinct rules and signed-zero behavior are
checked with explicit boundary neighborhoods and consecutive carried states.
Native bodies, definitions, solver controls and guards stay unchanged;
velocity passes preserve cached fields and position passes preserve the entire
joint and velocities. Both captures are identical. Private provenance is
`analysis/input/distance-joint-native.json`. World composition, game construction
policy, revolute and prismatic solvers remain open.

Revolute initialization constructs the coupled anchor matrix, motor mass,
angular limit state and warm-start impulses. Original constructor/initializer
(ELF `0x8d5d10`/`0x8d558c`) match 131,072 float words and 4,096 limit states.
Tests cover motor/limit flags, all preceding limit states, fixed angular mass,
reversed limits and widths around `0x3d8efa36`. The original retains angular
impulse when limits are disabled and preserves it when entering equal limits;
lower/upper transitions reset it only when the preceding state differs. These
behaviors are captured explicitly. Source bodies, positions, definition, solver
controls and guards remain unchanged. Repeated captures are identical; private
provenance is `analysis/input/revolute-initialize-native.json`.

Revolute velocity solving applies the clamped motor first, then coupled active
angular constraints or a free two-component anchor solve. Lower and upper
constraints recover a reduced anchor solve when the proposed angular impulse
has the wrong sign. Original `SolveVelocityConstraints` (ELF `0x8d5200`)
matches all 655,232 float words and 20,476 limit states across 4,096
initializations and 16,380 consecutive iterations. C starts from its own
initializer and carries all impulses and body velocities. Cached fields,
positions, source bodies, definitions, controls and guards stay unchanged;
both captures are identical. Private provenance is
`analysis/input/revolute-velocity-native.json`. Revolute position correction,
world membership, game policy and prismatic constraints remain open.

Revolute position solving applies angular limits before rebuilding rotated
anchors and correcting linear separation. The position matrix keeps its own
weighted multiplication order, distinct from velocity initialization. Original
`SolvePositionConstraints` (ELF `0x8d5928`) matches 254,756 float words,
4,096 initialized limit states and 16,518 convergence results across 4,096
cases. Dedicated neighborhoods cover linear slop, angular slop and maximum
angular correction. The solver retains native equal-limit clamping and signed
lower/upper angular error, then compares linear error measured before its
correction. C carries its own positions through consecutive passes. Native
joint state, velocities, source bodies, definition, controls and guards remain
unchanged, and both captures are identical. Private provenance is
`analysis/input/revolute-position-native.json`. World membership, game policy
and prismatic constraints remain open.

Prismatic construction normalizes local axes at length `2^-23`, retaining
shorter components and forming the perpendicular with its signed zeros.
Initialization then computes rotated axes, lever arms, coupled matrix, motor
mass, limit state and warm-start velocity changes. Original construction and
initialization (ELF `0x8d3e9c` and `0x8d3a14`) match all 163,840 float words
and 4,096 limit states over 4,096 cases. The native angular matrix row uses
one when summed angular inverse mass is zero. Disabled limits clear retained
axial impulse; narrow limits retain it. Boundary cases cover normalization,
subnormal and signed-zero axes, limit width and prior state. Native bodies,
positions, definitions, controls, preserved joint fields and guards remain
unchanged. Both captures are identical; private provenance is
`analysis/input/prismatic-initialize-native.json`. Iteration solving and world
composition remain open.

Prismatic velocity solving clamps motor force first, then solves perpendicular
and angular motion with an optional translation constraint. The active route
keeps the original three-component solve, clamps the retained axial impulse,
then always recomputes the two remaining components and applies differences
between accumulated impulses. Original `SolveVelocityConstraints` (ELF
`0x8d31f8`) matches 819,040 float words and 20,476 limit states over 4,096
initializations and 16,380 consecutive passes. C reconstructs each initialized
state independently and carries its own impulses and velocities. Native cached
fields, positions, bodies, definitions, controls and guards remain unchanged.
Both captures are identical; private provenance is
`analysis/input/prismatic-velocity-native.json`. Position correction and world
composition remain open.

Prismatic position solving recomputes current axes, lever arms and matrix,
using the current translation to select limit rows independently of cached
limit state. Original `SolvePositionConstraints` (ELF `0x8d3588`) matches
288,256 float words, 4,096 initialized limit states and 16,640 convergence
results over 4,096 cases. The captured equal-limit route clamps translation
itself, including when its narrow bounds are nonzero. Lower/upper correction
preserves literal negative zero inside slop; the free route retains a positive
zero axial impulse. Explicit neighborhoods cover slop, maximum correction and
limit width, with degenerate axes and consecutive carried positions. One
three-component correction rounds a float step above decimal 0.1 in both C
and native output; the test retains its exact word `0x3dccccce`. Joint state,
velocities, source bodies, definitions, controls and guards remain unchanged,
and both captures are identical. Private provenance is
`analysis/input/prismatic-position-native.json`. World membership, island
composition and game construction remain open.

Selected discrete islands now compose distance, weld, revolute and prismatic
constraints with contacts. Native `b2Island::Solve` (ELF `0x8e1ee8`) initializes
and warm-starts contacts before initializing joints, solves joints before
contacts in each velocity pass, and solves contacts before all joints in each
position pass. Both position groups must converge before stopping or sleeping.
The C dispatcher and island implementation retain that order, with borrowed
constraint objects and caller-supplied list order.

All 1,187,739 output words match across 4,096 guarded original island calls:
12,287 complete body records, 6,144 contact manifolds and 8,191 projected
joint states. Cases vary joint-only and mixed islands, all four joint types,
zero/varied velocity and position counts, duration, warm starting and ratios,
forces, damping, motion limits and sleep controls. Joint projections normalize
unused fields to zero and retain limit states separately; body/manifold records
compare preserved fields as well as solved values. Definitions and preserved
native joint fields remain unchanged, guards hold, and both captures are
identical. C reconstructs its own constraints and solver state from the same
explicit inputs. Private provenance is `analysis/input/joint-island-native.json`.
World graph traversal, joint creation/deletion and game-level integration remain
open. Existing contact/free/continuous island and reporting captures also pass
after the composition change.

The original world raycast (ELF `0x8d2e88`), instantiated dynamic-tree traversal
(`0x8d2a00`) and nearest callback (`0x343e10`) now have a first-party world-query
implementation. The tree visits its second child first, clips using each
positive callback return and stops on zero; negative and NaN returns keep the
current limit. Original nearest selection retains every hit, including a later
equal-distance hit. Reported points use weighted endpoints. Sensors participate
and collision filters do not reject ray hits.

All 68,664 words match across 32 worlds and 2,048 queries: 1,815 ordered reports
and 713 nearest hits. Rotated circles/polygons, inactive bodies, sensors,
zero category/mask, locked queries and varied initial fractions are covered,
including 227 adjacent equal-fraction reports and four zero-fraction nearest
hits. The diagnostic observer calls the original nearest method from its own
separate callback object; an independent nearest pass uses the original
constructor, vtable, world/tree path and GetResult without that observer.
No original vtable is replaced. Both captures are identical. The C contract
uses finite ray/shape inputs and a bounded traversal stack; native NaN query
inputs and game-object names are outside this comparison. Callback failure
preserves prior effects and leaves output unpublished. All default Debug,
Release and Windows targets build; 157 strict Debug sanitizer and 174 Release
affected checks and formatting pass. Private provenance is
`analysis/input/world-raycast-native.json`. Coupled impact prediction and owned
bird-following behavior remain open.

GameLua.raycast (`0x32b350`) now has a level-name adapter. The original returns
one Boolean false on a miss, or Boolean true, canonical GameObject name (nil
for null fixture user data), point X/Y and normal X/Y on a hit. Its string-view
boundary preserves explicit lengths. The original allocator, GameObject
constructor, std::map insertion and public rolling-sound setter create real
owned names; swapping the setter's string into the object's empty name keeps
native ownership intact. Original removal releases every object, body and map
node afterward. No fabricated map nodes or string implementations participate.

All 46,016 words match across 32 worlds and 2,048 original GameLua calls:
1,144 hits, including 245 unnamed hits. Names include empty strings, embedded
NUL and UTF-8 bytes. Both captures match. The C adapter resolves fixture user
data by identity without dereferencing foreign pointers, checks body ownership
and rejects removed bindings. Misses leave absent result fields untouched.
The owned-level entry point also passes a locked-world control. All default
Debug, Release and Windows targets build; 159 strict Debug sanitizer and 176
Release affected checks and formatting pass. Private provenance is
`analysis/input/game-raycast-native.json`. This is a constructed physics/name
boundary, not complete live script-object creation or camera following.

The recovered impact predictor also composes with real physics queries. In
32 constructed worlds, the unchanged original Class/Vector2/PhysicsQueries
graph calls the original GameLua.raycast through its own real LuaState wrapper.
Its one/six results return directly to the original graph; no synthetic hit
responses are supplied. C composes its independent predictor, level-name
adapter and world/tree/shape query implementations from the same inputs.

All 1,375,788 additional words match across 128 predictions, with 41,097 rays
and 68,623 checked ray/radius/print boundaries. Half retain a hit and half
return no hit. Comparisons cover every query endpoint, native result arity,
typed name bytes, point/normal, ordered radius/print calls, retained result,
returned time and counts. Gravity, starting pose, launch velocity, optional
steps and radius are explicit fixture inputs. The original predictor's radius
lookup receiver is controlled; its actual query/selection graph is unchanged.
Both captures match. The earlier independent predictor and GameLua query
captures remain byte-identical after shared fixture refactoring. All default
Debug, Release and Windows targets build; 160 strict Debug sanitizer and 177
Release affected checks and formatting pass. Private provenance is
`analysis/input/bird-impact-world-native.json`. Live launch/pose ownership,
radius-script dispatch and the full bird-following update remain open.

The follow camera's smoothstep easing is now available to the mover and owned
camera's resolved tween interface. Original Utils prototypes smoothstep
(`14736`) and easeSmoothStep (`14863`) compose the unchanged original clamp
(`8739`) and tweenLinear (`13780`) helpers from
`assets/data/scripts_common/utils.lua`. All four selected prototypes have zero
upvalues; their complete original bytes execute in the isolated original VM
with its real math library. No helper substitution or code/register patching
is used. The clamp is math.max(0, math.min(1, ratio)); NaN ratios retain one,
and the outer maximum retains positive zero when compared with negative zero.

All 40,960 words match across 4,096 raw-curve/easing compositions, including
inverted/equal bounds, zero durations, overflowed ratios, signed zeros,
1,739 NaN, 880 infinity and 1,788 subnormal input occurrences. Both captures
match. Start/change and final easing results are finite under the C tween
contract; the raw curve accepts every binary32 numeric input. C controls also
apply the selected curve to position and zoom, while the original scalar
composition is the native comparison scope. All default Debug, Release and
Windows targets build; 25 Debug sanitizer and 25 Release camera checks,
including prior native camera captures, and formatting pass. Private provenance
is `analysis/input/camera-smoothstep-native.json`. Follow-specific captured zoom
and live Y curves, the full follow update and owned bird lifecycle remain open.

FollowBirdCameraMovementBehavior now has an explicit first-party C state,
ordered typed service boundary and captured zoom/live Y curves. Its original
Class/Vector2/AutomaticCameraMovementBehaviors graph executes unchanged in the
isolated VM, including init (`1883`), update (`2274`) and both actual child
closures (`4200`, `4459`). Original clamp, smoothstep, linear, cubic, cosine and
deepCopy helpers remain active. Prediction, transform, projection, limits,
snap, zoom and sequence services are controlled numeric inputs. Caller checks
verify prototype size/upvalues, first/last instructions, PC, arity and receiver
identity; prediction also checks both argument identities. No original code,
register, upvalue or vtable is substituted.

All 143,944 words match across 256 configurations and 1,024 successive updates:
2,755 service boundaries, 174 target results, 44 captured zoom targets, 1,572
curve evaluations and 12 original indexing failures. Comparisons include
cancellation, absent launch data, predicted/cached/missing hits, zero and expired
scroll timers, strict snap boundaries, peak selection, retained fields, output
presence and closure results. The original apex X calculation starts from the
bird's Y position. Only the hit-position branch replaces apex time with hit
time. The zoom curve captures six numbers; the Y curve ignores tween arguments
and queries current screen X, projected Y and validation afresh. A cached hit
with absent launch velocity fails when the original later dereferences it.

Both captures match. All default Debug, Release and Windows targets build;
30 strict Debug sanitizer and 30 Release camera/prediction checks and formatting
pass. C controls preserve unpublished results and earlier state/host effects on
failure. Private provenance is `analysis/input/camera-follow-native.json`.
Typed numeric properties and ordinary helper bindings define this comparison;
owned CameraSystem composition, live launch/pose/radius dispatch and bird
lifecycle integration remain open.

CameraLimitEnforcer.getZoomScaleToFitOnScreen (`3981`, 36 instructions) and its
nested bottom helper (`4277`) now have an explicit numeric continuation after
projection. Equal point/projected Y returns the supplied closeZoom field.
Otherwise the original fits half the screen height around projected Y, checks
the resulting bottom against bottomBorder, and optionally refits full screen
height against that border. Separate division/multiplication/addition order and
unordered comparisons are preserved; no zoom clamp is added.

All 57,344 words match across 4,096 original calls and projection boundaries,
with exact receiver/point identity, caller PC 3 and arity checks. Cases include
equal Y, signed zeros, subnormals, border comparisons, overflow and nonfinite
results from finite inputs. The complete unchanged original method and nested
helper execute with the real original math library. Projected coordinates and
closeZoom are explicit controlled inputs. Both captures match; all default
Debug, Release and Windows targets build, with 32 strict Debug sanitizer and
32 Release camera/prediction checks and formatting passing. Private provenance
is `analysis/input/camera-zoom-fit-native.json`. Default initializeLimits assigns
closeZoomLevel, while this method reads closeZoom on equality; the owned default
binding must preserve that absence. Complete owner/follow composition remains
open.


The owned CameraSystem now composes FollowBird behavior with its mover, default
limit enforcer, snap selection, zoom fit and captured curves. The unchanged
original Class/Vector2/CameraSystem/CameraMover/CameraLimitEnforcer/
CameraSequenceManager/AutomaticCameraMovementBehaviors graphs execute together.
Their original helpers and naturally captured closures remain active. Resolved
input, prediction, coordinate transform, terminal sequence dispatch and native
publications are controlled boundaries with caller PC/arity/identity checks.
No original instructions, registers, upvalues or vtables are patched.

All 65,680 words match across 16 configurations and 512 successive owner updates:
1,649 checked boundaries, 225 active-follow frames, 120 custom-curve frames and
27 retained-curve frames after following ends. Both captures match. Six original
missing-closeZoom failures retain the same partial state. Default initialization
writes closeZoomLevel; the equality branch reads absent closeZoom. Other covered
transitions include cancellation, replacement, matching/unrelated bird removal,
nil removal clearing queued movement, partial input targets and queued snap/
position movement. Live Y curves read the previous published screen position,
and absent zoom duration sets zoom immediately. Sanitizer controls release a
removed target and its environment before continuing retained curves.

All default Debug, Release and Windows targets build; 33 strict Debug sanitizer
checks (12.97 seconds), 33 Release checks (2.37 seconds) and formatting pass.
Windows binaries were compiled, not executed. Private provenance is
`analysis/input/camera-follow-system-native.json`. Terminal sequence dispatch is
still a checked boundary; real end-sequence loading, live launch/pose/radius
ownership and full gameplay scheduling remain open.


The original FollowShotBirdEndCameraSequence resource now continues through the
owned camera. C reads its static table directly from the pinned APK without a
VM. The native comparison loads the complete unchanged original chunk into the
actual environment table supplied by CameraSequenceManager.loadSequenceFromFile
(`657`), using the original loader/setfenv APIs. The original owner, manager,
velocity-condition factory (`2203`) and captured timer closure (`2360`) remain
active. Resource paths, loader/print PCs, arity and typed outputs are checked.
Resolved input, prediction, transform and publication remain explicit boundaries.

The sequence moves to the castle at velocity 150, waits for no moving objects
with minimum duration three, and returns to the bird snap over one second.
Movement duration is vectorDistance(previous published screen, target)/velocity,
with binary32 intermediates and the original math square-root conversion. The
stage's duration property changes, while its condition captures a separate
number; later property changes do not change that condition. Zero velocity
preserves the ordinary duration path. Both the mover and strict timer transition
retain their independent state.

All 362,304 words match across 16 configurations and 2,560 original owner updates:
35 resource loads (three triggered by FollowBird), 70 checked print calls and
7,900 total checked boundaries. The comparison covers 558 castle-stage frames,
349 wait-stage frames, 120 return-stage frames, 24 complete return transitions,
96 moving-input frames, restart and input interruption. There are no original
failures. Both captures match. All default Debug, Release and Windows targets
build; 34 strict Debug sanitizer checks (14.68 seconds), 34 Release checks
(2.51 seconds) and formatting pass. Windows execution remains unverified.
Private provenance is `analysis/input/camera-follow-end-sequence-native.json`.
Live launch/pose/radius ownership and full gameplay scheduling remain open.


World ray queries now preserve overflow and unordered arithmetic for arbitrary
binary32 endpoints, while shapes and transforms retain their valid world
contracts and initial fractions remain finite/nonnegative. Native circle
RayCast (`0x8dfdd4`) requires ordered numerator >= zero and <= length*limit;
negating less-than/greater-than would incorrectly accept NaNs. Its unchanged
polygon counterpart (`0x8e0424`) and tree traversal remain active in the original
GameLua query comparisons. No early miss substitution is used in C: the actual
tree, shapes and ordered comparisons execute.

All 46,016 words match across 2,048 additional original named world queries:
704 NaN-input misses, 304 infinity-input misses, 1,040 finite queries and 340 hits
(including 64 unnamed hits). Cases also include signed zeros, subnormals, extreme
finite endpoints and overflowed intermediate lengths. Both captures match.
Private provenance is `analysis/input/game-raycast-numeric-native.json`.

The unchanged original prediction graph also composes with its original
GameLua.raycast for 128 additional zero-velocity/vertical/apex cases. All
2,565,888 additional prediction words match: 76,800 rays, including 19,488 with
NaN endpoints, 51,200 radius calls and 128 print calls. Every prediction returns
no retained hit in these explicit constructed worlds. Every endpoint, result
arity, query order, count, final time and absence matches; the same 46,016 direct
query words also match. Both captures repeat exactly. Radius lookup remains a
controlled numeric receiver. All default Debug, Release and Windows targets
build; 93 strict Debug sanitizer checks (25.29 seconds), 93 Release checks
(6.31 seconds) and formatting pass. Windows execution remains unverified.
Private provenance is `analysis/input/bird-impact-world-numeric-native.json`.
Live launch/pose/radius ownership and full gameplay scheduling remain open.


The predictor/world composition is now a runtime API, including an owned-level
entry using its actual world, canonical names and removed-object mask. The two
existing finite and zero/vertical native corpora execute through this production
path: 256 predictions, 3,941,676 prediction words, 117,897 rays and 196,751 ordered
boundaries still match. The optional diagnostic observer runs after the real
query and records immutable results. Radius/print services remain explicit;
pose, launch velocity and gravity remain caller-owned input fields.

Owned-level sanitizer controls exercise locked-world prediction, missing radius,
query-budget and observer failures, preserving prior output and earlier callback
effects. All default Debug, Release and Windows targets build; 39 strict Debug
sanitizer checks (15.82 seconds), 39 Release checks (2.83 seconds) and formatting
pass. Private provenance is `analysis/input/impact-world-runtime.json`.
Launch-to-camera activation and complete game scheduling remain open.


Shot-follow activation now uses the actual FollowShotBirdCameraSequence static
resource and its followTarget action. C exposes a borrowed game-state resolver;
the original executes its complete resolveTarget (`1813`) closure with an actual
getObject miss, then selects the controlled flyingBird global. The original
follow target, allowCancel, default compensation and sequence condition are
retained. Nil targets clear following before unused compensation validation.
General object/goal lookup remains a game binding, not an invented name alias.

The native graph also executes complete unchanged zero-upvalue behavior methods:
BirdCameraFollowBehavior.onLaunch (`269`, six instructions), its museum counterpart
(`311`, six instructions), and MuseumBluesCameraFollowBehavior.init (`272`, ten
instructions). Standard/museum launch starts FollowShotBirdCameraSequence; Blues
initialization cancels before starting it. Original class cancellation calls the
manager's endSequence. C implements the same service order, re-reading later
bindings after cancellation; explicit owner cancellation preserves independent
automatic behavior and mover curves.

All 365,376 words match over 16 configurations and 2,560 original updates:
48 shot-resource loads, 35 end-resource loads, 48 checked target lookups and
8,092 total checked boundaries. Original activation methods execute 15 standard,
15 museum and 18 Blues calls. Both behavior captures match the retained sequence
corpus exactly. Original owner/mover/limits/sequence/follow graphs, helper closures
and both static resources remain unchanged. Resolved input, getObject miss,
flyingBird identity, prediction, transform and publication are controlled inputs.

All default Debug, Release and Windows targets build; 40 strict Debug sanitizer
checks (17.71 seconds), 40 Release checks (3.16 seconds) and formatting pass.
Controls cover missing/rejecting resolvers, nil targets, zero/default compensation,
cancellation before reset, failed cancellation and later service rebinding.
Private provenance is `analysis/input/camera-shot-sequence-native.json`; complete
method extraction metadata is `analysis/camera/camera-activation-chunks.json`.
Actual behavior registration, full launch scheduling, live bird/world/radius
ownership and museum boomerang specialty activation remain open.


A runtime follow-world bridge now supplies real impact prediction, named world
queries, physics-to-world coordinates and the terminal sequence service to the
owned camera. It preserves captured prediction position and refreshes live
launch/gravity values after radius callbacks. Binding ownership is explicit:
its host refers to its own stable address and owns no resources. Constructed
owned-level controls cover locked queries, real predicted hits, transforms,
sequence identifiers, input cancellation and failed initialization.

All default Debug, Release and Windows targets build; 40 strict Debug sanitizer
checks (17.83 seconds), 40 Release checks (2.97 seconds) and formatting pass.
These checks initially established C integration alongside the independent
component captures; the complete constructed native comparison is recorded below. Private control provenance is
`analysis/input/camera-follow-world-controls.json`; its next capture plan is
`analysis/camera/camera-follow-world-next.json`. Full game object/launch/radius
ownership and scheduling remain open.


The camera/world bridge now matches the complete original camera, predictor and
physics-query composition. One real original LuaState wrapper owns the shared
VM; the unchanged camera/follow graphs call the unchanged PhysicsQueries graph,
whose original GameLua.raycast executes against real native constructed worlds.
Original body/fixture constructors, shape cloning and world destruction remain
active. Zero fixture user data produces actual nil names. The original
CoordinateTransforms.physicsToWorldTransform also executes. No synthetic hit or
prediction result is supplied. Radius getters are controlled numeric receivers.
The world remains frozen while original camera updates and sequences advance.

All 2,177,936 words match across 48 worlds and 7,680 updates: 142 predictions
(80 retaining a hit), 40,248 real rays (66 with NaN endpoints), 26,832 radius calls,
142 count prints and 91,421 total checked boundaries. Comparisons include every
query endpoint, actual result arity/point/normal, prediction result/time, controller
state, mover state, viewport publication and sequence transition. The corpus
contains 960 ray hits, 76 custom Y/zoom frames, 12 retained-curve frames without
following, 22 hit-before-apex frames, 288 moving-input frames, 144 shot-resource
loads and 103 end-resource loads. Both captures repeat exactly. The first 32
scenes retain the preceding captured scene payloads unchanged.

Caller checks retain original prototype/upvalue/instruction/PC/arity/identity
validation, including ray helper 22:19, predictor radius 115:58/73 and print
115:109. Native construction definitions are compared as well. C runs its own
camera/world bridge, predictor, tree and shapes. All default Debug, Release and
Windows targets build; 41 strict Debug sanitizer checks (19.76 seconds), 41 Release
checks (3.38 seconds) and formatting pass. Private provenance is
`analysis/input/camera-follow-world-native.json`. Frozen constructed worlds,
controlled bird values, input and radius receivers define this scope; actual
game object/launch/radius ownership, behavior registration, physics scheduling
and full rendering parity remain open.

The same constructed camera/world graph now runs the complete original
GameObject.init/get, ModeSystem, MuseumMode, ObjectFunctions and definition
lookup for every radius query. Original init links the actual physical and
script tables; the checked radius wrapper invokes the real getter and records
its returned numeric bits. C uses the owned level-property projection instead
of a numeric getter stub. All 2,177,936 words, 7,680 updates, 91,421 boundaries
and 26,832 radius reads match. The entire payload matches the prior world
capture; only the identifying header/footer differ. Both captures repeat
exactly, and the shared original-method loader also reproduces the separate
2,048-case property corpus unchanged. All default Debug/Release/Windows builds,
45 Debug and 45 Release related checks and formatting pass. Private provenance
is `analysis/input/camera-follow-properties-native.json`. This closes the
constructed graph's controlled radius-service gap. Bird pose/launch values,
catalog data, input and world construction are still explicit fixture inputs;
live gameplay ownership, registration and scheduling remain open.

A runtime level-camera owner now owns stable per-object follow targets, its
camera system, reusable mutable sequence stages and real property/world service
bindings. It refreshes from canonical cached poses and the current physical
launchVelocity, leaving that field absent until explicit post-onLaunch
publication. The level owns the first vector table, so closing the camera keeps
borrowed property references valid. Resource loading precedes both original
sequence messages and playback; missing logging retains a completed load.
Named world targets precede the flyingBird alias. C controls cover late launch,
real prediction, cached pose refresh, source/service limits, named precedence,
removal, Blues cancellation and teardown. Two whole-owner constructed native
comparisons now match 1,762,576 words across 15,360 updates, 21,463 exposed
service boundaries and 19,734 real rays, including 3,342 hits and 144 NaN rays.
The live-input corpus leaves launchVelocity absent through all 48 first
sequence loads, publishes it after onLaunch and subsequently refreshes cached
poses and existing vector fields. Both captures repeat exactly. All default
Debug/Release/Windows builds, 51 Debug and 51 Release related checks and
formatting pass. These use frozen three-body worlds, explicit camera/input
configuration and controlled pose/vector publication. Native fixture names are
nil from zero userdata; C retains canonical names, which Follow does not use.
Actual first-level frame integration, later launch-vector generations, the live strongest-goal
selector, catalog behavior registration and frame scheduling remain open.
Private controls, native provenance and the next integration plan are recorded
in `analysis/input/classic-camera-controls.json`, `classic-camera-native.json`,
`classic-camera-live-native.json` and `analysis/camera/classic-camera-next.json`.

Two independent cold launches now also provide actual first-level camera/world
comparisons. Fresh GameLua identities come from the real update entry, with
paired entry/return breakpoints and selected read-only fields. The APK-loaded C
level, interpolated property publication and camera owner match all 25 camera
boundaries and 24 physics update pairs in each capture: 72 steps, 40 collision
callbacks, four removals and score 10,070. The initial launch camera elapsed
inputs differ (approximately 32.543 and 17.192 ms). Both use actual APK snaps,
catalog and sequence resources; the C predictor's hit position/time match the
live follow state. Its one prediction issues 51 C rays, whose individual native
calls are not traced by this capture. Initial camera position, launch elapsed
time, queued poses, initial logical scalars and activity/specialty flags remain
explicit inputs. The second cold run contains an additional screen tap, with
the resulting specialty flag preserved. These snapshots do not compare native
viewport readback, register behaviors or establish the complete outer frame.

A separate same-process retry capture remains a failing comparison. Native
proxy identifiers and settled poses differ from the fresh C world, producing a
different predicted hit position before the first compared update. This agrees
with the earlier controlled retry identifier experiment; it is not a camera
tolerance adjustment or a passing retry result. Retain the unique capture and
connect actual pool/reset initialization before accepting it. Private
provenance is `analysis/debugger/level-camera-native.json`.

## Owned ordinary sling and bird queue

`AbClassicSling` now executes ordinary mouse selection/stretch/release, cancel,
held-body setters, default transfer/reload and the separate post-shot spring
phase against the existing level and shot owners. Selection stops the actual
camera's automatic movement and clears mover targets before assigning the bird.
The pinned Slingshot prototypes are 4214, 6245, 10773, 11295, 11591, 16373,
16540, 17049, 17173 and 28331; CameraSystem.stopMovement is 16051. The spring
helper is utils prototype 9132. Original roots stay private and unchanged.

The owned integration retains the native first-shot body comparison, then
reloads and fires both remaining birds through mouse input, real listeners,
statistics and decoded audio. It checks cancel, canonical held positions,
camera stop, all-birds delivery and retained vector aliases across reloads.
Bootstrap and camera-distance values are fixture inputs. No audio device opens.

Another 64 original full-method cases compare both band coordinates, angle,
distance, spring speed and previous distance after original reset/stretch setup.
They cover negative/zero time, the 0.05-second edge, repeated substeps, radial/
vertical limits and zero distance. The repeat is identical, SHA-256
`210ae24ad65af7c7a6f921a46728b481b5d7f2bdbd8000c12135dd4c924579cf`.
Capture only reads original scalar upvalues; controlled external camera/audio
services provide fixture boundaries. No original instructions or upvalues are
patched. Private provenance is `analysis/input/sling-wobble-native.json`.

The owner covers fixed initial objects and the ordinary default sling. Reached
hat work, custom reload/scope modes and exhausted storage reject explicitly.
Complete frame/contact/removal integration, playable sessions and original
all-level/pixel comparisons remain open.

## Canonical constructor and damage-visual state

The character owner now reads damage, blink and smile names from the live
physical object when selecting a face. Previously its startup snapshot could
restore an undamaged pig sprite after a collision. Gamelogic prototype 137493,
instructions 49/71/85, reads those current fields at each selection; the bird's
return-to-base face uses the same live damage field. A regression reproduces
the old failure after a real collision, then checks twelve blink transitions and
a smile. Additional APK integration cases retain each damaged health range,
including the final range before death. Release, ASan/UBSan and Windows cross
compilation pass; existing original animation traces remain matched. This is a
state/asset binding correction, without a new full-frame pixel parity claim.

The level now publishes selected raw constructor fields alongside existing motion
and shot fields. Native `GameObject::setupLuaTable` is ELF address `0x344b74`;
ObjectCreation prototype 883 supplies resolved material/collision values, while
MuseumMode prototype 2711 supplies the generated ground's separate assignments.
Script mass retains its creation value after a queued bird's body becomes static.

Seven typed fields match all 23 objects in the retained post-load startup capture:
controllable, strength, levelGoal, startNumber and damage/blink/smile names. This
is 161 field comparisons, with native numeric tags mapped explicitly to C types.
The earlier snapshot precedes the final queued bird's blink/smile assignments;
the completed-load check uses the retained next-update snapshot. Corpus SHA-256 is
`957ac810a673b4ce2d50363feb9c5a5e414c7bb00bf30df972427f565eb2d3bd`.
Private provenance is `analysis/input/classic-constructor-fields-native.json`.

Runtime getDamageSpriteNames/getDamageSprite, prototypes 151134/151699, now read
the same physical table and retained catalog range layouts. The visual owner
preserves prior sprite strings in a bounded immutable pool. C integration checks
cover health/initial-strength changes, canonical ownership, missing definitions,
frozen/skip gates and actual sprite changes while leaving the collision caller's
damage-name cache untouched. The underlying selector retains its separate native
comparison; these integration controls do not establish a complete collision or
frame replay. Mutable animation/range construction, insertion and full session
ownership remain open.

## Owned block collision binding

`AbClassicBlockCollisions` binds the separately native-matched block callback
(CollisionCallbacks prototype 6139) to the level's canonical physical tables,
owned behavior dispatcher, supplied BlockScriptManager attachments, actual
damage-visual owner, shared audio bank and bounded score state. It preserves
cached definition identity, current sensor/script fields and canonical
damage/blink/smile publication before sounds and particles. Typed audio-name
selection preserves borrowed spans without requiring C string conversion.

The original GameObject method, prototype 1776, forwards seven slots: other
name, force, damage, normal x/y and point x/y. The block-script collision event
receives the actual other physical table followed by force/damage/normal x/y.
These differ from the two physical-table arguments of the bird method.

The composed APK test changes pig health/art, queues actual decoded contact
audio, appends damage scores, exercises second-sensor recursion and both-sensor
early return, and retains canonical sprite/cache writes when the level-effect
service is deliberately missing. The fixture selects the base effect policy
explicitly and verifies that none of its initial definitions accepts the three
shipped component predicates before constructing empty initial attachments.
Additional recording handlers test the real event/method packet paths. Its pig
definition emits no damage-state particles; this is not a particle-simulation
or native complete-session comparison.

The same owner now binds ordinary non-bird/non-glue contact damage to
`AbClassicContacts`. MuseumCollisions prototype 2030 and its helpers 2681/2915
supply the ground/static predicate, script getter flags, cached velocity/mass,
ordered health/dead-map writes and joint-break boundaries before the callback.
The integration control steps the real level world with a pig/ground overlap,
deliberately changing cached script velocity while preserving zero engine
velocity. It verifies lethal health, the canonical physical-table removal value,
damage art and a single forty-point score append. The setup isolates that pair;
it is a C integration control, with the numeric helper retaining its separate
native coverage, rather than a captured native gameplay replay.

After separating that pair, the listener also runs the resolved empty end
method. GameLua prototype 146894 delegates through ModeSystem prototype 2346;
MuseumMode inherits PlayMode prototype 1437, whose sole instruction is RETURN.
Other mode overrides remain separate.

`AbClassicMuseumCollisions` now resolves live definition colliders and binds
the Museum single-bird and bird-pair numeric routes alongside the real ordinary
block adapter. Source helpers are prototypes 280/3297; MuseumMode prototype
2612 forwards without adding collision flags. Single-bird controls retain lethal
health/dead-map writes before a deliberately rejecting full callback, leave the
contact response unpublished on that failure and verify the original
point-to-normal/nil-point mapping. Pair controls retain both original geometry
vectors. Collision flags remain the full bird callback's responsibility. The
real pig/ground begin/end test also passes through this dispatcher. These are
owned-state C controls over separately matched numeric helpers. The bird callback
binding below continues that boundary; glue handling, actual removal, active
effect selection and full frame scheduling remain open.

## Owned bird collision binding

`AbClassicBirdCollisions` composes all recovered bird-callback phases over the
canonical physical tables and actual shared registries. It publishes changed
flags, damage names, specialty availability and score-list length before service
boundaries, then refreshes live values after successful services. Immutable
catalog definition rows preserve the notification's cached identity. The behavior
receiver is independent of its two physical-table arguments, including the
noncontrollable second receiver's original first/second packet.

`AbFloatingScores` owns score text, descriptors and the active list. Source
prototypes 145853/152638 supply positive-score admission, quarter-second scale
branches and removal only after time strictly exceeds lifetime. Retained records
and copied strings stay stable until close. `AbClassicCollisionAchievements`
implements AchievementProcessor prototype 1331 over real settings/EventManager
owners: bird-pair collision flags, equal-parent suppression, eagle/bait boundary
flags and both bullseye branches. SettingsWrapper prototypes 1503/1588/1684
confirm the wilhelmTell/bullsEye fields and the setters' absence of a save call.
The inactive flyingRedBirdCollided gate comes from prototype 9526; the active
red-special branch still rejects explicitly.

The APK composition fixture verifies positive damage scoring without duplicate
global score accumulation, copied floating text, collision sprites/flags,
bullseye event delivery and a real physics begin/end through Museum dispatch and
the full callback. It creates fresh behavior instances for this independent
fixture. The direct damage call has zero force; an isolated world contact has
zero cached momentum. A second real contact deliberately sets lethal cached
velocity while keeping initial engine velocity zero. It kills the pig, publishes
the removal-map entry, scores once, queues decoded contact audio and emits owned
red-feather/smoke particles through the binding below. Separate
controls verify receiver/table arguments, projectile return before unused NaN
force, and score/text retention before a deliberately missing achievement
service. Debug sanitizers and Release pass all eight focused checks, all default
platform builds pass, and formatting passes. New floating/achievement services
have source evidence and C controls; their native execution comparison and a
complete native gameplay replay remain open.

## Owned ordinary particle emission

`AbClassicParticles` preloads explicit packed particle definitions through the
first-party static reader, then binds addParticlesNew (gamelogic prototype
147816) to canonical world pose/size, retained descriptor hash order, the shared
Lua amount RNG, actual body angle and the owned particle engine. Ambient/damage
requests reach the same engine. Resource decoding occurs before frame work;
missing sprite lookups retain real missing-resource handles.

`AbEngineRandom` recovers the separate native math::RandomUtil stream. The seed
and next methods are Binary Ninja VA 0x866f80/0x867040 in the pinned view. Its
4096-word state, carry fold, lazy default seed and binary64/float conversions
match 128 native checkpoints across default/explicit seeds and wrap. This is
separate from the Android/Lua random48 stream.

`AbParticles` owns bounded normal/alternate arrays and prepared definition
caches. The initial supported path covers ordinary static sprites, box emitters,
randomized birth position, velocity/gravity, scale/angle and strict lifetime
removal. Native create/update methods are Binary Ninja VA 0x3050e4/0x304cd4;
addresses here include the recorded view base. A directed probe uses the original
class/cache/resource-query boundaries with actual decoded
redBuff/smokeBuffBirdsDefault data and prepared non-null sprite handles. Twenty
records across creation and six phases match all 340 state words; the repeat is
identical. The normal update removes positive pre-update world y; the alternate
selector uses a different vector and skips that gate. The first probe selected
that other vector while observing the normal vector; it was corrected before
comparison.

The composition test uses actual atlas queries and emits particles on the
lethal bird contact, then advances the owned list. This verifies live emission
and state ownership; it does not verify particle pixels. Circle/target emitters,
animated/composite sprites, level-layer particles, broad admission coverage,
draw/alpha policy, resource-group selection and full native frame comparison
remain open. Debug sanitizers and Release pass nine focused checks. Default
platform builds pass after explicit GCC scalar/aggregate initialization fixes;
formatting passes. Private provenance is
`analysis/input/runtime-particles-owned.json`.

## Open evidence and validation gaps

- Initial startup, main-menu, and episode-selection captures are retained in
  the isolated reference AVD. The user supplied no device or prior captures;
  the environment was prepared during this work. Full gameplay, audio,
  save/reload, results, timing, and replay evidence are still incomplete. See
  [reference environment](android-reference.md) for its exact configuration.
- Complete script integration, joints, remaining numeric domains and service
  behavior remain open.
- A playable game and persistence runtime remain unavailable. The audio
  bank is owned; 24-kHz playback, stream timing and native device integration
  remain open.
  GLES2 has an exact retained idle-scene comparison; full scene/presentation
  parity remains open, including the Metal idle differences described above.
- Incomplete native capture and replay coverage leaves the phase 1 fidelity
  gate open while independent resource and code work continues.


The owned particle path now includes the reached smokeBuff circle emitter and
lifetime sprite animation. An isolated execution of the original Particles
class, using the decoded target definition and a prepared non-null sprite host,
matches all 510 state words across three definitions and six phases each.
The third definition contributes ten records/170 words; its zero-speed circle
still consumes the native random draws and its animation starts at
SMOKE_CLOUD_1 without a random sprite draw. It advances through the measured
frames and expires strictly after lifetime. This does not establish arbitrary
circle-speed, composite, target-emitter, draw-alpha or particle-pixel parity.
The retained original output is runtime-particle-common-native.txt, SHA-256
29214946bacd4e9dfc34e2eb4bb792f89afe5c8cc109a92176dedcd26c672aec.

ObjectCreation prototypes 883 and 10961 select objects.dynamic by the resolved
collider classification, independently of a queued body's current static physics
type. The level now owns that separate canonical table; generated ground skips
it. All 592 catalog construction/render/behavior/simulation/character/shot/damage
query rows still pass the retained audit, with 18 levels retaining missing visual
names. This is construction/query coverage, not all-level gameplay validation.

The reached ordinary removeBlocks path now has an owned canonical binding.
It dispatches the real behavior onDestroy, normalized constructor/material
lookup and breaking score, typed catalog audio, copied floating score/sprite
metadata, shared script RNG, legacy/new particle emission, native physical
removal, joint/teleporter suffixes, then world/dynamic/goal/dead cleanup. Pinned
metadata survives body destruction and map erasure. The physical adapter keeps
the actual creation-order native bird vector, resets the shared rolling
accumulator, removes the scene member, detaches the ordinary visual and forgets
the level slot. Native Scene.remove(RenderableObject const&) at Binary Ninja VA
0x2e4e08 selects its current layer and exact identity; it does not use the numeric
Lua renderer ID. Retain/release accounting includes the object, scene and
removal's temporary reference. Scene/resource metadata stays owned until close.

Legacy addParticles prototype 147263 uses live world geometry and body angle;
its dense list path carries a truthy definition amount forward to later entries.
The pig's ordinary string path reaches the real smokeBuff engine. The floating
queue now retains absent text and copied sprite identifiers for .9-second
bitmap scores, alongside the existing .6-second text path.

New source-backed services implement updateDestroyedBlocksValues prototype
134861 and checkLevelGoalsDestroyed prototype 150347, plus removePig prototype
107487's continuation after removeBlocks clears world membership. Material
notification precedes the actual SettingsWrapper local increment; device sums
and the aggregate event follow. Eagle/device/gamerService gates remain explicit
shared inputs. Goal notification precedes the sent flag; the continuation then
writes lastScoreTime conditionally and oldScore unconditionally. The goal-only continuation rejects a still-live world member. The removal
owner now implements that separate standalone removePig prefix: shared breaking
score/append, private removal notification, physical/scene removal, then world,
dynamic and goal cleanup. It omits removeBlocks floating/statistics/particle
phases, as in the source. An additional live-object control checks this distinct
+500 path and repeated-removal idempotence, then advances another real fixed
step with the shared rolling audio bank. Native full-method comparison of these
new services is still open.

The actual APK fixture starts from the preceding lethal bird contact and runs
one fixed simulation step with an installed real contact listener. The recovered
phase host resolves disabled water from the loaded level, runs the supplied
script dispatcher, removes the pending pig after physics, then flushes surviving
native bird velocity proposals before history/post publication. It checks 5,000
points, the retained 5K_GREEN score record, actual animated smoke, removed body
and renderer, cleared canonical registries, ordered real counter/event delivery
and one goals-cleared event. It explicitly selects base level sound policy,
gamerService-enabled counter delivery and absent level/removal hooks. These are
C integration controls; they do not claim an original complete frame, actual
platform-service availability, native event listener coverage or playable Metal.
Required active water, explosions, rockets, gifts, spawns and callbacks reject
when their concrete service is unavailable. Complete root/session scheduling,
dynamic creation and native launched-frame comparison remain open. The reached
normal particle drawing route is recorded below. Private records are
runtime-removal-owner.json and runtime-particles-owned.json.

The removal adapter shares scoreTable.blocks.score with the block and bird
collision owners. Collisions append collisionScores; destruction and standalone
removePig append the separate individualDestructionScores list.
It publishes direct removal writes before callbacks and refreshes callback
changes afterward, including failure. The integration retains its previous
80 points when adding the pig's 5,000 and the standalone control's 500. The
separately recovered updateScore root sum and full session globals still need
an outer controller binding. All 592 construction/query audit rows pass after
the rolling-map/lifecycle changes; 18 missing visual-name levels remain.


The reached normal world particle vector now produces shared clip commands.
Original Particles::drawParticles (Binary Ninja VA 0x30155c), its normal-vector
wrapper (0x301a38) and original Sprite.draw execute in an isolated ARMv7 probe.
The decoded feather/default-smoke/animated-smoke definitions supply the existing
creation/update inputs. A prepared ordinary Sprite and context/image virtual
boundary capture 30 draw records across six phases per definition. All 870
render-state/rectangle/UV/color/opacity words match C; the native repeat is exact.
SHA-256 is 1bf1040473eac66df13e78236b895de0b6d93efa45451c925704af39208566d5.
The prepared metadata uses a 64x64 texture, rectangle (8,16,13,17), pivot (3,-2),
nonuniform camera zoom (.75,1.25) and explicit inherited color/opacity. An earlier
independent-UV prepared capture is retained separately, not used as a region
geometry comparison. No original instructions are patched and no GPU is opened
by this probe.

The C draw path keeps camera translation separate from particle position before
scale division, then preserves native pivot/angle/scale order. It does not add
lifetime fading. Cached absent resources skip; a retained resource requires its
unchanged preloaded provider binding. It appends the normal vector in order
through the existing native-matched Image rectangle/clip route with explicit
inherited material, and restores the list prefix on failure. The composed
lethal-contact/removal check uses its actual decoded atlas to submit visible
feather/smoke commands. This verifies owned data-to-command integration; original
GPU pixels, full Metal frame/order/material parity and menu/level-layer/composite
particle paths remain open. Provenance is runtime-particle-draw-owned.json.


The ordinary bird lifecycle now binds the source removeBird path to canonical
behavior destruction, definition hook, hat records, particles, camera, audio,
physical/scene removal and world/dynamic/birds/otherBirds cleanup. The Museum
retirement phase uses the loader's remove_time=1 and binary32 threshold
0x3b23d70a. Timer writes precede the camera query and actual removal. Launch
creates a fresh otherBirds table after the optional all-birds notification;
bounded retained generations preserve old aliases. The C composition retires
one stopped bird, emits two smoke and six feather particles, clears shared
current/flying/specialty state, retains its collision credit, then advances
another real fixed step over surviving bodies. New concrete bindings are
source-backed; the separately recovered generic retirement/removal methods
retain their existing native comparisons. Complete native method/session
comparison and active timed abilities remain open.

Museum loadLevel prototype 2711 instructions 239..259 initializes scoreTable
with blocks then birds. Collision callback prototype 489 creates named score
records at its positive-damage boundary. AbClassicScores owns this string-map
membership and retains records after physical removal. updateScore prototype
136245 sums each record's score in hash-slot order. It never resums
collisionScores or individualDestructionScores. The composed total is 5,640:
5,580 shared block points plus 60 named collision points. The concrete ending
binding now awards an actual 10,000 unused-bird bonus, producing 15,640. This
proves C composition and source ordering, not native complete-session scoring
or full-frame bonus scheduling.

AbClassicController now binds the native-matched ordinary GameUpdate tail to
actual sling, retirement, camera, character, floating-score, root-score,
block-script and GameObject behavior owners. It uses the measured initial world
order, skips removed keys and queries current script membership at the later
script boundary. GameObject.update prototype 1539 delegates update(seconds,time)
through the actual behavior map. Boundary prototype 108362 reaches the owned
standalone removePig path for a noncontrollable goal. Active animation, rockets,
boomerangs, custom object effects, mighty/red effects and joint operations still
require their concrete selected implementation. Missing reached services fail.

The base object effect's updateObject prototype 782 is empty. The separate
level-effects registry follows level_effects.lua prototype 1815's dense list of
update(self,seconds,time) calls. CameraNative.updateGFXEffects (Binary Ninja VA
0x2e3fd0) visits renderers in layer/vector order. GameObjectRenderer.update
(0x2dd69c) updates its optional primary effect then its dense effect vector;
the RenderableObject constructor (0x2de77c) initializes both absent/empty.
GameObjectRenderer's constructor (0x2dd8dc) preserves those defaults.
LuaAssetRenderer.update (0x2d772c) is empty. The owned ordinary scene currently
has no GFX attachment API and implements this actual empty-effects shape.
Adding attachments must extend that phase before publishing effects.

The APK composition now completes the bound controller tail, including automatic
retirement, root score and actual surviving object/particle draw-command
consumers. It uses one stable sprite provider for scene and canonical visuals;
separate wrappers around the same atlas do not satisfy their ownership contract.
Startup timers use an explicit fixture seed. An unshot bird has an explicitly
expired face timer; the level-effects registry is explicitly empty. The draw consumer
omits theme/HUD/floating-score rendering and does not claim the complete original drawGame function or
GPU pixels. A deliberate later draw failure preserves earlier state and makes
that controller reject further updates. The input prelude, native engine
particle scheduling, complete bootstrap and playable Metal remain open.
Private lifecycle/controller provenance is runtime-controller-owned.json.


AbClassicCharacterServices binds the character controller to real camera/sling
positions and decoded audio. updateCharacterAnimations prototype 137493 uses
physical goal x/y*20, or SlingshotSystem.getLevelStartPosition*20 for birds;
CameraSystem.getScreenX/getScreenY prototypes 4455/4530 return the current screen
center. utils.vLength prototype 9192 squares/adds as floats before math.sqrt's
promoted boundary. Voices select getAudioName before playAudio(gain,false,0).
Museum loadLevel instructions 161..164 select bird_misc and piglette; the binding
borrows those live typed globals instead of embedding a replacement voice.

The ordinary checkLevelFailed prototype 150733 first respects prevent-ending,
then timeout/motion/literal doNotWait gates and the actual remaining birds map.
A truthy controllable bird returns false before BurnManager.isBurningObjects.
The original concrete BurnManager status method is bound at that final query;
hats retain a required actual update boundary. The C composition checks
live-bird loss suppression, timeout bypass and prevent-ending precedence, advances a real
unshot bird's face timer through the controller tail without suppressing drawing,
reads camera/sling distance and queues actual bird_misc audio. These new bridges
are source-backed C integration; full native service/frame comparison remains
open. No native audio output device is opened by this check.


The complete original BurnManager.lua module now supplies an independent native
comparison of the new binding. Twenty-four consecutive original addBurnTime,
update, reset and isBurningObjects calls match all 96 state words in C; the repeat
is byte-identical, SHA-256
88de75775367d1c39ead7b1d0c101e7cd653d1fa8b5f8cbf8c3aa13f479323b2.
Add keeps the largest pending duration; update assigns time+duration, clears the
pending request and resets duration to zero. Deadline expiry is strictly less
than current time. Negative times, signed zero, unused nonfinite seconds and an
unused nonfinite time are included. Standard tostring's discarded string result
has no observable effect in this closed binding.

isBurningObjects reads a separate false upvalue. The original root's closure
captures show that no method writes it; native queries remain false even while
a deadline is active. The ordinary loss bridge uses that actual behavior.
Stripped debug upvalue names prevented lua_getupvalue from observing the state;
the successful probe reads checked closed TValue slots using the ABI rechecked
at Binary Ninja VA 0x83b2c4. It changes no original instructions, prototypes,
closures or upvalues. Four strict Debug/Release checks, default platform builds
and formatting pass. The earlier GameUpdate prefix still needs to schedule the
manager update; its init diagnostic remains bootstrap work. Private provenance
is runtime-burn-manager-owned.json. Complete native gameplay/Metal frames are
not established by this module comparison.


AbClassicTrajectories now owns the ordinary track and point-registry projection
through actual ordered scene renderers. It composes the existing native-matched
follow update and LuaAssetRenderer/scene/ID methods. New source-backed bridges
cover addPuff's BIRD_SPECIAL/layer-3 defaults, copied world pose, Museum and Blues
service adapters, keep lists with nil holes, and ordinary clear/disposal. Special
trail classes and registration remain separate and are not silently substituted.

The original renderers[id] dictionary uses the exposed float number. A new
rounded-key alias replaces its registry entry without disposing the old scene
renderer. Numeric disposal then uses the original ARM integer conversion and
can remove another entry or miss. The owner preserves these separate lifetimes.
Ordinary disposal calls for distinct converted registry IDs commute in this
closed binding: each removes at most the first matching ID and preserves other
scene order. Rebound disposal functions/external registry observers remain
outside the binding; this is not a native arbitrary numeric-table traversal claim.

The existing large-ID APK composition now binds the real Museum behavior launch
to the trajectory owner and actual decoded camera sequence loader/logger.
Moving the real body/cached pose emits TRAIL_WHITE_1 through the shared provider;
a default BIRD_SPECIAL puff creates a second scene renderer at the same exposed
float key. Automatic retirement invokes the actual behavior's remove and keep
calls before body/scene removal. It drops following and the current unattached
registry entry, while the older scene trail remains as in the original lifetime
split. The controller's actual object/puff/particle draw consumers submit the
survivors. Ten strict Debug and Release checks pass, including retained native
follow/Blues/renderer/scene/ID corpora; all platform builds and formatting pass.
New complete-manager and full native frame/session comparisons remain open.
Private provenance is runtime-trajectories-owned.json.


AbClassicGameEffects now schedules GameUpdate prototype 110400 instructions
62..79 against the actual shake/RNG, Hero screen-tint update, trajectory and burn
owners, in that order. The composed check emits the real trail and establishes
the burn deadline, then passes actual shake offset to the later camera. Hero
update prototype 265 rereads the live tween before updateValues and the final
finished gate; callback replacement and partial failure are preserved. Active
tween creation, tint descriptors/listeners and overlay rendering remain open.
Six strict Debug/Release checks, all default platform builds and formatting
pass. These new source-backed bindings do not establish a complete original
GameUpdate, root wrapper, native frame or playable Metal session.


AbClassicGameGlobals supplies canonical typed clock, score, timeout, tutorial
and specialty state to goals, shots, collision/bird life and controller bridges.
The pinned script inventory has one global time write: gamelogic.lua root update
prototype 57000 instruction 297. This follows native physics/removal and early
root menu/process/animation services. Nil/false starts at zero; later updates add
seconds. Native frame time and updateGame gameTimer remain distinct. The root
passes global time to currentGameMode at instructions 400..403. The composed
check preserves removal lastScoreTime=3 while the later script clock reaches 4.

Launch publishes lastScoreTime/oldScore before the all-birds event, and resets
shared extraTutorialTimer before later services. Collision and retirement share
the same specialty flag as later camera/input views. Root score publication
updates the same typed score read by goal continuations; callbacks refresh the
shot numeric view without type-punning. The runtime binding requires finite
numeric clock/score/tutorial fields initialized by its actual bootstrap.

AbClassicGamePrelude binds GameUpdate 110400 instructions 0..141: level time,
the required active cheat notification, canonical definition/tutorial lookups,
early effects, timeout, and old/current cursor publication through the actual
previous camera viewport. SettingsWrapper prototypes 19300/19414 initialize
tutorials only for nil and repeat the item lookup before reading showHelp.
The check uses the shared settings graph, verifies lazy creation and a restored
help record, emits real trajectories, updates burn state and then executes the
owned controller. A missing reached toy notification retains level-time writes
and stops later prelude calls. Seven strict Debug/Release checks, all default
platform builds and formatting pass. These source-backed bindings and controlled
prefix calls are not a complete original native wrapper/frame comparison.
Remaining startup/music/update callbacks, input/ending phases, runtime bootstrap
and interactive Metal remain open. Private provenance: runtime-prelude-owned.json.


AbClassicLevelFinish binds initLevelComplete prototype 104369 to actual retained
EventManager payloads, default sling cancellation, canonical current-bird fields,
getNextBird, the original LevelEndSequence and shared decoded level-clear audio.
The end resource disables input, then moves to the bird snap over one second
without input interruption. Mutable stages retain their original property tables
and use preallocated capacity. The completed flag publishes before metadata
resolution. The test observes notification before cancellation/completed and
checks rejection of reentry, actual physical position reset and real audio.

initLevelFailed prototype 105431 accumulates its timer without resetting. Only
strictly above 1.5 seconds, and without a truthy eagle flag, it sets the ending
timer to 0.5 and completing true before direct Resources audio. Repeated eligible
calls preserve repeated sound behavior. The ordinary win query prototype 149936
checks prevention and a literal-true eagle/timer prefix before actual goals, then
reads current timeout/motion/literal-true wait state after notification. A
controlled notification resets the sent flag and changes motion during dispatch;
the win query sees that change. Natural goal destruction had already notified
once. Loss and win now share the same canonical timeout global.

Seven strict Debug/Release checks pass, including the retained original complete
transition and both camera corpora; all platform builds and formatting pass.
These new concrete bridges are source-backed C composition, with explicit
fixture controller inputs and no output device. Full native bound methods,
original GameUpdate scheduling, UI/persistence and interactive Metal remain open.
Private provenance: runtime-level-finish-owned.json.


AbClassicLevelEnding binds updateLevelEnding101384/beforeLevelEnding104006 to
actual getNextBird, root bird score and a separate bounded bonus append list.
The strict negative bonus boundary preserves the ordinary 10,000 award, decoded
voice and copied canonical-position/definition floating score. Literal-true eagle
suppresses only score/append, preserving voice/floating effects. The binding
executes the before-ending hook gate, delay/extensions, canonical timeout, actual
win query and queued EID_LEVEL_ENDED records with retained distinct identities.

The fixture explicitly starts index1 after its earlier three-shot sequence has
retired bird3. Bird1 earns 10,000, yielding 15,640; a controlled eagle flag on
bird2 preserves effects without credit. Controlled calls cover exact zero bonus,
skip with ignored NaN, before-ending delay, repeated payload identities, late
win and prevented loss. Five strict Debug/Release checks, all platform builds
and formatting pass. No audio device opens. These are source-backed module
calls, not complete native ending/frame/session validation. Actual GameUpdate
scheduling, results UI/persistence and interactive Metal remain open. Private
provenance: runtime-level-ending-owned.json.


AbClassicGameProgress now binds GameUpdate110400 instructions312..360 to the
actual finish, ending, win/loss and EventManager owners. A positive ending timer
advances the actual ending. Otherwise loss is queried after completion and can
overwrite the earlier announcement decision. The trigger publishes before the
retained EID_LEVEL_WILL_END notification. Live cheat/key gates preserve Lua
truthiness and literal-true quadClick semantics. The composed fixture controls
prevention/cheats, positive bonus advancement, simultaneous win/loss and prior
trigger suppression. Reentry rejects; a deliberate late listener failure retains
completion/failure/trigger effects and stops further calls. Five strict
Debug/Release checks, all platform builds and formatting pass. This is
source-backed scoped scheduling, not native full-frame/session validation.
Startup/music/update callbacks, remaining input, root bootstrap and complete
Metal startup/GUI/gameplay remain open. Private provenance:
runtime-game-progress-owned.json.


AbClassicGameStart binds GameUpdate110400 instructions142..311 to sweep resets,
quad timer, actual sling-disabled state, direct decoded level-start audio/menu
stop, queue reset and pending-aware script ambient playback. The source checks
isAudioPlaying against Boolean false, then requests gain1/looptrue/channel7.
It runs the actual bounded string update registry and optional resolved level
hook before rereading the startup timer. Values in inclusive1..6 enable physics
and set timer10; others advance by seconds. Binary Ninja confirms the native
setPhysicsEnabled setter directly stores its Boolean field. Reached active
debug/editor operations require their actual service.

AbClassicGameUpdates preserves the initial nil upvalue until clear, duplicate
add behavior, nil-key removal and string-map pairs order. Two preallocated maps
retain the captured iterator table across repeated clears; callbacks then mutate
the current table. Iteration resumes after the current slot of the last key.
Pinned APK scans identify consumers including delayed callbacks, balloons and
active abilities; selected bootstrap/registrations remain open. The composed
startup probe uses controlled registry/level callbacks with the actual sling,
shared bank and mixer. It verifies audio/queue/registry/hook/physics order,
pending ambient suppression, callback timer6, disabled sling and failure at a
missing reached debug service. Separate registry checks cover retained old
iteration, repeated clear, duplicate add, self-removal, reentry and late failure.
Six strict Debug/Release checks, all platform builds and formatting pass. These
source-backed bindings do not establish native complete startup/frame/session
parity or interactive Metal. Private provenance: runtime-game-start-owned.json.


AbClassicGamePowerups binds GameUpdate361..380 over PowerupManager.update5418
and isTransitioning8459. Truthy enabled gates manager work; nonempty active state
plus positive speed sets skipInput first. Selected cheat1 queries the actual
current-index bird; other cheat keys require actual activation. Active entries
cancel real sling drag, then invoke their methods in captured numeric-array
order; the current passive list follows. Dense removal/shift can skip the row
moved into the current key, preserving pairs behavior. Distinct gameTimer reads
callback mutations and advances after manager work, including disabled frames.
Mouse now also refreshes its disabled query from the actual sling state.

Controlled active/passive records verify real drag/position cancellation,
input gating, dense mutation, passive timer/skip changes, ignored unused NaN
speed and late failure before passive/timer advance. Numeric-zero cheats, the
missing index0 bird and a missing reached activation service are checked. Six
strict Debug/Release checks, all platform builds and formatting pass. Actual
powerup initialization/registration/activation/effects, full native manager
comparison, remaining GameUpdate input and interactive Metal remain open.
Private provenance: runtime-game-powerups-owned.json.


The level-particle path now owns a third bounded list, separate from normal/menu
particles. Original addLevelParticles draws one engine double first, including
zero-count requests, choosing layer1 for >.5 and layer2 otherwise. It then uses
the shared emitter/cache/admission path. Original updateLevelParticles uses the
normal strict lifetime/positive-y removal. Original drawLevelParticles filters
exact layers (0 selects all) and multiplies combined particle/camera scale by
the positive requested layer, preserving separate translation/rectangle math.
Unchanged original ARMv7 methods match 510 particle state words and 3 following
RNG words across three definitions/six phases. Draw passes 0/1/2 match 1,740
reached state/rectangle/UV words and 3 following RNG words. Both captures repeat
identically. These prepared ordinary Sprite/Image boundaries validate numeric
and draw records, not full GPU pixels/material/root layer order.

AbClassicGameLevelParticles binds GameUpdate381..503 to actual mutable table
fields and the real level list. First-frame bursts use amount*15 and y/3;
ground-start uses height0, otherwise the original y. Boolean false publishes
after emission. Later timer updates precede strict interval/physics gating,
resetting before emission; false/nil height falls back to0. Signed zero rates
preserve their interval comparisons. The resolved extra update runs afterward,
including disabled-particle calls. Controlled metadata uses actually loaded
redBuff, shared engine RNG and real atlas bindings; all six commands and
separate layers reach the consumer. A missing definition retains firstFrame
and earlier effects and stops the producer. Eight strict Debug/Release checks,
all platform builds and formatting pass. Selected background bootstrap/settings,
broad emitter/composite/target coverage, full runtime/root/native frames and
Metal startup/GUI/gameplay remain open. Private provenance:
runtime-game-level-particles-owned.json.


AbClassicBirdSpecialty binds the ordinary triggerSpecialty125163 and flying-bird
wrapper125065. Availability clears before the nil gate; definition specialty and
physical flags publish before callbacks. The ordinary red callback4229 queries
the actual notifications Frame child before feature gates and queues its real
decoded fallback sound before a distinct retained notification and fresh canonical
GameObject behavior dispatch. Controlled live RedBird calls check these boundaries,
nil-bird retention, actual child insertion/removal, reentry and late failure.
Seven focused strict Debug/Release checks, all default macOS/Windows builds and
formatting pass. This is source-backed composition, not a complete native
specialty/frame comparison. Active tutorial/targeting/powered branches and other
named hooks reject when reached; input/bootstrap/full Metal sessions remain open.
Private provenance: runtime-bird-specialty-owned.json.


AbClassicGameInput binds ordinary GameUpdate504..1148: real hat/world lookup,
required reached hat release, actual bird specialty, touch routing and sling
mouse effects. Typed consumed values preserve the distinction between truthiness
and literal Boolean true; numeric zero does not block mouse input. Current
camera-mover zoom and logical-camera/base visibility feed the actual sling.
AbClassicTouches preserves primary ID0, current/previous copies, native numeric
copy order, real world transforms and actual GameObject touch delegation for the
supported Museum classes, which have no active touch methods. Raw cancellation
keeps the private primary/previous state. Active touch classes and nested payload
consumption remain outside this projection; input filtering is caller-owned.

Controller integration copies actual cursor history after object scripts and
before GFX/draw, then resets typed skip flags only after successful draw. The
composed draw callback checks this boundary; intentional draw failure retains
flags and cursor while stopping subsequent updates. Eight focused strict Debug/
Release checks, all default macOS/Windows builds and formatting pass. These are
source-backed controlled calls, not a new complete native input/GameUpdate/frame
comparison. Selected bootstrap, full root scheduling, startup/GUI and playable
Metal remain open. Private provenance: runtime-game-input-owned.json.


AbClassicGameUpdate now composes the actual owned prelude, startup, progress,
powerups, level particles, input and controller in source order. Initialization
requires shared cursor/physics cursor, cheat/touch/skip, sling/shots and physics
references, and the controller's same input owner. The actual updated shake
feeds the later camera. Controlled zero-time composition uses the real gameplay
owners and surviving object/particle draw consumer; registry/passive/extra
callbacks check ordering and reentry. A subsequent passive failure stops before
extra/input/draw while retaining earlier effects. The finish-completed flag is
shared with input and controller. Eight strict Debug/Release checks, all default
macOS/Windows builds and formatting pass. This does not establish selected
bootstrap, continuous session or native complete GameUpdate/frame parity; the
controller's limited ordinary tail domain still applies. Root scheduling,
platform input/audio, startup/GUI and a playable Metal session remain open.
Private provenance: runtime-game-update-owned.json.


Read-only original Level1 capture confirms one GameUpdate callback,
removeBalloonsTooHigh, despite zero initial balloons. AbClassicBalloons now
registers it in the shared registry and binds canonical physical membership and
real deadBlocks. Strict world nil drops stale membership; y<-200 queues the
same physical value for later removal. Controlled threshold, NaN, identity and
late-failure checks pass within eight strict Debug/Release checks; all platform
builds and formatting pass. Initial raw catalog selection is a fixed projection,
not the complete ObjectFunctions factory. Full native callback/session parity
remains open. Private provenance: runtime-balloons-owned.json.


Ordinary menu-particle drawing now uses its actual separate list, global scale
and positive-zero translation, ignoring the world camera. Three original
definitions/six phases match 870 draw words with an identical repeat, using
three explicit menu scales and dirty inherited transform state. These are
prepared Sprite/Image records, not full GPU pixels.

AbClassicFrameParticles binds the native flag reset and post-root scheduling:
live physics gates normal/level updates; menu particles always update. Explicit
menu drawing marks the flag before appending current poses; otherwise the late
automatic pass appends updated poses. A reached Spotlight follows. Actual
Level1 capture confirms physics enabled, menu scale1 and no Spotlight. Controlled
engine/consumer checks cover live flag changes, retained command snapshots and
late failures. The composed real-owner GameUpdate now draws before this tail.
All seven particle edge/native checks and composed GameUpdate pass strict Debug/
Release; all platform builds and formatting pass. Native complete frame/session,
Spotlight implementation, startup/GUI and playable Metal remain open. Private
provenance: runtime-frame-particles-owned.json.


AbClassicMenuInput binds actual MenuManager reset, ordinary left-button/wheel
routing and the two filtered-touch copy boundaries. All 4,096 retained original
cases match callback order and touch memberships/coordinates through the real
C manager. Immediate key writes survive later callback failure. Original Level1
extra mouse events are nil and hover scaling false; active services remain
required. Bounded scalar touch copies retain numeric table order and unused
NaN coordinates; arbitrary payload/retained table identities remain outside
this projection.

GameInput now obtains the actual view or copy after hat/specialty at the reached
touch boundary, sharing its raw key tables with the filter owner. The real
MenuManager update feeds composed GameUpdate; a separate nonempty ID77 control
uses its actual filtered coordinates despite invalid prepared snapshots. Seven
strict Debug/Release checks and the subsequent composed rerun pass; all platform
builds pass. Controlled empty roots are not selected GameHud assembly. Full
startup/root/GUI/session and playable Metal remain open. Private provenance:
runtime-menu-input-owned.json.


Actual compositor references now cover the original Rovio splash, main menu,
episode/level selection, introductory CutScene and initialized Level1/gameHud.
The muted headless API22 reference uses host graphics after SwiftShader
compositor captures became black. Installed ARMv7 library hash is reverified;
870 native particle draw words reproduce the prior corpus exactly. Prior GPU
captures retain their own recorded SwiftShader driver and are not replaced.
The new 320x480 captures represent a rotated 480x320 logical screen, retain
existing research userdata and are asynchronous, not deterministic replays.
Level1 initially enters CutScene with no level loaded; the captured skip_button
then leads to gameHud. A separate native birthdate overlay was dismissed without
entering a date. The ready image confirms existing RGB565 presentation evidence,
with32/64/32 channel levels. Full first-party startup/GUI assembly and playable
Metal remain open. Private provenance: runtime-host-gui-reference.json.


AbThemeDraw now binds the cached default-shader ThemeLayers background and
foreground to real immutable sprite/texture providers and shared render state.
The preview delegates to this runtime consumer. Ground fill resets state at its
source boundary; ordinary/composite image commands preserve original shader
selection, which ignores per-vertex tint. Reentry and a reached missing texture
check actual metadata/provider behavior and retain the native failure prefix.
Nine strict Debug/Release checks, all platform builds and formatting pass.
Remapping by texture content, all40 commands shared with the prior validated
sling GPU input remain bit-identical. Additional current HUD content is outside
that older input. Complete drawGame141561 ordering, root/GUI/session and playable
Metal remain open. Private provenance: runtime-theme-draw-owned.json.

Ordinary drawGame141561 now composes the actual cached theme, live base sling,
world, normal/layer particles, passive-before-active powerup drawing and owned
GameObject debug delegation. It shares the real controller/GameUpdate draw
boundary and post-root particle list. Canonical current-bird dimensions affect
the stretch pose; the base position stays fixed. Raw zero quality remains
truthy, foreground is independent of that gate, false popup is nonnil, and
absent eagle preserves existing darkness. Late errors retain prior commands
and oldZoomLevel publication before stopping the owner.

An isolated original drawGame capture records 320 service boundaries across
16 quality/overlay/darkness/layer cases. Running the previously tested real C
producers in this independently captured order matches all 296 commands,
including geometry, UVs, colors, opacity, blending and ordering. Two direct
captures and the checked CMake runner reproduce the same complete reference.
Eight focused checks pass strict Debug/Release; all default platform builds
and formatting pass. This is command composition from controlled native
boundaries, not complete native GPU frames. The selected original world/extra
registries are empty in this probe; the C fixture supplies its real surviving
world. Nonempty extra/z draws, selected editor/popup/restricted producers,
root/bootstrap/full GUI and continuous playable Metal remain open. Private
provenance: runtime-game-draw-owned.json.

ProcessManager now owns initialized state, bounded retained action-table
generations and distinct completion-event payloads, borrowing prepared action
tables/function bindings and the real EventManager. Original full-module
execution matches all 72 checkpoints and 56 ordered action/finish/log/event
records across eight consecutive-operation cases, with identical repeats.
The strict duration boundary, later automatic reset, nil/false/numeric finished
states, callback appends and live duration/finished changes are preserved.
resetProcess uses the original array-index action payload; reset uses target.
Earlier elapsed/skipInput/callback effects remain after a late failure.

The composed zero-time fixture now executes the real shared EventManager tick
and source-initialized inactive ProcessManager before GameUpdate. Four focused
checks pass strict Debug/Release; all default platform builds and formatting
pass. This remains a partial root projection: other root managers/clocks, full
GUI, native continuous frame/session replay and playable Metal remain open.
Private provenance: runtime-process-owned.json.

The current Binary Ninja ELF analysis view is rebased by 0x10000. For example,
GameLua.update has ELF symbol value 0x323ec0 and analysis address 0x333ec0;
AnimationWrapper.update has ELF value 0x2f94a4 and analysis address 0x3094a4.
Runtime library-relative addresses from process captures use the original ELF
values. Keep these address spaces distinct when following analysis references.

Loader eventTriggered426, finishLoading936 and update1220 now bind canonical
state, retained distinct show/hide payloads, actual shared EventManager, live
script time and prepared callbacks. Original selected functions execute
unchanged in the isolated native runtime. Six consecutive-operation cases match
all 42 checkpoints and ordered callbacks, including zero versus absent minimum
time, hidden requests, replaced item lists, completion changes and frame changes
from listeners. Two direct captures and the checked runner are identical.

The source-initialized inactive Loader runs before menu update in the composed
fixture. Its frame still increments while loading is false. Five focused strict
Debug/Release checks, all platform builds and formatting pass. Outer SubSystem
registration, actual loading pages/resource factories and full root/GUI/session
remain open. Private provenance: runtime-loader-owned.json.

The first-party native main queue preserves negative-delay pending posts,
zero-delay timed posts, pending-before-timed ordering, strict later-call firing
after positive-delay subtraction and callback posts deferred to the next call.
Callable records are copied; contexts are borrowed. Processing advances its
cursor before invoking, retains earlier effects on failure and allocates no
frame storage. Native worker queues and callback ownership/destructors remain
outside this single-threaded projection.

Original EventProcessor construction, addQueue, getProcessQueue and process,
plus AnimationWrapper.update against its actual empty host registry, match
128 checkpoints, 896 state words, 528 delayed-state words and 68 callback
records (408 words). The checked original runner repeats byte-identically.
AnimationWrapper passes zero to native processing regardless of frame seconds,
then raises/clears its system-update flag. The composed controlled fixture now
executes this real queue owner after ProcessManager and before GameUpdate.
Seven focused strict Debug/Release checks, all platform builds and format pass.

A bounded read-only snapshot of original initialized Level1 finds zero host
systems but eight positive-delay native event records, no pending/processing
records and cursor zero. They use the same native postDelay<void()> callback;
their producers, subscriptions and outer scheduling are not recovered by the
controlled empty-queue fixture. Do not infer a complete runtime state or frame
from the isolated queue comparison. Root/bootstrap/full GUI/continuous Metal
and native worker scheduling remain open. Private: runtime-native-events-owned.json.

AbClassicRoot now owns ordinary root57000 control flow: distinct global/gameplay
clocks, old-time menu/ad calls, captured subsystem lengths, prepared pair-order
callbacks, retained notifications, input-delay/double-click state, deferred URL
and resume gates, current mode, draw/layout/reset and post-root function order.
The real EventManager, inactive ProcessManager and native animation queue are
used inside its controlled comparison. Remaining source/platform services are
required live callback bindings; no implementation is supplied for a missing
binding. Reached unsupported restricted-aspect/eagle/PC/cursor paths reject.

The original full 631-instruction update executes unchanged with controlled
service boundaries and prepared lists. Sixteen cases over four frames match
all 64 checkpoints, 2,688 state words and 2,212 ordered boundaries (39,816 words).
These include nil/false/numeric clocks, large-float clock stalls, callback clock
changes, strict input deadlines, raw-zero keys, double clicks, deferred URL,
resume and resolution events. A checked repeat is byte-identical. Control-F
requires deviceModel osx; no cooldown preserves eagleInfoTimer. Nine focused
strict Debug/Release checks, all platform builds and format pass. Concrete
service/subsystem/GUI construction, excluded branches, native outer frame and
continuous gameplay remain open. Private: runtime-root-owned.json.

Root audio ramp61785 now executes actual music61897 then effects62137 updates
over canonical typed volume/length fields and the real shared mixer. Source
setMusicVolume92869 targets track7; setEffectsVolume92729 visits0..6. Numeric
zero remains active. Each update publishes volume+seconds/length before <=0 or
>1 clearing; exactly1 remains active. Intermediate gain is volume squared.
Zero-length infinity reaches its original clamp; indeterminate/nonfinite used
arithmetic is an explicit unsupported error with preceding effects retained.

Five unchanged original functions match all 320 checkpoints/3,840 state and
track-gain words across64 initial profiles and four updates. The checked repeat
is identical. Original initialized Level1 has all four ramp fields nil; the
composed fixture binds this inactive owner before loader/menu without gain
writes. AbClassicRoot now requires the concrete ramp owner in place of that
service callback, retaining its existing native comparison. Eleven focused
strict Debug/Release checks, all platform builds and formatting pass. Device
audio output, native continuous frame/bootstrap/full GUI remain open. Private:
runtime-audio-ramp-owned.json.

URLRequests.lua initialization/start226/update529/response806 now own first-party
string maps and distinct retained request records. Timer updates first decrement
every current record, then gather strictly-negative records into a separate map,
then invoke optional timeouts before removing their URLs. Response diagnostics
precede lookup; a present response invokes its required callback before removal.
Both paths remove a new request created for the same URL inside the callback.
Nil/false duration defaults10; numeric zero remains0. Callback fields/extra values
are live; old request records remain valid. Missing transport/callbacks reject
when reached, preserving preceding storage/diagnostic effects.

Original full-module execution matches 72 checkpoints/2,016 state words and 148
ordered diagnostic/fetch/callback records (1,184 words), with an identical checked
repeat. Root now requires this concrete owner. Read-only initialized Level1 has
an empty URL callback map; this state runs before audio/loader/menu in the real
gameplay fixture. Thirteen focused strict Debug/Release checks, all platform
builds and formatting pass. Actual network transport/responses, map replacement/
GC scheduling, other bootstrap factories and full continuous Metal remain open.
Private: runtime-url-owned.json.

Native getTimeAsStruct (analysis0x31cab4) zeroes tm_isdst and truncates numeric
calendar components toward zero, then offsets year/month before mktime. ARM
vcvt.s32.f32 establishes truncation; current decompiler Pseudo C incorrectly
renders it as roundf. timeDiff (analysis0x31cf2c) subtracts two signed32 native
mktime results through difftime; GameLua.getTimeDifferenceInSeconds
(analysis0x3399f8) converts that double result to binary32. getCurrentTime
(analysis0x33944c) publishes local year/month/day/hour/minutes/seconds from
time/localtime. All addresses use the existing0x10000 analysis rebase.

The isolated original native conversion matches all512 UTC cases/6,656 input
and result words, with an identical checked repeat. Fractional fields, overflow
normalization, leap dates, negative epochs and the signed32 time range are
covered. The first-party calendar boundary preserves -1 contributions from
unrepresentable target timestamps even on hosts with wider time_t. Fifteen
focused strict Debug/Release checks, all platform builds and formatting pass.
Other timezone/DST and Windows execution comparisons, immutable timer-state
ownership, AngerLevel binding and complete native frames remain open. Private:
runtime-calendar-owned.json.

The SessioningTimer numeric date/duration owner now executes init, start,
deactivation, force completion, remaining/completion/progress/formatting, end
date and save/load operations in first-party C. The unchanged original full
module, Class, selected deepCopy3468/formatTime6510 and native calendar wrapper
provide the isolated Android reference. A controlled clock supplies deterministic
six-field dates; actual original time conversion/difference remains native.
Twenty-four profiles over eighteen steps produce 432 checkpoints and 21,600
state/result/clock/save words. Corpus SHA-256
95d9254717381c532610adb8c7e301f21142ef42de60199bcc6a28fed16d4267
matches an independent repeat after shared oracle setup refactoring.

Inactive remaining/completion return nil; inactive formatted/progress return
numeric zero. Truthy forceCompleted, including raw numeric zero, returns without
clock access and survives restart, init, load and deactivation. Deactivation
clears dates/duration/cache; failed/empty load sets active=false directly and
preserves them. Duration arguments remain live while the computed duration cache
does not change on external mutation. Save/load snapshots copy fields. End dates
add hours/minutes/seconds without normalization. Progress is not clamped; compare
NaN classification for zero division without promising host NaN payload/sign.
Time formatting retains the original last-two-character padding and negative
remainders. Typed snapshots exclude arbitrary table graphs/overrides and exposed
owned snapshot identity. Late missing clock/duration and live clock mutation are
also checked in C. Provenance: runtime-session-owned.json.

The ordinary AngerLevel owner runs source init1381, update2067, add5238,
event5636/onLevelStart1887, activateCooldown5993 and save7109/load7566, including
the max comparison4575/default6352. The original complete AngerLevel,
SessioningTimer, EventManager and native calendar execute together in the Android
probe. Controlled LevelFlow, settings-file and popup-loading boundaries record
order without substituting their implementation in the runtime. Eight profiles
over fourteen operations yield 112 checkpoints, 5,376 live-state words, 3,472
settings words, 448 header words and 174 ordered twelve-word records. Corpus
SHA-256 877f4894fa36f6a3caa1b12c179a242fd0243b5235be6a9f64d0410a19df3c7a
has a byte-identical checked repeat. Fresh absent settings initialize level/max
to2, boost cost1, charges2, queued0 and both timers inactive; this is now source
evidence rather than an asynchronous ready-snapshot default assumption.

Level refills advance from the previous unnormalized end time, queue during a
level and notify/save outside it. A cooldown expiry with charges<1 restores2
directly; otherwise increment then disable at>=2. The remaining source restart
branch starts from the current clock, not the previous end date. Adding level
clamps only to the upper maximum and notifies before publishing settings. Late
level-start handling loads its real popup before clearing used anger, then flushes
positive queued refills only outside a level; the two settings saves retain their
separate publications. Records are bounded and distinct; reached persistence,
LevelFlow and popup providers are required, with explicit failure when missing.
Hero boosts/upgrades and popup assembly remain open.

Root57000 now requires and runs this real AngerLevel owner at its original early
phase, preserving all 64 prior root checkpoints/2,212 boundary records. The
composed gameplay fixture also performs actual source-default initialization and
inactive timer updates before URL/ramp/loader/menu work. All19 focused strict
Debug/Release tests, default macOS Debug/Release and Windows cross-builds, and
format checks pass. Production settings file binding, remaining root services,
thirteen concrete subsystem bindings, startup/GUI and continuous session remain
open. The port is still not playable. Provenance: runtime-anger-owned.json.

Source isSafeToQuit9757 and isFreeEagleEnabled159329 now run directly in root C
against actual MenuManager/Frame state and borrowed data tables. The builtin menu
getter is pure in this projection; overridden getters/metatables and concurrent
mutation are excluded. Frame fields now retain class-owned requireInput (base
Frame.init leaves it untouched), and menu nodes may bind their actual Frame tree.
Absent manager/root allows quitting; mainMenu/mainMenuRoot inspect only direct
children, ignoring visibility/activity. ForceUpdatePopup allows quitting, while
other names do not. Reached nonempty tree storage must share the actual root state.

Free-eagle queries visit valid typed unlock records, accepting any truthy enabled
value, then resolve current episode/mightyPower/free if needed. Numeric zero is
truthy both as a flag and an episode key. MAIN_MENU excludes episode-derived free
eagle but never overrides a truthy unlock record. Valid record values make source
hash order irrelevant to this Boolean result; malformed-record error ordering,
arbitrary table graphs and getter overrides are excluded from the comparison.

Both unchanged original selected prototypes run in the isolated Android Lua
oracle, with a controlled builtin-equivalent root getter and valid prepared
metadata. The C checker uses the actual first-party MenuManager and Frame tree,
including base Frame entry/layout. Its setup input-reset/notification boundaries
are controlled and do not claim actual menu assembly. All512 input profiles and
5,120 input/result words match. There are272 true quit results,272 true eagle
results and16 eagle results enabled only by episode metadata, covering both sides
of MAIN_MENU exclusion independently of unlock flags. Corpus SHA-256
26806827b75d7b83e3bc3061558011b1beae8a2f8c0b8fa5d9101172438f48d6
has a byte-identical checked repeat. Root's64 checkpoints/2,212 boundary records
remain exact with real C query bindings instead of injected Boolean answers.

All36 focused strict Debug and38 Release affected root/UI checks pass (the
Release configuration includes two additional geometry comparisons), along with
all default macOS/Windows builds and formatting. These queries neither construct
the actual MainMenu/GameHud tree nor close remaining root services, subsystem
factories, startup, GUI or session scheduling. Provenance:
runtime-root-queries-owned.json.

LevelStartScreenFlow.currentlyIngame3634 and NotificationsFrame.
isAnyPausingChildVisible3363 now query actual C MenuManager/Frame trees. The
original pure-getter projection returns raw nil for missing root or pausePage,
Boolean false for other menu names or a visible pausePage, and otherwise the
negation of direct global notification children with both visible and
notificationsFramePause truthy. Active is ignored; numeric zero is truthy.
All256 original profiles match1,024 input/type/result words;512 additional
getter-count words diagnose source branches without claiming duplicated C getter
reads. There are80 raw-nil,12 true-ingame and64 true-pausing results. Corpus
SHA-25604916118cf76144b3da61a420581c944c4cf0ebb3a7ec441d3fc6db8194fd464
has an identical checked repeat. All38 strict Debug/40 Release affected checks,
default platform builds and format pass. Actual class assembly, full level-start
flow and continuous sessions remain open. Provenance: runtime-gui-queries-owned.json.

AbClassicFrame now composes the prepared native frame body: reset menu-particle
draw state, actual simulation/interpolation/properties, pending updateAvailable,
actual root57000, then particle/Spotlight tail. ARM GameLua.update analysis
0x333ed0 retains the original argument in s17;0x334584 and0x334590 pass that same
word to both root arguments. Script and native clocks remain independent.
Actual level identity and shared physics-flag addresses are checked. Continuous
64-frame C integration exercises actual physics/root/particles with controlled
root service boundaries, pausing/resuming inside root, speed changes, reentry and
late failure retention. All18 strict Debug/Release affected checks, default
platform builds and format pass; prior original root64 checkpoints/2,212
boundary records remain exact. Native URL/local-notification/input prefix, actual
subsystem/menu/service factories and interactive session are still open. This
is not a complete native frame comparison or playable claim. Provenance:
runtime-continuous-frame-owned.json and runtime-continuous-frame-native-order.json.

The dense LevelStartScreenFlow owner now preserves staging latency, separate
initialization/ingame queues, per-target name deduplication, equal-priority sort
exchanges, initialization recursion and live listener callbacks. Removal precedes
onShow; showing clears after the actual ingame query, before pause publication.
Loading-init replaces the ingame queue and done/showing fields while retaining
old queue generations for active initialization iterators. Explicit limits stop
on exhaustion. No sparse/foreign queues, arbitrary raw aliases or getter overrides
are claimed. The source error caused by replacing an actively captured init queue
is retained, including its preceding callback and queue effects.

The unchanged complete source module, original Class/SubSystem/EventManager and
exists helper execute together in the private Android oracle. Prepared listener,
paused and pure getter boundaries are controlled; C uses actual MenuManager/Frame
queries and EventManager. All 864 checkpoints, 27,648 state words, 3,456 header
words and 529 ordered ten-word callbacks match, including eight stopped source
error profiles. SHA-256 7d1333abba29e012c9c35c297216280ac48e4fcf4664fc9e2cec85d4e0313185
has an identical checked repeat. Root integration now runs this actual idle owner
in its fourth subsystem slot, preserving the prior 64 root checkpoints and
2,212 boundary records. All 25 strict Debug/Release affected checks, default
platform builds and format pass. The earlier completion oracle remains distinct
and reproduces its original 4,096 cases and SHA-256
390726c460d6fe92d2fb69bbb9e597673e8a8a62efea07e15e2088482ea251d2.
Actual pause/mode routing, screen listeners, six remaining nonempty subsystem
updates, production factories and full interactive Metal sessions remain open.
Provenance: runtime-level-start-flow-owned.json.

Museum/pause queries retain the source distinction between pausePage.show
(literal Boolean true for subsystemsapi985) and pausePage.visible (truthiness for
LevelStartScreenFlow). MuseumLevelFlow128 permits an absent pausePage under a
gameHud root and returns raw nil for an absent root. All 256 prepared profiles
match: 64 nil, 12 true-pause and 60 true-in-level results. Corpus SHA-256
fd5741818ec09e622be0ab5630ae325fe881f335303c8926adb4950b1dd9da47
has an identical checked repeat. Base Frame.init preserves class-owned show.

The current-game-mode owner now implements setGameMode65192, setGamePaused161885
and isIngame161794 with actual Museum menu queries, live play-mode/function
identities and bounded retained draw-only functions. Native comparisons corrected
the earlier unverified branch interpretation: setGamePaused proceeds when ingame
is truthy OR the play mode differs from Museum; Museum outside a level returns.
Motion cursor receives currentGameMode != updateGame. Pause writes physics false
before publishing a fresh draw-only callback; resume publishes updateGame before
physics true. Paused callbacks call the live drawGame with zero arguments.

Seven unchanged source prototypes execute together in the Android oracle; other
play-mode queries and physics/cursor/update/draw boundaries are controlled. All
2,304 checkpoints, 20,736 state words and 2,172 ten-word boundary records match,
including 72 late-error prefixes. C controlled cursor failures preserve the same
prefix as the native absent cursor function; diagnostic strings are not compared.
SHA-256 a3a58a65c9d7b702001ab0faf1a0be6489753ae158cd95e4bc6017153946e638
has an identical checked repeat. Actual updateGame/drawGame adapters execute in
the owned gameplay fixture; the pause route appends real draw commands without
advancing gameplay timers. Its base-Frame gameHud/pausePage constructor is an
explicit test boundary. Actual root integration uses the router and idle flow,
retaining prior 64 checkpoints/2,212 boundaries. All 44 strict Debug/47 Release
affected checks, default platform builds and format pass. Complete ModeSystem
classes, platform cursor, source factories and full interactive startup/GUI remain
open. Provenance: runtime-museum-pause-owned.json and runtime-game-mode-owned.json.

The reusable launch host now binds actual generic statistics/events, sling vector
publication, ordinary shot flags/hooks and AudioBank calls. The composed shot
fixture uses it for both remaining birds. Reload geometry reads the real camera's
unshaken published screen center and Museum bird/castle snaps; direct next-bird
audio reads the live typed name, resolves it through the shared RNG, and plays the
resource with the computed gain, false loop and track zero. No fixture distance
or audio substitute remains in that path. The fixture still prepares its camera
coordinates and globals; this does not establish a production constructor.

Seven unchanged original camera/utils prototypes execute in the Android reload
oracle, with prepared snap lookup and initial screen center zero. All 128 distance
pairs and 896 input/result words match, including coincident and fractional snaps.
Corpus SHA-256 8c9ead3a49515dd75defb24160946b5e6dd294653d8bf4cde1853735ec1c1565
has an identical checked repeat. Existing camera captures cover moving-center
publication separately. All ten strict Debug/Release affected checks, default
platform builds and format pass. Full reload/frame timing, source constructors,
platform audio output and interactive Metal remain open. Provenance:
runtime-launch-host-owned.json.

AbClassicPlayfield now owns the prepared ordinary Museum level's eight real
visual/scene/trajectory/camera/behavior/shot-service/shot/sling components. Stable
internal owner slots resolve launch construction cycles. Bird behavior dispatch
uses actual camera sequences and trajectories, without the prior fixture's
successful observation-only behavior callbacks. The borrowed level, selected
sprite provider, camera resources, canonical globals, raw flags, statistics,
events and audio remain explicit. Source loading policy, fresh defaults and
listener registration are still outside this assembly. Blues uses the same
actual trajectory owner and borrowed live global fields; a reached child lookup
requires the actual other-birds table. Split-body creation remains separate.

The new integration test prepares a fresh APK level and source resources. Its
first launch matches all five body words in the retained native live-shot corpus
(SHA-256 eebe91cb537840a48484902ac677b2639b2ce5a700ec7e658a6563d10e9373b1).
This reuses that capture, not a new whole-constructor/native-frame comparison.
Both remaining birds use real reload, body, event/vector, decoded audio, camera
and trajectory owners. Explicit post-capture movement exercises point creation;
it does not stand in for continuous simulation. A late sling-construction error
releases partial owners and unbinds behavior tables; consumed renderer IDs survive.
The test discards the borrowing level and its atlas together before retrying.
All 17 strict Debug/Release affected checks, default platform builds and format
pass. Continuous simulation/removal/root/input wiring and complete startup/GUI/
Metal sessions remain open. Provenance: runtime-playfield-owned.json.

Blues trajectory hosts can now borrow the canonical flyingBird and otherBirds
fields. Read flyingBird after REMOVE and reread otherBirds independently before
each child lookup, preserving source callback mutations and table replacement.
The existing 512 original cases and 262 ordered boundaries match twice: with
prepared values, then with separate live fields while prepared values deliberately
differ. The original corpus remains unchanged (SHA-256
62b98fe3b76961be778e0127cfb67ab1e13f8e7034580ccfae86c9a293b69ae7).
This adds no new whole-session/native-frame capture.

The playfield integration also constructs actual Level8 and exercises its source
MuseumBluesTrajectoryRenderer launch/specialty behavior through the real trajectory
owner. Its geometry/visuals use the existing Android group/profile/resource/cache
owners; Level1's four-sheet fixture cannot construct that level's geometry. A
headless logical texture boundary validates image bindings and makes no GPU claim.
All six strict Debug/Release follow-up checks, default platform builds and format
pass. Full Blue launch/split-body creation, parent canonical state and continuous
gameplay remain open. Provenance: runtime-playfield-owned.json.

AbClassicDynamics owns simulation, physical removal, goals, destruction
statistics, removeBlocks and contact/physics-phase bindings for a prepared
Museum playfield. Rolling audio, destroyed sound, statistics and goal callbacks
now reach the actual component owners in the composed gameplay fixture. The
constructor borrows source state, contact records and actual block-step/water
providers; it selects no empty scripts, seed or gameplay defaults. Teardown
restores the previous world listener before releasing children; a replaced
listener preserves the handle for retry. Budget failures before and after child
construction preserve the previous listener and release partial storage.

All 34 affected strict Debug/Release checks, default platform builds and format
pass. This reuses existing component/native captures and the composed gameplay
fixture; there is no new whole-session native capture. The fixture still supplies
controlled empty script-step and contact metadata boundaries. Fixed initial
identities, actual parent registries, ordinary dynamic abilities, startup/full
GUI and continuous Metal gameplay remain open. Provenance:
runtime-dynamics-owned.json.

Bird collision lookup now resolves stable first-party C function records.
The actual inactive Red handler reads the live typed feature flag. Resolved
aliases remain callable across later global rebinding, including nil; unknown
names and foreign record identities reject. Additional handlers require actual
registered implementations or explicitly resolved nil. The gameplay fixture
uses these records in place of a synthetic callback number. Focused checks cover
aliasing, callback mutation, retained values, raw-zero truthiness and reentry;
the existing native collision/body captures remain unchanged. All six affected
strict Debug/Release checks, platform builds and format pass. Active Red cleanup
and other ordinary collision handlers still require implementation; this is not
a full game-global environment. Provenance: runtime-collision-hooks-owned.json.

Direct circle creation now implements the native createCircleLua boundary with
actual physics and raw physical-table publication. The sprite parameter is unused
at this boundary: sprite starts empty and definition/controllable/script fields
are absent. The original method executes unchanged in an isolated Android fixture
with prepared GameLua/LuaState/world owners and no renderer. All 128 profiles and
8,192 typed property/cache/body/fixture words match, including signed-zero inputs,
zero-density bodies and both controllable-vector gates. The checked repeat is
byte-identical, SHA-256
d1693570d133b92906eadee26e576f4316c944191dc1d785d5cf641174ae82d8.
The first-party raw result records the controllable flag; native vector and
physical object-map registration remain enclosing-owner work.

The level owner can append these circles into reserved stable slots without
moving earlier tables, names, fixtures or pose samples. A retained string map
preserves growing world order across preceding canonical nil writes; removed
physical slots may be reused by the engine while object identities remain
distinct. This ordering reuses the separately native-compared string-map engine;
the new direct-constructor corpus does not cover whole-level insertion order.
Old names cannot be reused, and no collection phase is inferred. Three focused
Debug checks and all 23 strict Debug/26 Release affected regressions pass, along
with all platform builds. New identities still need explicit registration in
fixed simulation/contact/removal/character adapters before those owners run.

Visual/ordinary-scene owners now reserve the level's full capacity. Dynamic
renderer registration follows native createRendererForGameObject: consume an ID,
register the renderer in layer/vector order, attach its asset, then set the actual
sprite. Level8's integration creates three controlled circles using real parent
geometry and Android group/profile sprite resources. Their addresses and retained
names remain stable; all three produce draw commands, and removing one drops one
command. A late name-storage error retains the consumed ID, registration and
attachment before teardown. The test camera explicitly includes the new circles;
the original zero-origin fixture correctly culled them. All eight strict Debug/
Release affected checks, the follow-up integration, platform builds and format
pass. This is headless resource/draw composition, with no new whole-native-renderer
or Metal screenshot capture. The existing Metal pixel test passes separately.

Blues splitting uses triggerSpecialty125163's child129468, rather than generic
createBirdClone123883. Its renderer uses damageSprite and its new GameObject
descriptor contains only a non-cluster MuseumBluesTrajectoryRenderer. Ordinary
split arithmetic/callbacks, dynamic adapter registration, continuous play and
startup/full GUI/all-level Metal remain open. Provenance:
runtime-create-circle-owned.json and runtime-blue-split-next.json.

Dynamic GameObject construction now binds the real script/physical/behaviors
identities before running an explicit source behavior descriptor through the
existing addBehavior/class factories. Optional object capacity reserves all maps
and instances at open; new objects append without reallocating earlier records.
Blues children use their own one-class descriptor, with no inherited isCluster
override. Level8's three controlled circles now construct these actual behaviors
and join the real trajectory owner during init. Their script and instance aliases
remain canonical. Non-cluster onDestroy correctly leaves the track; a real child
collision against ground removes it before explicit physical/visual teardown.

All 12 affected strict Debug/Release checks, platform builds and format pass.
Existing GameObject property, Museum class, Blues and initial graph native corpora
are reused; no new complete dynamic-constructor native capture is claimed.
The fixture still supplies prepared circle positions and does not run the full
split branch. Dynamic camera, force/post/contact/removal/shot/character registration,
outer specialty effects and the continuous Metal/startup/full-GUI session remain
open. Provenance: runtime-dynamic-behaviors-owned.json.

The actual Blues child129468 constructor now composes the real C circle,
renderer, raw field, transform, velocity, collider, birds-table and GameObject
owners. It preserves source reads and arithmetic order, shared damageFactors,
and the one-class non-cluster descriptor. Its supplied name pool retains all
escaped names until the owning level and observers release them. Late errors
retain earlier effects and stop that binding.

An isolated original-library capture executes the unchanged child closure,
actual native circle/world/transform methods, and original Class, BehaviorBase,
GameObject and Blues behavior construction. All 48 prepared cases match 47,958
typed argument/state words, including snapshots before each reached source
call and final cached poses, body mass/velocity and script/behavior aliases.
The checked repeat is identical. The prepared parent/upvalue, child offsets,
renderer attachment and scale calls are explicit boundaries; no outer split,
GPU rendering or native continuous-session claim follows from this capture.
Native setter analysis also confirms that setColliderType changes GameObject
metadata without rewriting Lua collider or fixture filtering.

The camera owner can now reserve stable rows and register subsequent physical
identities without moving an active follow target. Later addBehavior retains
the child trajectory group and constructs MuseumBluesCameraFollowBehavior.
Its source init cancels before starting FollowShotBirdCameraSequence; inherited
events remain empty. A cancellation callback observes the still-active old
sequence and newly published empty behavior group before the actual resource
loader/logs run. Tests also cover append gaps/duplicates, constructor reentry,
unresolved-class empty-group retention and exhausted child-name storage.

All 13 affected strict Debug and Release checks, macOS/Windows builds and format
pass. Existing original camera-init/follow corpora are reused for the new camera
binding; no new whole-class native camera capture is claimed. Full outer Blues
effects, dynamic force/post/contact/removal/shot/character registration and the
continuous macOS Metal/startup/full-GUI/all-level session remain open.
Provenance: runtime-blues-child-owned.json.

Direct-circle force and post-physics registration now uses pre-reserved storage.
Existing force interval/sound/context records stay at fixed addresses while the
object rows and source indices shift into native name order. Views must refresh
after registration. A new circle retains native disabled force preparation and
an empty rolling name; copying a definition or material afterward does not run
ObjectCreation again. The original constructor/createCircle analysis establishes
those boundaries. Movement exclusions now read the current physical definition
string, matching the original update's LuaTable lookup instead of an initial
C cache. Non-string coercion remains unsupported at that reached boundary.

The real simulation owner registers both adapters and refreshes preparation
count before each frame. An unregistered object rejects before clock/world work.
A prepared integration checks 12 actual fixed physics steps, ordered phase
callbacks, interpolation/publication, live definition exclusions and stable
specialty rows. Removing one circle and reusing its engine body under a new
identity preserves the old object's final history/properties. New circles remain
silent despite copied rolling-bearing definitions. Separate force checks retain
an active interval across first/middle/last insertion, RNG/resource dispatch,
gravity cancellation, queued forces and removed-name skipping.

All 34 strict Debug and 37 Release affected checks, the three platform builds
and format pass, including existing Metal pixel/pipeline checks. The original
initial force/post/level-frame corpora still match. These new dynamic-frame
checks use explicit prepared phase boundaries; no new complete original dynamic
frame, gameplay GUI or all-level Metal capture is claimed. Physical removal,
bird/character ownership, full outer abilities and production session binding
remain open. Provenance: runtime-dynamic-simulation-owned.json.

The level contact adapter now reserves explicit caller-owned metadata capacity
and registers subsequent canonical identities without replacing the listener.
Existing and newly supplied velocity proposals and physical Lua-table validity
remain untouched. That validity is distinct from the later GameObjectScript
field. Registration checks real body/fixture/userdata links and collider bounds;
gaps, duplicates, locked callbacks and exhausted capacity reject.

A real new-circle contact reaches the controlled collision response and queues
then flushes velocity to the correct new body. Reusing a retired bird's body slot
under another identity preserves its old proposal; retired name/body lookups
return absent and an attempted old-identity flush rejects. Removed orphan
fixture callbacks remain unsupported and fail explicitly. All 18 strict Debug
and Release contact/removal checks, platform builds and format pass. Existing
original contact/dispatch/world/glue/removal captures are reused; this adds no
whole original dynamic-contact capture. Physical removal/vector, bird/character
registration and complete gameplay/GUI/Metal session remain open.
Provenance: runtime-dynamic-contacts-owned.json.

Physical-removal ownership now reserves stable metadata, map nodes and native
bird-vector entries for later direct circles. Registration preserves an empty
native rolling name and absent camera animation, rather than applying the copied
definition's material again. Native controllable-vector insertion follows the
circle's creation-time Boolean argument independently of the later collider and
raw controllable field. Existing original createCircle analysis establishes that
order; initial ObjectCreation behavior remains separately bound.

Level8 registers all 48 source-created children against the real scene/visual
and post-physics owners after their native comparison. Controlled subsequent
birds remove through the actual physical/joint/teleporter/renderer path. This
erases the native vector entry and physical key, detaches the renderer, destroys
the body and forgets level membership before explicit later Lua-map cleanup.
A replacement reuses the retired engine body with distinct stable removal
metadata, then follows the same removal path. The retired metadata never becomes
the replacement's lookup result. The teleporter registry and later source cleanup
remain explicit fixture boundaries.

All 20 affected strict Debug and Release checks, three platform builds and
format pass. Original raw child and existing core/joint/teleporter/removal
captures are reused; no new whole native dynamic-removal capture is claimed.
Shot/bird/character growth, full outer source abilities, parent initialization
and complete macOS Metal GUI/all-level gameplay remain open.
Provenance: runtime-dynamic-removal-owned.json.

Shot ownership can now reserve later direct-circle rows and remaining owned
identifier bytes. Registration copies current names/sprites, creation mass,
cached pose, raw shot/collision truthiness and actual canonical script presence.
Controllable objects with an authored definition prepare its immutable hook,
flying-sprite and audio identifiers; native bare circles may have none. This
runs no source effects and does not add a launch-queue entry or change shot
state. Existing bird and identifier addresses remain stable.

Level8 checks all 48 source children after the native constructor comparison,
including owned NUL-terminated names, zero creation mass and canonical scripts.
The entire preexisting shot state remains byte-identical. The real renderer/
removal replacement path also registers distinct shot metadata after body-slot
reuse. All five affected strict Debug/Release checks, platform builds and format
pass. Existing native child/shot corpora are reused; this is ownership integration,
with no new whole original dynamic-shot capture. Canonical Birds membership,
character/controller traversal after insertion, full outer abilities and the
complete macOS Metal startup/GUI/all-level session remain open.
Provenance: runtime-dynamic-shots-owned.json.

The canonical bird owner now reserves later identities and retained otherBirds
table generations. Children borrow its actual birds field; fresh dense numeric
name tables preserve old aliases and later string-key nil writes. Level8 checks
real flying-child collision, particle/audio/camera removal and zero-speed timed
retirement, stable metadata, invalid registration and final generation bounds.
This reuses the existing native child/removal/component corpora.

The ordinary CLUSTER_BOMB composition now executes tutorial state, puff, three
actual children, shared hat transfer, fresh otherBirds and flying publication,
camera initialization, originalCluster and sound. Common specialty notification
and parent behavior dispatch precede actual parent removal. Source125163 uses
vNormalize(yVel,-xVel); utils10029 uses strict component threshold bits358637bd,
float square/add/divide and a promoted square root. Native wrapper82006c converts
float to double, uses vsqrt.f64 at820084 and converts back at820094.

The prepared Level8 C integration checks this ordering, actual services and an
active-teleporter failure prefix. Native GameLua325bf8 returns before child reads
when the parent key is absent; active virtual cloning remains unsupported.
All23 selected strict Debug and Release checks passed, including Metal pixels and
pipeline, with all three builds and formatting. Dynamic character/controller
traversal and startup/GUI/all-level Metal playability remain open.
Provenance: runtime-dynamic-birds-owned.json and runtime-blues-split-owned.json.

The enclosing native branch now matches 16 original profiles/48 children,
479 ordered calls and 5,711 typed words, including signed zeros, threshold
neighbors, large/asymmetric velocities and a camera callback that redirects
flyingBird before originalCluster lookup. Both outer prototype 125163 and
utils prototype 10029 run unchanged, with original settings, trajectory and class owners
and real native circle/transform/body operations. The Blues class captures its
trajectory table when loaded; the oracle loads that owner before the class and
launches the prepared parent through the actual source method.

Camera sequence/GPU/audio-name/play/notification/final parent removal remain
explicit oracle boundaries. C executes its actual owners; prepared audio is
drained between independent profiles. This does not establish complete native
audio/RNG/device timing or whole-session parity. The corpus repeats byte-for-byte
at SHA256 eb24b41b3b98047cf54bfdffe4cd8d99fb95b573514be68be10bcbf270c29ed3;
the prior child corpus remains unchanged. All 24 strict Debug/Release checks,
three builds and format pass, including Metal pixels/pipeline. Active native
teleporter cloning, live controller/character traversal and the full GUI/all-level
Metal session remain open. Provenance: runtime-blues-split-native.json.

Mutable string-key next now matches 24 original profiles with two independent
traversals, 216 visits, six invalid-key boundaries and 1,656 typed words. The
repeat is byte-identical at SHA256
6ce364debf49d4b66405be9e1132c9e0312f3e1940224a99382b8a2724f7fe5d.
Original base next, table values, rehash and explicit collection run in the
oracle. Cases retain a captured value across rebinding and observe a fresh
second pass. Native next at83b038 reaches table traversal at85bbc8; a missing
retained key reaches the original error path at84c47c.

The owned world cursor now resumes after the retained key in its current hash
slot. Synchronization no longer reinserts historical nil keys discarded by a
rehash. Controller world loops begin independently, retaining each canonical
object for its phase calls. Reserved rows support later identity registration
without timer initialization, effects, RNG or queue insertion. Tests combine
the actual level/world producer with the tail, remove a captured object, create
a real circle before the script boundary and verify current membership. Existing
actual shot/controller compositions also pass. These establish component
semantics, without a new whole original dynamic gameplay-frame comparison.

Selected14 Debug/16 Release checks and final8 checks in each mode pass, with
all three platform builds and formatting, including Metal pixels/pipeline.
Live Birds/character traversal, complete session ownership and full startup/GUI/
all-level Metal playability remain open. Provenance:
runtime-world-iteration-owned.json.

Concrete caller-owned Frame/Image/ImageButton trees now connect lifecycle,
child updates, hit geometry, canonical pointer state, animation and drawing.
Actual localization, sprite metadata, synchronized textures and audio-bank
requests bind through a stable resource adapter. Child mutations precede the
button animation continuation; ImageButton flag clearing stops at that button.
Reached failures retain their prefixes and stop the shared context.

The dynamic requirement reader decodes the pinned APK's
assets/data/scripts/dynamic/default.lua as literal table data, preserving local
table aliases. It validates the five terminal registration words without running
their call. The 235-word constructor permits no external globals, calls,
arithmetic or control flow. Resolving these requirements before image-group
loading supplies the actual menu logo and play-button resources.

A prepared two-widget fixture loads the pinned APK, uses actual localization,
resource owners and audio-bank dispatch, uploads16 textures and captures two
quads at480x320 through Metal. The capture was visually inspected. No window or
device audio is opened. This establishes integration, without a complete
MainMenu class, startup session or original whole-menu pixel comparison.
All17 selected checks pass in strict Debug and Release, including existing
native widget corpora, malformed-resource checks and Metal pixels. Full macOS
Debug/Release builds, the Windows cross-build and formatting pass. Runtime
scene requirement lifetimes, complete GUI classes and start-to-finish all-level
Metal playability remain open. Provenance: runtime-ui-widget-owned.json.

The concrete BGBox Frame subclass now composes border strips, corners and center
through the ordinary sprite renderer, retaining its source query/draw order,
component mutation, natural/scaled anchors, coordinate rounding, vertical cull,
total-size calculations and optional colored center. The source global `_` from
discarded bounds is retained in the shared widget context. Color fills use the
existing packed-color/rectangle reset boundary. Empty identifiers skip rendering;
this intended absence behavior has no new original-function comparison here.

Scaling now owns an actual EventManager listener. Initialization selects the
Android default profile or supplied template dimensions; later resolution events
keep that profile and recalculate only when useScaling is enabled. Controlled
composition checks cover changing dimensions, disabled-event retention, direct
disabled update, widget assignment and registration cleanup after failure.
Production application initialization and complete original GUI parity remain open.

The prepared APK fixture now loads the actual common/main-menu/theme requirements
and includes the six MainMenuSlider panel images alongside the logo/play button.
It uploads23 textures, emits eight quads and captures480x320 on Metal without a
window or device audio. The output was visually inspected; input still dispatches
PLAY with the actual widget identity and queues audio. This is source-derived
panel composition, not the MainMenuSlider/MainMenu classes or startup session.
Provenance: runtime-ui-panel-owned.json.

MainMenuSlider now uses concrete BGBox, Scaling and widget owners for its source
constructor, entry, layout, update, toggle and pointer methods. Source offsets
are376,1233,1598,1753,1975,2168 and2894 in
assets/data/scripts_common/menus/MainMenuSlider.lua. Resource dimensions determine
width/margins/element height. Entry first visits the Frame base, then assigns
unscaledHeight zero; ordinary layout remains a separate caller operation.
Opening clamps at the captured limit; closing changes state only below zero.
Children receive the source position clamp before their actual updates.

The existing EventManager delivers notify synchronously; queue buffering does
not defer notify. Slider events publish before state assignment and carry the
canonical widget as a borrowed C instance identity. Controlled checks cover
observer mutation, notifications during queued dispatch, input only when open,
strict closing endpoints and an observer failure preserving the prior state.
Literal-data readers do not produce C instance identities; static export rejects
them. Existing value kinds keep their numeric representation and storage size.

The APK integration reads the actual events.lua definitions and binds the real
resolution/slider IDs. It opens the concrete slider with an About ImageButton,
routes About and Play into actual audio-bank requests and captures nine quads
using23 textures at480x320 on Metal. The capture was visually inspected. Placement,
child selection and scaling environment are explicit fixture inputs; there is no
complete MainMenu, continuous application or whole original GUI comparison.
All18 selected Debug/Release checks, three full builds and formatting pass.
Provenance: runtime-ui-slider-owned.json.

Concrete ToggleButton and ToggleIndicator owners now preserve the source state
changes, mutable numeric state-image arrays, derived constructor image setter,
opening cap and first-update closing behavior. The main-menu options factory
keeps feature branches, keyboard insertion, per-control assignment order and both
entered-parent layouts. Four controlled feature profiles cover the factory;
production flag acquisition, saved-state synchronization and actions remain open.

The UI resource renderer now resolves legacy composite parts through their cached
sheet/region selections, independently of newer registry bindings. The current
route supports positive axis-aligned context scale and preserves part order,
anchors, context and callback failure prefixes. Explicit draw size is ignored for
composites, following the pinned native Resources dispatch. Rotated/direct/RVIO
UI composites still reject. C checks establish this implementation's behavior;
there is no new native whole-composite pixel comparison.

The interactive --main-menu preview and headless Metal check share the same
prepared owner. Actual APK common/mainMenuRoot groups produce 19 textures and 15
quads after controlled options, credits, sound, notification and play actions.
Both off-state composites are visible in the inspected 480x320 capture. No device
audio is opened. The Android APK lacks the desktop BUTTON_FULLSCREEN image;
the preview uses explicit Android feature inputs and Common's native F shortcut.
The complete MainMenu class, startup, background animation, application navigation
and gameplay remain unfinished. Native window input is not manually verified for
this milestone. All 18 selected Debug checks and 21 Release checks pass, as do
three full builds and formatting. Provenance: runtime-ui-options-owned.json.

ScrollFrame2.lua now has a concrete C owner over the existing widget tree and
EventManager. It retains canonical widget.scroll fields, copied anchors/bounds,
five drag samples, delayed sampling and edge resistance, strict flick/snap
thresholds, notification when the target is closer, pause release, and the source's two
child-pointer visits before some returned replies. Reset keeps dragHasMoved;
beginDrag keeps the prior sample index/timer. The xy branch records samples but
moves neither scroll coordinate, following this source. Recursive flag traversal
also visits children below buttons whose own clear method is nonrecursive.

Drawing uses the source's ordered background/midground/foreground methods,
per-child culling, cumulative scale, floored translation and clip-set/full-screen
reset ordering. Named child methods dispatch into their real C class. A reached
clip needs an actual host setter; none is simulated. Production renderer clip
binding, episode cards/selection graphs and Play navigation remain unfinished.
Controlled lifecycle/motion/input/phase/failure checks establish C integration;
there is no new original whole-scroll-frame pixel comparison. Frame position and
scale queries now read only their relevant fields. Provenance:
runtime-ui-scroll-owned.json. All 20 selected Debug checks and 24 Release
checks pass, alongside three full builds and formatting.

The screen-clip renderer binding is now concrete. gamelogic.lua10410 adds the
actual optional screen offset before res.setClipRect; LuaResources then rounds
each edge separately. Retained integer requests travel with UI sprite/composite
commands and BGBox fills, independently of subsequent render-state changes.
ScrollFrame binds the actual shared widget clip setter by default. Metal and
GLES2 apply attachment scissors; Common applies logical clip bounds in its shared
sprite shaders, preserving its native fitted viewport and draw order. Empty
intersections draw no fragments. Requests
retain their original bounds; the backend intersects only for submission.

The headless APK integration exercises the real scroller/default setter with
three actual sprites and a fractional offset. All 15 menu-component quads still
render. The scroller then emits 3 clipped quads: 10,228 pixels contain sprite data,
and every pixel outside the expected rectangle retains the clear color. The
capture was visually inspected. Metal RGBA8/BGRA8 cropped/empty rectangles and
GLES2 state/coordinate conversion also pass controlled checks. This establishes
first-party composition and submitted pixels for this fixture, without claiming
an original whole-scroll or full GUI comparison. Episode cards, selection screens,
startup/navigation and end-to-end gameplay remain unfinished. A separate silent
Common Metal window check validates all 1,104,000 pixels at 1200x920, with a fitted
1200x800 viewport and letterbox bars, covering clip overlap, draw order and empty
rectangles. Its GLSL shaders also link as core ES2. Common remains pinned and
unmodified. All 135 graphics checks pass in Debug and 146 in Release, as do
three full builds and formatting. Provenance: runtime-ui-clip-owned.json.

Concrete Text now shares its actual Frame with the widget tree. The recovered
constructor, layout, metrics, native leaf draw and following Frame.draw use real
font/catalog resources. FONT metadata, glyphs and the relative PNG/PVR texture
have a bounded first-party owner. Current font selection persists across calls;
native scale/angle/pivots and alpha remain in the shared render context. Glyph
commands retain the active screen clip. ImageButton.drawSelf can draw without
reading children, and Frame.drawWithTag uses each concrete child's actual tag.

The APK Metal integration decodes FONT_BASIC and FONT_LS_SMALL, constructs a Text,
then applies EpisodeButton's later score/font assignments. It checks five score
glyphs followed by the real image child, font/alpha/clip state, canonical ancestor
queries, hidden-text child traversal and tag filtering. Its combined 480x320
capture contains 6,988 resource pixels. This checks C composition and submitted
pixels against the existing recovered component contracts; it is not a new
original-game screen comparison. Clipped/colored Text, callable font getters,
profile/bootstrap font loading, episode cards, selection navigation and gameplay
remain unfinished. All 63 selected Debug checks and 72 Release checks pass;
all three builds and source formatting pass. Provenance:
runtime-ui-text-owned.json.

The standard EpisodeButton class now owns its actual lock, score/star/feather
boxes, three Text labels and new sign. It preserves constructor order, metadata
binding, disabled/open input, score/settings query order, base lifecycle, layout,
three named draw phases and the new-sign cubic/bounce/random timing. Its Metal
fixture uses the APK's actual resource groups, fonts and Poached Eggs metadata;
statistics/settings values are explicit controlled test inputs. The ordered
phase command counts are 2, 6 and 16. The capture was visually inspected. This
checks source-derived C integration, not an original whole-card pixel comparison.
The other episode-button subclasses, real statistics/settings acquisition and
the complete selection/navigation graph remain open. Provenance:
runtime-ui-episode-owned.json.

The complete episodes.lua catalog is now an owned C resource. Literal fragments
are decoded independently with no imported registers; pending Golden Egg tables
are composed in C around the recovered external-link gate. C also handles the
original China/device branches and ordered episode/feather IDs. The decoded
resource hash pins fragment boundaries. All metadata, parsed structures and
tables share a retained allocation limit; the baseline occupies 619,513 bytes,
and exact-limit/one-byte-short checks pass. No original calls, branches, loops
or functions execute.

Against the independently retained original Android memory capture, all 597
ordered episode/page/level paths, 160 page fields and 861 source level fields
match with zero differences. Later bootstrap-added level annotations are outside
this resource owner. Its baseline contains 17 catalog episodes and 14 selection
IDs. Twelve source feature/device configurations pass C graph checks, including
the optional China school level, Talkweb insertion, link restrictions and Apple
Golden Egg. Other configurations have source-derived checks rather than new
native captures. The real card integration now consumes this complete catalog.
All 66 selected Debug checks and 75 Release checks pass, alongside all three
builds and formatting. This establishes catalog recovery, not playable levels
or complete startup. Provenance: runtime-episode-catalog-owned.json.

Concrete C statistics now calculate episode stars, scores, feathers and completion
from the borrowed live highscore graph and actual starLimits.lua records. Egg
Defender's saved-star override and Golden Egg weighting retain their distinct
rules. Progress queries read actual metadata, local settings and feature tables,
including nil-only badge/visit tests, short-circuit eagle visibility and the
registration trial gate. The catalog test covers all 597 entries with explicit
empty/completed saved-state fixtures and real Level1 threshold boundaries. These
fixtures do not establish fresh-install or SDK defaults.

All 14 episode cards now use the C queries. Golden Eggs has its three-child graph;
Egg Defender retains its additional new-sign placement; BirdDay UGC has its
nine-child graph, shared module state, gift-box transition, ribbon/gift tweens
and reached first-party particle boundary. The concrete episode scroller owns
the cards in source dictionary child order and supplies their layout, clipping,
anchors and unlock/rebuild events over the actual ScrollFrame. Every card's
selection reply passes. First/last 480x320 Metal renders were visually inspected.
The muted fixture services the real audio mixer into discarded samples without
opening an audio device.

This integration exposed a legacy missing-icon query. Read-only Binary Ninja
analysis of the pinned ARMv7 library confirms Resources::getSpriteWidth at
0x59bc98 and getSpriteHeight at 0x59bce0 return zero when findSprite has no
supported result; LuaResources::getSpriteBounds at 0x5b5b10 calls both. The C
bounds adapter now retains that result. Pivot/draw availability is still checked
separately. No binary patches or user analysis annotations were made.

All three builds, 94 selected Debug checks with ASAN/UBSAN, 103 Release checks and
formatting pass. The scroller currently requires the new layout without browser
extras or accelerometer control. Outer menu roots, saved selection, service
placements, whole navigation/gameplay and independent original screen comparisons
remain unfinished. Provenance: runtime-episode-services-owned.json.

Episode preferences now mutate the same owned local settings graph read by
progress queries. Stable catalog-derived keys, selected episode/page getters and
setters, visited-page lists and version marks preserve the source gates and
nil/false rules. Numeric array leaves are bounded and reject string mutations;
existing counter dictionaries keep their native iteration behavior. Checks cover
actual catalog selection/page/visit round trips, duplicate visits, locked and
non-restored pages, malformed false parents and preserving a newer version mark.
All three builds, the same 94 Debug/103 Release regressions and formatting pass.
This adds in-memory state mutation; save-file restoration/writing and outer menu
integration remain open. Provenance: runtime-episode-preferences-owned.json.


The interactive Metal preview now connects Play, the concrete episode controls,
arrow motion, saved selection and Back. A controlled integration fixture forwards
the real loading-event payload through the C Loader and receives
LEVEL_SELECTION_1; that destination is not yet installed in the application.
The preview uses an explicit empty in-memory save and opens no audio device.

Menu rendering now reads scripts/themes.lua and its actual graphic-set list.
Its missing defaultTheme selects the recovered theme1 fallback; all five layer
bindings are checked. blocks/themes_new.lua was an incorrect preview input: its
beach metadata references art absent from the Android profile. No replacement
art was introduced. Source menu bird candidates read actual tutorial dictionary
order, reward flags, all-page completion and feather totals; animation consumes
the shared script RNG. Controlled tests cover gravity, balloon phase, strict
removal boundaries, truthy zero unlocks and retina reward scale. Metal captures
exercise both episode and main-menu graphs. Startup, effects/services, disk
persistence, level selection/gameplay and original full-screen parity remain open.

Read-only Binary Ninja analysis also confirms missing sprite pivots return zero
(rebased addresses 0x59bd28/0x59bd70). ScreenBorderThemeLayer's separate four-edge
point path is implemented and tested with explicit metadata fixtures; theme1
retains its unmodified five layers. LLVM independently checked the border's
conditional min-dimension instructions after a Binary Ninja disassembly display
error. Address conventions and limitations are retained in
runtime-menu-route-owned.json. No binary changes or analysis annotations were made.

The interactive menu now explicitly selects the largest regular APK UI/font
profile, 1024x768, independently of window size. The archive's 1440x960 splash
images are separate splash variants, not a larger UI atlas profile; theme groups
retain the Android theme asset policy. The menu defaults to a 1024x768 window and
lays out/draws in native backing pixels at the actual window/fullscreen aspect.
The controlled 480x320 component tests retain their explicit original fixture.
MainMenu.layout 31235 words 13..128 supplies the non-template Android positions,
source Scaling calls and option-slider dimensions; the preview still supplies
its documented offline feature inputs and zero layout margins. Whole-screen
Android parity, root service placements and effects remain unverified/open.

Common has an uncommitted additive drawable-size/native-pointer interface over
Metal, Windows and Linux GLES2 hosts. The first-party renderer submits a full
native viewport through Common's existing indexed-pass API, preserving the
legacy fitted entry point for research fixtures. Window/fullscreen sizing and
input use the same backing-pixel coordinate domain. A requested diagnostic
capture is prepared/restarted at the current drawable size after native events,
so resizing does not leave the indexed readback at its initial window size.
Ordinary menu resizing changes source layout, episode anchors, UI scale, font
screen inputs and theme camera without texture decoding/upload or registry
replacement. Fonts admit the actual 1,122,356-byte high-profile BASIC texture;
the old 1 MiB entry limit had incorrectly rejected that resource.

Validation: full macOS Debug ASAN/UBSAN, macOS Release and MinGW Windows builds
pass; selected menu/theme/statistics/loader tests pass 86/86 Debug and 95/95
Release. The real high-profile font metadata matches both corresponding APK
entries byte-for-byte; Play uses the actual 1024x768 sprite path. Controlled
resizes through 1024x768, 1920x1080, 2560x1080, 320x240, 768x1024 and back preserve
the decoded sprite identity/texture count and successfully route Play/Back with
all 14 episode cards. Actual Metal native and legacy fitted clipping captures
each match 1,104,000 checked pixels at 1200x920. A native 960x540-point menu
window captures 1920x1080 backing pixels without fitted bars; a 1024x768-point
window captures 2048x1536. Windows is build-validated here; Linux runtime and
manual native pointer/fullscreen interaction are not established by these tests.
No audio output device is opened. Private captures, build/test logs and profile
inventory are recorded under the runtime-menu-resolution evidence job.

The catalog's explicit startup annotation step now implements common gamelogic
prototype 81430: page episode/index links; per-page versus episode-wide decimal
numbering; level episode/page/index/index_in_episode fields; no-progression
exceptions; and ordered regular/bonus name lists. The raw episodes.lua graph
remains the constructor result until this separate call. Existing table
identities remain stable; new lists and retained decimal text share the catalog
memory budget. Successful repeated setup allocates/mutates nothing. A failed
budget-limited call retains a prefix and rejects further annotation until the
owner is discarded. Interactive episode setup now invokes this step before
binding live services, so later level selection reads the actual annotated data.

An independent read-only Android snapshot comparison matches all 597 levels
across 17 episodes/40 pages, including all five runtime annotation fields and
597 ordered names. The 3,065 checks include page episode/index against their
independently captured parent identities; those page fields were not themselves
captured. There are zero differences. Bonus lists are empty in this target
catalog, so their nonempty source branch is implemented but not native-validated.
Twelve catalog feature/device configurations, source numbering/progression
boundaries, preserved identities, idempotence and the exact annotation memory
limit pass C tests. macOS Debug ASAN/UBSAN and Release each pass the three affected
catalog/statistics/Metal integration tests; the affected Windows targets build.
The component UI still requires its concrete level-selection destination and
actual loader/theme/gameplay integration. Catalog metadata coverage is not level
playability. Private source recovery, exports and independent comparison are
retained in the runtime-level-annotation evidence job.

The basic level-button component now composes the recovered createLevelButton
1648/createBasicLevelButton3931, open4755/close5982 and grid layout7510 over real
ImageButton/Text/Image owners. It preserves the owned event-meta table, child
order, override precedence, canonical Scaling and used-in-history gates. The
source star-image closure5681 selects episode, page, then level overrides. The
live settings adapter reads eaglesUsedIn/freeEaglesUsedIn; absent/false collections
use an immutable empty read view, not a simulated service response. The separate
level-progression module implements settingsWrapper.isLevelOpen28396 and
LSGridLayout.isLevelOpen6885, including previous-page completion, Lua truthiness,
release flags and the reached China purchase-list boundary.

Actual APK checks construct/layout/open-or-close/click/draw all 555 grid entries
on 35 pages of the prepared no-external-links catalog, retaining canonical event
metadata and real high-resolution images/fonts. Ordinary and composite buttons
are both checked. Controlled saved-score/history inputs exercise three stars,
perfect feathers, Mighty Eagle completion, video-button visibility and the
optional starsEmpty subclass child. First-page Metal output is retained; these
component captures do not establish original whole-screen parity or playability.

LevelScrollFrame21232/23299/23956/24509/25106/26244/26679/27351 now owns basic grid
buttons, source image/text visibility caches, entry/exit and live sync listeners,
layout/update/pointer ordering and a real C event timer. Its current profile is
prepared grid pages without locked/placeholder signs or accelerometer behavior;
other layouts reject explicitly. Episode one's 63 buttons/three page anchors,
final-page Metal rendering, live score sync and actual level-click metadata pass.
The source outer LevelSelection still needs integration with these owners.
Timer/BinaryHeap methods273/407/705/1108/1227/1404/1540 are implemented over fixed
caller storage, including strict deadline<time and right-child tie selection on
removal. Source-derived C checks cover deadline boundaries, equal keys,
callback-time insertion, empty timers and dequeue-before-error behavior; no
native timer trace comparison is claimed. Full Debug ASAN/UBSAN, Release and
Windows MinGW builds pass; affected suites pass 88 Debug and 97 Release checks.
Private recovery, captures and checks are retained in the runtime-level-grid job.


The basic LevelSelection outer owner now follows init7450, entry12098, exit13759,
layout14140, update16789, pointer17256, color interpolation18084,
updateSelection18701 and event19535. Its source controls, saved/visited pages,
Back, intro/direct/trial launch event identities and strict deferred page scroll
pass actual-APK integration checks. The interactive preview consumes the actual
episode callable through C Loader, loads real theme groups and reaches basic
level-selection roots. Metal checks cover theme1/theme4 transitions, input
suppression, Back/reentry and native resizing without resource reload. Specialized
roots, loading overlay, full root/SDK factories, disk saves, cutscene/gameplay
consumption and original whole-screen comparisons remain open.

Resources::drawSprite's explicit-size overload (Binary Ninja ELF-view VA0x59bbd8)
checks findSprite before dispatch and returns without drawing when absent; Lua
wrapper VA0x5b6e74 returns no values. The concrete resource adapter now preserves
that missing-sprite no-op while retaining failures for present unsupported or
invalid resources. The target's absent LS_BACKGROUND exercises the real lookup;
an explicitly controlled existing-sprite alias checks the common mirrored
ceil/floor background geometry at width1025, without inventing missing artwork.
Mac Debug ASAN/UBSAN, Release and Windows MinGW builds pass. The affected59 Debug
checks and renderer/Metal component checks pass. Private recovery, captures and
logs are retained in the runtime-level-route job. Native whole-screen parity is
not established by these C/GPU checks.


CompoSprite's native transformed draw (ELF VA0x58639c, Sprite transform overload
BN ELF-view VA0x5b4700) now has a shared first-party point-geometry consumer.
Three native corpora cover6,144 cases,15,360 ordered part quads and122,880 exact
XY words, including all anchors, signed part scales/factors, per-part rotations
and fractional positive parent scales. Both direct repeats and the checked Android
runner agree. The fractional checked corpus SHA256 is
31ff6d55e43f7446c7cdce7b80a183b74a1ef7a78feb179901bf6a02428a0f29.
The native matrix order scales context/composite/part offsets separately, then
applies the scaled pivot matrix; collapsing these operations caused ULP differences
and is avoided. KA3D/RVIO UI composites now share this path with the existing
verified default point-image shader/alpha policy. Rotated/direct parent contexts
and full GPU/native screen comparison remain open.

The CutScene component implements the single-sprite, non-film height-fit domain
used by the actual six pack1_intro records in scripts/cutscenes.lua. Source methods
738/1805/3456/4437/9110/10721/13496/13780 and settings methods32159/32280/32401
supply construction, entry, fit/reference sizing, command timing, cubic scroll,
render-state/clipping order, watched state and skip behavior. Metal renders the
actual91-part STORY_1_BEGINNING with the actual skip control. C checks cover the
3.5-second scroll start, midpoint, natural completion/save notification and held-entry
skip suppression. The save fixture observes its boundary; it is not disk transport.
This component is not yet connected to interactive launch. Other commands, film
mode, complete root factories, persistence and original cutscene/frame parity stay
open. Full Debug/Release/Windows builds and formatting pass;132 Debug ASAN/UBSAN
and141 Release affected checks pass. Private recovery/captures/comparisons are
retained under the runtime-cutscene and runtime-composite-draw jobs.

The saved-table writer now matches all 256 exact outputs from the pinned native
LuaTable::write overload (Binary Ninja ELF-view VA 0x77d7b8). It preserves six
significant digits, root assignments, nested/dense formatting, reserved names
and string escaping. The checked isolated Android corpus has SHA-256
ce3b7a57e1026e0f232075eb67aa95046409c7c7052d1fda9d767edd17b38ede;
check_lua_write_reference.cmake locks that input. Separate save-key operand
recovery matches the retained native export's 32 bytes without embedding key
bytes in authored source. SettingsFile composes bounded literal reading, actual
owned hash/array iteration, target AES framing and Common atomic replacement.
Malformed existing files reject without automatic overwrite. The supported
restore graph remains three string-key path levels with scalar-array leaves;
Android save migration and the original global scheduler remain open.

The interactive menu now follows GameluaMenuListener.eventTriggered3611's
load/play-intro event split (instructions 621..704), through its own C Loader
operation, into the actual six-record pack1_intro. Connected Metal output was
inspected at 1024x768; resizing to 1920x1080 retains resources. Natural completion
persists watched state; a newly opened host restores it and menu preferences.
The isolated skip check still preserves the source absence of an immediate save.
The preview additionally checkpoints its own two settings files on clean exit.
Legacy CutSceneAssetManager release scheduling, other cutscene domains and
original full-cutscene timing/pixel comparison remain unverified. The retained
CHANGE_LEVEL now resolves canonical level metadata; world/session construction
and the full GameHud remain unfinished.

The page-list request resolver checks all 594 catalog records, including 589
existing level files and five explicitly distinct non-file soundboards. It
preserves prepareLevel28689's repeated selected-page-length offset and the
page-local getLevelNumber25101 result. Separately registered bonus requests are
outside this binding. The new preparation owner composes source globals,
SettingsWrapper43226/43343/43511/43733 attempt storage, page preferences,
analytics helper28090, loading events and closure30379. Controlled providers
exercise the real EventManager/Loader, previous stars, first/repeated attempts,
nil versus zero minimum time, live before-load path changes and stopped error
prefixes. These C integration checks do not establish complete native prepare
or selected factory/session fidelity. Private source provenance is the
runtime-level-preparation and runtime-museum-factory jobs.

Settings storage now accepts explicit capacities per creation slot. The save
reader plans capacities in actual import order, preserving constructor hints
while bounding both entries and hash storage. A 600-counter encrypted round trip
checks capacity planning and every restored value with less than 1 MiB of reserved
fixture storage. Interactive saves restore before preference construction and
reserve broader dictionaries for new campaign counters; no frame-time allocation
or resize-time resource loading was added. Native writer/key tests, strict
Debug/Release settings and connected Metal checks pass, and Windows cross-builds
pass. Full startup, special roots, complete GUI and playable levels remain open.

Native GameLua.getTimeDifference (Binary Ninja ELF-view VA 0x339728) takes the
absolute double difference, truncates to unsigned32 seconds, then uses integer
division/remainder for days/hours/minutes/seconds. ARM instructions confirm
truncation despite the decompiler's rounding expression. All 512 UTC vectors
match the original corpus, SHA-256
911531d1b4098bb0bf30d1b387d5397396330330f96800fd85a43f129ec9d2d3;
check_calendar_parts_reference.cmake locks it. This retains low seconds over
long intervals that the separate binary32 seconds operation would discard.

Both default_conf.lua and game_conf.lua set g_giftbox_enabled=true. The new local
session owner implements GiftBoxManager3979 and SettingsWrapper22446/23251/24621
against actual owned settings. Clock-table field insertion follows native
GameLua.getCurrentTime (ELF-view VA 0x33944c), with the existing target string-map
order used for the deep copy. Controlled C checks cover default/future timestamps,
exact thresholds, raw remote overrides, skipped invalid fields, clock mutations,
second-clock failures, stopped prefixes and retained replaced tables. Preparation
now increments this actual owner. Gift placement/rewards, complete native gift-box
method replay and enclosing Museum/GameHud/session assembly remain open.

The startLevel owner composes commongamelogic24424 with CameraSystem6389:
fresh default touch input, both movement curves reset, explosion duration/amount
removed, cameras-initialized notification, actual default/restart sequence, loading
done and conditional goto-game events, promo traversal and live level-hook lookup.
The camera owner now accepts the two startup sequence resource names in addition
to its existing follow/end resources. Its APK fixture checks original sequence
data, numeric-zero restart, editor mutation during events, retained event identities
and a stopped invalid-hook prefix. PromoCampaignSubsystem root instruction 9
creates the empty private registry captured only by init452. Fresh pinned recovery
matches the earlier 2,054-byte input, SHA-256
b060d4ee45a873fa180eb24542c380cb26e3d8c094b5f0f640f50763005c598c.
Its selected empty campaigns table entails no service response. Nonempty foreign
campaign classes, complete native start replay and the connected GameHud remain open.

The concrete Slider uses the pinned ui_components/Slider.lua, 8,618 bytes,
SHA-256 3ed926798d118cc6b125274e6604797cf04015abf20f3b459ebc49e569cd9638.
Its actual update3798/getTotalSize8109 and the four original Utils builtin curves
now match 512 independent native cases (16,384 words), including all four directions,
six states, negative/zero/positive elapsed time, fractional scaling and nil/false/
numeric-zero size overrides. The repeat capture is identical. Checked corpus
SHA-256 ab329b54f6978ec1073c48060d839eeda2aa4754a4300894dfbdda81cb7fda29;
check_ui_slider_reference.cmake locks it and run_android_ui_slider.cmake verifies
all six selected original function hashes before staging. RefreshImages and
BGBox.update are explicit observed boundaries in that original-method probe;
the C side executes its actual owned image setters and child updates. This is
update/layout parity for that numeric domain, not complete native widget/GPU
execution. Additional C integration checks
content insertion ahead of the toggle/indicator, source false-or-nil enabled default,
strict closing below zero, closing through the opening curve, disappearance,
directional layout, numeric-zero overrides, clipping reset, pointer notification
gating and stopped errors. Whole-widget draw and GPU pixel parity remain unverified.
Complete GameHud/session construction remains open.

The powerup catalog reads game_conf's ordinary six-name activation array and the
actual PowerupDefines literal resource (3,322 bytes, SHA-256
7738c56fbca451e8c73637096d7e1e883852eaad0a95104e307c0ef2f47f4234).
It retains definition tables, initial raw amount overrides, infinite flags and
floored reward probabilities. The first four weights produce 25 each; shockwave
and redsmightyfeathers produce zero. The latter's initial infinite=true is
metadata; PowerupManager.reset later clears it. This catalog does not resolve
effect functions or initialize a full manager, inventory or purchase service.

The concrete HUD powerup subtree follows GameHud.init4036's ordinary slider
branch and layout22112's corresponding subsection. Its four bar buttons keep
registration indices, distinct button/tag metadata tables, actual counter
background/Text/tag child order and source resource/touch-padding rules. The
shop/trial and no-birds branches are tested against actual 1024x768 SPRT atlases
and decoded fonts in sixteen C integration variants, including release-click
metadata. The actual generic Slider supplies animation, clipping and pointer
routing. PauseButton and SkipButton compose the real ImageButton base, with
source gesture drawing, appearance/disappearance scales, return-value clearing
and base-child mutations before derived updates. GameHud.lua provenance is
56,990 bytes, SHA-256
2cca717635bd10f8bd14b2b547bdb7378a1491e5c7eb46060b2062582c73eb27.
These component checks establish C integration and source interpretation;
complete original widget/GPU comparison, live
powerup action/inventory owner and full HUD/session remain open.

Binary Ninja confirms drawUITextInternal at ELF-view VA0x3372b8 reads text
through LuaTable::toString0x77c14c. That adapter calls lua_tolstring0x838f38,
constructing a C-string-based std::string; its numeric helper0x83b964 uses %.9g.
The concrete Text draw now projects raw numbers without changing their saved
field, truncates text at NUL and draws empty text for other plain kinds. A new
isolated original-binary probe matches all 65,536 conversions, including 65,110
numeric words and all 895,173 resulting bytes, with an identical repeat. Corpus
SHA-256 79a0f91bd6385b273df389dfc429bd33e6bffbd80ae7209e3920f15281e615c2;
check_ui_native_value_text_reference.cmake locks it. The probe executes the
actual conversion primitive; the C-string wrapper semantics are independently
verified by disassembly/decompilation. The earlier plain tostring corpus remains
unchanged and exact. No analysis annotations or file patches were made. All
three full builds, thirteen focused Debug/Release checks and formatting pass.

Concrete Text.clip now uses actual cumulative widget scales, retained decoded
font metrics, the existing native-compared wrap/line-constructor projection and
the clipped native draw plans. Source Text1719 creates a separate lines list;
it does not insert these objects into Frame.children. Caller-owned pools retain
successive line states, names and UTF bytes so re-clipping preserves earlier
identities and strings. Drawing visits those line states regardless of their
visible fields, then traverses the root's separate Frame children normally.
Height queries now use the actual lines count. An APK-backed 1024x768-font test
checks cumulative/own-scale wrapping, constructor scale reset, line spacing,
height, drawing, visibility bypass, retained re-clipping and stopped failures.
Sixteen focused Debug/Release checks and all three full builds pass, including
the unchanged native wrap/metrics/clipped-draw corpora. This adds C composition
over those verified primitives; it does not establish a complete native class
or GPU comparison. Colored text, nonstring raw clipText arguments, embedded-NUL
localized wrap inputs and custom line classes remain outside this contract.

The concrete ordinary PausePage now owns its actual dummy/middle/button/Text
graph, source layout, timer, panel draw, release input, channel/audio events,
show/hide ramp and lifecycle. APK-backed C checks cover ordinary, desktop,
shop-demo and Mattel profiles, real Resume replies and keyboard audio changes.
The common showPauseMenu service stops wood_rolling, rock_rolling and
light_rolling through the actual APK AudioBank before emitting distinct events;
setScalingEnabled retains its source private snapshot and misspelled public
clear. Registered C font-getter identities support Text constructor/layout calls
and explicit rejection of unknown function tokens. Twenty-three focused Debug
and Release checks and all three full builds pass. PausePage source provenance:
12,779 decoded bytes, SHA-256
7af3cd4187c97287ad80bb3464ac9e51035575a80642fd866bda9b4eae548992.
These establish source-derived C composition over separately compared primitives;
complete original class/GPU execution and live GameHud/session remain open.

MightyEagleButton now composes the actual button, registered font getter, clipped
timer Text, Scaling and EventManager owners. Source-derived C checks exercise
disabled-click cooldowns, strict visibility, event/animation order, childlock,
editor paths and the original onExit omission of LEVEL_COMPLETED cleanup. A
separate lifetime detach removes remaining subscriptions. Its bound services
read the same live global/settings/purchase/level graph, preserving raw flags,
metadata overrides, strict cooldown expiry and reached missing-service errors.
Utils6319 timeDiff subtracts time-of-day values and adds exactly one day whenever
day fields differ; it ignores year/month, unlike the native calendar helper.
AnimatedImage preserves hidden image refresh, child updates before stopped checks,
single-subtraction looping and frame-zero stopping. Its 1,608-byte source hash is
2e04db02e84e4db326a4c9aecf2465cef320207c55c27803b38aead7f659622f.

SettingsWrapper35270..36064 now supplies actual lazy inventory operations on the
owned settings graph. installationSpecificInventory has no extra installation-ID
level. New records receive totalCount=0 then usedCount=0; initial awards belong
to separate bootstrap work. Arithmetic preserves fractional and negative balances,
and malformed records retain source failure prefixes within the supported scalar
domain. PowerupButton.setEnabled also refreshes its actual bg child. Twenty-nine
focused Debug/Release checks pass, followed by the changed native shot replay,
inventory/storage roundtrips and bound-class checks. All three full builds and
formatting pass. These are C composition checks over separately compared
primitives, not complete original class/GPU or gameplay parity. Native inventory
replay, full GameHud construction and continuous gameplay remain open. Private
provenance: runtime-hud-eagle-services-provenance.json.

The ordinary Android GameHud constructor4036 and layout22112 now compose the
actual owned widgets at 1920x1080, 1600x1200 and 900x1600 using the highest
regular APK profile. The captured restart flag is root register0, selected at
class load; the separate global endGameAtPPBack1948 returns false. Construction
preserves child/HUD-list order, black-bird counter/animation, optional RMF and
tutorial-flow publication. Reached missing SaleBanner/flow services reject.
The root remains the source-initialized Frame pending the full GameHud methods.

Pause methods29572/30249/31142/35001 preserve normal/instant page changes,
visibility queries, the Sugotoku resume branch and RMF's true-only restoration.
The actual common pause actions stop rolling audio and capture/restore zoom.
Return queues a distinct BACK_TO_GAME_CLICKED table before calling the actual
sling-cancellation owner; the combined fixture tests the missing-sling prefix,
while the cancellation primitive retains its separate native comparison.
Live HUD queries follow GameHud17753/28631/29144/36134,
SettingsWrapper10434/25069/40138/40394/47127 and pagelist classification from
LevelUtils1809. Audio preference30590 initializes only a nil value in the actual
owned localSettings root. ShopOffer remains a required separate owner.
These source-derived C composition checks do not establish whole native-class
or GPU parity. Root lifecycle/input, powerup factories/actions and continuous
Metal gameplay remain open. Private provenance:
runtime-hud-controls-provenance.json.

The original full GameHud chunk and Class resource now execute three visibility
methods in the muted Android reference. All 12,288 typed replies match across
4,096 input sets, with byte-identical repeated captures. This exposed and fixed
an inverted RMF device gate: the target predicate accepts iPhone/iPad. Level
lookup/type, settings replies and ending state are controlled boundaries in
this probe. This establishes these methods' branching, not their complete SDK,
constructor, event or GPU integration. The 700,491-byte corpus SHA-256 is
164f6cbd3a3035214d1d4b18cd17930a453536d5211ae0aba9c23b2c4f078cbd.

Fresh level ownership now derives Museum scaleFactor from authored
physicsToWorld, using100 only for nil, divided by the actual global20.
gamelogic.initGame67409 supplies physicsScale=1/20. Its temporary defaultForce=-800
is overwritten by its later updateValues66976 call: gameplay uses -925. The same
call sets the block destruction increment to500, pig increment to5000, remaining
bird increment to10000 and collision sound threshold to3. The fresh owner now
uses that final force; the earlier temporary-value interpretation was incorrect.
The new level owner prepares actual sequence resources, responsive camera snaps,
world/playfield owners and first-bird pose. Its external canonical shot record
exists before level construction and survives adapter creation unchanged.
Source sling-reset state preserves allowResetToBirdCamera and resets reload,
drag/selection, cooldown, band and transfer state. The real APK construction and
normal transfer pass at1920x1080; native shot/playfield regressions also pass.
Queued birds remain static until their real transfer. This is preparation and
component composition; complete Museum event/construction order, continuous
frame/GUI assembly and desktop gameplay activation remain open. Private
provenance: runtime-session-owner-provenance.json.

The fresh ordinary world now owns shared death/hat/teleporter maps, score records,
default block-class attachments and the real contact/damage/removal assembly.
Ground bypasses ObjectFunctions' block-script application, matching Museum's
direct GameObject construction. The three default classes use their recovered
component-table type gates; reached active components still reject until their
concrete effect owners are connected. Particle preloading now collects actual
resolved object descriptors, legacy names and raw damage-state names before ticks.

The GameUpdate and rendering owners compose the existing source-derived phases
with shared canonical input, completion, physics and activity fields. Native
activity Booleans publish before rolling audio. Input reads the same hat map as
collisions and retirement. Initial coordinate queries derive constructor screen/
zoom extents without a synthetic camera tick; later queries use the published
viewport. The original camera constructor3081 establishes screen=(0,0), and its
CameraMover establishes zoom1.

The headless level-1 integration reaches its winning EID_LEVEL_ENDED event through
startup, synthetic mouse input, continuous physics, actual controller updates,
camera, decoded audio mixing, particles, scene commands and unused-bird bonuses.
The Release run uses seed 7, a 1/60 step, 1920x1080 logical pixels and the 1024x768 asset
profile. It produces 746 GameUpdate frames, 343 physics steps, 53,095 draw commands
and score 28,390. These are C integration results, not an original full-session or
Metal pixel comparison. Root/GUI construction, application transition activation
and the remaining active class domains remain open.

The interactive Metal composition now starts DefaultLevelStartCameraSequence with
the source CameraSystem input and cubic tween initialization. A real main-menu,
episode, level-grid and intro route reaches a mouse-driven level-1 win. Its actual
winning card uses LevelEnd/LevelComplete layout, score-count and star timing,
decoded fonts and images, highscore state, menu particles and audio. Next resolves
the canonical catalog entry (the second Poached Eggs resource is Level57), Restart
constructs a fresh session with RestartLevelCameraSequence, and Menu returns to the
retained level grid. ImageButton CLICK and HOVER replies do not become navigation
actions. Controlled later-level result events test navigation separately from the
actual level-1 win; they do not establish later-level gameplay or native parity.

GameHud.eventTriggered48422 makes the real SkipButton appear after goals are
cleared without eagle/gesture mode. The separate HUD group supplies
HUD_INGAME_1; it is absent from the gameHud dynamic requirements. The source's
createAssets54058 and cutscene return1223 load that group independently. The app
now loads it before layout, so the visible sprite and its hit rectangle share real
dimensions. The recovered skipToLevelEnd103193 binding recomputes remaining-bird
bonuses, preserves the individual-score append list, publishes score and queues
the actual level-ended event. The connected test clicks the bottom-right control
and reaches the card. Natural completion is tested independently. Full GameHud
ability/pause ownership, failed cards and native whole-session comparisons remain
open. Both Metal paths pass Release and ASan/UBSan; the Windows application
cross-build passes.

Metal scrollWheel, Windows wheel messages and X11 wheel buttons now publish a
shared first-party scroll event. The game applies a desktop wheel-to-zoom mapping
around the current pointer world anchor, using actual per-level close/far limits
and camera targets. The Metal integration checks both directions after canonical
Next navigation. This input mapping is an authored desktop policy; the Android
build has no mouse-wheel parity claim. Native Windows/Linux wheel interaction
still requires runtime checks on those platforms.

The reported louder pack click is separate from music gain. A live mixer route
from Play through Poached Eggs retains one music voice at gain0.8 with unchanged
master/track gains. ScrollFrame6059 calls its child pointer traversal twice;
EpisodeSelection3398 requests menu_confirm once more. The desktop application
now coalesces that confirmation within one episode pointer dispatch. This is an
intentional presentation adjustment requested by the user, not native audio-call
parity. The recovered widget traversals and independent gameplay mixer voices
retain their behavior. The Metal test requires one new confirmation voice for
the pack click and the same music voice before/after selection. Release and
ASan/UBSan routes and the Windows application cross-build pass.

DangerAboveLevelSelection and MineAndDineLevelSelection now wrap the actual base
level screen. Source ep3_level_selection.lua prototypes368/889/1355/1857 define
Danger Above's scroll-child egg, visibility and discovery threshold. Source
ep6_level_selection.lua prototypes358/679/979/1398/1827 define Mine and Dine's
rotated decoration-relative egg, release activation and shrinking timer. The
settings helpers20563/21001/21122 retain nil-only row creation and raw unlocked
values. Discovery uses EID_GOLDEN_EGG_FROM_MENU, the actual save boundary and
decoded goldenegg sound. GoldenEgg1088/1966/2118/2372 and showRewardPopup94726
with children95959/96292/96493 supply the reward graph, shade and three 1.2-second
actions. The application owns a scoped ProcessManager for this popup; general
NotificationsFrame composition and overlapping rewards remain open. Controlled
Metal selections discover both eggs, inspect the middle action, finish the
process and confirm hidden state on reentry. Release and ASan/UBSan pass; these
are integration checks, not native whole-screen comparisons. Bad Piggies,
Red's Mighty Feathers and Golden Eggs screen classes remain open.

The UI renderer now accepts collapsed Image transforms whose source draw plan
contains division-by-zero translations: it retains sprite lookup order and
transient state, suppresses zero-area geometry, then applies the original reset.
Mine and Dine's visible egg reaches this state at the end of its shrink. Direct
vertex draws retain their existing path. Edge checks and all 4,096 existing native
UI render cases pass; equivalence to the original GPU's singular geometry remains
unverified.

AbClassicLevelSource verifies the pinned archive once and retains its decoded
blocks.lua catalog, constructor metadata and resource key within 16 MiB. Its views
require the same immutable archive identity. The application keeps that owner
across Next, Restart and menu return, closing all borrowing gameplay before the
cache. Each new session still decodes its own level/camera/particle data and
creates fresh objects, world, score, timers and input state. No RNG or mutable
physical objects are cached. Camera and particle loaders reuse the verified key.
The optional AB_PROFILE_LEVEL_LOAD environment variable reports three setup
phases using the platform monotonic clock. In a macOS Release Metal integration
run with 1024x768 resources, Level1's three phases total about 357 ms, versus
640 ms before reuse. Canonical Next to Level57 totals 5.14 ms versus 620 ms;
Restart totals 3.93 ms versus 621 ms. These are measured application setup phases, not original
timing or a guarantee for other levels, cold themes or later GPU work. The test
checks stable catalog identity through Next and fresh Restart shot state. Seven
connected/native regression checks pass with ASan/UBSan, and Windows compilation
and formatting pass. Private measurements: load-profile-before.log and
load-profile-after.log under the analysis input directory.

The ordinary BOOST branch of triggerSpecialty125163 (instructions 43..101) now
uses actual settings, body, particle, audio, visual and trajectory owners.
utils.vNormalize10029 supplies the strict two-component 1e-6 gate and float
arithmetic around the double sqrt conversion. Binary Ninja inspection of
GameLua::applyImpulse at ELF VA 0x322790 confirms that the three-argument call
selects the body's transform origin; the existing impulse adapter preserves
that offset from its center of mass. No binary/database mutations were made.
The Level16 fixture deliberately separates cached script velocity(3,4) from body
velocity (10,20), checks the resulting boost, decoded sound, specialty sprite and
attached puff, then draws through Metal. This is controlled integration evidence;
an original active yellow-bird playthrough and the remaining bird abilities stay
open. Release and ASan/UBSan checks pass.

Losing EID_LEVEL_ENDED packets previously stopped the game without constructing
UI because the application only opened cards for wins. It now builds the
ordinary LevelFailed graph from 33998/37612/39483/40476: localized failed title,
theme-selected pig, source 206-height ReferenceScaler placement, two-button
layout for an uncleared level and the single inherited background update/draw.
It retains prior completion for the supported Next gate and uses the same actual
Retry/Menu navigation. Active Eagle/video/cleared-cutscene variants remain
explicit recovery boundaries. A new test misses all three Level1 birds using
mouse input, reaches the real failure timers/event, captures the card on Metal,
clicks Retry and returns through pause Menu. It injects no losing event.

The application now composes the actual PauseButton, PausePage and common-game
pause/scaling actions over the live audio/settings/event owners. Escape release
uses a desktop pause/resume mapping during gameplay, and Back on a result card;
it no longer requests application exit. F retains fullscreen control. The pause
test gives a dynamic body nonzero velocity, verifies its frozen pose, captures
the extended page on Metal, resizes without asset reload and resumes. Source
shop/channel capability gates remain unavailable on this offline host; tutorial
scene navigation, full GameHud lifecycle/ability composition and native whole-
session comparisons remain open. Private visual checks: pause-metal.png and
failure-metal.png under the analysis input directory.

Direct game launches now exit the active level grid before constructing gameplay,
and intro launches likewise exit their cutscene. The old direct route left the
grid subscribed, so pause Menu tried to register the same listeners again.
The application records whether its GUI graph is entered and enters the retained
grid once on return. Duplicate-listener rejection stays unchanged. The regression
repeatedly clicks the actual second level button, opens pause, uses Resume,
Restart and Menu, then verifies that grid listeners are absent during gameplay
and registered once after return. The Release Metal route passes with these
checks, alongside the separate actual win/loss routes. Both natural/fast-forward
Metal routes and the native shot regression pass with ASan/UBSan; the Windows
application cross-build and formatting checks pass.

The Red's Mighty Feathers level-screen subclass now composes its actual alternate
left/right decorations, two-stage slide and second-page background tint over the
retained LevelSelection owner. Evidence is the pinned episode9 subclass methods
485/1323/1557/1846/2600/2730; the Metal integration test traverses both pages and
returns, checks phase endpoints and covers all 30 catalog buttons. This recovers
selection, not Egg Defender gameplay or a native whole-session comparison.

Ham 'Em High's three additional levels were omitted by the application's
`disable_external_links` catalog input, which removed its fourth page. That input
now retains the full Android catalog. The LSFacebookLayout recovery uses source
methods 8492/8889/9344/9396/10130: the three ordinary level buttons are locally open
without a social login. The page retains original Like artwork, child order,
scaled row placement and settingsWrapper.shouldUseAgeFilter48481's strict Boolean
rule. Tests reach page 4 through real arrows, select all three canonical records,
verify actual APK resource paths, capture Metal output, and check both live
age-filter re-entry and filtered fresh construction. The enclosing root's social
service action and complete playthroughs of those levels remain open. No network,
authentication response or service reward is simulated.

Continuous native-drawable presentation previously called Common's explicitly
nonblocking empty-frame entry. Reusing one of its three slots while Metal still
owned it produced `indexed frame storage is still in flight`; capture-based tests
hid this because capture waits for each command. Common now also exposes a
waiting empty-frame entry, and the application uses that entry for native frames.
It waits only for the slot about to be reused, before dynamic vertices, textures
or uniforms are copied. Other slots remain queued; submission stays asynchronous.
The original nonblocking API and outside-frame depth-preparation rule are kept.
GLES2 uses its existing ordered path through the same new interface.

The regression submits 256 ordinary frames without capture, then compares every
pixel of a 1200x920 native frame. It reproduced the exact failure before the fix
and passes after it with Metal API validation. Metal and Apple GLES2 pass tests,
menu/level/result/pause integration, ASan/UBSan natural/fast-forward routes, and
the Windows GLES2 cross-build pass. The primary Files/build cache had retained
AB_SANITIZE=ON despite Release mode; it is now rebuilt with AB_SANITIZE=OFF, as
shown explicitly in normal README build commands. The rebuilt app links no ASan
runtime and its actual APK-only launch emits neither the nano-zone warning nor
the renderer error. No allocator environment override suppresses diagnostics.
Private logs use the `metal-slot-` prefix in the analysis input directory.

BadPiggiesLevelSelection now wraps the actual three-page/45-level base with its
original particle Frame and pig ImageButton at child positions 1/2. Methods1751,
2403,2753,3012,3273 and 3608 supply feature gates, GREEN_UPSELL entry/release,
shared-script-RNG animation changes, four-frame artwork, entrance cubic tween,
bob/rotation and alternate-list green_rocket emissions. The source's unused
floorCoodinates misspelling does not disable Image.floorCoordinates. Its sqrt
call passes dx*dx and dy*dy separately; only the first argument is consumed.
Menu and gameplay particle owners now share the retained engine RNG.

Binary Ninja confirms the early menu-particle draw and later update in the pinned
ARMv7 library: ELF VA0x3070b8 GameLua::drawMenuParticlesInAdvance and
VA0x323ec0 GameLua::update. This session's Binary Ninja ELF candidate is rebased
by 0x10000; its displayed addresses are 0x3170b8/0x333ec0. Dynamic ELF symbols and
PT_LOAD headers independently distinguish those addresses from file/raw VAs.
The app draws menu particles at the actual first child boundary, then advances
that list after script draw. The muted reference emulator remains configured
with `-audio none`.

Rotating Bad Piggies artwork exposed the prior unsupported composite-parent
rotation. CompoSprite::draw (raw ELF VA0x58639c; Binary Ninja view 0x59639c) measures
its context columns, applies positive column lengths to context translation and
anchor, then composes the part offset/rotation and sprite pivot. Rotation origins
are unused by this overload. The C consumer now supports signed finite nonzero
parent scales and rotation while retaining its existing positive-axis path.
A new isolated original-binary probe covers 2,048 cases / 5,120 ordered part quads,
including reflected contexts and independently rotated parts. Every XY word
matches. SHA256: 3e4dfdcd3365c73ae71cb38108466a5fe59421874dbf87c474d17cbfb6c85693.
The original positive-context corpus still matches. Optional
AB_REFERENCE_COMPOSITE_CONTEXT_DRAW checks the new pinned trace separately.

The Bad Piggies Metal integration test verifies 45 canonical resource paths,
three pages through actual arrows, real loaded rocket sprite handles, source
button/event routing, changing animation, and fresh exit/re-entry. Its captured
screen was visually inspected. Release checks pass for the integration route,
both native composite corpora and 4,096 existing UI-image native cases. Both
natural/fast-forward integration routes and composite/UI edges pass under
ASan/UBSan in the separate build-reference directory; the Windows application
cross-build and project formatting pass. Service-link dispatch and all 45 level
playthroughs remain open; these checks do not establish whole-screen native
fidelity. Private recovery/captures/logs use `pigs-` in the analysis input directory.

Golden Eggs now uses its actual LSGoldenEggsLayout and GoldenEggsLevelSelection
rather than an ordinary grid. Layout methods 11169/11489/12280/12400/12786/13050/
13670 create page halos before base ImageButtons, each with its own carved-star
child. They preserve source scattered coordinates, disabled-override silhouettes,
unlock/played/completed visibility, scaling and rotating halos. Screen methods
308/752/1548/1711 retain LEVELSELECTION_GOLDEN_EGGS lifecycle, second-page
unlock/anchor/dot rules, and the King Pig control with the original seven-episode
gold-score gate and explicit debug-build bypass. Enclosing hint dialogs, King Pig
mode routing and gameplay remain separate recovery work.

The application's highscore graph formerly reserved 16 entries per nested map;
creating all 32 Golden Egg rows exhausted that bound. It now reserves 64 entries
per map within a 16 MiB bound, retaining the 1024-entry root and 1024 table slots.
These are implementation capacities; native constructor hints, hash growth,
iteration and table identities are unchanged. A separate test-owned profile
writes all 32 actual unlock/played records through the application opener and
APK-key encrypted save, releases both owners, reopens/restores, and verifies
all 32 flags. Runtime user profiles are not used by this fixture.

Metal checks cover all 32 source button classes/metadata, 27 physical APK paths,
the five fileless registered music/toy entries, silhouettes, an initially hidden
second page, unlock/re-entry/selection, played/completed refresh, halo clock and
release/debug King Pig visibility with its real artwork. SOUNDBOARD1, RADIO,
KEYBOARD, SEQUENCER and ACCORDION have no physical level file; their registered
controllers remain unimplemented and are not counted as playable by this test.
Captures use the application's actual theme clear color, unlike earlier explicitly
black component fixtures, and both pages were visually inspected. Release and
both ASan/UBSan natural/fast-forward integration routes pass, as do the Windows
application build and formatting. Native whole-screen fidelity and all-level
playthroughs remain unverified. Private logs/captures use the `golden-` prefix.


Green-bird flight now binds triggerSpecialty125163 instructions 413..519 and
updateBoomerangBirds108793 through a shared first-party owner. Activation reads
cached physical velocity and creation mass, records the source turning forces
and velocity limits using the current ordinary sling power, changes the actual
sprite, plays its resolved sound, and attaches a trajectory puff. The controller
runs pre-activation lift/spin and activated force/spin updates after the camera.
Forces enter the existing measured per-physics-step queue; the application now
reserves 1024 requests rather than leaving that queue at zero capacity. The
Facebook branch retains the source native-velocity/cache separation; it is not
claimed as exercised by an ordinary target catalog bird.

Museum load bounds are now retained alongside level bodies before generated
ground construction, including the original full-width circle sentinel. Swish
volume uses these immutable physical edges, not camera bounds or moved bodies.
The selected ordinary sling exposes its existing power-one state: Slingshot
constructor26738 initializes power=1, getPower29185 reads it, and
SlingshotSystem.getSlingshotPower23570 delegates to that selected sling. Powered
sling classes remain separate work.

The actual Danger Above 6-5 metadata selects LevelP3_224 and three BoomerangBird
objects. A controlled APK test checks queued lift, ordered turning forces, spin
caps, real specialty sprite/audio/puff and the angle-crossing swish with edge
attenuation. A fresh session then uses only mouse aim/release/activation and
ordinary frame updates to launch right and reverse the bird's simulated X
velocity. Its final game frame renders on Metal. The native original full-flight
trace has not yet been compared; these are functional checks and source recovery,
not a claim of whole-session 1:1 fidelity.

That cross-episode test exposed a separate menu-theme lifetime defect under
ASan: menu_theme_select retained a level-owned name after the level closed. It
now resolves the identical key owned by the persistent decoded theme catalog,
which also keeps the settings/theme observers valid. Both natural and
fast-forward Metal integration routes pass ASan/UBSan after this repair. The
ordinary Release Metal route, all 14 episode screen factories, 105 related
classic/level/object/bird/force/controller regressions, and Windows compilation
also pass. Private recovery, captures and logs use the boomerang- prefix.

Black-bird BOMB activation now binds the shared explosion boundary used by TNT
and removeBlocks as well as the source timed fuse. The recovered source is
triggerSpecialty125163, Museum.updateBird19137, makeExplosion152242,
ObjectFunctions.museumCreateExplosion166 and inflictDamage151911. Preserve
strict timer<0 detonation, both warning sprites, source radius/defence and
distance arithmetic, physical impulse/cache separation, death-map insertion,
damage-sprite writes, particle order, channel-one sound at 0.7 and shake. The
explosion scale changes shake only; addExplosionScore152497 is empty. Joint
break callbacks remain explicit reached boundaries where no concrete queue is
bound. This does not establish joint-connected explosion fidelity.

Actual Poached Eggs 2-5 selects Level88 and four BlackBirds. Controlled checks
cover manual activation, real pig damage/death, impulse, art/audio/effects and
parent removal; fuse checks retain a live bird at exactly zero. Fresh sessions
use actual mouse aim/release/activation and a natural ground collision to start
and expire the fuse, then draw on Metal. The related 130 regressions, Windows
compilation and both Metal ASan/UBSan routes pass. Full original black-bird
flight/detonation traces remain unverified. Private logs use the bomb- prefix.

That first explosion exposed zero-scale world particles at their first frame.
Native Particles.drawParticles (BN VA0x30155c) submits zero-context-scale quads
whose coordinates are nonfinite after division by scale. Thirty original draw
records for three actual definitions and six phases are retained, SHA-256
4522033e1256087ee91f9b32abf66b2aece58098ab1a59e10a6fc70fbd360eaa.
The ordinary draw adapter suppresses only those nonrasterizable submissions;
particle simulation, lifetime, RNG and resource validation remain active. Their
invisibility is inferred from submitted geometry, without a separate original
GPU pixel capture. Existing nonzero native particle comparisons still pass.

The catalog-wide private startup audit resolves actual episode/page metadata,
opens the application session and runs three ordinary frames per request. It
does not grant progression or replace missing scripts. The first run exposed
the absent res.stopAllAudio call at Museum.loadLevel2711 instructions50..52;
restoring that original load boundary prevents accumulated music/effect voices
across transitions. The harness explicitly drains audio even after constructor
failure, reflecting the application audio thread's normal work.

Source newParticles152993 returns before the engine for nil/empty names. Static
fragile materials legitimately retain an empty string; the preloader no longer
treats it as a filename. Contact RNG and geometry/angle queries still precede
the wrapper at their original boundaries. The actual LevelP3_216 regression
checks that the empty single-name call consumes neither engine nor script RNG.
The removal owner now reserves exact-size normalized immutable definitions in
one bounded pool, reusing aliases, instead of reserving the full per-definition
ceiling for every object. Large Level745 now starts and renders without raising
its memory budget. The full 590-request pre-rectangle audit completes under
ASan/UBSan without a memory error, with 468 constructors and 432 three-frame
starts. Inventory scope excludes full playthroughs and native session parity.

RectThemeLayer constructor/draw (BN VAs 0x2f491c/0x2f2110; raw ELF VAs subtract
0x10000) now bind the recovered nested RGBA/relative rectangle data. Relative
bottom is actually used as a top-down Y fraction. Preserve the original
binary32 color reciprocal, nil/default rules, alpha>0 gate, nonfused extent
arithmetic and existing GameLua.drawRect color packing/reset boundary. All
57,344 output words from 4,096 original constructor/draw cases match, SHA-256
ded27318b8e86350cfd114ab6c0e8a3dd77ef9a8b607261b0b52375b44303469.
This verifies the constructor and rectangle submission, not full-screen GPU
composition or later mutable theme operations. Short Fuse BlackBird_1 now gets
past theme construction and reaches its still-unbound flame component.

Theme drawing also resolves cached sheet/region identities through the retained
binding service. A later resource group's registry entry can mask a composite
part's name while its original sprite remains live. Resolving the name again
caused thirteen real level frames to reject. All thirteen pass after retaining
the original selection; actual EggDefender_classic_1 is an always-on Metal
regression. Resources still reject stale, foreign or unloaded selections.

Ordinary updateAnimation136934 now advances canonical physical animation state
and invokes the real damage-sprite adapter. Preserve nil-only timer setup,
single-frame advancement despite overshoot, false-vs-nil frame presence,
looping, retained final frames and finished/disabled gates. An isolated original
ARM oracle runs the unchanged updateAnimation, getDamageSpriteNames151134 and
getDamageSprite151699 prototypes. All 20,480 field words from 2,048 calls match,
SHA-256 f018fa43bd0c2a5d435bb7d9b4b035f7ebf4c963e23587ac40f7424231dac575.
The corpus covers raw timer/frame/completion/override/sprite writes with the
original nil-definition damage boundary and suppressed setSprite. Actual
EggDefender_classic_2 additionally exercises its catalog frames and Metal draw;
full native animated-level visuals remain unverified. The latest all 590 audit
has 487 constructors and 487 three-frame starts, with 103 explicit constructor
failures and no frame failures. Ambient/flame owners, registered level hooks,
remaining bird powers, full HUD and complete playthroughs remain open.


The live game now binds the recovered Blues owner, rather than leaving its
specialty slot absent. Direct-circle children receive stable behavior, shot,
camera, physical, simulation, contact, collision, scoring, removal, controller
and character rows. Reservations derive from each authored bird count. The
actual Level8 mouse test creates three children, removes the parent, continues
flight and captures Metal output. The existing original child/split/trajectory
corpora remain separate from this application integration check; complete native
flight and collision parity remains unverified.

Nine standard pack intros now use their selected source artwork and command
lists, including vertical width fitting, scroll positions and restrictions.
Each reaches its source end time and renders on Metal. The three special actor
intros remain open. Level5 now runs its registered treasure update and the real
root double-click transition. An actual mouse/zoom test grants LevelGE_15,
constructs the zero-score record, publishes the gained event, draws its reward
and verifies the encrypted save after reopening.

The application now owns the source power-up slider and counters. The ordinary
six-entry catalog supplies metadata and its infinite flags; --inf and --infinite
apply source makeInfinite behavior to all registrations. Source inventory
consumption and use/events stay separate from activation eligibility. KingSling
uses its recovered 1.5-second activation, PowerupEffect notification, actual
AngerActivationCameraSequence, class power increment, images, audio, base
particles and named flight spawner. Private source evidence covers
PowerupManager3432/4438/4806/6798/8715/8815, PowerupEffect498..2217,
Powerup_KingSling331..1764 and SlingshotSystem14427/23659/23933.
The normal/infinite mouse test starts from controlled balances of three/zero,
checks consumption of one/zero, source infinity text, notification expiry,
resize without asset reload, the powered launch and its actual particle trail.
Other power-up effects, Eagle/Shockwave/RMF controls and full HUD parity remain
open; these changes do not establish native full-session equivalence.


Super Seeds now binds Powerup_SuperSeed230/679/736/1070/2228 and the
MuseumMode updateBird growth continuation. Activation uses the actual source
notification/camera and delays. The bird receives its definition-specific radius,
density and velocity modifier; each growth visit destroys the old native body,
recreates its circle and renderer, copies missing physical fields and builds a
fresh GameObject. Native bird-vector membership, force/activity caches and launch
mass refresh with the replacement. Logical object indices remain stable; this
representation does not establish fidelity for arbitrary retained physical-table
aliases outside the supported world/birds views.

The isolated original ARMv7 superSeedGetRadius736 function matches 2,048 cases
and 6,144 output words, including nil timers, growth oscillation, source bird
radii and both neighboring float values at the half-second boundary. Corpus
SHA-256: 5a04f2c4e15f950aaeb35d1cc84575377cf1df74aa004d99d873b7e9d4248ebf.
The original selected function is a private research input; no original bytecode
or runtime is shipped. This verifies radius arithmetic and timer clearing, not
native whole-session physics or image parity.

The shared named power-up spawner owner now includes Super Seeds bubbles and
flight trails. It preserves the recovered random bubble interval, position,
physics/removal gates and immediate trail behavior. A powered launch deep-copies
its own particle descriptor graph, changes collision/removal smoke to
smokeBuffBirdsPurple and retains the shared catalog unchanged. The source copy's
empty-table growth order is kept for later emission traversal. Complete native
manager/RNG ordering remains unverified. Actual HUD mouse tests cover finite and
zero-balance infinite Red activation, and Blues, Chuck, Bomb and Hal activation,
growth, launch and subsequent flight/collisions on Metal. Tests check the native
bird-vector size, rebuilt shape/mass, refreshed launch mass, per-bird eligibility,
particle isolation and live trail. Scope/Quake/Shockwave/RMF effects, separate
Eagle controls and full HUD/session equivalence remain open.

The Super Seeds integration passes all 759 primary checks, both complete Metal
UI routes and both radius checks under ASan/UBSan, the Windows MinGW build and
the format/diff checks. The normal Metal build keeps AB_SANITIZE disabled;
sanitizers remain in the separate reference build.

The latest full catalog audit reaches 488 constructors and 488 three-frame starts
out of 590 requests, with 102 explicit construction failures and no frame
failures. This includes the recovered Level5 hook. It is a loading audit, not
488 completed playthroughs.

Birdquake now binds the actual source activation195, reset817 and update993,
cancelLevelEnding88106, setPigSpecialExpression136690 and the named
BirdquakeCameraSequence. Explicit tweenEaseCubicInOut selects the already recovered
default curve. Preserve the one-second startup, two-second scaled impulse phase,
strict 0.05-second particle loop, one-element random selection, current world
iteration, airborne-bird exclusion and definition-group dust. The source impulse
uses current physical mass and position; live bodies receive it directly.

The application shares the actual prevention/darkness state with completion,
controller and drawing. The existing GameUpdate darkness subtraction/clamp now
publishes to that canonical global at source service boundaries; completion
clears it. Pig extraSprites are available as immutable catalog-backed views in
the supported read-only path. Special expression assignment and later expiry
update both character state and the raw physical fields. Mutations of those
extraSprites tables require separate owned-copy recovery.

The finite/infinite mouse integration test reaches the actual notification,
camera, scared expressions, decoded earthquake sound, real body impulses and
menu dust, renders Metal output, then checks release of completion/activation
and darkness gates. Native full-Birdquake numeric/RNG/frame parity remains open.


### Sling Scope and repeated Blues lifecycle

The same pinned APK/native library now supplies Sling Scope activation323,
reset802, update867 and draw1423, with SlingshotSystem helpers
24653/24786/25285/26068. Its angle/phase table survives level resets; activation
moves the effect to the actual passive list. The real accessory, basic/King
scope-front composites, stretched-shot gates, definition force/acceleration,
current mass, direct laser audio and launch/reset rearm policy are bound.
King selection uses actual active/passive scope membership, as setSlingshot4762
requires. No replacement trajectory art or ballistic approximation is used.

An isolated Android oracle calls unchanged native GameLua.drawSlingScope with
controlled world/object/resource/render boundaries. Across 256 cases, all 3,942
dots match the C plan exactly: speed limiting, gravity/force, interpolation,
phase spacing, image rectangle, translation, scale, alpha and final native state.
The retained corpus SHA-256 is
`3671d0184625f7f5f66a43b85f75f75dc67a6ebd15eaec9c6f6581fa88019349`.
`AB_REFERENCE_SLING_SCOPE` validates that hash before enabling the native test.
This validates native numeric/draw submissions, not a full original GPU frame
or the angle/update lifecycle. ARM scalar VMOV register copies were checked
against LLVM instruction decoding after the Binary Ninja rendering disagreed.
The original library was not patched.

The actual mouse/Metal integration activates finite and zero-balance infinite
Scope, checks inventory and passive membership, aims with the sight/sound,
combines Super Seeds then King Sling without discarding the scope, captures the
result and launches the bird. The front decorations use the existing native-
validated composite renderer; they are composite resources, not atlas sprites.

Repeated Blues exposed three initial-queue assumptions and a capacity mismatch.
The default otherBirds generation budget now reserves one initial table and
both launch/split replacements per queued bird, preserving old table aliases.
Physics-phase flushing resolves a live name/body pair, since destroyed body
slots can be reused by new children. The source loss query visits canonical
dynamic birds as well as initial birds. Application trajectory renderer capacity
now matches its already allocated point budget. No shared renderer slots or
old table identities are reused while retained aliases may remain.

The live Level8 regression launches and splits all five original Blues through
mouse input, pans back to the sling between launches, verifies all 15 children
and original retained numeric table entries, then runs 240 further frames and
captures Metal output. This covers repeated construction, retirement, body-slot
reuse and the former generation exhaustion. Original full-flight/collision
parity and unbounded retained trajectory storage remain separate open work.


### Concurrent power-up notifications

Each reached activation now owns a separate stable PowerupEffect widget in the
actual NotificationsFrame. The notification pool and child storage cover the
registered six-effect catalog. One active notification no longer rejects a
second activation. Each widget retains its own source entry audio, timer,
scale/shake/particle state and 1.25-second removal. Detached storage is reused
only after that widget has left the tree; cleanup removes every live child.
The native drawMenuParticlesInAdvance guard remains shared for the frame.

The mouse regression activates all four visible powers consecutively in both
finite and zero-balance infinite modes, verifies four sibling widgets and
separate ages, captures Metal output, observes staggered removal without
removing siblings, checks all four effects finish independently, then activates
a second Birdquake through a reused detached slot. Both complete primary Metal
UI routes pass. The five-Blues/Scope change also passed all 761 primary checks,
six selected ASan/UBSan checks and a complete Windows cross-build; the newer
notification change has its own sanitizer and Windows validation logs.

### Matilda egg lifecycle and identity-key tables

The white-bird GRENADE branch of triggerSpecialty125163 now creates the actual
EggGranade projectile, retains the parent's physical properties and source
damage fields, selects DROPPABLE_EGG, registers the new native body/renderer and
applies the recovered rebound impulse to Matilda. The projectile has its own
flyingGrenades record, separate from the physical object and bird queue. Its
source timer value of five is retained; no invented countdown fuse is applied.
The GameUpdate110400 grenade phase observes the collision's explode flag,
emits the original shell particle groups, calls the shared explosion service
with the source audio, removes the body/world entry and compacts the list.

The real Level26 mouse test launches Matilda, triggers her ability, checks the
egg and parent rebound, waits for an actual collision, checks the explosion,
shell particles and complete projectile retirement, and continues for another
180 frames. Metal captures were inspected. All 761 primary checks and both
complete Metal UI routes under ASan/UBSan pass; the Windows cross-build passes.
This establishes the exercised functional path. A whole native Matilda flight
and frame comparison remains open.

The new bounded identity-key map preserves the ARMv7 native table's pointer-key
hash, collision relocation, growth, traversal, retained nil entries and resume
behavior. An isolated original-library oracle uses 64 real table identities
across 16 instances and 256 commands per instance, comparing every slot, link,
lookup and traversal result. Both edge and native comparison tests pass.
The retained corpus SHA-256 is
`a94ee2eeb12fdde90d5dcbd0269be3b4ce68472023160ffae07c9cfbbc98ad2d`;
`AB_REFERENCE_IDENTITY_MAP` checks this hash. The map compares complete host
identities while preserving the native 32-bit hash input. This does not establish
native allocation addresses, garbage collection, mixed-key tables or full-frame
random ordering, and ambient component integration remains open.

### Theme scenery attachment seams

The reported plant/leaf/bush cuts reproduce with the unchanged theme4 DAT/PVR
resources at 2560x1440. Three pairs of authored integer attachment edges acquire
a one-source-pixel gap when negative pivots pass through the legacy add-half,
truncate-toward-zero getter. SheetLoaderDAT passes the signed integer pivots
unchanged into Sprite construction; the getter arithmetic is confirmed by LLVM
at ELF 0x5a4988/0x5a49a0 and Binary Ninja analysis of the pinned library. Its UV
construction uses region edges, without a half-texel inset.

Desktop ThemeDraw now requests integer pivot alignment before the shared sprite
transform. This intentionally corrects that legacy negative-pivot artifact for
scenery assembly. It does not alter atlas pixels, UVs, sampling, screen-coordinate
rounding or the native getter reference path. The same placement correction
feeds Metal and GLES2 commands. The APK regression checks all three attachment
pairs at 1024x768, 1920x1080, 2560x1440 and 768x1024 and renders each with Metal.
The enlarged Metal capture shows continuous stems, leaves and bushes. This is
an explicit desktop presentation correction, not a claim that native theme4
frames have been matched pixel for pixel.

### Terrain texture masks

The flat gray platforms reported in Poached Eggs 1-7 were an omitted
MaskRenderer route. The catalog selects that branch when an object definition
has a truthy texture field. ObjectCreation's resolved helper selects the image
from the current theme's texture field, rather than using the historical image
name in each block definition. The mask sprite supplies shape and alpha; its
RGB is editor artwork and does not contribute to the final terrain color.

The pinned ARMv7 MaskRenderer.draw, MaskedImage.add and MaskedImage.render are
at analysis addresses 0x2e17cc, 0x34e570 and 0x34ef88. These analysis addresses
include the established 0x10000 load bias. The independent C planner matches
2,048 calls to unchanged MaskedImage.add, with 1,382 visible quads: rotated
positions, source culling, both independent UV sets, depth and homogeneous
coordinates. Terrain UVs stay anchored to world coordinates; mask UVs use the
sprite's atlas region. The retained corpus SHA-256 is
`9ea8a2ecbcd1e652d575f8e47e0f83f90001b55a085cbd21a036cbf7024204bc`;
AB_REFERENCE_MASK_DRAW validates this exact input.

The authored material samples terrain color and multiplies only its alpha by
the mask alpha, then uses source-alpha blending. It preserves the existing
ordered scene membership and ordinary sprite material. Common Metal's real
640x480 framebuffer check validates every pixel of a half-alpha colored mask,
straight blending and a subsequent ordinary draw. The Level4 catalog request
(Poached Eggs 1-7) produces four visible terrain-mask commands with the actual
theme texture; its Metal capture shows textured earth in the platform shapes.
The shared terrain resource is retained and uses power-of-two repeat sampling;
unchanged policies reuse the cached upload and decoding is not repeated.

These checks establish the recovered geometry and exercised desktop material.
They do not establish a whole original-game terrain frame comparison or a
complete native MaskRenderer/Camera/Scene scheduling comparison. GLES2 command
recording and shader-link checks cover the corresponding material interface.
The first-party backend also executes the same controlled material on the
isolated Android 5.1.1 EGL/GLES2 driver: all 4,096 framebuffer pixels match the
expected RGBA bytes, including scissor and return to ordinary rendering. The
16,529-byte capture SHA-256 is
`c933d7adb10205be9c03170f4e2e1b639ebf90c8992be313525297f11499b1f2`.
This compares our backend with explicit material equations, not with an
original-game rendered frame. All 765 primary checks and the Windows build
pass after the terrain and scenery fixes.

### Live ambient block components

AmbientAnimations and AmbientParticles now bind their recovered default class
methods to actual level objects. Separate identity-keyed maps retain independent
copied descriptors, timers and registration order. Each of the eight animation
and five particle catalog components has one descriptor; its source pairs order
is therefore unambiguous. Repeated applications append separate mutable copies.
Disposal replaces the logical map. Every update checks the current physical name
in objects.world, retains native nil-key erasure and uses the shared script RNG.
The native pointer-table implementation is compared separately; original
allocation addresses and whole-frame RNG ordering remain unverified.

Animations call the actual physical/visual setSprite boundary. Particle effects
use the current named body's angle, canonical script position, recovered x20
coordinate transform and the existing engine emission boundary with three
trailing zeroes and two false flags. Their exact resource names are collected
and preloaded during level preparation. No frame-time decoding or allocation is
introduced. The controller calls these classes in their original manager order
and honors the actual physics-enabled pause gate.

The real BlackBird_2 fixture plays an ambient frame, restores its original
sprite, checks pause, physical-name erasure, repeated apply and disposal.
BlackBird_7 emits its source dropBuff particles through the shared engine and
checks loaded sprite handles, a Metal capture and paused effect timers. Both
captures were inspected with the camera centered on the relevant actual object;
this controlled camera setup is not a native camera replay. The animation
checkpoint passes all 765 primary checks, both complete ASan/UBSan Metal routes
and Windows compilation. At that checkpoint, the catalog inventory improves
from 488 to 520 starts out of 590 requests, each followed by three ordinary
frames. The newer particle integration has passed the complete primary Metal
UI route, all 765 primary checks, both full ASan/UBSan Metal routes and the
Windows build. At the particle checkpoint, the inventory reaches 531 starts
out of 590 requests; the remaining loading failures are flame application and
registered level hooks/scripts. None of these loading checks establishes
all-level playthroughs or full original-game frame fidelity.

Flame draw recovery also confirms that native Camera.drawGame (analysis
0x2e43a4) calls drawZOrdered with each scene layer before drawing its members.
drawGame141561 flushes remaining z draws at the later 1e10 boundary. The original
numeric comparator141482 and native table.sort at ELF0x821390 / helper0x820e50
produce 2,048 exact independently matched permutations, including equal z
values, signed zero and arrays of zero through 128 entries. The first-party
implementation uses explicit bounded stack ranges and finite binary32 keys.
Its retained corpus SHA-256 is
`6ebc71055fa6405e4590e9ddd476e019064cb96ab9ed1b26a6a24d1ff7c7075d`.
Original comparator instructions/constants are unchanged.

### Live extra draws and flames

The owned extra-draw registry now captures numeric-z records in native string
map order, sorts their references, dispatches them before each scene layer and
flushes at the source 1e10 boundary. Non-z records retain their separate before-
blocks and late passes. The actual hasExtraZOrderedDraws value is published
during preparation, before subsequent callbacks can observe it. Callbacks can
remove their own map entries without invalidating the retained z references.
Ordinary dispatch performs no allocation.

The original drawGame141561, drawZOrdered141286 and comparator141482 run through
256 controlled registry cases over two consecutive frames. All event orders,
per-callback hasZ values and final flags match. Native table.sort runs unchanged;
the camera's test layer enumeration is a controlled C boundary, with native
layer placement checked separately in Camera.drawGame. The corpus SHA-256 is
`bf640813916e25070395dc90a4a7eba35cd8016af7873d8e2ec7b4775aa9d85b`.
Original instructions and constants are unchanged. The extracted z dispatcher
adds an empty debug upvalue name so the oracle can bind the original closure;
that metadata change is not a replacement implementation.

FlameEffect applies its actual physical flameEffect table, shared object-update
function, initial zero-time update and extra-draw registration at the raw block
definition's z_order + 1. Updates use the existing native-matched flame helpers,
shared script RNG, current native body angle/velocity and canonical sprite.
Rendering uses the recovered drawGameSprite12979 division by physics scale,
integer-pivot conversion and current X scale. Both catalog flame components
preload their authored frames; no per-frame resource lookup or allocation is
introduced. Missing world objects remove the draw registration and prune the
registered tail callback while retaining the object's update property.

The real BlackBird_1 fixture checks visible flame animation, base/flame draw
order, a manually inspected Metal capture, paused timers and callback pruning.
The flame checkpoint passes all 769 primary checks, seven selected ASan/UBSan
checks including both complete Metal UI routes, the Windows build and format
check. Its catalog inventory reaches 548 of 590 starts, each followed by three
ordinary frames, with no frame errors. The 42 remaining loading gates are level
scripts and birthday-hat construction hooks. These figures do not establish
complete level playthroughs or native whole-frame fidelity. Dynamic flame
construction, arbitrary script-table replacement and native allocation/RNG
interleaving beyond the independently tested boundaries remain open.

### Common hidden-egg level hooks

The common level-script factory3681 and helpers3189/3395/3533 now have an owned
C implementation for their 18 registrations. The module captures the full/free
version gate. Preparation queries the actual egg unlock state only when reached,
then removes the named loaded placement and connected joints before native body
construction. Decorative keep-object registrations omit preparation entirely.
Destruction checks the current physical name and eagle flag before calling the
existing achievement, encrypted-save, reward and event services.

Post-construction nil writes retain every original key and the constructor hash
capacity. The C ordering helper rebuilds that layout and emits surviving values;
compacting the input would change collision chains, iteration order and later
physical identities. World and joint constructors now consume this live order.
This boundary supports deletion of existing keys, with no subsequent insertion,
rehash or arbitrary metatable behavior.

An isolated ARM oracle executes the entire unchanged level_scripts.lua module
from the pinned APK (SHA-256
`28ed02c661dc9fd522ce5c3d9501c215170b609c13d520ff5952f86c27394498`).
External module loaders and the unused birthday level list are controlled
boundaries. Across 1,152 cases, all 18 actual registrations match unlock-query
counts, achievement calls and surviving world/joint iteration order, including
full/free flags, kept decorations, matching/unrelated physical names and four
eagle truthiness values. The retained corpus SHA-256 is
`978d42407af14ea84370b5a47773291fd852bfc004ebb7321b9f8827df921f90`.
This verifies the selected script paths, not complete original-game sessions.

The actual LevelP2_64 integration fixture queues a real after-physics removal,
checks eagle suppression and ordinary reward/event/save effects, captures the
Metal notification and reopens encrypted progress. The collected egg is then
absent before body construction; Level34 retains its decorative beach ball
after unlock. At this checkpoint, all 771 primary checks, seven selected
ASan/UBSan checks including both full Metal UI routes, the Windows build and
format check pass. The catalog inventory improves to 564 of 590 starts with
three ordinary frames each and no frame errors. The remaining 26 gates include
15 birthday-hat construction hooks and 11 special level scripts.

### Clock golden-egg level

LevelGE_20 now executes its recovered onLoadLevel9849 placement adjustment
before physics construction. The platform's existing getCurrentTime equivalent
provides local calendar fields. The C hook retains the original binary32 steps
around double pow/sqrt and script sine/cosine, copies the first wooden piece's
position to the second, and uses the minute angle for the light piece as the
source does. The separately calculated seconds angle is unused in that source.
Missing or false clock objects skip the placement work.

The unchanged 837-byte selected prototype has SHA-256
`e8b5da8bf23ee8941ea4d5e360801483cd4419dfc7ff385ea828ed0e3dafedf8`.
An isolated ARM oracle controls the current-time boundary and original loaded
placements. All 4,096 cases match each position bit, covering all 3,600 normal
minute/second pairs with varied coordinates plus 496 missing/false object cases.
The corpus SHA-256 is
`9ff3f589c6f9c3618484abbc27ab2c5772aad749dba1ee88af462a691430cf63`.
Local-time acquisition is tested separately by the calendar boundary; the
controlled trace does not establish cross-platform timezone equivalence.

At this checkpoint, all 773 primary checks, the full Metal UI sanitizer route
and both clock checks under ASan/UBSan, Windows compilation and formatting pass.
The live catalog reaches 565 of 590 starts with three ordinary frames each and
no frame errors. Fifteen birthday construction hooks and ten special level
scripts remain loading gates; complete playthrough fidelity remains open.

### Direct runtime box construction

The birthday-hat catalog contains five box definitions, with explicit dimensions
and the source density override 0.001. A new first-party raw box constructor
composes the recovered fixture/body path and native physical-table projection,
before renderer, script, definition and enclosing world registration. Native
GameLua.createBoxLua is ELF0x320d9c (analysis0x330d9c); its collision flag is stored
at object offset0xa2, also checked by the existing native contact-filter oracle.
The controllable and sprite arguments are unused by this native box constructor;
it does not append a controllable-vector entry. The later script assignments
remain separate. Positive dimensions are the current supported domain.

An isolated native constructor oracle compares 256 cases, including static and
dynamic density, signed-zero position/material inputs, both collision flags,
the ignored controllable/sprite gates and actual hat dimensions/density. All 19
raw property queries and 43 numeric/flag words match, including mass/inertia,
pose samples, box skin/centroid, four vertices/normals and vector count. The
retained corpus SHA-256 is
`4852bee479a33054de9c08222e97080687a4243ab3f8361284af6a2dcc326c7f`.
This closes the tested raw creation boundary; it does not yet enable birthday
levels or establish their attachment/release lifecycle.

### Illustrated intro framing and desktop pan

Constructing an intro directly at 1920x1080 exposed a separate scaling reference:
the menu's highest-artwork 1024x768 reference made the 480x320 story canvas fit
across the window and set no_scroll. The application now owns a distinct story
scaling profile while retaining the menu profile for the Skip control. Both
profiles share resolution events and release their listeners before the event
owner closes. Highest-resolution texture selection remains independent of the
story's logical coordinate reference.

The final pack1 frame also exposed background beyond the illustration's right
edge. STORY_1_BEGINNING has a nonzero composite pivot; its width includes a
negative left extent, while the scroll target uses the full width. Desktop
presentation now caps the horizontal camera at width minus pivot, with one
logical pixel of coverage allowance for fractional/filtered component edges.
This cap leaves the initial placement, command times and interpolated scroll
state intact. It applies to horizontally scrolling height-fit illustrations;
centered no-scroll framing retains its intended borders.

This coverage allowance is a requested presentation correction, not a recovered
native script operation. CutScene.draw10721 and LuaResources.drawSprite at
analysis VA0x5b6e74 retain the default pivot anchors. In processCommand4437 the
queried pivot is unused by the non-film scroll calculation; no native pivot
subtraction is claimed. Full original-game cutscene pixel parity remains open.

The Metal integration fixture opens a fresh highest-artwork menu at 1920x1080,
plays all nine actual intro command lists and repeats at 2560x1440, 3840x2160
and 1919x1079. Actual readbacks at the opening, middle and final frames must
change when a scrolling command is reached. Pack1 additionally checks both top
edges at all three samples and the full right column at the final frame. The
original uncovered sky columns and 58-row gap beside the grass are absent in
the inspected final capture. At this checkpoint all 775 primary checks, the
Windows build and formatting pass. Private captures and logs are retained under
the comic-pan and comic-edge jobs. Both complete Metal UI routes also pass under
ASan/UBSan, including the final full-height edge check.

### Canonical registration of runtime boxes

Raw runtime boxes now publish through the same stable level-slot boundary as
runtime circles. Each new object retains its fixture identity, physical-table
storage, frame samples and world-name insertion order. Existing names, removed
names, locked worlds and exhausted storage reject as before. The raw box path
does not append a controllable-vector entry or invent definition/render fields.

The level-owner fixture creates a real hat-sized box, queries it through the
named world raycast, appends a circle and checks that existing physical-table
aliases remain valid. Invalid geometry, duplicate names, locked creation and
capacity exhaustion are covered. This verifies registration and mixed-shape
ownership; birthday hat construction, joints and release remain separate work.
All 775 primary checks pass after this registration change, as do the level
owner and both box checks under ASan/UBSan, both full Metal UI sanitizer routes,
Windows compilation and formatting.

### Hat creation and release script lifecycle

The first-party hat component implements createHat97169 and releaseHat97988.
Creation initializes an absent/false shared counter before testing the actual
bird's catalog offset. It retains generated object/joint names in stable caller
storage, requests the density-0.001 object, makes it static, assigns defence
1000000, creates the recovered zero-anchor weld and publishes the hat record.
The counter and catalog fields are reread after callbacks at their original
boundaries. The collision/input view remains the record's first member.

Release prints before its registry lookup and preserves source truthiness for
jointDestroyed. It queues joint destruction, sets damping0.4, switches the body
to dynamic and reads current physical velocity/position/height for the impulse.
Only successful impulse application marks the retained record released. A
second release still prints but does not repeat the physics effects. Missing
services and malformed reached data reject rather than silently skipping work.

The selected unchanged original prototypes contain 831 and 527 bytes, with SHA-256
`cc044aed8a7f05d4771fe7ae2ba92de2db1c9dbf37cb023e8ea77dfa3b2ec6ab`
and `5b964577e5b10a6c471140aee4300890c6308f297d96b12463ef4d32748750dc`.
An isolated ARM oracle executes those functions with controlled external object,
joint and physics services. All 1,024 cases match 128 output words each, including
both complete generated names, final counter/record fields, ordered host calls
and every impulse component. Cases cover missing/false offsets, scalar truthiness,
large/fractional/signed-zero counters, repeated releases, a registry changed by
the print callback and state/catalog changes during creation or body transition.
The retained corpus SHA-256 is
`2d8bc9986f2a2104df1513fe38510bab571619b1346defa59680c74baa4b1638`.
Both component checks pass under ASan/UBSan, and Windows compilation passes.
The complete primary suite now passes all 777 checks, and formatting passes.

This closes the tested script orchestration, not the enclosing birthday level
integration. The actual constructor, static/dynamic transitions, weld registry,
deferred queue, shared hat map and episode selection still require their real
services. Sling-held, transfer, ready and character-jump hat motion also remain
explicit integration boundaries. MuseumMode.loadLevel2711 invokes
onAfterLevelObjectsCreated at words
1608..1633 after its ordinary object/timer pass; the birthday hook must be placed
there, before later game setup. The 15 birthday loading gates remain explicit.


### Defined boxes through the live application adapters

The definition-aware box factory resolves the APK catalog and density override,
then uses the recovered raw box constructor and canonical property owner. Its
supported domain is explicit ordinary box definitions without custom creation,
update, animation, component or extra-sprite factories. It publishes the actual
material, collider, gravity, sensor, strength/defence, damage sprites and dynamic
name-map membership. It does not manufacture a native launch-queue bird.

The APK fixture constructs all five birthday hat definitions at density 0.001.
A separate live-owner fixture binds their actual artwork, force/post-physics
metadata, block-script application, scene, camera, shot lookup, behaviors and
physical-removal rows between world setup and GameUpdate construction. An
ordinary real mouse launch, physics, damage and winning result still complete.
These checks establish the adapter composition; the birthday hook, joint/hat
motion and release integration remain separate gates.

### Danger Above leaf boundary colors

The reported dotted leaf boundaries are present in the pinned APK's
`assets/data/images/themes_android/THEME_06_PARALLAX_6_2.pvr`, including the
canopy in THEME_6_ELEMENT_1. This is a 984x992 RGBA4444 image. Its SHA-256 is
`ae4e43360c99954f0f62266386af507cb7587b4f6def8787e55bd595c013a4d0`;
the decoded RGBA8 SHA-256 is
`376a31ede8f0c8c26c36c8d830036180a50813a01416433eac5826cfd5396bad`,
matching the earlier independent texture conversion inventory. The intermediate
canopy colors occur in fully opaque original texels; they are not introduced by
alpha blending over another layer. The four-bit channels retain their original
17-step values. The renderer retains the original premultiplied blend and linear
sampling. No recoloring or smoothing is applied. This identifies the artifact
in the original asset; it is not a claim of whole-screen native pixel parity.
Private evidence: `analysis/input/danger-above-leaf-evidence-01.json` and the
`leaf-theme6-before-01` atlas/render captures.

### Mighty Hoax level 8 impact joint destruction

A fresh highest-resource 1920x1080 application replay of LevelP2_80 reproduced
failure after repeated Matilda launches and egg drops near the structure. The
continuous-world wrapper hid a stored game-contact error: the Museum collision
route had no deferred joint-destruction owner. The reached name was
WoodBlock4_30WoodBlock4_31. The failure was a missing application binding rather
than evidence that numerical solver constants should be changed.

The joint-update owner now supplies that queue, the actual named joint registry,
visible metadata and motor-limit checks to the existing GameUpdate joint phase.
Initial metadata insertion follows the level's native constructor traversal and
the measured growing string map. Destruction requests retain append order and
duplicates. common_logic.lua prototype 24125 prints, notifies
EID_JOINT_DESTROYED with the current joint metadata, then calls the saved native
named removal. Repeated requests still notify with an absent joint. The C binding
preserves this order, uses the existing recovered native removal operation and
retains notification and metadata storage until level close. Event callbacks may
append further queued requests; a failed callback preserves preceding effects
and stops the session. Future joint factories must register their new metadata
at creation. No physics solve failure is converted into success.

The application also reports a stored collision-callback error before the outer
physics diagnostic, and the continuous-world wrapper preserves an existing
error. The exact failing replay now completes all four Matilda launches and reaches
its natural ending. Two ability requests produce eggs before the parent has
already collided; the other clicks do not create additional projectiles. Controlled tests additionally check event-before-removal,
repeated names, callback-appended requests, motor-limit dispatch, retained event
payloads and listener-failure behavior. The APK regression drives all four white
birds in a fresh host and captures the resulting Metal frame. These are
first-party integration checks, not original complete native gameplay traces.
Private replay logs are `analysis/input/level8-physics-cause-01.log` and
`level8-physics-fixed-01.log`; validation uses the `joint-updates-*` logs.

Validation for this checkpoint covers all 779 primary gates: the full run
passed 778, then the new motor fixture's missing enabled-limit flag was fixed
and its rerun passed. Both complete Metal UI routes passed. All eight selected
ASan/UBSan checks passed, including both complete Metal UI routes, the defined
APK boxes, native box/hat corpora and joint updates. The fresh application-owner
fixture with five defined boxes also passed separately under ASan/UBSan.
Windows app, box, hat and joint-update test targets compile; source formatting
and diff whitespace checks pass. Native complete-playthrough parity and the
remaining catalog construction/script gates stay open.
