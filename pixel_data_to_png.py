#!/usr/bin/env python3
import argparse
import PIL
from PIL import Image
import time
import numpy as np

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('height_map_path', help='Input height map data path.')
  parser.add_argument('png_path', help='Output path for height map png.')
  parser.add_argument('--downsample', type=int, help='Optional downsample factor.')
  flags = parser.parse_args()

  # load data
  t0 = time.time()
  with open(flags.height_map_path, 'rb') as f:
    heightmap_data = np.load(f)
  print('loaded master image in {} seconds'.format(time.time() - t0))

  # downsample
  if flags.downsample:
    t0 = time.time()
    heightmap_data = heightmap_data[::flags.downsample, ::flags.downsample] / flags.downsample
    print('decimated in {} seconds'.format(time.time() - t0))

  # normalize image height
  t0 = time.time()
  heightmap_data -= np.nanmin(heightmap_data)
  heightmap_data = heightmap_data / np.nanmax(heightmap_data) * (2.**8-1)
  heightmap_data = heightmap_data.astype(np.uint8)
  print("Normalized heightmap in {} seconds.".format(time.time() - t0))

  # write heightmap image
  t0 = time.time()
  heightmap_image = PIL.Image.fromarray(heightmap_data)
  heightmap_image.save(flags.png_path)
  print('Wrote png in {} seconds to {}'.format(time.time() - t0, flags.png_path))

if __name__=='__main__':
  main()
