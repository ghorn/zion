#ifndef _LIBTRIX_H_
#define _LIBTRIX_H_

#include <stdint.h>

/*
 * The maximum number of faces allowed in a mesh.
 * This value is constrained by the four bytes available
 * to indicate the number of faces in a binary STL file.
 * Technically, no such constraint applies to ASCII STL, but
 * libtrix applies this limit in all cases for consistency.
 * (This value is also given in stdint.h as UINT32_MAX.)
 */
#define TRIX_FACE_MAX 4294967295U

/*
 * When recalculating the normal vectors of faces in a mesh,
 * face orientation is determined from the winding order of
 * the face triangle's vertices. By default, libtrix assumes
 * vertices a, b, and c appear counter-clockwise as seen from
 * "outside" the face (the "right hand rule").
 */
typedef enum {
  TRIX_WINDING_CCW,
  TRIX_WINDING_CW,
  TRIX_WINDING_DEFAULT = TRIX_WINDING_CCW
} trix_winding_order;

/*
 * A trix_vertex is a simple three dimensional vector used
 * to represent triangle vertex coordinates and normal vectors.
 */
typedef struct {
  float x, y, z;
} trix_vertex;

/*
 * A trix_triangle is comprised of three vertices (a, b, and c)
 * as well as a normal vector (n). Applications that generate
 * meshes will typically do so by repeatedly defining a
 * trix_triangle and appending it to the mesh with trixAddTriangle.
 *
 * The normal vector specifies the orientation of the triangle -
 * it points in the direction the triangle is facing. For many
 * applications, the normal vector may be a null vector (0 0 0)
 * to imply orientation from vertex coordinate winding order.
 * trixUpdateNormals applies this interpretation explicitly.
 */
typedef struct {
  trix_vertex n, a, b, c;
} trix_triangle;

void trixUpdateTriangleNormal(trix_triangle *triangle, trix_winding_order *order);

#endif
