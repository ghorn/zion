#pragma once

#include <inttypes.h>

typedef struct {
  int32_t base; // boolean; output walls and bottom as well as terrain surface if true
  char *input; // path to input file; use stdin if NULL
  char *output; // path to output file; use stdout if NULL
  float zscale; // scaling factor applied to raw Z values
  float baseheight; // height in STL units of base below lowest terrain (technically, offset added to scaled Z values)
} Settings;

Settings ParseArgs(int32_t argc, char **argv);
