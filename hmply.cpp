#include <string.h>
#include <ctype.h>
#include <getopt.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <cmath>
#include <unordered_map>
#include <iostream>

#include "heightmap.hpp"

#define TRIX_FACE_MAX 4294967295U

typedef struct {
  int32_t base; // boolean; output walls and bottom as well as terrain surface if true
  char *input; // path to input file; use stdin if NULL
  char *output; // path to output file; use stdout if NULL
  float zscale; // scaling factor applied to raw Z values
  float baseheight; // height in STL units of base below lowest terrain (technically, offset added to scaled Z values)
} Settings;

Settings CONFIG = {
  1,    // generate base (walls and bottom)
  NULL, // read from stdin
  NULL, // write to stdout
  1.0,  // no z scaling (use raw heightmap values)
  1.0   // minimum base thickness of one unit
};

template <class T>
inline void hash_combine(std::size_t & seed, const T & v)
{
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

struct vertex_t {
  float x;
  float y;
  float z;
  bool operator==(const vertex_t &other) const {
    return
      x == other.x &&
      y == other.y &&
      z == other.z;
  }
};

namespace std {
  template <>
  struct hash<vertex_t>
  {
    std::size_t operator()(const vertex_t& vertex) const
    {
      //using std::size_t;
      //using std::hash;
      //using std::string;

      std::size_t seed = 0;
      hash_combine(seed, vertex.x);
      hash_combine(seed, vertex.y);
      hash_combine(seed, vertex.z);
      return seed;
//      // Compute individual hash values for first,
//      // second and third and combine them using XOR
//      // and bit shifting:
//      return ((hash<string>()(k.first)
//               ^ (hash<string>()(k.second) << 1)) >> 1)
//               ^ (hash<int>()(k.third) << 1);
    }
  };
}

struct triangle_t {
  vertex_t a;
  vertex_t b;
  vertex_t c;
};

//// hash function for vertices
//template<typename S, typename T> struct pair_hash<std::pair<S, T>>
//{
//    inline std::size_t operator()(const std::pair<S, T> & v) const
//    {
//         std::size_t seed = 0;
//         hash_combine(seed, v.first);
//         hash_combine(seed, v.second);
//         return seed;
//    }
//};
using vertex_map_t = std::unordered_map<vertex_t, uint32_t>;

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

static void WriteTriangle(const Pass pass,
                          FILE * const output,
                          vertex_map_t *const vmap,
                          uint32_t *const triangle_count,
                          const triangle_t &triangle) {
  const vertex_t vertices[3] = {triangle.a, triangle.b, triangle.c};
  switch (pass)
  {
  case Pass::kCountVerticesAndTriangles:
  {
    if (*triangle_count == TRIX_FACE_MAX) {
      printf("Too many triangles!!!\n");
      exit(1);
    }
    (*triangle_count)++;

    // Add each vertex to the hashmap if it doesn't exist.
    for (const vertex_t &vertex : vertices) {
      auto search = vmap->find(vertex);
      if (search == vmap->end()) {
        // Will insert vertex.

        // Check for size limit.
        if (vmap->size() == (size_t)TRIX_FACE_MAX) {
          printf("Too many verticestriangles!!!\n");
          exit(1);
        }

        // insert vertex to map
        vmap->insert({vertex, (uint32_t)(vmap->size())});
      }
    }
    break;
  }
  case Pass::kVertexList:
  {
    // Add each vertex to the hashmap if it doesn't exist.
    for (const vertex_t &vertex : vertices) {
      auto search = vmap->find(vertex);
      if (search == vmap->end()) {
        // Will insert vertex.

        // Check for size limit.
        if (vmap->size() == (size_t)TRIX_FACE_MAX) {
          printf("Too many verticestriangles!!!\n");
          exit(1);
        }

        // insert vertex to map
        vmap->insert({vertex, (uint32_t)(vmap->size())});
  
        // write vertex to list
        // vertex struct is 3 floats in sequence needed for output!
        if (fwrite(&vertex, 4, 3, output) != 3) {
          printf("Error writing vertex\n");
          exit(1);
        }
      }
    }
    break;
  }
  case Pass::kTriangleList:
  {
    const uint8_t three = 3;
    if (fwrite(&three, 1, 1, output) != 1) {
      printf("Error writing 'three' as uint8_t\n");
      exit(1);
    }

    for (const vertex_t &vertex : vertices) {
      auto search = vmap->find(vertex);
      assert(search != vmap.end());
      const uint32_t vertex_index = search->second;
      if (fwrite(&vertex_index, 4, 1, output) != 1) {
        printf("Error writing vertex index for triangle %u\n", *triangle_count);
        exit(1);
      }
    }
    break;
  }
  default:
  {
    assert(false);
    break;
  }
  }
}

static void Wall(const Pass pass,
                 FILE * const output,
                 vertex_map_t *const vmap,
                 uint32_t * const triangle_count,
                 const vertex_t &a,
                 const vertex_t &b) {
  vertex_t a0 = a;
  vertex_t b0 = b;
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
  WriteTriangle(pass, output, vmap, triangle_count, t1);
  WriteTriangle(pass, output, vmap, triangle_count, t2);
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

static inline float hmzat(const Heightmap &hm, uint32_t x, uint32_t y) {
  return CONFIG.baseheight + CONFIG.zscale * LookupIndex(hm, x, y);
}

// given four vertices and a mesh, add two triangles representing the quad with given corners
static void Surface(const Pass pass,
                    FILE * const output,
                    vertex_map_t *const vmap,
                    uint32_t * const triangle_count,
                    const vertex_t &v1,
                    const vertex_t &v2,
                    const vertex_t &v3,
                    const vertex_t &v4) {
  triangle_t i, j;

  i.a = v4;
  i.b = v2;
  i.c = v1;

  j.a = v4;
  j.b = v3;
  j.c = v2;

  WriteTriangle(pass, output, vmap, triangle_count, i);
  WriteTriangle(pass, output, vmap, triangle_count, j);
}

static void Mesh(const Heightmap &hm,
                 const Pass pass,
                 FILE *const output,
                 vertex_map_t * const vmap, 
                 uint32_t * const triangle_count) {
  uint32_t x, y;
  float az, bz, cz, dz, ez, fz, gz, hz;
  vertex_t vp, v1, v2, v3, v4;

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
        az = hmzat(hm, x - 1, y - 1);
      }

      if (y == 0) {
        bz = -1;
      } else {
        bz = hmzat(hm, x, y - 1);
      }

      if (y == 0 || x + 1 == hm.width) {
        cz = -1;
      } else {
        cz = hmzat(hm, x + 1, y - 1);
      }

      if (x + 1 == hm.width) {
        dz = -1;
      } else {
        dz = hmzat(hm, x + 1, y);
      }

      if (x + 1 == hm.width || y + 1 == hm.height) {
        ez = -1;
      } else {
        ez = hmzat(hm, x + 1, y + 1);
      }

      if (y + 1 == hm.height) {
        fz = -1;
      } else {
        fz = hmzat(hm, x, y + 1);
      }

      if (y + 1 == hm.height || x == 0) {
        gz = -1;
      } else {
        gz = hmzat(hm, x - 1, y + 1);
      }

      if (x == 0) {
        hz = -1;
      } else {
        hz = hmzat(hm, x - 1, y);
      }

      // pixel vertex
      vp.x = (float)x;
      vp.y = (float)(hm.height - y);
      vp.z = hmzat(hm, x, y);

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

      // Upper surface
      Surface(pass, output, vmap, triangle_count, v1, v2, v3, v4);

      // nothing left to do for this pixel unless we need to make walls
      if (!CONFIG.base) {
        continue;
      }

      // north wall (vertex 1 to 2)
      if (y == 0 || Masked(hm, x, y - 1)) {
        Wall(pass, output, vmap, triangle_count, v1, v2);
      }

      // east wall (vertex 2 to 3)
      if (x + 1 == hm.width || Masked(hm, x + 1, y)) {
        Wall(pass, output, vmap, triangle_count, v2, v3);
      }

      // south wall (vertex 3 to 4)
      if (y + 1 == hm.height || Masked(hm, x, y + 1)) {
        Wall(pass, output, vmap, triangle_count, v3, v4);
      }

      // west wall (vertex 4 to 1)
      if (x == 0 || Masked(hm, x - 1, y)) {
        Wall(pass, output, vmap, triangle_count, v4, v1);
      }

      // bottom surface - same as top, except with z = 0 and reverse winding
      v1.z = 0; v2.z = 0; v3.z = 0; v4.z = 0;
      Surface(pass, output, vmap, triangle_count, v4, v3, v2, v1);
    }
  }
}

static void WriteHeader(FILE * const output, const uint32_t vertex_count, const uint32_t triangle_count) {
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

static void HeightmapToPLY(const Heightmap &hm) {
  // Traverse the heightmap and count the triangles.
  uint32_t triangle_count = 0;
  FILE *output = NULL;
  vertex_map_t vmap;
  Mesh(hm, Pass::kCountVerticesAndTriangles, output, &vmap, &triangle_count);
  printf("mesh has %.2e triangles and %.2e vertices\n",
         (double)triangle_count, (double)vmap.size());

  // Open output file
  output = fopen(CONFIG.output, "w");
  if (output == NULL) {
    printf("Error opening output file\n");
    exit(1);
  }

  // Write header.
  WriteHeader(output, (uint32_t)vmap.size(), triangle_count);

  // Traverse the heightmap again but this time write it out to file.
  vmap.clear();
  Mesh(hm, Pass::kVertexList, output, &vmap, &triangle_count);
  Mesh(hm, Pass::kTriangleList, output, &vmap, &triangle_count);

  // Close output.
  fclose(output);
}

// https://www.gnu.org/software/libc/manual/html_node/Example-of-Getopt.html
// returns 0 if options are parsed successfully; nonzero otherwise
static int32_t parseopts(int32_t argc, char **argv) {

  int32_t c;

  // suppress automatic error messages generated by getopt
  opterr = 0;

  while ((c = getopt(argc, argv, "az:b:o:i:m:t:rhs")) != -1) {
    switch (c) {
    case 'z':
      // Z scale (heightmap value units relative to XY)
      if (sscanf(optarg, "%20f", &CONFIG.zscale) != 1 || CONFIG.zscale <= 0) {
        fprintf(stderr, "Z scale must be a number greater than 0.\n");
        return 1;
      }
      break;
    case 'b':
      // Base height (default 1.0 units)
      if (sscanf(optarg, "%20f", &CONFIG.baseheight) != 1 || CONFIG.baseheight < 1) {
        fprintf(stderr, "BASEHEIGHT must be a number greater than or equal to 1.\n");
        return 1;
      }
      break;
    case 'o':
      // Output file (default stdout)
      CONFIG.output = optarg;
      break;
    case 'i':
      // Input file (default stdin)
      CONFIG.input = optarg;
      break;
    case 's':
      // surface only mode - omit base (walls and bottom)
      CONFIG.base = 0;
      break;
    case '?':
      // unrecognized option OR missing option argument
      switch (optopt) {
      case 'z':
      case 'b':
      case 'i':
      case 'o':
      case 'm':
      case 't':
        fprintf(stderr, "Option -%c requires an argument.\n", optopt);
        break;
      default:
        if (isprint(optopt)) {
          fprintf(stderr, "Unknown option -%c\n", optopt);
        }
        else {
          fprintf(stderr, "Unknown option character \\x%x\n", optopt);
        }
        break;
      }
      return 1;
      break;
    default:
      // My understand is getopt will return ? for all unrecognized options,
      // so I'm not sure what other cases will be caught here. Perhaps just
      // options specified in optstring that I forget to handle above...
      fprintf(stderr, "Unexpected getopt result: %c\n", optopt);
      return 1;
      break;
    }
  }

  // should be no parameters left
  if (optind < argc) {
    fprintf(stderr, "Extraneous arguments\n");
    return 1;
  }

  if (CONFIG.input == NULL) {
    std::cout << "No input path set. Use -i to set input path." << std::endl;
    std::exit(1);
  }

  if (CONFIG.output == NULL) {
    std::cout << "No output path set. Use -o to set output path." << std::endl;
    std::exit(1);
  }

  return 0;
}

int32_t main(int32_t argc, char **argv) {
  if (parseopts(argc, argv)) {
    fprintf(stderr, "option parsing failed\n");
    return 1;
  }

  Heightmap hm{};
  ReadHeightmap(CONFIG.input, &hm);
  DumpHeightmap(hm);
  HeightmapToPLY(hm);

  return 0;
}
