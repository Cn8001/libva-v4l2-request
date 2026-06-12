/*
 * C implementations of tiled-to-linear YUV conversions.
 *
 * tiled_to_planar (Sunxi 32x32 tiles) is a fallback used on architectures
 * without the NEON assembly version (tiled_yuv.S only builds on 32-bit ARM).
 *
 * sand_to_planar handles the Broadcom SAND128 layout (used by the Raspberry
 * Pi HEVC decoder): the plane is stored as columns of 128 bytes, with each
 * column holding col_stride / 128 consecutive lines.
 */

#include <stdint.h>
#include <string.h>

#include "tiled_yuv.h"

#ifdef __aarch64__

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

void sand_to_planar(void *src, void *dst, unsigned int dst_pitch,
		    unsigned int col_stride, unsigned int width,
		    unsigned int height)
{
	unsigned int i, j;

	for (i = 0; i < height; i++) {
		for (j = 0; j < width; j += 128) {
			unsigned int offset = (j / 128) * col_stride + i * 128;
			unsigned int length = (width - j) < 128 ?
					      (width - j) : 128;

			memcpy((uint8_t *)dst + i * dst_pitch + j,
			       (uint8_t *)src + offset, length);
		}
	}
}
