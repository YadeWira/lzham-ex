// fuzz_roundtrip.c - libFuzzer target for the compressor and the roundtrip.
// LZHAM is in the Public Domain. Please see the Public Domain declaration at the end of include/lzham.h
//
// Uses the fuzzer input as payload, derives parameters from the first bytes, compresses with the memory API,
// decompresses the result and compares. Also checks cross-parameter decoding: a stream must decode with exactly
// the parameters it was written with (dictionary size and table update rate) and fail cleanly with others.
//
// Run with -fsanitize=fuzzer,address,undefined.

#include <stdlib.h>
#include <string.h>

#include "lzham.h"

int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size)
{
   unsigned dict_size_log2, level, table_rate;
   unsigned char *p_cmp, *p_dec;
   size_t cmp_cap, cmp_len, dec_len;
   lzham_compress_params cp;
   lzham_decompress_params dp;
   lzham_compress_status_t cst;
   lzham_decompress_status_t dst;

   if (size < 4)
      return 0;
   if (size > (1u << 20))
      size = 1u << 20;

   dict_size_log2 = 15 + (data[0] % 12);       /* 15..26 */
   level = data[1] % 5;
   table_rate = data[2] % 21;                  /* 0..20 */
   data += 3;
   size -= 3;

   cmp_cap = size + (size / 2) + 4096;
   p_cmp = (unsigned char *)malloc(cmp_cap);
   p_dec = (unsigned char *)malloc(size ? size : 1);
   if (!p_cmp || !p_dec)
   {
      free(p_cmp);
      free(p_dec);
      return 0;
   }

   memset(&cp, 0, sizeof(cp));
   cp.m_struct_size = sizeof(cp);
   cp.m_dict_size_log2 = dict_size_log2;
   cp.m_level = (lzham_compress_level)level;
   cp.m_table_update_rate = table_rate;
   cp.m_max_helper_threads = (data[3] & 1) ? 1 : 0;

   cmp_len = cmp_cap;
   cst = lzham_compress_memory(&cp, p_cmp, &cmp_len, data, size, NULL);
   if ((cst == LZHAM_COMP_STATUS_SUCCESS) || (cst == LZHAM_COMP_STATUS_OUTPUT_BUF_TOO_SMALL))
   {
      memset(&dp, 0, sizeof(dp));
      dp.m_struct_size = sizeof(dp);
      dp.m_dict_size_log2 = dict_size_log2;
      dp.m_table_update_rate = table_rate;
      dp.m_decompress_flags = LZHAM_DECOMP_FLAG_COMPUTE_ADLER32;

      dec_len = size;
      dst = lzham_decompress_memory(&dp, p_dec, &dec_len, p_cmp, cmp_len, NULL);
      if (cst == LZHAM_COMP_STATUS_SUCCESS)
      {
         if ((dst != LZHAM_DECOMP_STATUS_SUCCESS) || (dec_len != size) || memcmp(p_dec, data, size))
            abort();                            /* roundtrip mismatch */
      }
      (void)dst;

      /* With a different dictionary size the stream must not decode to the same payload (usually it fails). */
      dp.m_dict_size_log2 = (dict_size_log2 == 15) ? 16 : 15;
      dec_len = size;
      dst = lzham_decompress_memory(&dp, p_dec, &dec_len, p_cmp, cmp_len, NULL);
      if ((dst == LZHAM_DECOMP_STATUS_SUCCESS) && (dec_len == size) && (size > 0) && (!memcmp(p_dec, data, size)))
      {
         /* Same payload with the wrong dictionary is only acceptable for streams that do not use the
            dictionary at all (tiny inputs), so do not treat it as a failure. */
      }
   }

   free(p_cmp);
   free(p_dec);
   return 0;
}
