#include "ply.hpp"

#include <cstdlib>
#include <cstdio>

void WritePlyHeader(FILE * const output, const uint32_t vertex_count, const uint32_t triangle_count) {
  fprintf(output, "ply\r\n");
  fprintf(output, "format binary_little_endian 1.0\r\n");
  fprintf(output, "element vertex %u\r\n", vertex_count);
  fprintf(output, "property float x\r\n");
  fprintf(output, "property float y\r\n");
  fprintf(output, "property float z\r\n");
  fprintf(output, "element face %u\r\n", triangle_count);
  fprintf(output, "property list uchar uint vertex_indices\r\n");
  fprintf(output, "end_header\r\n");
}

void WriteTriangleHeader(FILE * const output) {
  // Write triangle header.
  const uint8_t three = 3; // This triangle will have three vertices, big surprise.
  if (fwrite(&three, 1, 1, output) != 1) {
    fprintf(stderr, "Error writing 'three' as uint8_t\n");
    std::exit(1);
  }
}

void WriteVertex(FILE * const output, const glm::vec3 &vertex) {
  static_assert(sizeof(glm::vec3) == 3*sizeof(float));
  // Write this new vertex to file.
  if (fwrite(&vertex, 4, 3, output) != 3) {
    fprintf(stderr, "Error writing vertex\n");
    std::exit(1);
  }
}

void WriteVertexIndex(FILE * const output, const uint32_t vertex_index) {
  static_assert(sizeof(vertex_index) == 4);
  // write the vertex index to the triangle file
  if (fwrite(&vertex_index, 4, 1, output) != 1) {
    fprintf(stderr, "Error writing vertex indexu\n");
    std::exit(1);
  }
}

void SavePly(const std::string &path,
             const std::vector<glm::vec3> &points,
             const std::vector<glm::ivec3> &triangles) {
  FILE *output = fopen(path.c_str(), "w");
  if (output == NULL) {
    fprintf(stderr, "Error opening output file %s.\n", path.c_str());
    exit(1);
  }

  WritePlyHeader(output, (uint32_t)points.size(), (uint32_t)triangles.size());

  // write vertex list
  for (const glm::vec3 & vertex : points) {
    WriteVertex(output, vertex);
  }

  // Write triangle list
  for (const glm::ivec3 & triangle : triangles) {
    WriteTriangleHeader(output);
    WriteVertexIndex(output, static_cast<uint32_t>(triangle[0]));
    WriteVertexIndex(output, static_cast<uint32_t>(triangle[1]));
    WriteVertexIndex(output, static_cast<uint32_t>(triangle[2]));
  }
}
