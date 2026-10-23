# Changelog

All notable changes to this fork are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses
[Semantic Versioning](https://semver.org/). The public C ABI (`LZHAM_DLL_VERSION` = 0x1010) and the
bitstream format are frozen and do not change between 1.x releases.

## [1.0.1] - 2026-10-23

First maintained release. The version continues the upstream lineage instead of restarting at 1.0.0: the
codec declares `LZHAM_DLL_VERSION` = 0x1010 (1.0.1), and upstream's frozen bitstream is "v1.0". Upstream
(`richgel999/lzham_codec`) had no commits since January 2022 and never published a release.

### Added

- Linux ARMv7 and ARM64 as supported targets: pthreads threading, GCC atomics, real memory barriers and
  per-architecture spin back-off. Previously ARM silently fell back to the ANSI build, which disables
  multithreaded compression.
- macOS x86_64 and arm64 (including universal binaries) support.
- New CMake build (3.15+) that works on Linux, Windows (MSVC and MinGW-w64) and macOS:
  - shared library (`liblzhamdll.so`/`.dylib` with SONAME/install name 1, or `lzham_x86.dll`/`lzham_x64.dll`),
  - static library (`liblzhamlib.a`, or `lzhamlib_x86.lib`/`lzhamlib_x64.lib`),
  - `lzhamtest` and the new `lzham_roundtrip_test`,
  - install rules, a CMake package (`find_package(lzham)`, `lzham::lzham`, `lzham::lzham_static`) and a
    pkg-config file,
  - options `LZHAM_BUILD_SHARED`, `LZHAM_BUILD_STATIC`, `LZHAM_BUILD_TESTS`,
    `LZHAM_ENABLE_MULTITHREADING`, `LZHAM_INSTALL`.
- `tests/test_roundtrip.c`: C API smoke test over 6 buffer sizes, 4 data patterns and 3 compression levels,
  verifying payload and Adler-32. Registered with CTest.
- `tests/consumer`: minimal project that consumes the installed package (used by CI).
- CI matrix: Linux x64/x86 (GCC and Clang), Linux ARM64/ARMv7 (QEMU), Windows x86/x64 (MSVC), macOS
  arm64/universal/x86_64.
- `docs/upstream.md`: triage of the upstream pull requests and issues, with the reason every one was
  adopted, rewritten or rejected.

### Changed

- Windows minimum target is Windows 7 (`_WIN32_WINNT` 0x0601 instead of 0x500). Verified on Windows 7 SP1
  and Windows 10, both x86 and x64, including cross-architecture stream compatibility.
- The three interdependent shared libraries of the old CMake build are now two internal static libraries
  plus a single public shared library per platform.
- `LZHAM_DLL_EXPORT` now maps to `__declspec(dllexport)` on Windows and to
  `__attribute__((visibility("default")))` elsewhere, so the exported API survives `-fvisibility=hidden`.
- The public headers no longer depend on `<stdint.h>`: `UINT16_MAX`/`UINT32_MAX`/`UINT64_MAX` uses were
  replaced by the codec's own `LZHAM_UINT*_MAX` constants (from upstream PR #30).
- Platform detection in `lzham_core.h` was consolidated: `LZHAM_CPU_X86/X64/ARM/ARM64` and
  `LZHAM_CPU_BIG_ENDIAN`, with the legacy macros still defined.
- MinGW-w64 builds link the static libgcc/libstdc++ runtime, so the DLL and test executables are
  self-contained.

### Fixed

- Build failure on modern GCC/Clang (`UINT16_MAX`/`UINT64_MAX` not declared), reported upstream as PRs
  #33/#34 and issues #20/#24.
- macOS build failure: libc++ removed `std::__is_pod` (upstream issues #6/#26) and
  `lzham_debug_break()` used MSVC inline assembly that Clang cannot parse (upstream PR #8).
- `SIGBUS` on 32-bit ARM: the match accelerator compared matches through a `uint64*` cast, which makes GCC
  emit `LDRD` on unaligned addresses. All unaligned reads now go through memcpy-based helpers.
- Undefined memory barriers on weakly ordered CPUs; `LZHAM_MEMORY_*_BARRIER` is now a real barrier on ARM.
- `lzham_yield_processor()` emitted `pause` on ARM; it now dispatches per architecture (`pause`, `yield`,
  or nothing).
- Unbounded `sprintf` calls in `lzhamtest`'s temporary-file names (from upstream PR #35).

### Fixed (upstream issue tracker)

- **#19** (`lzham_compress_memory` crashed with buffers close to 4 GB): the codec's internal buffers use
  32-bit element counters, so appending past 4 GB wrapped the size, shrank the allocation and then wrote
  past its end. Growth is now checked and refused, and the API returns the new
  `LZHAM_COMP_STATUS_FAILED_OUTPUT_LIMIT` (instead of corrupting memory or calling `exit()` through
  `LZHAM_FAIL`). `LZHAM_COMP_STATUS_FAILED_ALLOC` was added for allocation failures, and the 4 GB per-call
  limit is documented in `lzham.h`.
- **#22** (`lzham_z_compress2` failed with `lzham_z_compressBound()` buffers): the bound assumed 0.098%
  expansion, but incompressible data is stored as raw blocks and costs about 0.122% with the 4 KB blocks of
  the zlib API, so compressing more than ~700 KB of incompressible data returned `Z_BUF_ERROR`.
  `lzham_z_deflateBound()` now allows 0.78% plus a fixed slack.
- **#13** (`lzham_z_deflateInit2` wrote a zlib header even for a negative `window_bits`): the "raw stream"
  request was lost when the Deflate method alias overwrote `window_bits`. The flag is now captured before
  the alias is applied, so raw streams are 2 bytes shorter and inflate correctly with `inflateInit2(-w)`.
- **#14** (`m_dict_size_log2` = 0 was rejected): 0 now selects `LZHAM_DEFAULT_DICT_SIZE_LOG2` (26, 64 MB)
  on both the compression and decompression side. That default is the largest one every supported build can
  decode, so default streams stay portable between x86/ARMv7 and x64/ARM64. The header documents the
  x86/ARMv7 limit for explicit values.
- **#18** (undifferentiated failure codes): the two new status codes above are returned where the cause is
  known, and every documented failure status is listed in `lzham.h`.
