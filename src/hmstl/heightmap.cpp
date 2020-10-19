#include <iostream>
#include <fstream>
#include <cmath>

#include "heightmap.hpp"
#include "src/common/heightmap_data.hpp"

void ScanHeightmap(Heightmap *hm) {
  if (hm == NULL) {
    fprintf(stderr, "heightmap is null\n");
    std::exit(1);
    return;
  }

  float min = 1e22f;
  float max = -1e22f;

  for (const float datum : hm->data) {
    if (std::isnan(datum)) {
        continue;
    }
    if (datum < min) {
      min = datum;
    }
    if (datum > max) {
      max = datum;
    }
  }

  hm->min = min;
  hm->max = max;
}

void ReadHeightmap(const std::string &path, Heightmap * const hm) {
  int32_t width, height;
  ReadHeightmapData(path, &width, &height, &(hm->data));

  hm->width = (uint32_t)width;
  hm->height = (uint32_t)height;
  hm->size = (uint64_t)width * (uint64_t)height;

  ScanHeightmap(hm);
}

void DumpHeightmap(const Heightmap &hm) {
  fprintf(stderr, "Width: %u\n", hm.width);
  fprintf(stderr, "Height: %u\n", hm.height);
  fprintf(stderr, "Pixels: %.2e\n", (double)hm.size);
  fprintf(stderr, "Min: %f\n", (double)hm.min);
  fprintf(stderr, "Max: %f\n", (double)hm.max);
  fprintf(stderr, "Range: %f\n", (double)hm.max - (double)hm.min);
}
