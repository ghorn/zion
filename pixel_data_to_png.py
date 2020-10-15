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
  #f.write(image.tobytes())
  f.close()

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('height_map_path', help='Input height map data path.')
  parser.add_argument('--png_path', required=True, help='Output path for height map png.')
  parser.add_argument('--mask_path', required=True, help='Output path for mask png.')
  parser.add_argument('--downsample', type=float, help='Optional downsample factor.')
  flags = parser.parse_args()

  t0 = time.time()
  with open(flags.height_map_path, 'rb') as f:
    heightmap_data = np.load(f)
  print('loaded master image in {} seconds'.format(time.time() - t0))
  min_z = np.nanmin(heightmap_data)
  max_z = np.nanmax(heightmap_data)
  print('min height: {}'.format(min_z))
  print('max height: {}'.format(max_z))
  print('recommended scale factor: {}'.format((max_z -min_z)/255.))

  # make a mask version
  t0 = time.time()
  mask_data = heightmap_data.copy()
  print("Copied mask in {} seconds.".format(time.time() - t0))
  t0 = time.time()
  #mask_data[np.where(~np.isnan(mask_data))] = 255
  mask_data = mask_data * 0 + 255
  print("Converted mask non-nans to 255 in {} seconds.".format(time.time() - t0))
  t0 = time.time()
  #mask_data[np.where(np.isnan(mask_data))] = 0
  np.nan_to_num(mask_data, copy=False)
  print("Converted mask nans to 0 in {} seconds.".format(time.time() - t0))
  t0 = time.time()
  mask_data = mask_data.astype(np.uint8)
  print("Converted mask to uint8 in {} seconds.".format(time.time() - t0))


  # normalize image height
  t0 = time.time()
  heightmap_data -= np.nanmin(heightmap_data)
  heightmap_data = heightmap_data / np.nanmax(heightmap_data) * (2.**8-1)
  heightmap_data = heightmap_data.astype(np.uint8)
  print("Normalized heightmap in {} seconds.".format(time.time() - t0))

  # downsample and write heightmap image
  heightmap_image = PIL.Image.fromarray(heightmap_data)
  if flags.downsample:
    t0 = time.time()
    new_size = (int(heightmap_image.width/flags.downsample), int(heightmap_image.height/flags.downsample))
    heightmap_image = heightmap_image.resize(new_size, resample=PIL.Image.NEAREST)#resample=PIL.Image.BICUBIC)
    print('Downsampled heightmap in {} seconds.'.format(time.time() - t0))
  t0 = time.time()
  heightmap_image.save(flags.png_path)
  print('Write heightmap in {} seconds to {}'.format(time.time() - t0, flags.png_path))

  # downsample and write mask image
  mask_image = PIL.Image.fromarray(mask_data)
  if flags.downsample:
    t0 = time.time()
    new_size = (int(mask_image.width/flags.downsample), int(mask_image.height/flags.downsample))
    mask_image = mask_image.resize(new_size, resample=PIL.Image.NEAREST)#resample=PIL.Image.BICUBIC)
    print('Downsampled mask in {} seconds.'.format(time.time() - t0))
  t0 = time.time()
  mask_image.save(flags.mask_path)
  print('Write mask in {} seconds to {}'.format(time.time() - t0, flags.mask_path))

  # Print recommended scale factor to correct 8 bit conversion and downsampling.
  downsamplef = flags.downsample if flags.downsample else 1.0
  print('Recommended scale factor: {}'.format((max_z - min_z) / 255. / downsamplef))

if __name__=='__main__':
  main()
