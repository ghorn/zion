#include <iostream>
#include "src/common/stl.hpp"
#include "src/common/ply.hpp"

int main(int argc, char* argv[]) {
  if (argc != 3) {
    std::cerr << "Need exactly 2 arguments, input and output" << std::endl;
    exit(1);
  }
  const std::string input_path = argv[1];
  const std::string output_path = argv[2];
  std::vector<glm::vec3> points;
  std::vector<glm::ivec3> triangles;
  LoadPly(input_path, &points, &triangles);
  SaveBinarySTL(output_path, points, triangles);
}
