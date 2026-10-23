# Changelog

All notable changes to this fork are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses
[Semantic Versioning](https://semver.org/). The public C ABI (`LZHAM_DLL_VERSION` = 0x1010) and the
bitstream format are frozen and do not change between 1.x releases.

## [1.0.0] - 2026-10-22

First maintained release. Upstream (`richgel999/lzham_codec`) had no commits since January 2022 and never
published a release.

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
