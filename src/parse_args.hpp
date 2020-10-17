#pragma once

#include <inttypes.h>

typedef struct {
  bool generate_base; // boolean; output walls and bottom as well as terrain surface if true
  char *input; // path to input file; use stdin if NULL
  char *output; // path to output file; use stdout if NULL
  float xy_size; // desired size of maximum X/Y dimension
  float z_scale; // scaling factor applied to raw Z values
  float baseheight_frac; // height in fraction of base below lowest terrain (technically, offset is added to scaled Z values)
} Settings;

Settings ParseArgs(int32_t argc, char **argv);
