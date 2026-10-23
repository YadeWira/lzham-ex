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
| Issue #19 | Crash (or `exit()`) with incompressible buffers close to 4 GB | Internal vectors use 32-bit element counters; a >4 GB append wrapped the size and wrote past the allocation. Growth is now checked, and the API returns `LZHAM_COMP_STATUS_FAILED_OUTPUT_LIMIT`. Verified with a 4.3 GB incompressible buffer. |
| Issue #22 | `lzham_z_compress2` returning `Z_BUF_ERROR` with `lzham_z_compressBound()` buffers | The bound was 0.098% but the real worst case with the zlib API's 4 KB blocks is 0.122%. `lzham_z_deflateBound()` now allows 0.78% + 4096 bytes. Reproduced and re-tested with incompressible data from 4 KB to 32 MB. |
| Issue #13 | `deflateInit2` writing a zlib header for a negative `window_bits` | The raw-stream flag was captured after the Deflate alias overwrote `window_bits`. It is now captured first; raw streams are 2 bytes shorter and inflate with `inflateInit2(-w)`. |
| Issue #14 | Dictionary size 0 rejected, and a confusing x86/x64 default | 0 now means `LZHAM_DEFAULT_DICT_SIZE_LOG2` (26, decodeable on every supported target) on both sides, and the header documents the x86/ARMv7 limit for explicit values. |
| Issue #18 | Undifferentiated `FAILED` codes | New ABI-safe statuses `LZHAM_COMP_STATUS_FAILED_ALLOC` and `LZHAM_COMP_STATUS_FAILED_OUTPUT_LIMIT`, returned where the cause is known, and the failure list is documented in `lzham.h`. |

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
