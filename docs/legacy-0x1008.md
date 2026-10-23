# Pre-1.0 streams (LZHAM_DLL_VERSION 0x1008)

This page records why the pre-1.0 alpha format is incompatible with LZHAM v1.x, so nobody has to rediscover
it. It came up when a downstream user (zpaq-std) tried to swap its bundled copy for this fork: its copy is the
`0x1008` line (from the old `richgel999/lzham` repository, the one with `lzham_polar_codes.cpp`), and streams
neither decode in 1.0.1 nor the other way around.

Upstream already warned about this in its v1.0 release notes:

> Note LZHAM v1.x bitstreams are NOT backwards compatible with any of the previous alpha versions on Google Code.

## What is different in the bitstream

Both formats derive their Huffman tables implicitly from locally updated symbol frequencies (no table bytes
are transmitted), so any difference in the update schedule or in the model contexts desynchronizes the decoder
immediately. There are four of them:

1. **Stream prefix.** `0x1008` writes two configuration bits before the first block, chosen per compression
   level:

   | Level | fast adaptive updating | polar codes |
   |---|---|---|
   | FASTEST, FASTER | 1 | 1 |
   | DEFAULT | 0 | 1 |
   | BETTER, UBER | 0 | 0 |

   v1.0 writes nothing there: `lzcompressor::send_configuration()` is commented out. So the very first two
   bits of a stream already mean different things (configuration flags vs. a 2-bit block header).

2. **Table update schedule.** `0x1008` grows the update interval geometrically (doubling with fast adaptive
   updating, ×1.25 otherwise) up to a ceiling derived from the symbol count. v1.0 uses
   `m_max_update_interval` / `m_table_update_interval_slow_rate` from the compression parameters
   (defaults 64/32).

3. **Model contexts.** `0x1008` selects the `is_match` models with previous-character bits plus the state
   (`LZHAM_IS_MATCH_MODEL_INDEX(prev_char, cur_state)`) and literals/delta-literals with 64 contexts built
   from the last two bytes (`(pc >> 5) | ((ppc >> 5) << 3)`). v1.0 uses the 12-state model index only
   (`LZHAM_IS_MATCH_MODEL_INDEX(cur_state)`).

4. **Polar codes.** `0x1008` can compute the code sizes with "polar codes" (`lzham_polar_codes.cpp`) instead
   of the Moffat/Huffman construction, selected by the second configuration bit. v1.0 has no polar codes.

## A decoder-only legacy mode

Feasible, with two constraints:

* **It cannot be automatic.** A LZHAM stream carries no version marker, and the wrapper header only encodes
  the method (14 = LZHAM) and the dictionary size. Legacy decoding has to be requested explicitly, e.g. a new
  additive entry point `lzham_decompress_memory_legacy_0x1008()` or a decompression flag.
* **Decoder only.** Supporting the old format on the compression side would mean porting the old encoder too;
  nobody needs that (old archives only have to be readable).

Two implementation routes:

| Route | Size | Risk |
|---|---|---|
| Port the `0x1008` decoder sources (10 files, ~4.5k lines) into a separate namespace | large | low, but namespace/infra surgery and dead encoder paths |
| Reimplement a self-contained decoder from the reference specification | ~1,000-1,300 lines | medium, but zero coupling with the v1.x decoder |

The second route is recommended. zpaq-std maintains an executable specification of the format it feeds to its
ZPAQL decoder (a 284-line Python decoder covering blocks, models, arithmetic-coded flags, prefix codes,
polar codes and the LZ loop), which makes the port mechanical and testable against real archives.

Initial scope would be: unbuffered memory decoding, dictionary sizes up to 2^26, no seed/delta dictionaries,
no zlib wrapper. Estimated effort: 1-2 days including a corpus roundtrip against the reference decoder and a
sanitizer run with damaged blocks.
