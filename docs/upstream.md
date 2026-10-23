# Upstream triage

The original repository (`richgel999/lzham_codec`) has had no commits since January 2022, no published
releases, 26 open issues and 12 open pull requests. This fork keeps the public API and the bitstream
format identical, but it does not merge upstream pull requests: several of them are years old, overlap
each other, and some contain changes that look correct but never execute.

This file records what was taken from upstream, what was rejected, and why, so that the work does not
have to be rediscovered later. Upstream PR/issue numbers are from `richgel999/lzham_codec`.

## Adopted (adapted, not merged verbatim)

| Upstream | What we took | Notes |
|---|---|---|
| PR #30 | `LZHAM_UINT*_MAX` constants instead of `UINT*_MAX` macros | The codec's own constants are in `lzham_types.h`; using them removes any dependency on `<stdint.h>` and fixes the GCC/Clang build (`UINT16_MAX`/`UINT64_MAX` were not declared). |
| PR #34 | Diagnosis: the build break comes from `UINT16_MAX`/`UINT64_MAX` | The patch adds `<stdint.h>` in two translation units; we replaced the macro uses instead, which covers every translation unit. |
| PR #35 (parts) | `__builtin_debugtrap()`/`__builtin_trap()` in `lzham_debug_break()`; `-fexpensive-optimizations` removal (implicit in the new build); `snprintf` in `lzhamtest` | The macOS `os_unfair_lock` hunk was not taken, see the rejected table. |
| PR #31 / PR #35 (part 3) | ARM/AArch64 `yield` for the spin back-off | Reimplemented with `LZHAM_CPU_*` macros. The upstream patch only touched a branch that is never compiled on ARM (see below). |
| PR #8 | `__is_pod` fix for Apple, `int3` inline asm is MSVC-only | libc++ removed `std::__is_pod`, so every modern macOS build was broken. We use the `__is_pod` builtin on every compiler. |
| PR #17 | Idea of a threading kill-switch | Implemented as the `LZHAM_ENABLE_MULTITHREADING` CMake option (maps to `LZHAM_ANSI_CPLUSPLUS`), not as a new macro. |
| Issue #26 / #6 | macOS build break | Fixed by the `__is_pod` change plus the `lzham_debug_break()` rewrite (`__asm {int 3}` does not parse in Clang at all). |
| Issue #3 | `decompress_memory`/`compress_memory` mismatch | Covered by `tests/test_roundtrip.c`, which exercises the memory API at several sizes, modes and levels. |

## Rejected (with reasons)

| Upstream | Reason |
|---|---|
| PR #35, part "arm: use yield" | Dead code as written. It adds the `yield` inside `#if defined(__GNUC__) && LZHAM_PLATFORM_PC`, but on AArch64 `lzham_core.h` fell into the ANSI fallback, which does not define `LZHAM_PLATFORM_PC`, so the block never compiled. It also did not enable threading or atomics on ARM, which is what actually matters. |
| PR #35, `__is_pod` hunk | Both sides of the new `#if` expand to `__is_pod(T)`. We kept the single definition. |
| PR #35, `os_unfair_lock` | The `spinlock` class it replaces is never instantiated anywhere in the codec (dead code), so the change fixes nothing observable. Kept out to avoid new deployment-target requirements. |
| PR #27 / #10 (Android/AIX) | Mixes four unrelated ports, disables pthreads on Android (`LZHAM_USE_PTHREADS_API 0`) and changes the allocator paths in ways that are not testable here. The useful part (`malloc_usable_size`/`malloc_size` selection) already exists in `lzham_mem.cpp`. |
| PR #28 | Contains the same `LZHAM_UINT*_MAX` fix (good) plus `(void*)` casts to silence warnings and a CMake change that compiles every source twice to produce static and shared variants. Superseded by the new build. |
| PR #33 | Duplicate of #34. |
| PR #16 / #20 / #24 | Symptomatic proposals (`#undef INT8_MAX`, etc.) for the macro collision fixed properly by PR #30's approach. |
| PR #29 | Style fix for the old CMake files, which no longer exist. |
| Issues #11, #12, #15, #21, #23 | Out of scope: AES encryption, zlib-API purpose, directory compression, header hygiene, seekability. |
| Issue #13 | Real bug (`deflateInit2` with negative `window_bits` still writes a zlib header) but in the zlib compatibility layer, which this fork keeps frozen until it has tests. Tracked, not fixed yet. |
| Issue #14 | Real documentation/defaults problem (dictionary size). Tracked; the defaults are unchanged for ABI compatibility. |
| Issue #18 | Real API limitation (too many undifferentiated `FAILED` codes). Adding new enum values at the end is ABI-safe and is planned. |
| Issue #19 | Real bug: crash with buffers close to 4 GB in `lzham_compress_memory`. Not reproduced yet; needs a >4 GB test, which the 32-bit targets cannot run. |
| Issue #22 | Real bug in `lzham_z_compress2`/`compressBound`. Needs a reproducer before changing the zlib layer. |
| Issue #32 | "Create a new release": that is what this fork does. |

## Platform work that upstream never had

* ARMv7 and AArch64 as first-class targets: pthreads, GCC atomics, real memory barriers, per-architecture
  spin back-off. Upstream only had an ANSI fallback for ARM, which silently disabled multithreaded
  compression.
* Unaligned access made explicit: the matcher compared matches through a `uint64*` cast, which makes GCC
  emit `LDRD` on 32-bit ARM and faults with `SIGBUS`. All unaligned reads now go through
  `lzham::load_unaligned_uint32()`/`load_unaligned_uint64()` (memcpy-based, folded into a single load).
* Windows build that works with MSVC (`/W3`, no GCC flags), Windows 7 as the minimum (`_WIN32_WINNT`
  0x0601 instead of 0x500) and the historical DLL names (`lzham_x86.dll`/`lzham_x64.dll`) plus export
  plumbing.
* A single shared library per platform instead of three interdependent ones.
* CMake package export, pkg-config file, install rules, CTest smoke test, and a CI matrix.
