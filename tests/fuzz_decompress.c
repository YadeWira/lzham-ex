// fuzz_decompress.c - libFuzzer target for the decompressor.
// LZHAM is in the Public Domain. Please see the Public Domain declaration at the end of include/lzham.h
//
// Feeds arbitrary bytes to the decompressor through the memory API and, for a subset of the inputs, through
// the streaming API with tiny buffers. It must always return an error status, never crash or read/write out of
// bounds (run it with -fsanitize=fuzzer,address,undefined).

#include <stdlib.h>
#include <string.h>

#include "lzham.h"

int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size)
{
   static const unsigned dict_sizes[] = { 15, 16, 20, 26 };
   unsigned char *out;
   unsigned i;

   out = (unsigned char *)malloc(1u << 20);
   if (!out)
      return 0;

   for (i = 0; i < sizeof(dict_sizes) / sizeof(dict_sizes[0]); i++)
   {
      lzham_decompress_params params;
      size_t out_len = 1u << 20;

      memset(&params, 0, sizeof(params));
      params.m_struct_size = sizeof(params);
      params.m_dict_size_log2 = dict_sizes[i];
      params.m_decompress_flags = LZHAM_DECOMP_FLAG_COMPUTE_ADLER32;
      lzham_decompress_memory(&params, out, &out_len, data, size, NULL);
   }

   /* Occasionally also drive the streaming API (buffered) and the zlib-compatible entry points. */
   if (size && ((size % 7) == 0))
   {
      lzham_decompress_params params;
      lzham_decompress_state_ptr state;
      size_t in_ofs = 0;

      memset(&params, 0, sizeof(params));
      params.m_struct_size = sizeof(params);
      params.m_dict_size_log2 = 15 + (data[0] % 12);
      state = lzham_decompress_init(&params);
      if (state)
      {
         unsigned steps = 0;
         while ((in_ofs < size) && (steps++ < 1000))
         {
            size_t in_len = size - in_ofs;
            size_t out_len = 1 + (steps % 64);
            lzham_decompress_status_t st;
            if (in_len > 256)
               in_len = 1 + (steps % 256);
            st = lzham_decompress(state, data + in_ofs, &in_len, out, &out_len,
                                  (lzham_bool)((in_ofs + in_len) >= size));
            in_ofs += in_len;
            if (st >= LZHAM_DECOMP_STATUS_FIRST_SUCCESS_OR_FAILURE_CODE)
               break;
         }
         lzham_decompress_deinit(state);
      }

      if (((size & 15) == 0))
      {
         lzham_z_ulong dst_len = 1u << 20;
         lzham_z_uncompress(out, &dst_len, data, (lzham_z_ulong)size);
      }
   }

   free(out);
   return 0;
}
