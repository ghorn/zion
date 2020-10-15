#ifndef _HEIGHTMAP_H
#define _HEIGHTMAP_H

#include <inttypes.h>
#include <assert.h>

static_assert(sizeof(unsigned char) == sizeof(uint8_t), "whow");
static_assert(sizeof(unsigned int) == sizeof(uint32_t), "wow");
static_assert(sizeof(unsigned long) == sizeof(uint64_t), "wowie");

typedef struct {
  // xy dimensions (size = width * height)
  uint32_t width, height;
  uint64_t size;

  // z dimensions (range = max - min = relief)
  uint8_t min, max, range;

  // raster with size pixels ranging in value from min to max
  uint8_t *data;

} Heightmap;

Heightmap *ReadHeightmap(const char *path);
void FreeHeightmap(Heightmap **hm);
void DumpHeightmap(const Heightmap *hm);

#endif
