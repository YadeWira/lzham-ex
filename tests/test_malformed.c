// test_malformed.c - Regression tests for corrupt input.
// LZHAM is in the Public Domain. Please see the Public Domain declaration at the end of include/lzham.h
//
// Decompressing a corrupt stream must fail cleanly: no crash, no out-of-bounds access and no uninitialised
// data in the output. The data file is the input that AddressSanitizer flagged as a memcpy-param-overlap
// (a match distance larger than the bytes produced so far, at the start of the stream); the synthetic inputs
// cover other distances and the buffered path.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lzham.h"

static unsigned g_seed = 0x5EED5EEDu;

static unsigned next_random(void)
{
   g_seed = g_seed * 1103515245u + 12345u;
   return (g_seed >> 16) ^ g_seed;
}

/* Returns 1 if the stream decoded "successfully", 0 if it was rejected. The point is that it never crashes. */
static int try_decompress(const unsigned char *data, size_t size, unsigned dict_log2, int buffered)
{
   unsigned char out[4096];
   lzham_decompress_params params;
   lzham_decompress_state_ptr state;
   size_t in_ofs = 0;
   unsigned steps = 0;

   memset(&params, 0, sizeof(params));
   params.m_struct_size = sizeof(params);
   params.m_dict_size_log2 = dict_log2;
   params.m_decompress_flags = LZHAM_DECOMP_FLAG_COMPUTE_ADLER32;

   if (!buffered)
   {
      size_t out_len = sizeof(out);
      lzham_decompress_status_t st = lzham_decompress_memory(&params, out, &out_len, data, size, NULL);
      return st == LZHAM_DECOMP_STATUS_SUCCESS;
   }

   state = lzham_decompress_init(&params);
   if (!state)
      return 0;
   for (;;)
   {
      size_t in_len = size - in_ofs;
      size_t out_len = 1 + (steps % sizeof(out));
      lzham_decompress_status_t st;

      if (in_len > 512)
         in_len = 512;
      if (out_len > sizeof(out))
         out_len = sizeof(out);

      st = lzham_decompress(state, data + in_ofs, &in_len, out, &out_len,
                            (lzham_bool)((in_ofs + in_len) >= size));
      in_ofs += in_len;
      if (st == LZHAM_DECOMP_STATUS_SUCCESS)
      {
         lzham_decompress_deinit(state);
         return 1;
      }
      if (st >= LZHAM_DECOMP_STATUS_FIRST_SUCCESS_OR_FAILURE_CODE)
         break;
      if (++steps > 20000)
         break;
   }
   lzham_decompress_deinit(state);
   return 0;
}

int main(int argc, char **argv)
{
   static const unsigned dict_sizes[] = { 15, 16, 20, 26 };
   int failures = 0;
   unsigned i, b;
   unsigned char *junk;
   size_t junk_len = 0;

   if (argc > 1)
   {
      FILE *f = fopen(argv[1], "rb");
      unsigned char *data = NULL;
      long size = 0;

      if (!f)
      {
         printf("FAIL: cannot open %s\n", argv[1]);
         return 1;
      }
      fseek(f, 0, SEEK_END);
      size = ftell(f);
      fseek(f, 0, SEEK_SET);
      data = (unsigned char *)malloc((size_t)size);
      if (!data || (fread(data, 1, (size_t)size, f) != (size_t)size))
      {
         printf("FAIL: cannot read %s\n", argv[1]);
         fclose(f);
         free(data);
         return 1;
      }
      fclose(f);

      for (i = 0; i < sizeof(dict_sizes) / sizeof(dict_sizes[0]); i++)
      {
         for (b = 0; b < 2; b++)
         {
            if (try_decompress(data, (size_t)size, dict_sizes[i], (int)b))
            {
               printf("FAIL: malformed stream decoded successfully (dict=%u buffered=%u)\n", dict_sizes[i], b);
               failures++;
            }
         }
      }
      junk = data;
      junk_len = (size_t)size;
      free(data);
   }

   /* Synthetic garbage, including buffers that start like a stream header. */
   {
      unsigned char buf[2048];
      for (i = 0; i < sizeof(dict_sizes) / sizeof(dict_sizes[0]); i++)
      {
         unsigned trial;
         for (trial = 0; trial < 8; trial++)
         {
            size_t n = (trial == 0) ? 1 : (size_t)(next_random() % sizeof(buf));
            size_t k;
            for (k = 0; k < n; k++)
               buf[k] = (unsigned char)next_random();
            if (n > 1)
               buf[0] = 0x0E;                 /* first byte of the LZHAM zlib-style header */
            try_decompress(buf, n, dict_sizes[i], (int)(trial & 1));
         }
      }
   }

   (void)junk;
   (void)junk_len;

   if (failures)
   {
      printf("%d test(s) FAILED\n", failures);
      return 1;
   }
   printf("Malformed streams rejected without crashing.\n");
   return 0;
}
