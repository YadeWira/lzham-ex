// test_streaming.c - Stress tests for the streaming and zlib-style APIs.
// LZHAM is in the Public Domain. Please see the Public Domain declaration at the end of include/lzham.h
//
// The memory API is covered by test_roundtrip.c; this test drives the state machines that the test app and
// the zlib compatibility layer use, with tiny and irregular buffer sizes, which is where bookkeeping bugs
// hide. It also covers lzham_decompress_reinit() switching to a larger dictionary (a heap overflow before
// 1.1.0).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lzham.h"

static unsigned g_seed = 0xC0FFEEu;

static unsigned next_random(void)
{
   g_seed = g_seed * 1103515245u + 12345u;
   return (g_seed >> 16) ^ g_seed;
}

static void fill(unsigned char *p, size_t n, int mode)
{
   size_t i;
   for (i = 0; i < n; i++)
      p[i] = (mode == 0) ? (unsigned char)(i * 31 + (i >> 7)) : (unsigned char)next_random();
}

/* Compresses with the streaming API, writing through a caller-chosen output chunk size, then decompresses with
 * the streaming API reading through a caller-chosen input chunk size. */
static int streaming_roundtrip(size_t src_len, unsigned dict_log2, int level, size_t out_chunk, size_t in_chunk,
                               int mode)
{
   unsigned char *p_src, *p_cmp, *p_dec;
   size_t cmp_cap = src_len + (src_len / 2) + 4096, cmp_len = 0, dec_len = 0;
   lzham_compress_params cp;
   lzham_decompress_params dp;
   lzham_compress_state_ptr cs;
   lzham_decompress_state_ptr ds;
   size_t src_ofs = 0, dec_ofs = 0;
   int result = 0;

   p_src = (unsigned char *)malloc(src_len ? src_len : 1);
   p_cmp = (unsigned char *)malloc(cmp_cap);
   /* The decoder never writes more than src_len bytes, but a spare chunk keeps the output-buffer arithmetic
      simple when the caller asks for tiny buffers. */
   p_dec = (unsigned char *)malloc(src_len + 4096 + 1);
   if (!p_src || !p_cmp || !p_dec)
   {
      printf("FAIL: out of memory\n");
      free(p_src); free(p_cmp); free(p_dec);
      return 1;
   }
   fill(p_src, src_len, mode);

   memset(&cp, 0, sizeof(cp));
   cp.m_struct_size = sizeof(cp);
   cp.m_dict_size_log2 = (lzham_uint32)dict_log2;
   cp.m_level = (lzham_compress_level)level;
   cp.m_max_helper_threads = 1;

   cs = lzham_compress_init(&cp);
   if (!cs)
   {
      printf("FAIL: compress_init (len=%lu dict=%u level=%d)\n", (unsigned long)src_len, dict_log2, level);
      free(p_src); free(p_cmp); free(p_dec);
      return 1;
   }

   for (;;)
   {
      const size_t chunk = (out_chunk == 1) ? 1 : out_chunk;
      size_t in_len = (src_len - src_ofs < chunk) ? (src_len - src_ofs) : chunk;
      size_t out_len = (cmp_cap - cmp_len < chunk) ? (cmp_cap - cmp_len) : chunk;
      lzham_compress_status_t st = lzham_compress(cs, p_src + src_ofs, &in_len, p_cmp + cmp_len, &out_len,
                                                  (lzham_bool)((src_ofs + in_len) == src_len));
      src_ofs += in_len;
      cmp_len += out_len;
      if (st == LZHAM_COMP_STATUS_SUCCESS)
         break;
      if ((st != LZHAM_COMP_STATUS_NOT_FINISHED) && (st != LZHAM_COMP_STATUS_NEEDS_MORE_INPUT) &&
          (st != LZHAM_COMP_STATUS_HAS_MORE_OUTPUT))
      {
         printf("FAIL: compress status %d (len=%lu)\n", (int)st, (unsigned long)src_len);
         result = 1;
         goto done;
      }
      if ((src_ofs == src_len) && (out_len == 0))
      {
         printf("FAIL: compressor made no progress (len=%lu)\n", (unsigned long)src_len);
         result = 1;
         goto done;
      }
   }
   lzham_compress_deinit(cs);
   cs = NULL;

   memset(&dp, 0, sizeof(dp));
   dp.m_struct_size = sizeof(dp);
   dp.m_dict_size_log2 = (lzham_uint32)dict_log2;
   dp.m_decompress_flags = LZHAM_DECOMP_FLAG_COMPUTE_ADLER32;

   ds = lzham_decompress_init(&dp);
   if (!ds)
   {
      printf("FAIL: decompress_init\n");
      result = 1;
      goto done;
   }

   for (;;)
   {
      size_t in_len = (cmp_len - dec_ofs == 0) ? 0 : ((cmp_len - dec_ofs < in_chunk) ? (cmp_len - dec_ofs) : in_chunk);
      size_t out_len = (src_len + 4096) - dec_len;
      if (out_len > in_chunk)
         out_len = in_chunk;
      /* Only the last input chunk may set the EOF flag; setting it earlier tells the decoder the stream is
         truncated. */
      const lzham_bool no_more_input = (lzham_bool)((dec_ofs + in_len) == cmp_len);
      lzham_decompress_status_t st = lzham_decompress(ds, p_cmp + dec_ofs, &in_len, p_dec + dec_len, &out_len, no_more_input);
      dec_ofs += in_len;
      dec_len += out_len;
      if (st == LZHAM_DECOMP_STATUS_SUCCESS)
         break;
      if ((st != LZHAM_DECOMP_STATUS_NOT_FINISHED) && (st != LZHAM_DECOMP_STATUS_HAS_MORE_OUTPUT) &&
          (st != LZHAM_DECOMP_STATUS_NEEDS_MORE_INPUT))
      {
         printf("FAIL: decompress status %d (len=%lu)\n", (int)st, (unsigned long)src_len);
         result = 1;
         goto done;
      }
      if ((dec_ofs == cmp_len) && (out_len == 0))
      {
         printf("FAIL: decompressor made no progress (len=%lu)\n", (unsigned long)src_len);
         result = 1;
         goto done;
      }
   }

   if ((dec_len != src_len) || memcmp(p_src, p_dec, src_len) != 0)
   {
      printf("FAIL: payload mismatch (len=%lu out=%lu)\n", (unsigned long)src_len, (unsigned long)dec_len);
      result = 1;
   }

done:
   if (cs)
      lzham_compress_deinit(cs);
   if (ds)
      lzham_decompress_deinit(ds);
   free(p_src);
   free(p_cmp);
   free(p_dec);
   return result;
}

/* Regression test: reinit from a small dictionary to a larger one must resize the ring buffer. Before 1.1.0 it
 * kept the old size and the buffered decoder wrote past the allocation. */
static int reinit_larger_dict(void)
{
   const size_t src_len = 1u << 20;
   unsigned char *p_src, *p_cmp, *p_out;
   size_t cmp_cap = src_len + (src_len / 8) + 65536, cmp_len = cmp_cap;
   lzham_compress_params cp;
   lzham_decompress_params dp, dp2;
   lzham_decompress_state_ptr ds;
   size_t total = 0;
   lzham_decompress_status_t st;
   int result = 0;

   p_src = (unsigned char *)malloc(src_len);
   p_cmp = (unsigned char *)malloc(cmp_cap);
   p_out = (unsigned char *)malloc(1 << 16);
   if (!p_src || !p_cmp || !p_out)
   {
      free(p_src); free(p_cmp); free(p_out);
      return 1;
   }
   fill(p_src, src_len, 0);

   memset(&cp, 0, sizeof(cp));
   cp.m_struct_size = sizeof(cp);
   cp.m_dict_size_log2 = 20;
   cp.m_level = LZHAM_COMP_LEVEL_DEFAULT;
   cp.m_max_helper_threads = 1;
   if (lzham_compress_memory(&cp, p_cmp, &cmp_len, p_src, src_len, NULL) != LZHAM_COMP_STATUS_SUCCESS)
   {
      printf("FAIL: reinit test: compression failed\n");
      result = 1;
      goto done;
   }

   memset(&dp, 0, sizeof(dp));
   dp.m_struct_size = sizeof(dp);
   dp.m_dict_size_log2 = LZHAM_MIN_DICT_SIZE_LOG2;
   ds = lzham_decompress_init(&dp);
   if (!ds)
   {
      printf("FAIL: reinit test: decompress_init failed\n");
      result = 1;
      goto done;
   }

   dp2 = dp;
   dp2.m_dict_size_log2 = 20;
   if (!lzham_decompress_reinit(ds, &dp2))
   {
      printf("FAIL: reinit test: decompress_reinit failed\n");
      result = 1;
      goto done;
   }

   {
      size_t in_ofs = 0;
      for (;;)
      {
         size_t in_len = cmp_len - in_ofs;
         size_t out_len = 1 << 16;
         st = lzham_decompress(ds, p_cmp + in_ofs, &in_len, p_out, &out_len, (lzham_bool)(in_len == (cmp_len - in_ofs)));
         in_ofs += in_len;
         total += out_len;
         if (st == LZHAM_DECOMP_STATUS_SUCCESS)
            break;
         if ((st != LZHAM_DECOMP_STATUS_NOT_FINISHED) && (st != LZHAM_DECOMP_STATUS_HAS_MORE_OUTPUT))
         {
            printf("FAIL: reinit test: decompress status %d\n", (int)st);
            result = 1;
            break;
         }
      }
   }
   if (!result && (total != src_len))
   {
      printf("FAIL: reinit test: decoded %lu of %lu bytes\n", (unsigned long)total, (unsigned long)src_len);
      result = 1;
   }

done:
   if (ds)
      lzham_decompress_deinit(ds);
   free(p_src);
   free(p_cmp);
   free(p_out);
   return result;
}

/* The zlib-style API with a one-byte output buffer, wrapped and raw. */
static int zlib_small_buffers(size_t src_len, int window_bits)
{
   unsigned char *p_src, *p_cmp, *p_dec;
   size_t cap = src_len + (src_len / 8) + 4096, cmp_len = 0, dec_len = 0;
   lzham_z_stream zs, zd;
   int status, result = 0;

   p_src = (unsigned char *)malloc(src_len);
   p_cmp = (unsigned char *)malloc(cap);
   p_dec = (unsigned char *)malloc(src_len);
   if (!p_src || !p_cmp || !p_dec)
   {
      free(p_src); free(p_cmp); free(p_dec);
      return 1;
   }
   fill(p_src, src_len, 1);

   memset(&zs, 0, sizeof(zs));
   status = lzham_z_deflateInit2(&zs, 6, LZHAM_Z_LZHAM, window_bits, 9, LZHAM_Z_DEFAULT_STRATEGY);
   if (status != LZHAM_Z_OK)
   {
      printf("FAIL: zlib deflateInit2 status %d\n", status);
      free(p_src); free(p_cmp); free(p_dec);
      return 1;
   }

   {
      size_t in_ofs = 0;
      for (;;)
      {
         size_t in_len = src_len - in_ofs;
         if (in_len > 4096)
            in_len = 4096;
         size_t out_len = cap - cmp_len;
         if (out_len > 1)
            out_len = 1;               /* force many tiny output calls */
         zs.next_in = p_src + in_ofs;
         zs.avail_in = (unsigned)in_len;
         zs.next_out = p_cmp + cmp_len;
         zs.avail_out = (unsigned)out_len;
         status = lzham_z_deflate(&zs, (in_ofs + in_len == src_len) ? LZHAM_Z_FINISH : LZHAM_Z_NO_FLUSH);
         in_ofs += in_len - zs.avail_in;
         cmp_len += out_len - zs.avail_out;
         if (status == LZHAM_Z_STREAM_END)
            break;
         if ((status != LZHAM_Z_OK) && (status != LZHAM_Z_BUF_ERROR))
         {
            printf("FAIL: zlib deflate status %d\n", status);
            result = 1;
            goto done;
         }
      }
   }
   lzham_z_deflateEnd(&zs);

   memset(&zd, 0, sizeof(zd));
   status = lzham_z_inflateInit2(&zd, window_bits);
   if (status != LZHAM_Z_OK)
   {
      printf("FAIL: zlib inflateInit2 status %d\n", status);
      result = 1;
      goto done;
   }

   {
      size_t in_ofs = 0;
      for (;;)
      {
         size_t in_len = cmp_len - in_ofs;
         if (in_len > 1)
            in_len = 1;
         size_t out_len = src_len - dec_len;
         if (out_len > 4096)
            out_len = 4096;
         zd.next_in = p_cmp + in_ofs;
         zd.avail_in = (unsigned)in_len;
         zd.next_out = p_dec + dec_len;
         zd.avail_out = (unsigned)out_len;
         /* Z_FINISH requires the whole remaining output to fit in next_out; stream with NO_FLUSH instead. */
         status = lzham_z_inflate(&zd, LZHAM_Z_NO_FLUSH);
         in_ofs += in_len - zd.avail_in;
         dec_len += out_len - zd.avail_out;
         if (status == LZHAM_Z_STREAM_END)
            break;
         if ((status != LZHAM_Z_OK) && (status != LZHAM_Z_BUF_ERROR))
         {
            printf("FAIL: zlib inflate status %d\n", status);
            result = 1;
            break;
         }
      }
   }

   if (!result && ((dec_len != src_len) || memcmp(p_src, p_dec, src_len) != 0))
   {
      printf("FAIL: zlib payload mismatch (%lu of %lu)\n", (unsigned long)dec_len, (unsigned long)src_len);
      result = 1;
   }

done:
   lzham_z_inflateEnd(&zd);
   free(p_src);
   free(p_cmp);
   free(p_dec);
   return result;
}

int main(void)
{
   static const size_t sizes[] = { 0, 1, 15, 4096, 65536, 262144 };
   static const size_t chunks[] = { 1, 7, 4096 };
   int failures = 0;
   size_t s, c;

   for (s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++)
   {
      for (c = 0; c < sizeof(chunks) / sizeof(chunks[0]); c++)
      {
         failures += streaming_roundtrip(sizes[s], 20, LZHAM_COMP_LEVEL_FASTEST, chunks[c], chunks[c], 0);
         failures += streaming_roundtrip(sizes[s], 15, LZHAM_COMP_LEVEL_UBER, chunks[c], chunks[c], 1);
      }
   }

   failures += reinit_larger_dict();
   failures += zlib_small_buffers(65536, LZHAM_Z_DEFAULT_WINDOW_BITS);
   failures += zlib_small_buffers(65536, -LZHAM_Z_DEFAULT_WINDOW_BITS);

   if (failures)
   {
      printf("%d test(s) FAILED\n", failures);
      return 1;
   }
   printf("All streaming tests passed.\n");
   return 0;
}
