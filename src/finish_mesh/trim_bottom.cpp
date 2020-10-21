#include <string>
#include <getopt.h>
#include <cassert>
#include <iostream>
#include <fstream>
#include <vector>
#include <glm/glm.hpp>

#include "src/common/ply.hpp"

// Usage: ./trim_bottom inputpath outputpath
int32_t main(int32_t argc, char *argv[]) {
  // Parse flags.
  if (argc != 3) {
    fprintf(stderr, "Need 2 command line arguments, input and output\n");
    exit(1);
  }
  std::string input_path = argv[1];
  std::string output_path = argv[2];
  assert(input_path.size() != 0);
  assert(output_path.size() != 0);

  // Read inputs.
  std::vector<glm::vec3> vertices;
  std::vector<glm::ivec3> triangles;
  LoadPly(input_path, &vertices, &triangles);

  // Trim triangles
  std::vector<glm::ivec3> trimmed_triangles;
  for (const glm::ivec3 &triangle : triangles) {
    const float z0 = vertices[(uint32_t)triangle[0]].z;
    const float z1 = vertices[(uint32_t)triangle[1]].z;
    const float z2 = vertices[(uint32_t)triangle[2]].z;
    if (z0 != 0 || z1 != 0 || z2 != 0) {
      trimmed_triangles.emplace_back(triangle);
    }
  }

  // Write outputs.
  SavePly(output_path, vertices, trimmed_triangles);
}
