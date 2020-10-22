#include <string>
#include <getopt.h>
#include <cassert>
#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>

#include <finish_mesh/earcut.hpp>
#include "src/common/hash.hpp"
#include "src/common/ply.hpp"


static void GetBottomEdges(const std::vector<glm::vec3> &vertices,
                           const std::vector<glm::ivec3> &triangles,
                           std::vector<glm::ivec2> *bottom_edges) {
  bottom_edges->clear();
  for (const glm::ivec3 &triangle : triangles) {
    glm::ivec2 edge;
    uint64_t num_zero_z = 0;
    // Look at the three vertices in the triangle.
    // If two of them have z==0 then it's a bottom edge.
    for (int k = 0; k < 3; k++) {
      const int vertex_index = triangle[k];
      const glm::vec3 vertex = vertices.at((uint64_t)vertex_index);
      if (vertex.z == 0) {
        if (num_zero_z < 2) {
          edge[(int)num_zero_z] = vertex_index;
        }
        num_zero_z++;
      }
    }
    if (num_zero_z == 2) {
      bottom_edges->emplace_back(edge);
    }
  }
}

template <class K, class V>
static void InsertOrAppend(std::unordered_map<K, std::vector<V> > *map,
                    const K &key,
                    const V &value) {
  // see if the key is in the map yet
  auto search = map->find(key);
  if (search == map->end()) {
    // if it's not in the map, insert it in a singleton vector
    std::vector<V> singleton;
    singleton.push_back(value);
    map->insert({key, singleton});
  } else {
    // if it's already in the map, append to the vector
    //std::vector<V> &vec = map->at(key);
    //vec.emplace_back(value);
    search->second.emplace_back(value);
  }
}

static std::vector<glm::ivec2> FilterEdge(const glm::ivec2 current_edge,
                                          const std::vector<glm::ivec2> &connected_edges) {
  std::vector<glm::ivec2> filtered;
  for (const glm::ivec2 &candidate : connected_edges) {
    if (candidate != current_edge) {
      filtered.push_back(candidate);
    }
  }
  return filtered;
}

static int OtherNode(const glm::ivec2 edge, const int node) {
  if (edge[0] == node) {
    return edge[1];
  }
  if (edge[1] == node) {
    return edge[0];
  }

  fprintf(stderr, "Error in OtherNode: edge (%d, %d) does not contain node %d\n", edge[0], edge[1], node);
  std::exit(1);
}

static inline glm::vec2 ToXy(const glm::vec3 v3) {
  return {v3.x, v3.y};
}

static glm::vec2 IntoNode(const glm::ivec2 edge,
                          const int node,
                          const std::vector<glm::vec3> &vertices) {
  return ToXy(vertices.at((uint64_t)node)) - ToXy(vertices.at((uint64_t)OtherNode(edge, node)));
}

static glm::vec2 Normalize(const glm::vec2 &vec) {
  const float x = vec.x;
  const float y = vec.y;
  const float mag = sqrtf(x * x + y * y);
  if (mag == 0) {
    fprintf(stderr, "Normalize called on zero-magnitude vector\n");
    std::exit(1);
  }
  return vec / mag;
}


enum class Winding {
  kRight,
  kLeft
};

// Select the correct next edge edge when there are multiple choices.
// Looks like:
//
// ---1----x---5---x
//        / \
//       2   4
//      /     \
//     x---3---x
//
// 1 - previous edge
// 2,4,5 - candidate edges connected to common node
// 3 - hidden edge connected 2,4
static glm::ivec2 ResolveNonmanifoldVertex(const int current_node,
                                           const glm::ivec2 previous_edge,
                                           const std::vector<glm::ivec2> &connected_edges,
                                           //const glm::ivec2 edge2,
                                           //const glm::ivec2 edge4,
                                           //const glm::ivec2 edge5,
                                           const std::vector<glm::vec3> &vertices,
                                           const Winding winding) {

  const glm::vec2 v0 = Normalize(IntoNode(previous_edge, current_node, vertices));
  glm::ivec2 leftmost;
  float best_angle;
  for (uint64_t k = 0; k < connected_edges.size(); k++) {
    const glm::ivec2 candidate_edge = connected_edges.at(k);
    const glm::vec2 vk = -Normalize(IntoNode(candidate_edge, current_node, vertices));
    float angle = acosf(glm::dot(v0, vk));
    const float cross_product = v0.x*vk.y - v0.y*vk.x;
    if (cross_product > 0) {
      angle *= -1;
    }
    if (winding == Winding::kLeft) {
      angle *= -1;
    }
    if ((k == 0) || angle < best_angle) {
      best_angle = angle;
      leftmost = candidate_edge;
    }
  }
  return leftmost;
}

static glm::ivec2 ChooseNextEdge(const int current_node,
                                 const glm::ivec2 previous_edge,
                                 const std::vector<glm::ivec2> &connected_edges,
                                 const std::vector<glm::vec3> &vertices,
                                 const Winding winding) {
  // Usually there is only one edge left.
  // Ez.
  if (connected_edges.size() == 1) {
    return connected_edges.at(0);
  }

  if (connected_edges.size() == 3) {
    //fprintf(stderr, "Oh shit, three connected edges [(%d, %d), (%d, %d), (%d, %d)].",
    //        connected_edges.at(0)[0],
    //        connected_edges.at(0)[1],
    //        connected_edges.at(1)[0],
    //        connected_edges.at(1)[1],
    //        connected_edges.at(2)[0],
    //        connected_edges.at(2)[1]);
    //
    //fprintf(stderr, " That's a non-manifold vertex. Time to throw a hail mary...\n");

    // try to filter the connected_edges
    return ResolveNonmanifoldVertex(current_node,
                                    previous_edge,
                                    connected_edges,
                                    //connected_edges.at(0),
                                    //connected_edges.at(1),
                                    //connected_edges.at(2),
                                    vertices,
                                    winding);
  }

  fprintf(stderr, "Got a weird and unhandled number of candidate edges %lu.\n", connected_edges.size());
  std::exit(1);
}

static void EraseEdgeFromNode(std::unordered_map<int, std::vector<glm::ivec2> > *edge_pool,
                              const glm::ivec2 &edge,
                              const int node) {
  auto it = edge_pool->find(node);
  if (it == edge_pool->end()) {
    fprintf(stderr, "Error! Can't find node (%d) to erase!\n", node);
    std::exit(1);
  }

  edge_pool->insert_or_assign(node, FilterEdge(edge, it->second));
}

static void EraseEdge(std::unordered_map<int, std::vector<glm::ivec2> > *edge_pool, const glm::ivec2 &edge) {
  // The edge will be here twice, once for each node.
  EraseEdgeFromNode(edge_pool, edge, edge[0]);
  EraseEdgeFromNode(edge_pool, edge, edge[1]);
}

static std::vector<int> SortEdgesTurningOneWay(const std::vector<glm::ivec2> &bottom_edges,
                                               const std::vector<glm::vec3> &vertices,
                                               const Winding winding) {
  // Edge pool is a map from each vertex to (initially) the two edges which containt that vertex.
  std::unordered_map<int, std::vector<glm::ivec2> > edge_pool;
  for (const glm::ivec2 &edge : bottom_edges) {
    InsertOrAppend(&edge_pool, edge[0], edge);
    InsertOrAppend(&edge_pool, edge[1], edge);
  }

  std::vector<int> sorted_nodes;

  // Initialize with any old edge. Grab the first one.
  glm::ivec2 previous_edge = edge_pool.begin()->second.back();
  int current_node = previous_edge[0];
  EraseEdge(&edge_pool, previous_edge);
  sorted_nodes.push_back(current_node);

  while (true) {
    // Get all edges connected to current node.
    // Does not include edges already visited because they've been deleted from the pool.
    const std::vector<glm::ivec2> connected_edges = edge_pool.at(current_node);
    if (connected_edges.empty()) {
      break;
    }

    // Choose next edge from among edges connected to this node.
    const glm::ivec2 next_edge =
      ChooseNextEdge(current_node, previous_edge, connected_edges, vertices, winding);
    EraseEdge(&edge_pool, next_edge);

    // Get the node on the other side of this edge.
    const int next_node = OtherNode(next_edge, current_node);

    previous_edge = next_edge;
    current_node = next_node;
    sorted_nodes.push_back(current_node);
  }

  return sorted_nodes;
}

static std::vector<int> SortEdges(const std::vector<glm::ivec2> &bottom_edges,
                                  const std::vector<glm::vec3> &vertices) {
  const std::vector<int> left_sorted_nodes = SortEdgesTurningOneWay(bottom_edges, vertices, Winding::kLeft);
  if (left_sorted_nodes.size() == bottom_edges.size()) {
    return left_sorted_nodes;
  }

  const std::vector<int> right_sorted_nodes = SortEdgesTurningOneWay(bottom_edges, vertices, Winding::kRight);
  if (right_sorted_nodes.size() == bottom_edges.size()) {
    return right_sorted_nodes;
  }

  fprintf(stderr, "Unable to sort edges with the left/right heuristic\n");
  fprintf(stderr, "There are %lu edges to sort\n", bottom_edges.size());
  fprintf(stderr, "Going left produced %lu edges\n", left_sorted_nodes.size());
  fprintf(stderr, "Going right produced %lu edges\n", right_sorted_nodes.size());
  //std::exit(1);
  if (right_sorted_nodes.size() > left_sorted_nodes.size()) {
    return right_sorted_nodes;
  }
  return left_sorted_nodes;
}

static std::vector<glm::ivec3> Earcut(const std::vector<int> &sorted_edges,
                                      const std::vector<glm::vec3> &vertices) {
  std::vector<std::array<float, 2> > polygon;
  std::vector<int> backwards_map;
  for (const int node : sorted_edges) {
    const glm::vec3 vertex = vertices.at((uint64_t)node);
    backwards_map.push_back(node);
    polygon.push_back({vertex.x, vertex.y});
  }

  std::vector<std::vector<std::array<float, 2> > > polygons;
  polygons.push_back(polygon);

  // Call earcut.
  // The indices are indexed based on the polygon, we will have to map them back
  const std::vector<int> earcut_bottom_triangles = mapbox::earcut<int>(polygons);

  // add the new triangles to the existing ones
  std::vector<glm::ivec3> bottom_triangles;
  for (uint64_t k=0; k < earcut_bottom_triangles.size() / 3; k++) {
    glm::ivec3 new_triangle;
    for (int j=0; j<3; j++) {
      const int mapbox_node_index = earcut_bottom_triangles.at(3*k + (uint64_t)j);
      const int node_index = backwards_map.at((uint64_t)mapbox_node_index);
      new_triangle[j] = node_index;
    }
    bottom_triangles.push_back(new_triangle);
  }

  return bottom_triangles;
}

static void WriteMatplotlibOutput(const std::string &output_path,
                                  const std::vector<glm::vec3> &vertices,
                                  const std::vector<glm::ivec2> &bottom_edges,
                                  const std::vector<glm::ivec3> &bottom_triangles) {
  FILE *output = fopen(output_path.c_str(), "w");
  if (output == NULL) {
    fprintf(stderr, "Error opening output %s\n", output_path.c_str());
    std::exit(1);
  }
  fprintf(output, "#!/usr/bin/env python3\n");
  fprintf(output, "import sys\n");
  fprintf(output, "import matplotlib.pyplot as plt\n");
  fprintf(output, "edges = [");
  for (const glm::ivec2 &edge : bottom_edges) {
    const glm::vec2 v0 = vertices.at((uint64_t)edge[0]);
    const glm::vec2 v1 = vertices.at((uint64_t)edge[1]);
    fprintf(output, "((%.9f, %.9f), (%.9f, %.9f)), ", (double)v0.x, (double)v0.y, (double)v1.x, (double)v1.y);
  }
  fprintf(output, "]\n");

  fprintf(output, "triangles = [");
  for (const glm::ivec3 &triangle : bottom_triangles) {
    const glm::vec2 v0 = vertices.at((uint64_t)triangle[0]);
    const glm::vec2 v1 = vertices.at((uint64_t)triangle[1]);
    const glm::vec2 v2 = vertices.at((uint64_t)triangle[2]);
    fprintf(output, "((%.9f, %.9f), (%.9f, %.9f), (%.9f, %.9f)), ",
           (double)v0.x, (double)v0.y,
           (double)v1.x, (double)v1.y,
           (double)v2.x, (double)v2.y);
  }
  fprintf(output, "]\n");

  fprintf(output, "plt.figure()\n");
  fprintf(output, "ax = plt.subplot(2, 1, 1)\n");
  fprintf(output, "for (x0,y0), (x1, y1) in edges:\n");
  fprintf(output, "  plt.plot([x0, x1], [y0, y1])\n");

  fprintf(output, "plt.subplot(2, 1, 2, sharex=ax, sharey=ax)\n");
  fprintf(output, "for (x0, y0), (x1, y1), (x2, y2) in triangles:\n");
  fprintf(output, "  plt.fill([x0, x1, x2], [y0, y1, y2])\n");

  fprintf(output, "if len(sys.argv) == 1:\n");
  fprintf(output, "  plt.show()\n");
  fprintf(output, "elif len(sys.argv) == 2:\n");
  fprintf(output, "  plt.savefig(sys.argv[1])\n");
  fprintf(output, "else:\n");
  fprintf(output, "  print('unexpected number of arguments: ' + str(sys.argv))\n");

  fclose(output);
}

// Usage: ./fill_bottom inputpath outputpath
int32_t main(int32_t argc, char *argv[]) {
  // Parse flags.
  if (argc != 4) {
    fprintf(stderr, "Need 3 command line arguments: input, ply output, matplotlib output\n");
    exit(1);
  }
  std::string input_path = argv[1];
  std::string ply_output_path = argv[2];
  std::string matplotlib_output_path = argv[3];
  assert(input_path.size() != 0);
  assert(output_path.size() != 0);

  // Read inputs.
  std::vector<glm::vec3> vertices;
  std::vector<glm::ivec3> triangles;
  LoadPly(input_path, &vertices, &triangles);

  // Get bottom edges in no particular order
  std::vector<glm::ivec2> bottom_edges;
  GetBottomEdges(vertices, triangles, &bottom_edges);
  fprintf(stderr, "Bottom has %zu edges\n", bottom_edges.size());

  // Sort the edges.
  std::vector<int> sorted_edges = SortEdges(bottom_edges, vertices);

  // call mapbox earcut
  std::vector<glm::ivec3> bottom_triangles = Earcut(sorted_edges, vertices);
  triangles.insert(triangles.end(), bottom_triangles.begin(), bottom_triangles.end());

  // Write outputs.
  SavePly(ply_output_path, vertices, triangles);
  WriteMatplotlibOutput(matplotlib_output_path, vertices, bottom_edges, bottom_triangles);
}
