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
  f.write(nx.to_bytes(4, byteorder='little', signed=True))
  f.write(ny.to_bytes(4, byteorder='little', signed=True))
  image.tofile(f)
  f.close()

def trim_nans(heightmap_data):
  isnan = np.isnan(heightmap_data)

  allnan_axis0 = np.all(isnan, axis=0)
  allnan_axis1 = np.all(isnan, axis=1)

  where_nonnan0 = np.squeeze(np.argwhere(~allnan_axis0))
  where_nonnan1 = np.squeeze(np.argwhere(~allnan_axis1))

  if where_nonnan0.size > 0:
    heightmap_data = heightmap_data[where_nonnan1[0]:where_nonnan1[-1]+1, :]
  if where_nonnan1.size > 0:
    heightmap_data = heightmap_data[:, where_nonnan0[0]:where_nonnan0[-1]+1]

  return heightmap_data


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('height_map_path', help='Input height map data path.')
  parser.add_argument('output_path', help='Output path for blob.')
  parser.add_argument('--decimation', type=int, help='Optional downsample factor.')
  parser.add_argument('--trim', action='store_true')

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

  if flags.trim:
    #heightmap_data[:11036, 21793:] = np.nan
    #heightmap_data[:12431, 23064:] = np.nan
    heightmap_data[:12090, 22475:] = np.nan

  # trim leading and trailing rows/cols that are all nans
  t0 = time.time()
  heightmap_data = trim_nans(heightmap_data)
  print('trimming nans took {} s'.format(time.time() - t0))

  # downsample
  if flags.decimation:
    t0 = time.time()
    heightmap_data = heightmap_data[::flags.decimation, ::flags.decimation] / flags.decimation
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
