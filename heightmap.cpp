#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

//#include "stb_image.h"
#include "heightmap.hpp"

// placeholders for stb_image.h
uint8_t *stbi_load(char const *filename, int32_t *x, int32_t *y, int32_t *comp, int32_t req_comp);
unsigned char *stbi_failure_reason();
unsigned char *stbi_image_free(uint8_t *);


void ScanHeightmap(Heightmap *hm) {
  uint64_t i;
  uint8_t min, max;

  if (hm == NULL || hm->data == NULL) {
    return;
  }

  min = 255;
  max = 0;

  for (i = 0; i < hm->size; i++) {
    if (hm->data[i] < min) {
      min = hm->data[i];
    }
    if (hm->data[i] > max) {
      max = hm->data[i];
    }
  }

  hm->min = min;
  hm->max = max;
  hm->range = max - min;
}

// Returns pointer to Heightmap
// Returns NULL on error
Heightmap *ReadHeightmap(const char *path) {
  int32_t width, height, depth;
  uint8_t *data = stbi_load(path, &width, &height, &depth, 1);

  if (data == NULL) {
    fprintf(stderr, "%s\n", stbi_failure_reason());
    return NULL;
  }

  Heightmap *hm;
  if ((hm = (Heightmap *)malloc(sizeof(Heightmap))) == NULL) {
    fprintf(stderr, "Cannot allocate memory for heightmap structure\n");
    free(data);
    return NULL;
  }

  hm->width = (uint32_t)width;
  hm->height = (uint32_t)height;
  hm->size = (uint64_t)width * (uint64_t)height;
  hm->data = data;

  ScanHeightmap(hm);

  return hm;
}

void FreeHeightmap(Heightmap **hm) {

  if (hm == NULL || *hm == NULL) {
    return;
  }

  if ((*hm)->data != NULL) {
    stbi_image_free((*hm)->data);
  }

  free(*hm);
  *hm = NULL;
}

void DumpHeightmap(const Heightmap *hm) {
  fprintf(stderr, "Width: %u\n", hm->width);
  fprintf(stderr, "Height: %u\n", hm->height);
  fprintf(stderr, "Size: %lu\n", hm->size);
  fprintf(stderr, "Min: %d\n", hm->min);
  fprintf(stderr, "Max: %d\n", hm->max);
  fprintf(stderr, "Range: %d\n", hm->range);
}
