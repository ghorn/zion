#!/usr/bin/env python3
import argparse
import PIL
from PIL import Image
import time
import numpy as np

def save_image(filename, image):
  nx = image.shape[0]
  ny = image.shape[1]

  f = open(filename, 'wb')
  f.write(nx.to_bytes(8, byteorder='little', signed=True))
  f.write(ny.to_bytes(8, byteorder='little', signed=True))
  image.tofile(f)
  f.close()

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('height_map_path', help='Input height map data path.')
  parser.add_argument('output_path', help='Output path for blob.')
  parser.add_argument('--downsample', type=int, help='Optional downsample factor.')
  flags = parser.parse_args()

  # load data
  t0 = time.time()
  with open(flags.height_map_path, 'rb') as f:
    heightmap_data = np.load(f)
  print('loaded master image in {} seconds'.format(time.time() - t0))

  # convert to float
  t0 = time.time()
  heightmap_data = heightmap_data.astype(np.float32)
  print('converted to 32 bit floats in {} seconds'.format(time.time() - t0))

  # downsample
  if flags.downsample:
    t0 = time.time()
    heightmap_data = heightmap_data[::flags.downsample, ::flags.downsample] / flags.downsample
    print('decimated in {} seconds'.format(time.time() - t0))

  # normalize image height
  t0 = time.time()
  heightmap_data -= np.nanmin(heightmap_data)
  print("Normalized heightmap in {} seconds.".format(time.time() - t0))

  # write blob
  t0 = time.time()
  save_image(flags.output_path, heightmap_data)
  print('Wrote heightmap in {} seconds to {}'.format(time.time() - t0, flags.output_path))
  print('Dimensions {}, num elements {}'.format(str(heightmap_data.shape), heightmap_data.size))

if __name__=='__main__':
  main()
