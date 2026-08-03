# Jungle scene file version 0

## Purpose

`JungleRuins.fjscene` is the boundary between the OpenUSD cooker and the
runtime renderer. Version 0 is specific to the fixed Intel Jungle Ruins scene
and serializes the complete project-owned `JungleScene`; it is not a general
asset container or the final GPU layout.

The default paths are:

- source: `assets/scene/JungleRuins_1_0_1b/USD/JungleRuins_Karma.usda`
- cooked output: `assets/cooked/JungleRuins.fjscene`

`FastJungleCooker` imports and validates the USD scene, writes the file, reads
it back, and requires exact equality with the imported scene. `FastJungle.exe`
uses only the runtime-neutral reader and does not link or deploy OpenUSD.

## Header

All integers and IEEE floating-point values use little-endian byte order. The
fixed 40-byte header is:

| Offset | Size | Value |
| ---: | ---: | --- |
| 0 | 8 | `FJSCENE\0` magic |
| 8 | 4 | format version (`0`) |
| 12 | 4 | header size (`40`) |
| 16 | 4 | endian marker (`0x01020304`) |
| 20 | 4 | reserved, must be zero |
| 24 | 8 | payload byte count |
| 32 | 8 | FNV-1a 64-bit payload checksum |

The reader rejects unknown versions, wrong byte order, reserved values,
truncation, trailing payload bytes, invalid enum and Boolean values,
unreasonable allocation counts, and checksum mismatches.

## Payload

The payload follows the member order in `JungleScene.hpp`. Strings are stored
as a 64-bit byte count followed by their UTF-8 bytes. Vectors start with a
64-bit element count. Numeric arrays are stored contiguously; compound objects
write their members explicitly. The fixed-layout `Float2`, `Float3`, `Float4`,
and `Quaternion` value arrays are covered by compile-time size and trivial-copy
checks before being stored contiguously.

The payload includes stage metadata and statistics, source layers, the prim
hierarchy, meshes and all observed primvars, mesh subsets, point instancers and
their separate attribute arrays, native instances and prototypes, shader
graphs and asset references, materials, camera, dome light, and import
diagnostics. Relationships remain indices or paths exactly as represented by
`JungleScene`; instances are not expanded and object kinds are not merged.

OpenUSD's used-layer iteration order and generated `/__Prototype_N` names are
not authored scene facts and change between processes. The importer sorts
source layers, gives the anonymous session layer a stable display identifier,
and assigns native prototypes stable project paths according to the
lexicographically first composed instance that uses each prototype. Authored
instance paths and every instance-to-prototype relationship remain intact.

## Deliberate limits

Version 0 is a correctness boundary before optimization:

- It has no compression, chunk table, random access, memory mapping, texture
  conversion, mesh packing, or culling data.
- It preserves resolved source texture paths, so the copied source package is
  still required for the current textured renderer.
- The verified file is 1,751,144,586 bytes because all source attribute arrays
  are retained. Runtime-oriented instance and texture cooking is later work.
- A change to serialized `JungleScene` data requires a format version change;
  the reader never guesses a newer layout.

These limits keep the first boundary small and auditable while proving the
complete USD-to-Cooker-to-file-to-runtime path without allowing USD APIs into
renderer code.
