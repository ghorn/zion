#include <string>
#include <vector>
#include <glm/glm.hpp>

#include "src/common/ply.hpp"

static float Size(const std::vector<glm::vec3> &vertices) {
  float min_x = vertices.at(0).x;
  float max_x = vertices.at(0).x;
  float min_y = vertices.at(0).y;
  float max_y = vertices.at(0).y;
  for (const glm::vec3 &vertex : vertices) {
    min_x = std::min(min_x, vertex.x);
    max_x = std::max(max_x, vertex.x);
    min_y = std::min(min_y, vertex.y);
    max_y = std::max(max_y, vertex.y);
  }

  const float x_size = max_x - min_x;
  const float y_size = max_y - min_y;
  const float size = std::max(x_size, y_size);

  // fprintf(stderr, "min_x %.3f, max_x %.3f, x size %.3f\n", (double)min_x, (double)max_x, (double)x_size);
  // fprintf(stderr, "min_y %.3f, max_y %.3f, y size %.3f\n", (double)min_y, (double)max_y, (double)y_size);
  // fprintf(stderr, "size %.3f\n", (double)size);

  return size;
}

// Usage: ./scale max_dim inputpath outputpath
int32_t main(int32_t argc, char *argv[]) {
  // Parse flags.
  if (argc != 4) {
    fprintf(stderr, "Need 3 command line arguments: max xy size, input, and output\n");
    exit(1);
  }
  const float max_xy = std::stof(argv[1]);
  const std::string input_path = argv[2];
  const std::string output_path = argv[3];
  assert(input_path.size() != 0);
  assert(output_path.size() != 0);

  fprintf(stderr, "Desired max size: %.3f\n", (double)max_xy);

  // Read inputs.
  std::vector<glm::vec3> vertices;
  std::vector<glm::ivec3> triangles;
  LoadPly(input_path, &vertices, &triangles);

  // Get min/max dimensions.
  const float initial_size = Size(vertices);

  // Scale vertices.
  const float scale = max_xy / initial_size;
  fprintf(stderr, "scale factor: %.5f\n", (double)scale);
  for (glm::vec3 &vertex : vertices) {
    vertex.x *= scale;
    vertex.y *= scale;
    vertex.z *= scale;
  }

  // Write outputs.
  SavePly(output_path, vertices, triangles);
}
