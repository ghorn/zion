#!/usr/bin/env python3
import argparse
from osgeo import gdal
import numpy as np
import time

EXPECTED_PROJECTION_REF = 'PROJCS["NAD_1983_2011_UTM_Zone_12N",GEOGCS["NAD83",DATUM["North_American_Datum_1983",SPHEROID["GRS 1980",6378137,298.257222101,AUTHORITY["EPSG","7019"]],TOWGS84[0,0,0,-0,-0,-0,0],AUTHORITY["EPSG","6269"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4269"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-111],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["Meter",1],AUTHORITY["EPSG","26912"]]'


class Dem():
  def __init__(self, filename):
    #print('loading {}'.format(filename))
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
    #print('raster band array shape: {}'.format(raster_band_array.shape))
    #print('dataset raster xsize: {}'.format(self.dataset.RasterXSize))
    #print('dataset raster ysize: {}'.format(self.dataset.RasterYSize))
    assert raster_band_array.shape[0] == self.dataset.RasterYSize
    assert raster_band_array.shape[1] == self.dataset.RasterXSize
    #print('no data value: {}'.format(self.raster_band.GetNoDataValue()))
    #print('unit type: {}'.format(self.raster_band.GetUnitType()))
    #print('offset: {}'.format(self.raster_band.GetOffset()))
    #print('scale: {}'.format(self.raster_band.GetScale()))
    #print('value with no data: ' + str(self.raster_band.GetNoDataValue()))
    #print('num points without value: ')
    #print(len(np.where(raster_band_array == self.raster_band.GetNoDataValue())))
    #print('num points with very low values: ')
    #print(len(np.where(raster_band_array < -1e38)))
    #exit(1)
    
    #raster_band_array[np.where(raster_band_array == self.raster_band.GetNoDataValue())] = np.nan
    return raster_band_array


  def plot(self, ax):
    x0 = self.raw_geo_transform[0]
    x1 = x0 + self.raster_band_array.shape[1]
    y0 = self.raw_geo_transform[0]
    y1 = y0 - self.raster_band_array.shape[0]
    extent = [x0, x1, y0, y1]
    im = ax.imshow(self.raster_band_array, extent=extent)#, interpolation='bilinear', cmap=cm.RdYlGn),
#                 origin='lower', extent=[-3, 3, -3, 3],
#                 vmax=abs(Z).max(), vmin=-abs(Z).max())

  def plot_surface(self, ax):
    nx = self.raster_band_array.shape[0]
    ny = self.raster_band_array.shape[1]

    x0 = self.raw_geo_transform[0]
    x1 = x0 + nx
    y0 = self.raw_geo_transform[1]
    y1 = y0 - ny

    X = np.linspace(x0, x1, nx)
    Y = np.linspace(y0, y1, ny)
    X, Y = np.meshgrid(X, Y)
    Z = self.raster_band_array

    # Plot the surface.
    surf = ax.plot_surface(X, Y, Z, cmap=cm.coolwarm,
                           linewidth=0, antialiased=False)

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('output', help='Output path for pixel array.')
  parser.add_argument('dem_path', help='Input DEM file.')
  flags = parser.parse_args()

  gdal.UseExceptions()

  print('loading DEM...')
  dem = Dem(flags.dem_path)
  print('x size: ' + str(dem.x_size()))
  print('y size: ' + str(dem.y_size()))

  print()
  print('making master image...')
  t0 = time.time()
  master_image = dem.get_raster_band_array()
  print('assembled master image in {} seconds'.format(time.time() - t0))

  print('filling nans to no-data regions...')
  t0 = time.time()
  master_image[np.where(master_image < -1e30)] = np.nan
  print('filled nans in {} seconds'.format(time.time() - t0))

  print('rescaling z...')
  assert np.abs(dem.pixel_width()) == np.abs(dem.pixel_height())
  t0 = time.time()
  master_image = master_image * np.abs(dem.pixel_width())
  print('rescaled z in {} seconds'.format(time.time() - t0))

  print('saving {} ({} MB)'.format(flags.output, master_image.size*8/1024/1024))
  with open(flags.output, 'wb') as f:
    t0 = time.time()
    np.save(f, master_image)
  print('saved in {} seconds'.format(time.time() - t0))


if __name__=='__main__':
  main()
