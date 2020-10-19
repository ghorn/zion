#include "src/common/heightmap_data.hpp"

#include <fstream>

void ReadHeightmapData(const std::string &path, int32_t *nx, int32_t *ny, std::vector<float> *image) {
  std::ifstream image_file(path, std::ios::in|std::ios::binary);
  if (!image_file.is_open()) {
    fprintf(stderr, "Failed to open image.\n");
    std::exit(1);
  }

  image_file.read(reinterpret_cast<char*>(ny), 4);
  image_file.read(reinterpret_cast<char*>(nx), 4);
  fprintf(stderr, "Reading (%d x %d) doubles...\n", *nx, *ny);

  const size_t num_floats = static_cast<size_t>(*nx) * static_cast<size_t>(*ny);
  image->resize(num_floats, 0);
  image_file.read(reinterpret_cast<char*>(image->data()), 4*static_cast<int64_t>(num_floats));

  if (!image_file) {
    fprintf(stderr, "error: only %lu bytes could be read of %s\n", image_file.gcount(), path.c_str());
    std::exit(1);
  }
  image_file.close();
}

