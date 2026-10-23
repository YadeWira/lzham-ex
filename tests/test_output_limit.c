// test_output_limit.c - Regression test for upstream issue #19.
// LZHAM is in the Public Domain. Please see the Public Domain declaration at the end of include/lzham.h
//
// This test is linked against a build of the library whose vector limit was lowered to
// LZHAM_MAX_VECTOR_ELEMENTS (8 MB, see CMakeLists.txt), so that the 4 GB compressed-stream limit can be
// exercised without allocating gigabytes.
//
// Compressing more data than the limit can hold must fail cleanly with
// LZHAM_COMP_STATUS_FAILED_OUTPUT_LIMIT. Before the fix, the size counter wrapped around, the vector shrank
// its allocation and the following memcpy wrote past the end of the heap block.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lzham.h"

static unsigned g_seed = 0x12345678u;

static unsigned next_random(void)
{
   g_seed = g_seed * 1103515245u + 12345u;
   return (g_seed >> 16) ^ g_seed;
}

static int compress_buffer(size_t src_len, lzham_compress_status_t expected, size_t dst_len)
{
   unsigned char *p_src, *p_dst;
   lzham_compress_params params;
   lzham_compress_status_t status;
   size_t i;
   int result = 0;

   p_src = (unsigned char *)malloc(src_len);
   p_dst = (unsigned char *)malloc(dst_len);
   if (!p_src || !p_dst)
   {
      printf("FAIL: out of memory\n");
      free(p_src);
      free(p_dst);
      return 1;
   }

   /* Incompressible data, so the compressed stream is at least as large as the input. */
   for (i = 0; i < src_len; i++)
      p_src[i] = (unsigned char)next_random();

   memset(&params, 0, sizeof(params));
   params.m_struct_size = sizeof(params);
   params.m_dict_size_log2 = LZHAM_MIN_DICT_SIZE_LOG2;
   params.m_level = LZHAM_COMP_LEVEL_FASTEST;
   params.m_max_helper_threads = 1;

   status = lzham_compress_memory(&params, p_dst, &dst_len, p_src, src_len, NULL);
   if (status != expected)
   {
      printf("FAIL: %lu byte input returned status %d, expected %d\n",
             (unsigned long)src_len, (int)status, (int)expected);
      result = 1;
   }

   free(p_src);
   free(p_dst);
   return result;
}

int main(void)
{
   int failures = 0;

   /* A stream that fits in the lowered limit still has to work. */
   failures += compress_buffer(1u << 20, LZHAM_COMP_STATUS_SUCCESS, 2u << 20);

   /* A stream that cannot fit must be reported, not crash. */
   failures += compress_buffer(16u << 20, LZHAM_COMP_STATUS_FAILED_OUTPUT_LIMIT, 1u << 20);

   if (failures)
   {
      printf("%d test(s) FAILED\n", failures);
      return 1;
   }

   printf("Output limit handled correctly.\n");
   return 0;
}
