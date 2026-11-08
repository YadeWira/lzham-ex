LZHAM - Lossless Data Compression Codec
=============

| Component | Status |
|---|---|
| CI | [![CI][ciBadge]][ciLink] |
| Latest release | [![Release][relBadge]][relLink] |

Public Domain (see [LICENSE](LICENSE)).

<p>LZHAM is a lossless data compression codec written in C/C++ (C++03), with a compression ratio similar to
LZMA but with 1.5x-8x faster decompression. It supports Linux x86/x64/ARM64, Windows x86/x64 (Windows 7 or
newer, plus 32-bit Windows XP) and macOS arm64.</p>

| OS | Architectures | Compilers |
|---|---|---|
| Linux | x86, x64, ARM64 | GCC, Clang |
| Windows | x86, x64 (Windows 7 or newer) and x86 (Windows XP SP3) | MSVC 2019/2022, MinGW-w64 |
| macOS | arm64 | Apple Clang |

<p>The bitstream format and the public C ABI (<code>lzham_get_version()</code> returns
<code>LZHAM_DLL_VERSION</code> = 0x1010) are frozen: every supported build reads and writes the same streams.
Streams from the pre-1.0 alpha line (0x1008) are not compatible in either direction; see
<a href="https://github.com/YadeWira/lzham-ex/wiki/Migrating-from-upstream">Migrating from upstream</a>.</p>

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

On Windows use `cmake -S . -B build -A x64` and `cmake --build build --config Release` (or `-A Win32` for x86).
Cross-compilation, Windows XP, the install rules and the CMake package are covered in
[Building](https://github.com/YadeWira/lzham-ex/wiki/Building).

## Documentation

The wiki has the rest:

* [About](https://github.com/YadeWira/lzham-ex/wiki/About) - background, benchmarks and memory usage.
* [Building](https://github.com/YadeWira/lzham-ex/wiki/Building) - every platform, options, install, tests.
* [API](https://github.com/YadeWira/lzham-ex/wiki/API) - how to drive the codec and usage tips.
* [Tools](https://github.com/YadeWira/lzham-ex/wiki/Tools) - `lzhamtest`, `lzham_compress`.
* [Platform support](https://github.com/YadeWira/lzham-ex/wiki/Platform-support) - what is tested where.
* [Format reference](https://github.com/YadeWira/lzham-ex/wiki/Format-reference) - the Python reference decoder.
* [Known problems](https://github.com/YadeWira/lzham-ex/wiki/Known-problems),
  [Migrating from upstream](https://github.com/YadeWira/lzham-ex/wiki/Migrating-from-upstream),
  [Upstream triage](https://github.com/YadeWira/lzham-ex/wiki/Upstream-triage).

**Downstream:** [zpaq-std](https://github.com/YadeWira/zpaq-std) vendors these sources for its `-ma:lzh` blocks
(LZHAM 1.0, decoded by its own ZPAQL implementation). See the wiki's
[Downstream](https://github.com/YadeWira/lzham-ex/wiki/Downstream) page.

[ciBadge]: https://github.com/YadeWira/lzham-ex/actions/workflows/ci.yml/badge.svg "Continuous Integration"
[ciLink]: https://github.com/YadeWira/lzham-ex/actions/workflows/ci.yml
[relBadge]: https://img.shields.io/github/v/release/YadeWira/lzham-ex "Latest release"
[relLink]: https://github.com/YadeWira/lzham-ex/releases/latest
