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
    #print('raw geo transform:')
    #print(self.raw_geo_transform)
    assert self.raw_geo_transform[1] == 1.0 # pixel width
    assert self.raw_geo_transform[2] == 0.0
    assert self.raw_geo_transform[4] == 0.0
    assert self.raw_geo_transform[5] == -1.0 # pixel height

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
    assert self.projection_ref == EXPECTED_PROJECTION_REF

    self.gcp_projection = self.dataset.GetGCPProjection()
    assert self.gcp_projection == ''

    return

    #print('raster band array type:')
    #print(type(self.raster_band_array))
    #print('raster band array dim:')
    #print(self.raster_band_array.shape)

  def x_origin(self):
    return self.raw_geo_transform[0]

  def y_origin(self):
    return self.raw_geo_transform[3]

  def x_size(self):
    return self.dataset.RasterXSize

  def y_size(self):
    return self.dataset.RasterYSize

  def get_raster_band_array(self):
    raster_band_array = self.raster_band.ReadAsArray().T
    #print('raster band array shape: {}'.format(raster_band_array.shape))
    #print('dataset raster xsize: {}'.format(self.dataset.RasterXSize))
    #print('dataset raster ysize: {}'.format(self.dataset.RasterYSize))
    assert raster_band_array.shape[0] == self.dataset.RasterXSize
    assert raster_band_array.shape[1] == self.dataset.RasterYSize
    #print('no data value: {}'.format(self.raster_band.GetNoDataValue()))
    #print('unit type: {}'.format(self.raster_band.GetUnitType()))
    #print('offset: {}'.format(self.raster_band.GetOffset()))
    #print('scale: {}'.format(self.raster_band.GetScale()))
    raster_band_array[np.where(raster_band_array == self.raster_band.GetNoDataValue())] = np.nan
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
  parser.add_argument('dem_paths', help='Input DEM files, comma separated.')
  parser.add_argument('--output', required=True, help='Output path for pixel array.')
  #parser.add_argument('--only_square_tiles', action='store_true', help='Ignore DEMs that are not 1500/1500 pix.')
  #parser.add_argument('--no_missing_data', action='store_true', help='Ignore DEMs that have missing pixels.')
  flags = parser.parse_args()

  flags.dem_paths = flags.dem_paths.split(',')
  
  gdal.UseExceptions()

  print('loading DEMs...')
  dems = [Dem(filename) for filename in flags.dem_paths]#[29:31]#[0:20]#[29:35]
  #if flags.only_square_tiles:
  dems = [dem for dem in dems if dem.x_size() == 1500 and dem.y_size() == 1500]
  #if flags.no_missing_data:
  dems = [dem for dem in dems if np.count_nonzero(~np.isnan(dem.get_raster_band_array())) > 0]

  # Xgeo = GT(0) + Xpixel*GT(1) + Yline*GT(2)
  # Ygeo = GT(3) + Xpixel*GT(4) + Yline*GT(5)
  #
  # In case of north up images, the GT(2) and GT(4) coefficients are zero, and the GT(1) is pixel
  # width, and GT(5) is pixel height. The (GT(0),GT(3)) position is the top left corner of the top
  # left pixel of the raster.
  #
  # Note that the pixel/line coordinates in the above are from (0.0,0.0) at the top left corner
  # of the top left pixel to (width_in_pixels,height_in_pixels) at the bottom right corner of
  # the bottom right pixel. The pixel/line location of the center of the top left pixel would
  # therefore be (0.5,0.5).
  print()
  print('getting min/max extents...')
  min_x = np.min([dem.x_origin() for dem in dems])
  min_y = np.min([dem.y_origin() for dem in dems])
  max_x = np.max([dem.x_origin() + dem.x_size() - 1 for dem in dems])
  max_y = np.max([dem.y_origin() + dem.y_size() - 1 for dem in dems])
  nx = int(max_x - min_x + 1)
  ny = int(max_y - min_y + 1)
  print('min x: {}, max x: {}, num x: {}'.format(min_x, max_x, nx))
  print('min y: {}, may y: {}, num y: {}'.format(min_y, max_y, ny))

  print()
  print('initializing master image ({} x {})...'.format(nx, ny))
  master_image = np.empty((nx, ny))
  master_image[:] = np.nan

  print()
  print('making master image...')
  t0 = time.time()
  for k, dem in enumerate(dems):
    #print('inserting {} of {}'.format(k+1, len(dems)))
    x0 = int(dem.x_origin() - min_x)
    y0 = int(dem.y_origin() - min_y)
    xf = int(dem.x_origin() + dem.x_size() - min_x)
    yf = int(dem.y_origin() + dem.y_size() - min_y)
    raster_band_array = dem.get_raster_band_array()
    master_image[x0:xf, y0:yf] = np.fliplr(raster_band_array)
  print('assembled master image in {} seconds'.format(time.time() - t0))

  print('saving {} ({} MB)'.format(flags.output, master_image.size*8/1024/1024))
  with open(flags.output, 'wb') as f:
    t0 = time.time()
    np.save(f, master_image)
  print('saved in {} seconds'.format(time.time() - t0))


if __name__=='__main__':
  main()
