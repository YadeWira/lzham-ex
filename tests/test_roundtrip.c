// test_roundtrip.c - Smoke test for the public LZHAM API.
// LZHAM is in the Public Domain. Please see the Public Domain declaration at the end of include/lzham.h
//
// Compresses and decompresses a set of synthetic buffers through lzham_compress_memory()/
// lzham_decompress_memory(), verifying both the payload and the Adler-32 checksums. This is
// intentionally dependency-free so it can run on every supported platform (including under QEMU).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lzham.h"

static unsigned g_seed = 123456789u;

static unsigned next_random(void)
{
   g_seed = g_seed * 1103515245u + 12345u;
   return g_seed >> 16;
}

static void fill_buffer(unsigned char *p, size_t n, int mode)
{
   size_t i;
   static const char *pattern = "LZHAM roundtrip test pattern 0123456789 ";
   const size_t pattern_len = strlen(pattern);

   for (i = 0; i < n; i++)
   {
      switch (mode)
      {
         case 0: p[i] = 0; break;
         case 1: p[i] = (unsigned char)(pattern[i % pattern_len]); break;
         case 2: p[i] = (unsigned char)next_random(); break;
         default: p[i] = (unsigned char)(pattern[i % pattern_len] + (next_random() & 3u)); break;
      }
   }
}

static int test_one(size_t src_len, int mode, int level, unsigned dict_size_log2)
{
   const size_t cmp_capacity = src_len + (src_len / 2) + 4096;
   unsigned char *p_src, *p_cmp, *p_dec;
   size_t cmp_len, dec_len;
   lzham_compress_params comp_params;
   lzham_decompress_params decomp_params;
   lzham_compress_status_t comp_status;
   lzham_decompress_status_t decomp_status;
   lzham_uint32 comp_adler32 = 0, decomp_adler32 = 0;
   int result = 0;

   p_src = (unsigned char *)malloc(src_len ? src_len : 1);
   p_cmp = (unsigned char *)malloc(cmp_capacity);
   p_dec = (unsigned char *)malloc(src_len ? src_len : 1);
   if (!p_src || !p_cmp || !p_dec)
   {
      printf("FAIL: out of memory\n");
      result = 1;
      goto cleanup;
   }

   fill_buffer(p_src, src_len, mode);

   memset(&comp_params, 0, sizeof(comp_params));
   comp_params.m_struct_size = sizeof(comp_params);
   comp_params.m_dict_size_log2 = dict_size_log2;
   comp_params.m_level = (lzham_compress_level)level;
   comp_params.m_table_update_rate = 0;
   comp_params.m_max_helper_threads = -1;

   cmp_len = cmp_capacity;
   comp_status = lzham_compress_memory(&comp_params, p_cmp, &cmp_len, p_src, src_len, &comp_adler32);
   if (comp_status != LZHAM_COMP_STATUS_SUCCESS)
   {
      printf("FAIL: compress status %d (len=%lu mode=%d level=%d dict=%u)\n",
             (int)comp_status, (unsigned long)src_len, mode, level, dict_size_log2);
      result = 1;
      goto cleanup;
   }

   memset(&decomp_params, 0, sizeof(decomp_params));
   decomp_params.m_struct_size = sizeof(decomp_params);
   decomp_params.m_dict_size_log2 = dict_size_log2;
   decomp_params.m_table_update_rate = 0;
   decomp_params.m_decompress_flags = LZHAM_DECOMP_FLAG_COMPUTE_ADLER32;

   dec_len = src_len;
   decomp_status = lzham_decompress_memory(&decomp_params, p_dec, &dec_len, p_cmp, cmp_len, &decomp_adler32);
   if (decomp_status != LZHAM_DECOMP_STATUS_SUCCESS)
   {
      printf("FAIL: decompress status %d (len=%lu mode=%d level=%d dict=%u)\n",
             (int)decomp_status, (unsigned long)src_len, mode, level, dict_size_log2);
      result = 1;
      goto cleanup;
   }

   if (dec_len != src_len || memcmp(p_src, p_dec, src_len) != 0)
   {
      printf("FAIL: payload mismatch (len=%lu mode=%d level=%d dict=%u)\n",
             (unsigned long)src_len, mode, level, dict_size_log2);
      result = 1;
      goto cleanup;
   }

   if (comp_adler32 != decomp_adler32)
   {
      printf("FAIL: adler32 mismatch 0x%08X != 0x%08X (len=%lu mode=%d level=%d dict=%u)\n",
             comp_adler32, decomp_adler32, (unsigned long)src_len, mode, level, dict_size_log2);
      result = 1;
   }

cleanup:
   free(p_src);
   free(p_cmp);
   free(p_dec);
   return result;
}

int main(void)
{
   static const size_t sizes[] = { 0, 1, 15, 1024, 65536, 1u << 20 };
   static const int modes[] = { 0, 1, 2, 3 };
   static const int levels[] = { LZHAM_COMP_LEVEL_FASTEST, LZHAM_COMP_LEVEL_DEFAULT, LZHAM_COMP_LEVEL_UBER };
   const unsigned dict_size_log2 = LZHAM_MIN_DICT_SIZE_LOG2 + 2;
   int failures = 0;
   size_t s, m, l;

   printf("LZHAM version: 0x%04X\n", (unsigned)lzham_get_version());

   for (s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++)
   {
      for (m = 0; m < sizeof(modes) / sizeof(modes[0]); m++)
      {
         for (l = 0; l < sizeof(levels) / sizeof(levels[0]); l++)
            failures += test_one(sizes[s], modes[m], levels[l], dict_size_log2);
      }
   }

   if (failures)
   {
      printf("%d test(s) FAILED\n", failures);
      return 1;
   }

   printf("All roundtrip tests passed.\n");
   return 0;
}
