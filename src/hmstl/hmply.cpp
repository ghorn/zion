#include <cmath>
#include <cassert>
#include <chrono>
#include <unordered_map>
#include <glm/glm.hpp>

#include "heightmap.hpp"
#include "parse_args.hpp"
#include "src/common/hash.hpp"
#include "src/common/ply.hpp"

#define TRIX_FACE_MAX 4294967295U

struct Scale {
  bool generate_base;
  float x_scale;
  float y_scale;
  float z_scale;
  float z_offset;
};

static Scale ComputeScale(const Settings &config, const Heightmap &hm) {
  // make sure min heightmap is non-negative
  assert(hm.min >=0);

  Scale scale;
  scale.generate_base = config.generate_base;
  // if xy size is specified, compute scale to be applied to each xyz point
  const float max_height_width = (float)std::max(hm.height, hm.width);
  const float xyz_scale = config.xy_size <= 0 ? 1 : config.xy_size / max_height_width;
  scale.x_scale = xyz_scale * config.x_scale;
  scale.y_scale = xyz_scale * config.y_scale;
  scale.z_scale = xyz_scale * config.z_scale;
  // compute z offset from min/max height
  // relief == max - min
  // zoff / (relief + zoff) == frac
  // zoff == relief * frac / (1 - frac)
  scale.z_offset = scale.z_scale * (hm.max - hm.min) * config.baseheight_frac / (1 - config.baseheight_frac);

  //fprintf(stderr, "config.xy_size: %.3f\n", (double)config.xy_size);
  //fprintf(stderr, "max_height_width: %.3f\n", (double)max_height_width);
  //fprintf(stderr, "baseheight_frac: %.3f\n", (double)config.baseheight_frac);
  //fprintf(stderr, "xy_scale: %.3f\n", (double)scale.xy_scale);
  //fprintf(stderr, "z_scale: %.3f\n", (double)scale.z_scale);
  //fprintf(stderr, "z_offset: %.3f\n", (double)scale.z_offset);
  return scale;
}

struct triangle_t {
  glm::vec3 a;
  glm::vec3 b;
  glm::vec3 c;
};

using vertex_map_t = std::unordered_map<glm::vec3, uint32_t>;

static inline float LookupIndex(const Heightmap &hm, uint32_t x, uint32_t y) {
  return hm.data[(uint64_t)y * hm.width + (uint64_t)x];
}

// If a mask is defined, only portions of the heightmap that are visible through the mask are output.
// Bright areas of the mask image are considered transparent and dark areas are considered opaque.
static inline bool Masked(const Heightmap &hm, uint32_t x, uint32_t y) {
  return std::isnan(LookupIndex(hm, x, y));
}

enum class Pass {
  kCountVerticesAndTriangles,
  kVertexList,
  kTriangleList
};

static void WriteTriangle(FILE * const vertex_output,
                          FILE * const triangle_output,
                          vertex_map_t *const vmap,
                          uint32_t *const triangle_count,
                          const triangle_t &triangle) {
  const glm::vec3 vertices[3] = {triangle.a, triangle.b, triangle.c};

  if (*triangle_count == TRIX_FACE_MAX) {
    fprintf(stderr, "Too many triangles!!!\n");
    exit(1);
  }
  (*triangle_count)++;

  // Write triangle header.
  WriteTriangleHeader(triangle_output);

  // Add each vertex to the hashmap if it doesn't exist.
  for (const glm::vec3 &vertex : vertices) {
    auto search = vmap->find(vertex);
    uint32_t vertex_index = 0;
    if (search == vmap->end()) {
      // This is a new vertex, write it to the vertex output and insert it to the hashmap.
      // Check for size limit.
      if (vmap->size() == (size_t)TRIX_FACE_MAX) {
        fprintf(stderr, "Too many vertices!!!\n");
        exit(1);
      }

      // Write this new vertex to file.
      WriteVertex(vertex_output, vertex);

      // Insert vertex into hashmap
      vertex_index = static_cast<uint32_t>(vmap->size());
      vmap->insert({vertex, vertex_index});
    } else {
      vertex_index = search->second;
    }

    // write the vertex index to the triangle file
    WriteVertexIndex(triangle_output, vertex_index);
  }
}

static void Wall(FILE * const vertex_output,
                 FILE * const triangle_output,
                 vertex_map_t *const vmap,
                 uint32_t * const triangle_count,
                 const glm::vec3 &a,
                 const glm::vec3 &b) {
  glm::vec3 a0 = a;
  glm::vec3 b0 = b;
  triangle_t t1;
  triangle_t t2;
  a0.z = 0;
  b0.z = 0;
  t1.a = a;
  t1.b = b;
  t1.c = b0;
  t2.a = b0;
  t2.b = a0;
  t2.c = a;
  WriteTriangle(vertex_output, triangle_output, vmap, triangle_count, t1);
  WriteTriangle(vertex_output, triangle_output, vmap, triangle_count, t2);
}

// returns average of all non-negative arguments.
// If any argument is negative, it is not included in the average.
// argument zp will always be nonnegative.
static float avgnonneg(const float z0, const float z1, const float z2, const float z3) {
  float sum = 0;
  const float z[4] = {z0, z1, z2, z3};
  int32_t n = 0;
  for (int32_t i = 0; i < 4; i++) {
    if (z[i] >= 0) {
      sum += z[i];
      n++;
    }
  }

  assert(n > 0);
  return sum / (float)n;
}

static inline float hmzat(const Heightmap &hm, uint32_t x, uint32_t y, const Scale &scale) {
  return scale.z_offset + scale.z_scale * LookupIndex(hm, x, y);
}

// given four vertices and a mesh, add two triangles representing the quad with given corners
static void Surface(FILE * const vertex_output,
                    FILE * const triangle_output,
                    vertex_map_t *const vmap,
                    uint32_t * const triangle_count,
                    const glm::vec3 &v1,
                    const glm::vec3 &v2,
                    const glm::vec3 &v3,
                    const glm::vec3 &v4) {
  triangle_t i, j;

  i.a = v4;
  i.b = v2;
  i.c = v1;

  j.a = v4;
  j.b = v3;
  j.c = v2;

  WriteTriangle(vertex_output, triangle_output, vmap, triangle_count, i);
  WriteTriangle(vertex_output, triangle_output, vmap, triangle_count, j);
}

static void Mesh(const Heightmap &hm,
                 FILE *const vertex_output,
                 FILE *const triangle_output,
                 vertex_map_t * const vmap,
                 uint32_t * const triangle_count,
                 const Scale &scale) {
  uint32_t x, y;
  float az, bz, cz, dz, ez, fz, gz, hz;
  glm::vec3 vp, v1, v2, v3, v4;

  for (y = 0; y < hm.height; y++) {
    for (x = 0; x < hm.width; x++) {

      if (Masked(hm, x, y)) {
        continue;
      }

      /*

        +---+---+---+
        |   |   |   |
        | A | B | C |
        |   |   |   |
        +---1---2---+
        |   |I /|   |
        | H | P | D |
        |   |/ J|   |
        +---4---3---+
        |   |   |   |
        | G | F | E |
        |   |   |   |
        +---+---+---+

        Current pixel position is marked at center as P.
        This pixel is output as two triangles, I and J.
        Points 1, 2, 3, and 4 are offset half a unit from P.
        Neighboring pixels are A, B, C, D, E, F, G, and H.

        Vertex 1 z is average of ABPH z
        Vertex 2 z is average of BCDP z
        Vertex 3 z is average of PDEF z
        Vertex 4 z is average of HPFG z

        Averages do not include neighbors that would lie
        outside the image, but do included masked values.

      */

      // determine elevation of neighboring pixels in order to
      // to interpolate height of corners 1, 2, 3, and 4.
      // -1 is used to flag edge pixels to disregard.
      // (Masked neighbors are still considered.)

      if (x == 0 || y == 0) {
        az = -1;
      } else {
        az = hmzat(hm, x - 1, y - 1, scale);
      }

      if (y == 0) {
        bz = -1;
      } else {
        bz = hmzat(hm, x, y - 1, scale);
      }

      if (y == 0 || x + 1 == hm.width) {
        cz = -1;
      } else {
        cz = hmzat(hm, x + 1, y - 1, scale);
      }

      if (x + 1 == hm.width) {
        dz = -1;
      } else {
        dz = hmzat(hm, x + 1, y, scale);
      }

      if (x + 1 == hm.width || y + 1 == hm.height) {
        ez = -1;
      } else {
        ez = hmzat(hm, x + 1, y + 1, scale);
      }

      if (y + 1 == hm.height) {
        fz = -1;
      } else {
        fz = hmzat(hm, x, y + 1, scale);
      }

      if (y + 1 == hm.height || x == 0) {
        gz = -1;
      } else {
        gz = hmzat(hm, x - 1, y + 1, scale);
      }

      if (x == 0) {
        hz = -1;
      } else {
        hz = hmzat(hm, x - 1, y, scale);
      }

      // pixel vertex
      vp.x = (float)x;
      vp.y = (float)(hm.height - y);
      vp.z = hmzat(hm, x, y, scale);

      // Vertex 1
      v1.x = (float)x - 0.5f;
      v1.y = ((float)hm.height - ((float)y - 0.5f));
      v1.z = avgnonneg(az, bz, vp.z, hz);
      // Vertex 2
      v2.x = (float)x + 0.5f;
      v2.y = v1.y;
      v2.z = avgnonneg(bz, cz, dz, vp.z);

      // Vertex 3
      v3.x = v2.x;
      v3.y = ((float)hm.height - ((float)y + 0.5f));
      v3.z = avgnonneg(vp.z, dz, ez, fz);

      // Vertex 4
      v4.x = v1.x;
      v4.y = v3.y;
      v4.z = avgnonneg(hz, vp.z, fz, gz);

      // Scale XY coordinates.
      for (glm::vec3 *vert : {&vp, &v1, &v2, &v3, &v4}) {
        vert->x *= scale.x_scale;
        vert->y *= scale.y_scale;
      }

      // Upper surface
      Surface(vertex_output, triangle_output, vmap, triangle_count, v1, v2, v3, v4);

      // nothing left to do for this pixel unless we need to make walls
      if (!scale.generate_base) {
        continue;
      }

      // north wall (vertex 1 to 2)
      if (y == 0 || Masked(hm, x, y - 1)) {
        Wall(vertex_output, triangle_output, vmap, triangle_count, v1, v2);
      }

      // east wall (vertex 2 to 3)
      if (x + 1 == hm.width || Masked(hm, x + 1, y)) {
        Wall(vertex_output, triangle_output, vmap, triangle_count, v2, v3);
      }

      // south wall (vertex 3 to 4)
      if (y + 1 == hm.height || Masked(hm, x, y + 1)) {
        Wall(vertex_output, triangle_output, vmap, triangle_count, v3, v4);
      }

      // west wall (vertex 4 to 1)
      if (x == 0 || Masked(hm, x - 1, y)) {
        Wall(vertex_output, triangle_output, vmap, triangle_count, v4, v1);
      }

      // bottom surface - same as top, except with z = 0 and reverse winding
      v1.z = 0; v2.z = 0; v3.z = 0; v4.z = 0;
      Surface(vertex_output, triangle_output, vmap, triangle_count, v4, v3, v2, v1);
    }
  }
}

static void HeightmapToPLY(const Heightmap &hm,
                           const Scale &scale) {
  // Open output files
  FILE *vertex_output = fopen("vertices.dat", "w");
  if (vertex_output == NULL) {
    fprintf(stderr, "Error opening vertex output file\n");
    exit(1);
  }

  FILE *triangle_output = fopen("triangles.dat", "w");
  if (triangle_output == NULL) {
    fprintf(stderr, "Error opening triangle output file\n");
    exit(1);
  }

  // Traverse the heightmap and count the triangles.
  vertex_map_t vmap;
  uint32_t triangle_count = 0;
  auto t0 = std::chrono::steady_clock::now();
  Mesh(hm, vertex_output, triangle_output, &vmap, &triangle_count, scale);
  auto t1 = std::chrono::steady_clock::now();
  fprintf(stderr, "Meshed in %.2f s\n", std::chrono::duration<double>(t1-t0).count());
  fprintf(stderr, "mesh has %.2e triangles and %.2e vertices\n",
         (double)triangle_count, (double)vmap.size());
  fclose(vertex_output);
  fclose(triangle_output);

  // Open header file
  FILE *header_output = fopen("header.dat", "w");
  if (header_output == NULL) {
    fprintf(stderr, "Error opening output file\n");
    exit(1);
  }

  // Write header.
  WritePlyHeader(header_output, (uint32_t)vmap.size(), triangle_count);

  // Close output.
  fclose(header_output);
}


int32_t main(int32_t argc, char **argv) {
  const Settings config = ParseArgs(argc, argv);
  Heightmap hm{};
  auto t0 = std::chrono::steady_clock::now();
  ReadHeightmap(config.input, &hm);
  DumpHeightmap(hm);
  auto t1 = std::chrono::steady_clock::now();
  fprintf(stderr, "Read heightmap in %.2f s\n", std::chrono::duration<double>(t1-t0).count());
  const Scale scale = ComputeScale(config, hm);
  HeightmapToPLY(hm, scale);

  return 0;
}
