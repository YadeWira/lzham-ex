LZHAM - Lossless Data Compression Codec
=============

[![CI](https://github.com/YadeWira/lzham-ex/actions/workflows/ci.yml/badge.svg)](https://github.com/YadeWira/lzham-ex/actions/workflows/ci.yml)

Public Domain (see LICENSE)

<p>LZHAM is a lossless data compression codec written in C/C++ (specifically C++03), with a compression ratio similar to LZMA but with 1.5x-8x faster decompression speed. It officially supports Linux x86/x64/ARMv7/ARM64, Windows x86/x64 (Windows 7 or newer) and
macOS x86_64/arm64. See "Platforms/Compiler Support" below.</p>

An improved version of LZHAM, with better compression, is [here](https://github.com/richgel999/lzham_codec_devel).

<p>The old alpha version of LZHAM (bitstream incompatible with the v1.x release) is here: https://github.com/richgel999/lzham_alpha</p>

<h3>Introduction</h3>

<p>LZHAM is a lossless (LZ based) data compression codec optimized for particularly fast decompression at very high compression ratios with a zlib compatible API. 
It's been developed over a period of 3 years and alpha versions have already shipped in many products. (The alpha is here: https://code.google.com/p/lzham/)
LZHAM's decompressor is slower than zlib's, but generally much faster than LZMA's, with a compression ratio that is typically within a few percent of LZMA's and sometimes better.</p>

<p>LZHAM's compressor is intended for offline use, but it is tested alongside the decompressor on mobile devices and is usable on the faster settings.</p>

<p>LZHAM's decompressor currently has a higher cost to initialize than LZMA, so the threshold where LZHAM is typically faster vs. LZMA decompression is between 1000-13,000 of 
*compressed* output bytes, depending on the platform. It is not a good small block compressor: it likes large (10KB-15KB minimum) blocks.</p>

<p>LZHAM has simple support for patch files (delta compression), but this is a side benefit of its design, not its primary use case. Internally it supports LZ matches up 
to ~64KB and very large dictionaries (up to .5 GB).</p>

<p>LZHAM may be valuable to you if you compress data offline and distribute it to many customers, care about read/download times, and decompression speed/low CPU+power use 
are important to you.</p>

<p>I've been profiling LZHAM vs. LZMA and publishing the results on my blog: http://richg42.blogspot.com</p>

<p>Some independent benchmarks of the previous alpha versions: http://heartofcomp.altervista.org/MOC/MOCADE.htm, http://mattmahoney.net/dc/text.html</p>

<p>LZHAM has been integrated into the 7zip archiver (command line and GUI) as a custom codec plugin: http://richg42.blogspot.com/2015/02/lzham-10-integrated-into-7zip-command.html</p>

<h3>10GB Benchmark Results</h3>

Results with [7zip-LZHAM 9.38 32-bit](http://richg42.blogspot.com/2015/02/7zip-938-custom-codec-plugin-for-lzham.html) (64MB dictionary) on [Matt Mahoney's 10GB benchmark](http://mattmahoney.net/dc/10gb.html):

```
LZHAM (-mx=8): 3,577,047,629 Archive Test Time: 70.652 secs
LZHAM (-mx=9): 3,573,782,721 Archive Test Time: 71.292 secs
LZMA  (-mx=9): 3,560,052,414 Archive Test Time: 223.050 secs
7z .ZIP      : 4,681,291,655 Archive Test Time: 73.304 secs (unzip v6 x64 test time: 61.074 secs)
```

<h3>Most Common Question: So how does it compare to other libs like LZ4?</h3>

There is no single compression algorithm that perfectly suites all use cases and practical constraints. LZ4 and LZHAM are tools which lie at completely opposite ends of the spectrum:

* LZ4: A symmetrical codec with very fast compression and decompression but very low ratios. Its compression ratio is typically less than even zlib's (which uses a 21+ year old algorithm). 
LZ4 does a good job of trading off a large amount of compression ratio for very fast overall throughput.
Usage example: Reading LZMA/LZHAM/etc. compressed data from the network and decompressing it, then caching this data locally on disk using LZ4 to reduce disk usage and decrease future loading times.

* LZHAM: A very asymmetrical codec with slow compression speed, but with a very competitive (LZMA-like) compression ratio and reasonably fast decompression speeds (slower than zlib, but faster than LZMA).
LZHAM trades off a lot of compression throughput for very high ratios and higher decompression throughput relative to other codecs in its ratio class (which is LZMA, which runs circles around LZ4's ratio).
Usage example: Compress your product's data once on a build server, distribute it to end users over a slow media like the internet, then decompress it on the end user's device.

<h3>How Much Memory Does It Need?</h3>

For decompression it's easy to compute:
* Buffered mode: decomp_mem = dict_size + ~34KB for work tables
* Unbuffered mode: decomp_mem = ~34KB

I'll be honest here, the compressor is currently an angry beast when it comes to memory. The amount needed depends mostly on the compression level and dict. size. It's *approximately* (max_probes=128 at level -m4):
comp_mem = min(512 * 1024, dict_size / 8) * max_probes * 6 + dict_size * 9 + 22020096

Compression mem usage examples from Windows lzhamtest_x64 (note the equation is pretty off for small dictionary sizes):
* 32KB: 11MB
* 128KB: 21MB
* 512KB: 63MB
* 1MB: 118MB
* 8MB: 478MB
* 64MB: 982MB
* 128MB: 1558MB
* 256MB: 2710MB
* 512MB: 5014MB

<h3>Compressed Bitstream Compatibility</h3>

<p>v1.0's bitstream format is now locked in place, so any future v1.x releases will be backwards/forward compatible with compressed files 
written with v1.0. The only thing that could change this are critical bugfixes.</p>

<p>Note LZHAM v1.x bitstreams are NOT backwards compatible with any of the previous alpha versions on Google Code.</p>

<h3>Platforms/Compiler Support</h3>

<p>This fork (lzham-ex) maintains the codec and its build system. Supported targets:</p>

<table>
<tr><th>OS</th><th>Architectures</th><th>Compilers</th><th>Status</th></tr>
<tr><td>Linux</td><td>x86, x64, ARMv7, ARM64</td><td>GCC, Clang</td><td>Tested natively and under QEMU</td></tr>
<tr><td>Windows</td><td>x86, x64 (Windows 7 or newer)</td><td>MSVC 2019/2022, MinGW-w64</td><td>Tested on Windows 7 SP1 and Windows 10</td></tr>
<tr><td>macOS</td><td>x86_64, arm64 (universal binaries supported)</td><td>Apple Clang</td><td>Built and tested in CI</td></tr>
</table>

<p>Multithreaded compression uses Win32 threads or POSIX threads; atomic operations use MSVC intrinsics
or GCC built-ins. The plain ANSI C/C++ fallback (no threads, no atomics) still exists for new ports;
see "ANSI C/C++" below.</p>

<p>The bitstream format and the public C ABI (<code>lzham_get_version()</code> returns
<code>LZHAM_DLL_VERSION</code> = 0x1010) are frozen. Builds for every supported architecture read and write
the same streams, so data compressed on x64 can be decompressed on x86, ARM or ARM64.</p>

<p>Streams from the pre-1.0 alpha builds (LZHAM_DLL_VERSION 0x1008 and earlier, the one with
<code>lzham_polar_codes.cpp</code>) are <b>not</b> compatible with v1.x in either direction, exactly as the
upstream release notes warned. The format changed before v1.0, and a stream carries no version marker
(<code>send_configuration()</code> is a no-op), so a decoder cannot tell the two apart; a legacy alpha mode
would have to be requested explicitly. See <code>docs/legacy-0x1008.md</code> for the exact bitstream
differences and what a decoder-only legacy mode would cost.</p>


<h3>API</h3>

LZHAM supports streaming or memory to memory compression/decompression. See include/lzham.h. LZHAM can be linked statically or dynamically, just study the 
headers and the lzhamtest project. 
On Linux/macOS both the static and the shared library are tested (the test app links the static one).

LZHAM also supports a usable subset of the zlib API with extensions, either include/zlib.h or #define LZHAM_DEFINE_ZLIB_API and use include/lzham.h.

<h3>Usage Tips</h3>

* Always try to use the smallest dictionary size that makes sense for the file or block you are compressing, i.e. don't use a 128MB dictionary for a 15KB file. The codec
doesn't automatically choose for you because in streaming scenarios it has no idea how large the file or block will be.
* The larger the dictionary, the more RAM is required during compression and decompression. I would avoid using more than 8-16MB dictionaries on iOS.
* For faster decompression, prefer "unbuffered" decompression mode vs. buffered decompression (avoids a dictionary alloc and extra memcpy()'s), and disable adler-32 checking. Also, use the built-in LZHAM API's, not the
zlib-style API's for fastest decompression.
* Experiment with the "m_table_update_rate" compression/decompression parameter. This setting trades off a small amount of ratio for faster decompression.
Note the m_table_update_rate decompression parameter MUST match the setting used during compression (same for the dictionary size). It's up to you to store this info somehow.
* Avoid using LZHAM on small *compressed* blocks, where small is 1KB-10KB compressed bytes depending on the platform. LZHAM's decompressor is only faster than LZMA's beyond the small block threshold.
Optimizing LZHAM's decompressor to reduce its startup time relative to LZMA is a high priority.
* For best compression (I've seen up to ~4% better), enable the compressor's "extreme" parser, which is much slower but finds cheaper paths through a much denser parse graph.
Note the extreme parser can greatly slow down on files containing large amounts of repeated data/strings, but it is guaranteed to finish.
* The compressor's m_level parameter can make a big impact on compression speed. Level 0 (LZHAM_COMP_LEVEL_FASTEST) uses a much simpler greedy parser, and the other levels use 
near-optimal parsing with different heuristic settings.
* Check out the compressor/decompressor reinit() API's, which are useful if you'll be compressing or decompressing many times. Using the reinit() API's is a lot cheaper than fully 
initializing/deinitializing the entire codec every time.
* LZHAM's compressor is no speed demon. It's usually slower than LZMA's, sometimes by a wide (~2x slower or so) margin. In "extreme" parsing mode, it can be many times slower. 
This codec was designed with offline compression in mind.
* One significant difference between LZMA and LZHAM is how uncompressible files are handled. LZMA usually expands uncompressible files, and its decompressor can bog down and run extremely 
slowly on uncompressible data. LZHAM internally detects when each 512KB block is uncompressible and stores these blocks as uncompressed bytes instead. 
LZHAM's literal decoding is significantly faster than LZMA's, so the more plain literals in the output stream, the faster LZHAM's decompressor runs vs. LZMA's.
* General advice (applies to LZMA and other codecs too): If you are compressing large amounts of serialized game assets, sort the serialized data by asset type and compress the whole thing as a single large "solid" block of data.
Don't compress each individual asset, this will kill your ratio and have a higher decompression startup cost. If you need random access, consider compressing the assets lumped 
together into groups of a few hundred kilobytes (or whatever) each.
* LZHAM is a raw codec. It doesn't include any sort of preprocessing: EXE rel to abs jump transformation, audio predictors, etc. That's up to you
to do, before compression.

<h3>Codec Test App</h3>

lzhamtest_x86/x64 is a simple command line test program that uses the LZHAM codec to compress/decompress single files. 
lzhamtest is not intended as a file archiver or end user tool, it's just a simple testbed.

-- Usage examples:

- Compress single file "source_filename" to "compressed_filename":
	lzhamtest_x64 c source_filename compressed_filename
	
- Decompress single file "compressed_filename" to "decompressed_filename":
    lzhamtest_x64 d compressed_filename decompressed_filename

- Compress single file "source_filename" to "compressed_filename", then verify the compressed file decompresses properly to the source file:
	lzhamtest_x64 -v c source_filename compressed_filename

- Recursively compress all files under specified directory and verify that each file decompresses properly:
	lzhamtest_x64 -v a c:\source_path
	
-- Options	
	
- Set dictionary size used during compressed to 1MB (2^20):
	lzhamtest_x64 -d20 c source_filename compressed_filename
	
Valid dictionary sizes are [15,26] for x86, and [15,29] for x64. (See LZHAM_MIN_DICT_SIZE_LOG2, etc. defines in include/lzham.h.)
The x86 version defaults to 64MB (26), and the x64 version defaults to 256MB (28). I wouldn't recommend setting the dictionary size to 
512MB unless your machine has more than 4GB of physical memory.

- Set compression level to fastest:
	lzhamtest_x64 -m0 c source_filename compressed_filename
	
- Set compression level to uber (the default):
	lzhamtest_x64 -m4 c source_filename compressed_filename
	
- For best possible compression, use -d29 to enable the largest dictionary size (512MB) and the -x option which enables more rigorous (but ~4X slower!) parsing:
	lzhamtest_x64 -d29 -x -m4 c source_filename compressed_filename

See lzhamtest_x86/x64.exe's help text for more command line parameters.

Note that <code>lzhamtest</code> writes a small container around the stream (the 4-byte magic <code>LZH0</code>,
the dictionary size log2 and the source size) because it is a test harness. Use the library's memory API, or
<code>tools/lzham_compress</code>, when you need a bare LZHAM stream.

<h3>Compiling LZHAM</h3>

The CMake build (CMake 3.15 or newer) is the supported one on every platform and produces:

* the shared library: <code>liblzhamdll.so</code> / <code>liblzhamdll.dylib</code> (with SONAME/install name version 1),
  or <code>lzham_x86.dll</code> / <code>lzham_x64.dll</code> on Windows;
* the static library: <code>liblzhamlib.a</code>, or <code>lzhamlib_x86.lib</code> / <code>lzhamlib_x64.lib</code> on Windows;
* <code>lzhamtest</code> (command line test tool) and <code>lzham_roundtrip_test</code> (registered with CTest).

<pre>
Linux / macOS:
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
  cmake --build build -j
  ctest --test-dir build --output-on-failure

Windows (MSVC):
  cmake -S . -B build -A x64
  cmake --build build --config Release
  ctest --test-dir build -C Release --output-on-failure
  (use -A Win32 for x86)
</pre>

Common options: <code>LZHAM_BUILD_SHARED</code>, <code>LZHAM_BUILD_STATIC</code>, <code>LZHAM_BUILD_TESTS</code>,
<code>LZHAM_ENABLE_MULTITHREADING</code>, <code>LZHAM_INSTALL</code> (all ON by default except none).

Other configurations:

* 32-bit Linux: <code>-DCMAKE_C_FLAGS=-m32 -DCMAKE_CXX_FLAGS=-m32</code>.
* Cross-compiling ARM64/ARMv7: pass a toolchain file, or
  <code>-DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=aarch64
  -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++</code>.
  Add <code>-DCMAKE_CROSSCOMPILING_EMULATOR="qemu-aarch64;-L;/usr/aarch64-linux-gnu"</code> to run the tests under QEMU.
* macOS universal binary: <code>-DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0</code>.
* MinGW-w64: use a toolchain file; the DLLs and test executables are linked against the static
  libgcc/libstdc++ runtime, so no extra DLLs are needed next to them.

Installing: <code>cmake --install build --prefix /usr/local</code> installs the headers, the libraries and a CMake
package (<code>find_package(lzham)</code>, targets <code>lzham::lzham</code> and <code>lzham::lzham_static</code>) plus
a pkg-config file (<code>lzham.pc</code>).

IMPORTANT: With clang or gcc compile LZHAM with "No strict aliasing" ENABLED: -fno-strict-aliasing
(the CMake build does this for you).

I DO NOT test or develop the codec with strict aliasing:
* https://lkml.org/lkml/2003/2/26/158
* http://stackoverflow.com/questions/2958633/gcc-strict-aliasing-and-horror-stories

It might work fine, I don't know yet. This is usually not a problem with MSVC, which defaults to strict aliasing being off.

<h3>ANSI C/C++</h3>

LZHAM supports compiling as plain vanilla ANSI C/C++. To see how the codec configures itself check out lzham_core.h and search for "LZHAM_ANSI_CPLUSPLUS". 
All platform specific stuff (unaligned loads, threading, atomic ops, etc.) should be disabled when this macro is defined. Note, the compressor doesn't use threads 
or atomic operations when built this way so it's going to be pretty slow. (The compressor was built from the ground up to be threaded.)

<h3>Known Problems</h3>

<p>LZHAM's decompressor is like a drag racer that needs time to get up to speed. LZHAM is not intended or optimized to be used on "small" blocks of data (less 
than ~10,000 bytes of *compressed* data on desktops, or around 1,000-5,000 on iOS). If your usage case involves calling the codec over and over with tiny blocks 
then LZMA, LZ4, Deflate, etc. are probably better choices.</p>

<p>The decompressor still takes too long to init vs. LZMA. On iOS the cost is not that bad, but on desktop the cost is high. I have reduced the startup cost vs. the 
alpha but there's still work to do.</p>

<p>The compressor is slower than I would like, and doesn't scale as well as it could. I added a reinit() method to make it initialize faster, but it's not a speed demon. 
My focus has been on ratio and decompression speed.</p>

<p>I use tabs=3 spaces, but I think some actual tabs got in the code. I need to run the sources through ClangFormat or whatever.</p>

<h3>Special Thanks</h3>

<p>Thanks to everyone at the http://encode.ru forums. I read these forums as a lurker before working on LZHAM, and I studied every LZ related 
post I could get my hands on. Especially anything related to LZ optimal parsing, which still seems like a black art. LZHAM was my way of 
learning how to implement optimal parsing (and you can see this if you study the progress I made in the early alphas on Google Code).</p>

<p>Also, thanks to Igor Pavlov, the original creator of LZMA and 7zip, for advancing the start of the art in LZ compression.</p>
