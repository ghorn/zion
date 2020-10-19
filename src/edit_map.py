#!/usr/bin/env python3
import argparse
import cv2
import numpy as np
import time

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('height_map_path', help='Input height map data path.')
  parser.add_argument('--target_dimension', default=1000, type=int, help='Target size of smallest dimension (length or width).')
  flags = parser.parse_args()

  # load data
  t0 = time.time()
  with open(flags.height_map_path, 'rb') as f:
    heightmap_data = np.load(f)
  print('loaded master image in {} seconds'.format(time.time() - t0))

  # downsample
  old_shape = heightmap_data.shape
  t0 = time.time()
  min_shape = np.min(heightmap_data.shape)
  decimation = int(min_shape / flags.target_dimension)
  heightmap_data = heightmap_data[::decimation, ::decimation]
  print('decimated in {} seconds'.format(time.time() - t0))
  print('old shape: {}, new shape: {}'.format(str(old_shape), str(heightmap_data.shape)))

  # normalize image height
  t0 = time.time()
  heightmap_data -= np.nanmin(heightmap_data)
  heightmap_data = heightmap_data / np.nanmax(heightmap_data) * (2.**8-1)
  heightmap_data = heightmap_data.astype(np.uint8)
  print("Normalized heightmap in {} seconds.".format(time.time() - t0))

  # Mouse callback function
  global click_list
  click_list = []
  def callback(event, x, y, flags, param):
    if event == 1:#cv2.EVENT_LBUTTONDBLCLK:
      click_list.append('heightmap_data[:{}, {}:] = np.nan'.format(y*decimation, x*decimation))
      heightmap_data[:y, x:] = 0
  cv2.namedWindow('img')
  cv2.setMouseCallback('img', callback)

  # Mainloop - show the image and collect the data
  while True:
    cv2.imshow('img', heightmap_data)
    # Wait, and allow the user to quit with the 'esc' key
    k = cv2.waitKey(1)
    # If user presses 'esc' break
    if k == 27: break
  cv2.destroyAllWindows()

  for cl in click_list:
    print(cl)

if __name__=='__main__':
  main()
