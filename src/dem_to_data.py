#!/usr/bin/env python3
import argparse
from osgeo import gdal
import numpy as np
import time

EXPECTED_PROJECTION_REF = 'PROJCS["NAD_1983_2011_UTM_Zone_12N",GEOGCS["NAD83",DATUM["North_American_Datum_1983",SPHEROID["GRS 1980",6378137,298.257222101,AUTHORITY["EPSG","7019"]],TOWGS84[0,0,0,-0,-0,-0,0],AUTHORITY["EPSG","6269"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4269"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-111],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["Meter",1],AUTHORITY["EPSG","26912"]]'

class Dem():
  def __init__(self, filename):
    self.dataset = gdal.Open(filename, gdal.GA_ReadOnly)

    # get geo transform
    self.raw_geo_transform = self.dataset.GetGeoTransform()
    print('raw geo transform:')
    print(self.raw_geo_transform)
    assert self.raw_geo_transform[2] == 0.0
    assert self.raw_geo_transform[4] == 0.0

    #self.geo_transform = {
    #  'origin_lon' : self.raw_geo_transform[0],
    #  'lon_per_pixel' : self.raw_geo_transform[1],
    #  'rot_x' : self.raw_geo_transform[2],
    #  'origin_lat' : self.raw_geo_transform[3],
    #  'rot_y' : self.raw_geo_transform[4],
    #  'lat_per_pixel' : self.raw_geo_transform[5],
    #}
    #print('geo transform:')
    #print(self.geo_transform)

    # get raster band
    assert self.dataset.RasterCount == 1
    self.raster_band = self.dataset.GetRasterBand(1)

    # get projection ref and check it
    self.projection_ref = self.dataset.GetProjectionRef()

    #print('projection ref:')
    #print(self.projection_ref)
    #print('expected projection ref:')
    #print(EXPECTED_PROJECTION_REF)
    #assert self.projection_ref == EXPECTED_PROJECTION_REF

    self.gcp_projection = self.dataset.GetGCPProjection()
    print("projection:")
    print(self.gcp_projection)
    #assert self.gcp_projection == ''

    #print('raster band array type:')
    #print(type(self.raster_band_array))
    #print('raster band array dim:')
    #print(self.raster_band_array.shape)

  def x_origin(self):
    return self.raw_geo_transform[0]

  def y_origin(self):
    return self.raw_geo_transform[3]

  def pixel_width(self):
    return self.raw_geo_transform[1]

  def pixel_height(self):
    return self.raw_geo_transform[5]

  def x_size(self):
    return self.dataset.RasterXSize

  def y_size(self):
    return self.dataset.RasterYSize

  def get_raster_band_array(self):
    raster_band_array = self.raster_band.ReadAsArray()
    assert raster_band_array.shape[0] == self.dataset.RasterYSize
    assert raster_band_array.shape[1] == self.dataset.RasterXSize
    return raster_band_array

def _save_image(filename, image):
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
  parser.add_argument('dem_path', help='Input DEM file.')
  parser.add_argument('output_path', help='Output path for pixel array.')
  parser.add_argument('--decimation', type=int, help='Optional downsample factor.')
  parser.add_argument('--trim')
  flags = parser.parse_args()

  gdal.UseExceptions()

  print('loading DEM...')
  dem = Dem(flags.dem_path)
  print('x size: ' + str(dem.x_size()))
  print('y size: ' + str(dem.y_size()))

  print()
  print('making master image...')
  t0 = time.time()
  heightmap_data = dem.get_raster_band_array()
  print('assembled master image in {} seconds'.format(time.time() - t0))

  print('filling nans to no-data regions...')
  t0 = time.time()
  heightmap_data[np.where(heightmap_data < -1e30)] = np.nan
  print('filled nans in {} seconds'.format(time.time() - t0))

  # convert to float
  print('converting to 32 bit floats...')
  t0 = time.time()
  heightmap_data = heightmap_data.astype(np.float32)
  print('converted to 32 bit floats in {} seconds'.format(time.time() - t0))

  # trim
  if flags.trim:
    # Expect flags.trim to be a string like '22,2222,44,4444'
    # Parse this into (min_x, max_x, min_y, max_y).
    trim = eval(flags.trim)
    assert len(trim) == 4
    min_x, max_x, min_y, max_y = trim
    heightmap_data[    0:min_x, :] = np.nan
    heightmap_data[max_x:   -1, :] = np.nan
    heightmap_data[:,     0:min_y] = np.nan
    heightmap_data[:, max_y:   -1] = np.nan

    # trim leading and trailing rows/cols that are all nans
    print('trimming nans...')
    t0 = time.time()
    heightmap_data = trim_nans(heightmap_data)
    print('trimming nans took {} s'.format(time.time() - t0))

  # downsample
  if flags.decimation:
    print('decimating...')
    t0 = time.time()
    heightmap_data = heightmap_data[::flags.decimation, ::flags.decimation] / flags.decimation
    print('decimated in {} seconds'.format(time.time() - t0))

  # normalize image height
  print("Normalizing heightmap...")
  t0 = time.time()
  heightmap_data -= np.nanmin(heightmap_data)
  print("Normalized heightmap in {} seconds.".format(time.time() - t0))

  # write blob
  print(f"Writing image to {flags.output_path}...")
  t0 = time.time()
  _save_image(flags.output_path, heightmap_data)
  print('Wrote heightmap in {} seconds to {}'.format(time.time() - t0, flags.output_path))
  print('Dimensions {}, num elements {}'.format(str(heightmap_data.shape), heightmap_data.size))

#  
#
#  print('saving {} ({} MB)'.format(flags.output, heightmap_data.size*8/1024/1024))
#  with open(flags.output, 'wb') as f:
#    t0 = time.time()
#    np.save(f, heightmap_data)
#  print('saved in {} seconds'.format(time.time() - t0))


if __name__=='__main__':
  main()
