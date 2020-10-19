#pragma once

#include <inttypes.h>

#include <vector>
#include <string>

void ReadHeightmapData(const std::string &path, int32_t *nx, int32_t *ny, std::vector<float> *image);
