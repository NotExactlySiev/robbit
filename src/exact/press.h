#ifndef _EXACT_H
#define _EXACT_H

#include <types.h>

// Compression methods used in EXACT Archive and compressed executables

// the decompressed size reported by the archive (how much to allocate)
u32 exact_get_decompressed_size(void *compressed_data);
// return the compressed size, rounded up? expects the buffer to be at
// least as big as the input data
int exact_decompress(void *decompressed_data, const void *compressed_data);
u32 exact_compress(void *compressed_data, const void *data, u32 data_size);

#endif
