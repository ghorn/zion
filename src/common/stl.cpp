#include "stl.hpp"

#define GLM_ENABLE_EXPERIMENTAL

#include <fstream>
#include <glm/gtx/normal.hpp>
#include <cstring>
#include <iostream>

void SaveBinarySTL(
    const std::string &path,
    const std::vector<glm::vec3> &points,
    const std::vector<glm::ivec3> &triangles)
{
    // TODO: properly handle endian-ness
    const uint64_t numBytes = triangles.size() * 50 + 84;
    char *dst = (char *)calloc(numBytes, 1);

    const uint32_t count = static_cast<uint32_t>(triangles.size());

    // Check for overflow. Quit if num triangles too big.
    if (triangles.size() != static_cast<size_t>(count)) {
      std::cerr << "Error: too many triangles to represent as uint32 (" << triangles.size() << ")" << std::endl;
      exit(1);
    }

    memcpy(dst + 80, &count, 4);

    for (uint32_t i = 0; i < triangles.size(); i++) {
        const glm::ivec3 t = triangles[i];
        const glm::vec3 p0 = points[static_cast<uint64_t>(t.x)];
        const glm::vec3 p1 = points[static_cast<uint64_t>(t.y)];
        const glm::vec3 p2 = points[static_cast<uint64_t>(t.z)];
        const glm::vec3 normal = glm::triangleNormal(p0, p1, p2);
        const uint64_t idx = 84 + i * 50;
        memcpy(dst + idx, &normal, 12);
        memcpy(dst + idx + 12, &p0, 12);
        memcpy(dst + idx + 24, &p1, 12);
        memcpy(dst + idx + 36, &p2, 12);
    }

    std::fstream file(path, std::ios::out | std::ios::binary);
    file.write(dst, static_cast<int64_t>(numBytes));
    file.close();

    free(dst);
}
