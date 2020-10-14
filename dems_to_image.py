#!/usr/bin/env python3

import numpy
from osgeo import gdal

from mpl_toolkits.mplot3d import Axes3D
import matplotlib.pyplot as plt
import numpy as np

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
  gdal.UseExceptions()

  print('loading DEMs...')
  dems = [Dem(to_filename(name)) for name in dem_names()][29:31]#[0:20]#[29:35]
  dems = [dem for dem in dems if dem.x_size() == 1500 and dem.y_size() == 1500]
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
  master_image = np.zeros((nx, ny)) + np.nan

  print()
  print('making master image...')
  for k, dem in enumerate(dems):
    print('inserting {} of {}'.format(k+1, len(dems)))
    x0 = int(dem.x_origin() - min_x)
    y0 = int(dem.y_origin() - min_y)
    xf = int(dem.x_origin() + dem.x_size() - min_x)
    yf = int(dem.y_origin() + dem.y_size() - min_y)
    raster_band_array = dem.get_raster_band_array()
    #print(type(raster_band_array))
    #print(raster_band_array.dtype)
    #exit()
    master_image[x0:xf, y0:yf] = np.fliplr(raster_band_array)

  print('transposing image')
  master_image = master_image.T
  master_image = np.flipud(master_image)
  master_image -= np.nanmin(master_image)

  total_count = nx*ny
  num_non_nan = np.count_nonzero(~np.isnan(master_image))
  print('image has {} elements, {} non-nan ({} %)'.format(total_count,
                                                          num_non_nan,
                                                          100. * num_non_nan / total_count))

  output_file = 'yolo.dat'
  print('writing output file {}'.format(output_file))
  save_image(output_file, master_image)
  print('success!')

  #print('showing image')
  #plt.imshow(master_image)
  #plt.show()

  #print('showing 3d surface')
  #fig = plt.figure()
  #ax = fig.gca(projection='3d')
  #X, Y = np.meshgrid(np.linspace(min_x, max_x, nx), np.linspace(min_y, max_y, ny))
  #Z = master_image
  #print(np.nanmin(Z))
  #print(np.nanmax(Z))
  #surf = ax.plot_surface(X, Y, Z,
  #                       cmap=plt.cm.jet, #cmap=cm.coolwarm,
  #                       vmin=np.nanmin(Z),
  #                       vmax=np.nanmax(Z),
  #                       linewidth=0,
  #                       antialiased=False)
  #cbar = plt.colorbar(surf)
  #plt.show()

def to_zip_filename(name):
  return "dems/zips/{}.zip".format(name)

def to_filename(name):
  return "dems/{}.img".format(name)

def dem_names():
  return [
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG240115_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG255115_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG270115_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG285115_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG300115_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG240130_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG255130_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG270130_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG285130_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG300130_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG315130_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG330130_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG345130_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG135145_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG150145_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG165145_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG180145_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG195145_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG210145_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG225145_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG240145_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG255145_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG270145_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG285145_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG300145_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG315145_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG330145_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG345145_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG135160_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG150160_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG165160_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG180160_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG195160_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG210160_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG225160_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG240160_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG255160_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG270160_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG285160_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG300160_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG315160_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG330160_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG345160_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG135175_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG150175_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG165175_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG180175_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG195175_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG210175_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG225175_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG240175_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG255175_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG270175_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG285175_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG300175_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG315175_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG330175_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG345175_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG120190_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG135190_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG150190_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG165190_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG180190_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG195190_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG210190_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG225190_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG240190_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG255190_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG270190_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG285190_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG300190_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG315190_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG330190_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG345190_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG120205_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG135205_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG150205_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG165205_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG180205_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG195205_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG210205_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG225205_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG240205_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG255205_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG270205_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG285205_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG300205_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG315205_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG330205_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG345205_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG105220_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG120220_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG135220_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG150220_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG165220_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG180220_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG195220_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG210220_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG225220_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG240220_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG255220_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG270220_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG285220_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG300220_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG315220_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG330220_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG345220_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG105235_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG120235_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG135235_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG150235_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG165235_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG180235_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG195235_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG210235_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG225235_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG240235_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG255235_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG270235_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG285235_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG300235_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG315235_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG330235_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG345235_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG120250_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG135250_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG150250_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG165250_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG180250_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG195250_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG210250_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG225250_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG240250_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG255250_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG270250_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG285250_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG300250_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG315250_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG330250_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG120265_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG135265_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG150265_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG165265_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG180265_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG195265_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG210265_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG225265_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG240265_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG255265_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG270265_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG285265_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG300265_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG315265_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG120280_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG135280_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG150280_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG165280_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG180280_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG195280_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG210280_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG225280_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG240280_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG255280_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG270280_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG285280_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG300280_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG315280_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG120295_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG135295_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG150295_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG165295_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG180295_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG195295_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG210295_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG225295_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG240295_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG255295_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG270295_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG285295_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG300295_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG315295_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG120310_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG135310_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG150310_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG165310_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG180310_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG195310_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG210310_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG225310_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG240310_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG255310_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG270310_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG285310_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG300310_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG315310_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG120325_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG135325_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG150325_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG165325_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG180325_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG195325_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG210325_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG225325_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG240325_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG255325_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG270325_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG285325_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG300325_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG315325_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG120340_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG135340_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG150340_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG165340_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG180340_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG195340_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG210340_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG225340_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG240340_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG255340_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG270340_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG285340_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG300340_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG315340_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG105355_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG120355_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG135355_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG150355_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG165355_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG180355_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG195355_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG210355_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG225355_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG240355_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG255355_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG270355_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG285355_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG300355_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG315355_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG045370_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG060370_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG075370_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG090370_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG105370_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG120370_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG135370_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG150370_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG165370_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG180370_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG195370_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG210370_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG225370_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG240370_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG255370_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG270370_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG285370_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG300370_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG315370_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG045385_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG060385_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG075385_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG090385_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG105385_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG120385_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG135385_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG165385_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG180385_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG195385_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG210385_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG225385_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG045400_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG060400_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG075400_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG090400_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG105400_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG120400_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG135400_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG150400_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG180400_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG195400_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG210400_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG225400_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG030415_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG045415_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG060415_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG075415_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG090415_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG105415_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG120415_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG135415_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG150415_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG030430_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG045430_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG060430_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG075430_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG090430_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG105430_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG120430_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG135430_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG150430_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG030445_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG045445_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG060445_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG075445_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG090445_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG105445_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG120445_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG135445_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG150445_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG015460_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG030460_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG045460_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG060460_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG075460_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG090460_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG105460_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG120460_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG135460_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG150460_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG015475_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG030475_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG045475_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG060475_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG075475_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG090475_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG030490_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG045490_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG060490_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG075490_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG090490_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG045505_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG060505_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG075505_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG090505_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG045520_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG060520_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG075520_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG090520_IMG_2017",
      "USGS_NED_OPR_UT_ZionNP_QL2_2016_12SUG105520_IMG_2017",
  ]

if __name__=='__main__':
  main()
