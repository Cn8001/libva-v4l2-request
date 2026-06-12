/*
 * C fallback implementation of the Sunxi 32x32 tiled NV12 untiling,
 * used on architectures without the NEON assembly version (tiled_yuv.S
 * only builds on 32-bit ARM).
 */

#ifdef __aarch64__

#include <stdint.h>
#include <string.h>

#include "tiled_yuv.h"

void tiled_to_planar(void *src, void *dst, unsigned int dst_pitch,
		     unsigned int width, unsigned int height)
{
	unsigned int tiles_per_row = (width + 31) / 32;
	unsigned int i, j;

	for (i = 0; i < height; i++) {
		for (j = 0; j < width; j += 32) {
			unsigned int tile = (i / 32) * tiles_per_row + (j / 32);
			unsigned int offset = tile * 32 * 32 + (i % 32) * 32;
			unsigned int length = (width - j) < 32 ? (width - j) : 32;

			memcpy((uint8_t *)dst + i * dst_pitch + j,
			       (uint8_t *)src + offset, length);
		}
	}
}

#endif
