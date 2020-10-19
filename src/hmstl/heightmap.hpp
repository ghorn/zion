#pragma once

#include <inttypes.h>
#include <vector>
#include <string>

typedef struct {
  // xy dimensions (size = width * height)
  uint32_t width, height;
  uint64_t size;

  // z dimensions
  float min, max;

  // raster with size pixels ranging in value from min to max
  std::vector<float> data;

} Heightmap;

void ReadHeightmap(const std::string &path, Heightmap * const hm);
void DumpHeightmap(const Heightmap &hm);
