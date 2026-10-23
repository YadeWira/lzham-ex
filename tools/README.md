# Format tooling

LZHAM has no formal format specification: the bitstream was documented by the codec itself. These tools make
the format testable and give third-party decoders (for example a ZPAQL implementation) something to check
against.

| File | What it is |
|---|---|
| `lzham_ref.py` | Reference decoder for LZHAM 1.0 streams (`LZHAM_DLL_VERSION` 0x1010). Self-contained, ~500 lines of Python, and doubles as the readable specification of the format. |
| `lzham_compress.c` | Raw-stream compressor CLI: the counterpart. Compresses one file with the memory API (`lzham_compress_memory()`), no helper threads, so the output is deterministic. |
| `test_reference.py` | Runs the pair over five fixtures (empty, tiny, compressible text, incompressible bytes, long runs) x dict 15/20 x levels 0/2/4 and compares the roundtrip. Registered with CTest as `lzham_reference_decoder`. |

## Usage

```sh
# compress a file into a raw stream (dict 2^20, level 4)
./build/bin/Release/lzham_compress input.bin stream.lzham 20 4

# decode it back
python3 tools/lzham_ref.py stream.lzham output.bin --dict 20

# run the whole check (also part of ctest)
python3 tools/test_reference.py ./build/bin/Release/lzham_compress
```

## What the reference decoder covers

* Blocks: compressed, raw, sync and EOF, including the flush types that reset the table update rates or all
  the models.
* The binary arithmetic-coded flags (`is_match`, `is_rep`, `is_rep0`, `is_rep1`, `is_rep2`, single-byte
  matches) with the codec's 11-bit probability update.
* The quasi-adaptive Huffman tables: implicit, rebuilt every `update_cycle` symbols from both sides' own
  frequency counts, with the codec's exact cycle growth rule, rescaling and code-length limiting.
* Literal, delta-literal, rep0/rep1/rep2/rep3, normal matches, large lengths, the distance LSB table and the
  "huge" match lengths.
* The trailing Adler-32 of the source data, which is always written even without a zlib wrapper.

## What it does not cover

* Seed/delta dictionaries (`m_pSeed_bytes`).
* Buffered streaming: the decoder wants the whole stream in memory.
* zlib-wrapped streams (the 2-byte `0x0E ...` header); skip it before decoding, or use the library.

## A note about `lzhamtest` files

`lzhamtest` writes its own container around the stream: the 4-byte magic `LZH0`, one byte with the dictionary
size log2 and eight bytes of little-endian source size, followed by the raw LZHAM stream. Those files are not
raw streams; use `lzham_compress` (or the library's memory API) when you need the bare format.

## Pre-1.0 streams

The alpha line (`LZHAM_DLL_VERSION` 0x1008, with `lzham_polar_codes.cpp`) uses a different format. See
[`docs/legacy-0x1008.md`](../docs/legacy-0x1008.md) for the exact differences.
