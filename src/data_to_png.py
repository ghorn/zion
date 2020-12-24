#!/usr/bin/env python3
import argparse
import PIL
from PIL import Image
import time
import numpy as np
import ctypes

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('height_map_path', help='Input height map data path.')
  parser.add_argument('png_path', help='Output path for height map png.')
  parser.add_argument('--target_dimension', type=int, help='Target size of smallest dimension (length or width).')
  flags = parser.parse_args()

  # load data
  t0 = time.time()
  with open(flags.height_map_path, 'rb') as f:
    nx = ctypes.c_uint32.from_buffer_copy(f.read(4)).value
    ny = ctypes.c_uint32.from_buffer_copy(f.read(4)).value
    buf = f.read()
    assert len(buf) == nx * ny * 4
    heightmap_data = np.fromstring(buf, dtype=np.float32, count=nx * ny).reshape(nx, ny)
    print(heightmap_data.shape)
  print('loaded master image in {} seconds'.format(time.time() - t0))

  # downsample
  if flags.target_dimension and flags.target_dimension < np.min(heightmap_data.shape):
    t0 = time.time()
    min_shape = np.min(heightmap_data.shape)
    decimation = int(min_shape / flags.target_dimension)
    old_shape = heightmap_data.shape
    heightmap_data = heightmap_data[::decimation, ::decimation]
    print('decimated in {} seconds'.format(time.time() - t0))
    print('old shape: {}, new shape: {}'.format(str(old_shape), str(heightmap_data.shape)))

  # normalize image height
  t0 = time.time()
  heightmap_data -= np.nanmin(heightmap_data)
  heightmap_data = heightmap_data / np.nanmax(heightmap_data) * (2.**8-1)
  heightmap_data = heightmap_data.astype(np.uint8)
  print("Normalized heightmap in {} seconds.".format(time.time() - t0))

  # write heightmap image
  t0 = time.time()
  PIL.Image.fromarray(heightmap_data).save(flags.png_path)
  print('Wrote png in {} seconds to {}'.format(time.time() - t0, flags.png_path))

if __name__=='__main__':
  main()
