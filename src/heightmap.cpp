#include <iostream>
#include <fstream>
#include <cmath>

#include "heightmap.hpp"

void ScanHeightmap(Heightmap *hm) {
  if (hm == NULL) {
    std::cout << "heightmap is null" << std::endl;
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

void ReadHeightmapData(const std::string &path, int64_t *nx, int64_t *ny, std::vector<float> *image) {
  std::ifstream image_file(path, std::ios::in|std::ios::binary);
  if (!image_file.is_open()) {
    std::cout << "Failed to open image." << std::endl;
    std::exit(1);
  }

  image_file.read(reinterpret_cast<char*>(ny), 8);
  image_file.read(reinterpret_cast<char*>(nx), 8);
  std::cout << "Reading (" << *nx << " x " << *ny << ") doubles..." << std::endl;

  image->resize(static_cast<uint64_t>(*nx) * static_cast<uint64_t>(*ny), 0);
  image_file.read(reinterpret_cast<char*>(image->data()), (*nx)*(*ny)*4);

  if (!image_file) {
    std::cout << "error: only " << image_file.gcount() << " bytes could be read" << std::endl;
    std::exit(1);
  }
  image_file.close();
}

void ReadHeightmap(const std::string &path, Heightmap * const hm) {
  int64_t width, height;
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
