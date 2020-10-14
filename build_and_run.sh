#!/usr/bin/env bash

set -e

./dems_to_image.py
g++ -Wall -Wextra -Werror -std=c++17 -O2 image_to_stl.cpp -o image_to_stl && ./image_to_stl
