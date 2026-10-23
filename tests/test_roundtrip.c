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

// Compresses with the zlib-style API using exactly lzham_z_compressBound() bytes and checks that it succeeds
// and roundtrips. This is a regression test for upstream issue #22 (the bound used to be smaller than the real
// worst case for incompressible data, so lzham_z_compress2() failed with Z_BUF_ERROR).
static int test_zlib_bound(size_t src_len, int mode)
{
   unsigned char *p_src, *p_cmp, *p_dec;
   lzham_z_ulong cmp_len, dec_len;
   int status, result = 0;

   p_src = (unsigned char *)malloc(src_len ? src_len : 1);
   p_dec = (unsigned char *)malloc(src_len ? src_len : 1);
   cmp_len = lzham_z_compressBound((lzham_z_ulong)src_len);
   p_cmp = (unsigned char *)malloc((size_t)cmp_len);
   if (!p_src || !p_cmp || !p_dec)
   {
      printf("FAIL: out of memory\n");
      free(p_src);
      free(p_cmp);
      free(p_dec);
      return 1;
   }

   fill_buffer(p_src, src_len, mode);

   status = lzham_z_compress2(p_cmp, &cmp_len, p_src, (lzham_z_ulong)src_len, 9);
   if (status != 0)
   {
      printf("FAIL: zlib compress2 status %d with compressBound (len=%lu mode=%d)\n",
             status, (unsigned long)src_len, mode);
      result = 1;
      goto cleanup;
   }

   dec_len = (lzham_z_ulong)src_len;
   status = lzham_z_uncompress(p_dec, &dec_len, p_cmp, cmp_len);
   if ((status != 0) || (dec_len != (lzham_z_ulong)src_len) || memcmp(p_src, p_dec, src_len) != 0)
   {
      printf("FAIL: zlib uncompress status %d (len=%lu mode=%d)\n", status, (unsigned long)src_len, mode);
      result = 1;
   }

cleanup:
   free(p_src);
   free(p_cmp);
   free(p_dec);
   return result;
}

// Regression test for upstream issue #13: a negative window_bits must produce a raw stream (no 2-byte LZHAM
// "zlib" header), and the raw stream must inflate with inflateInit2(-window_bits) but not as a wrapped stream.
static int test_raw_deflate(size_t src_len)
{
   unsigned char *p_src, *p_raw, *p_wrapped, *p_dec;
   const lzham_z_ulong cap = lzham_z_compressBound((lzham_z_ulong)src_len) + 64;
   lzham_z_ulong raw_len, wrapped_len = cap, dec_len;
   lzham_z_stream stream;
   int status, result = 0;

   p_src = (unsigned char *)malloc(src_len);
   p_raw = (unsigned char *)malloc((size_t)cap);
   p_wrapped = (unsigned char *)malloc((size_t)cap);
   p_dec = (unsigned char *)malloc(src_len);
   if (!p_src || !p_raw || !p_wrapped || !p_dec)
   {
      printf("FAIL: out of memory\n");
      result = 1;
      goto cleanup;
   }

   fill_buffer(p_src, src_len, 2);

   memset(&stream, 0, sizeof(stream));
   status = lzham_z_deflateInit2(&stream, 9, LZHAM_Z_DEFLATED, -(int)LZHAM_DEFAULT_DICT_SIZE_LOG2, 9, LZHAM_Z_DEFAULT_STRATEGY);
   if (status != 0)
   {
      printf("FAIL: deflateInit2(raw) status %d\n", status);
      result = 1;
      goto cleanup;
   }
   stream.next_in = p_src;
   stream.avail_in = (unsigned)src_len;
   stream.next_out = p_raw;
   stream.avail_out = (unsigned)cap;
   status = lzham_z_deflate(&stream, LZHAM_Z_FINISH);
   raw_len = (lzham_z_ulong)stream.total_out;
   lzham_z_deflateEnd(&stream);
   if (status != LZHAM_Z_STREAM_END)
   {
      printf("FAIL: deflate(raw) status %d\n", status);
      result = 1;
      goto cleanup;
   }

   // The wrapped stream of the same data is exactly two bytes larger (the LZHAM "zlib" header).
   if ((lzham_z_compress2(p_wrapped, &wrapped_len, p_src, (lzham_z_ulong)src_len, 9) != 0) ||
       (wrapped_len != raw_len + 2))
   {
      printf("FAIL: raw stream has a zlib header (raw=%lu wrapped=%lu)\n",
             (unsigned long)raw_len, (unsigned long)wrapped_len);
      result = 1;
      goto cleanup;
   }

   memset(&stream, 0, sizeof(stream));
   status = lzham_z_inflateInit2(&stream, -(int)LZHAM_DEFAULT_DICT_SIZE_LOG2);
   if (status != 0)
   {
      printf("FAIL: inflateInit2(raw) status %d\n", status);
      result = 1;
      goto cleanup;
   }
   stream.next_in = p_raw;
   stream.avail_in = (unsigned)raw_len;
   stream.next_out = p_dec;
   stream.avail_out = (unsigned)src_len;
   status = lzham_z_inflate(&stream, LZHAM_Z_FINISH);
   dec_len = (lzham_z_ulong)stream.total_out;
   lzham_z_inflateEnd(&stream);
   if ((status != LZHAM_Z_STREAM_END) || (dec_len != (lzham_z_ulong)src_len) || memcmp(p_src, p_dec, src_len) != 0)
   {
      printf("FAIL: raw inflate status %d len=%lu\n", status, (unsigned long)dec_len);
      result = 1;
   }

cleanup:
   free(p_src);
   free(p_raw);
   free(p_wrapped);
   free(p_dec);
   return result;
}

// Regression test for upstream issue #14: m_dict_size_log2 == 0 must select the portable default dictionary on
// both the compression and the decompression side, instead of returning INVALID_PARAMETER.
static int test_default_dict_size(size_t src_len)
{
   unsigned char *p_src, *p_cmp, *p_dec;
   size_t cmp_len = src_len + (src_len / 2) + 4096, dec_len;
   lzham_compress_params comp_params;
   lzham_decompress_params decomp_params;
   int result = 0;

   p_src = (unsigned char *)malloc(src_len);
   p_cmp = (unsigned char *)malloc(cmp_len);
   p_dec = (unsigned char *)malloc(src_len);
   if (!p_src || !p_cmp || !p_dec)
   {
      printf("FAIL: out of memory\n");
      free(p_src);
      free(p_cmp);
      free(p_dec);
      return 1;
   }

   fill_buffer(p_src, src_len, 1);

   memset(&comp_params, 0, sizeof(comp_params));
   comp_params.m_struct_size = sizeof(comp_params);
   comp_params.m_dict_size_log2 = 0;
   comp_params.m_level = LZHAM_COMP_LEVEL_FASTEST;
   comp_params.m_max_helper_threads = 1;

   if (lzham_compress_memory(&comp_params, p_cmp, &cmp_len, p_src, src_len, NULL) != LZHAM_COMP_STATUS_SUCCESS)
   {
      printf("FAIL: compress with the default dictionary size failed\n");
      result = 1;
      goto cleanup;
   }

   memset(&decomp_params, 0, sizeof(decomp_params));
   decomp_params.m_struct_size = sizeof(decomp_params);
   decomp_params.m_dict_size_log2 = 0;

   dec_len = src_len;
   if ((lzham_decompress_memory(&decomp_params, p_dec, &dec_len, p_cmp, cmp_len, NULL) != LZHAM_DECOMP_STATUS_SUCCESS) ||
       (dec_len != src_len) || memcmp(p_src, p_dec, src_len) != 0)
   {
      printf("FAIL: decompress with the default dictionary size failed\n");
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

   // Upstream issue #22: the zlib-style bound must be large enough for incompressible data.
   for (s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++)
   {
      for (m = 0; m < sizeof(modes) / sizeof(modes[0]); m++)
         failures += test_zlib_bound(sizes[s], modes[m]);
   }

   // Upstream issues #13 (raw deflate) and #14 (default dictionary size).
   failures += test_raw_deflate(65536);
   failures += test_raw_deflate(1u << 20);
   failures += test_default_dict_size(65536);
   failures += test_default_dict_size(1u << 20);

   if (failures)
   {
      printf("%d test(s) FAILED\n", failures);
      return 1;
   }

   printf("All roundtrip tests passed.\n");
   return 0;
}
