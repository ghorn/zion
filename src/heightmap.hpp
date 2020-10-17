#ifndef _HEIGHTMAP_H
#define _HEIGHTMAP_H

#include <inttypes.h>
#include <assert.h>
#include <vector>
#include <string>

static_assert(sizeof(unsigned char) == sizeof(uint8_t), "whow");
static_assert(sizeof(unsigned int) == sizeof(uint32_t), "wow");
static_assert(sizeof(unsigned long) == sizeof(uint64_t), "wowie");

typedef struct {
  // xy dimensions (size = width * height)
  uint32_t width, height;
  uint64_t size;

  // z dimensions (range = max - min = relief)
  float min, max, range;

  // raster with size pixels ranging in value from min to max
  std::vector<float> data;

} Heightmap;

void ReadHeightmap(const std::string &path, Heightmap * const hm);
void DumpHeightmap(const Heightmap &hm);

#endif
