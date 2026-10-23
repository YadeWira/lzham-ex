// main.c - Minimal consumer of the installed LZHAM package.
// LZHAM is in the Public Domain. Please see the Public Domain declaration at the end of include/lzham.h
//
// Used by CI to verify that `find_package(lzham)` + `target_link_libraries(... lzham::lzham)` and the
// pkg-config file work on a real installed tree.

#include <stdio.h>
#include <string.h>

#include "lzham.h"

static int roundtrip(const char *src)
{
   const size_t src_len = strlen(src);
   unsigned char cmp[512];
   unsigned char dec[512];
   size_t cmp_len = sizeof(cmp);
   size_t dec_len = sizeof(dec);
   lzham_compress_params comp_params;
   lzham_decompress_params decomp_params;

   memset(&comp_params, 0, sizeof(comp_params));
   comp_params.m_struct_size = sizeof(comp_params);
   comp_params.m_dict_size_log2 = LZHAM_MIN_DICT_SIZE_LOG2;

   memset(&decomp_params, 0, sizeof(decomp_params));
   decomp_params.m_struct_size = sizeof(decomp_params);
   decomp_params.m_dict_size_log2 = LZHAM_MIN_DICT_SIZE_LOG2;
   decomp_params.m_decompress_flags = LZHAM_DECOMP_FLAG_COMPUTE_ADLER32;

   if (lzham_compress_memory(&comp_params, cmp, &cmp_len, (const lzham_uint8 *)src, src_len, NULL) != LZHAM_COMP_STATUS_SUCCESS)
      return 1;
   if (lzham_decompress_memory(&decomp_params, dec, &dec_len, cmp, cmp_len, NULL) != LZHAM_DECOMP_STATUS_SUCCESS)
      return 2;
   if (dec_len != src_len || memcmp(src, dec, dec_len) != 0)
      return 3;

   return 0;
}

int main(void)
{
   if (lzham_get_version() != LZHAM_DLL_VERSION)
      return 4;
   if (roundtrip("hello lzham consumer"))
      return 5;
   if (roundtrip(""))
      return 6;

   printf("lzham consumer ok (library version 0x%04X)\n", (unsigned)lzham_get_version());
   return 0;
}
