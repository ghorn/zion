#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "libtrix.h"

// sets result to difference of vectors a and b (b - a)
static void vector_difference(const trix_vertex *a, const trix_vertex *b, trix_vertex *result) {
  result->x = b->x - a->x;
  result->y = b->y - a->y;
  result->z = b->z - a->z;
}

// sets result to cross product of vectors a and b (a x b)
// https://en.wikipedia.org/wiki/Cross_product#Mnemonic
static void vector_crossproduct(const trix_vertex *a, const trix_vertex *b, trix_vertex *result) {
  result->x = a->y * b->z - a->z * b->y;
  result->y = a->z * b->x - a->x * b->z;
  result->z = a->x * b->y - a->y * b->x;
}

// sets result to unit vector codirectional with vector v (v / ||v||)
static void vector_unitvector(const trix_vertex *v, trix_vertex *result) {
  float mag = sqrtf((v->x * v->x) + (v->y * v->y) + (v->z * v->z));
  result->x = v->x / mag;
  result->y = v->y / mag;
  result->z = v->z / mag;
}

void trixUpdateTriangleNormal(trix_triangle *triangle, trix_winding_order *order) {
  trix_vertex u, v, cp, n;

  if (triangle == NULL) {
    printf("Null triangle argument\n");
    exit(1);
  }

  // vectors u and v are triangle sides ab and bc
  vector_difference(&triangle->a, &triangle->b, &u);
  vector_difference(&triangle->b, &triangle->c, &v);

  // the cross product of two vectors is perpendicular to both
  // since vectors u and v both lie in the plane of triangle abc,
  // the cross product is perpendicular to the triangle's surface
  if (order == NULL || *order == TRIX_WINDING_CCW) {
    vector_crossproduct(&u, &v, &cp);
  } else {
    vector_crossproduct(&v, &u, &cp);
  }

  // normalize the cross product to unit length to get surface normal n
  vector_unitvector(&cp, &n);

  triangle->n.x = n.x;
  triangle->n.y = n.y;
  triangle->n.z = n.z;
}
