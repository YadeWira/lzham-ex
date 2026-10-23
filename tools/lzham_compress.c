/* lzham_compress.c - Compress one file into a raw LZHAM stream using the memory API.
 *
 * This is the companion of tools/lzham_ref.py: it produces exactly the stream format that the reference
 * decoder documents (no container, no zlib wrapper), which makes it easy to check a decoder implementation
 * against this codec.
 *
 * Usage: lzham_compress <input> <output> [dict_size_log2] [level]
 *        dict_size_log2  15..26 (default 20)
 *        level           0=fastest .. 4=uber (default 4)
 *
 * LZHAM is in the Public Domain. See LICENSE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lzham.h"

int main(int argc, char **argv)
{
   FILE *pIn, *pOut;
   unsigned char *pSrc, *pDst;
   long src_len;
   size_t dst_len, buf_len;
   lzham_compress_params params;
   lzham_compress_status_t status;

   if (argc < 3)
   {
      printf("usage: %s <input> <output> [dict_size_log2] [level]\n", argv[0]);
      return 2;
   }

   pIn = fopen(argv[1], "rb");
   if (!pIn)
   {
      printf("cannot open %s\n", argv[1]);
      return 2;
   }
   if ((fseek(pIn, 0, SEEK_END) != 0) || ((src_len = ftell(pIn)) < 0) || (fseek(pIn, 0, SEEK_SET) != 0))
   {
      printf("cannot size %s\n", argv[1]);
      fclose(pIn);
      return 2;
   }

   pSrc = (unsigned char *)malloc(src_len ? (size_t)src_len : 1);
   /* The bound used by tools/lzham_ref.py tests: input + 12.5% + 64 KB. */
   buf_len = (size_t)src_len + (size_t)src_len / 8 + 65536;
   pDst = (unsigned char *)malloc(buf_len);
   if (!pSrc || !pDst)
   {
      printf("out of memory\n");
      free(pSrc);
      free(pDst);
      fclose(pIn);
      return 2;
   }

   if (fread(pSrc, 1, (size_t)src_len, pIn) != (size_t)src_len)
   {
      printf("cannot read %s\n", argv[1]);
      free(pSrc);
      free(pDst);
      fclose(pIn);
      return 2;
   }
   fclose(pIn);

   memset(&params, 0, sizeof(params));
   params.m_struct_size = sizeof(params);
   params.m_dict_size_log2 = (argc > 3) ? (lzham_uint32)atoi(argv[3]) : 20;
   params.m_level = (argc > 4) ? (lzham_compress_level)atoi(argv[4]) : LZHAM_COMP_LEVEL_UBER;
   /* Deterministic output: no helper threads. */
   params.m_max_helper_threads = 0;

   dst_len = buf_len;
   status = lzham_compress_memory(&params, pDst, &dst_len, pSrc, (size_t)src_len, NULL);
   if (status != LZHAM_COMP_STATUS_SUCCESS)
   {
      printf("compression failed: status %d\n", (int)status);
      free(pSrc);
      free(pDst);
      return 1;
   }

   pOut = fopen(argv[2], "wb");
   if (!pOut)
   {
      printf("cannot create %s\n", argv[2]);
      free(pSrc);
      free(pDst);
      return 2;
   }
   if (fwrite(pDst, 1, dst_len, pOut) != dst_len)
   {
      printf("cannot write %s\n", argv[2]);
      fclose(pOut);
      free(pSrc);
      free(pDst);
      return 2;
   }
   fclose(pOut);

   printf("%ld -> %lu bytes (dict %u, level %d)\n", src_len, (unsigned long)dst_len,
          (unsigned)params.m_dict_size_log2, (int)params.m_level);

   free(pSrc);
   free(pDst);
   return 0;
}
