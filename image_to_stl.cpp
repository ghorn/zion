#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>

void ReadInputs(int64_t *nx, int64_t *ny, std::vector<double> *image);
void WriteStl(const int64_t nx, const int64_t ny, const std::vector<double> &image);

int main() {
  int64_t nx = 0;
  int64_t ny = 0;
  std::vector<double> image;

  // Read inputs.
  ReadInputs(&nx, &ny, &image);

  WriteStl(nx, ny, image);
  
  std::cout << "Success." << std::endl;
}

void ReadInputs(int64_t *nx, int64_t *ny, std::vector<double> *image) {
  std::cout << "Reading input file..." << std::endl;
  std::ifstream image_file("yolo.dat", std::ios::in|std::ios::binary);
  if (!image_file.is_open()) {
    std::cout << "Failed to open image." << std::endl;
    std::exit(1);
  }

  image_file.read(reinterpret_cast<char*>(nx), 8);
  image_file.read(reinterpret_cast<char*>(ny), 8);
  std::cout << "Reading (" << *nx << " x " << *ny << ") doubles..." << std::endl;

  image->resize((*nx) * (*ny), 0);
  image_file.read(reinterpret_cast<char*>(image->data()), (*nx)*(*ny)*8);

  if (image_file) {
    std::cout << "Read " << (*nx)*(*ny) << " floats successfully." << std::endl;
  } else {
    std::cout << "error: only " << image_file.gcount() << " bytes could be read" << std::endl;
    std::exit(1);
  }
  image_file.close();
}

static inline float IndexImage(const int64_t kx, const int64_t ky,
                               const std::vector<double> &image,
                               const int64_t nx) {
  return static_cast<float>(image[kx + ky * nx]);
}

static inline void WriteTriangle(std::ofstream *stl_file,
                                 const int64_t center_x_int, const int64_t center_y_int, float center_z,
                                 const int64_t left_x_int, const int64_t left_y_int, float left_z,
                                 const int64_t right_x_int, const int64_t right_y_int, float right_z) {
  center_z = center_z / 100;
  left_z = left_z / 100;
  right_z = right_z / 100;
  
  // Convert int indices to meters.
  const float center_x = static_cast<float>(center_x_int) / 100;
  const float center_y = static_cast<float>(center_y_int) / 100;

  const float left_x = static_cast<float>(left_x_int) / 100;
  const float left_y = static_cast<float>(left_y_int) / 100;

  const float right_x = static_cast<float>(right_x_int) / 100;
  const float right_y = static_cast<float>(right_y_int) / 100;

  // Compute cross product.
  const float delta0_x = left_x - center_x;
  const float delta0_y = left_y - center_y;
  const float delta0_z = left_z - center_z;

  const float delta1_x = right_x - center_x;
  const float delta1_y = right_y - center_y;
  const float delta1_z = right_z - center_z;

  float normal_x = delta0_y*delta1_z - delta0_z*delta1_y;
  float normal_y = delta0_z*delta1_x - delta0_x*delta1_z;
  float normal_z = delta0_x*delta1_y - delta0_y*delta1_x;

  if (std::isnan(center_x)) {std::cout << "NAN!" << std::endl; std::exit(1);}
  if (std::isnan(center_y)) {std::cout << "NAN!" << std::endl; std::exit(1);}
  if (std::isnan(center_z)) {std::cout << "NAN!" << std::endl; std::exit(1);}

  if (std::isnan(left_x)) {std::cout << "NAN!" << std::endl; std::exit(1);}
  if (std::isnan(left_y)) {std::cout << "NAN!" << std::endl; std::exit(1);}
  if (std::isnan(left_z)) {std::cout << "NAN!" << std::endl; std::exit(1);}

  if (std::isnan(right_x)) {std::cout << "NAN!" << std::endl; std::exit(1);}
  if (std::isnan(right_y)) {std::cout << "NAN!" << std::endl; std::exit(1);}
  if (std::isnan(right_z)) {std::cout << "NAN!" << std::endl; std::exit(1);}

  if (std::isnan(normal_x)) {std::cout << "NAN!" << std::endl; std::exit(1);}
  if (std::isnan(normal_y)) {std::cout << "NAN!" << std::endl; std::exit(1);}
  if (std::isnan(normal_z)) {std::cout << "NAN!" << std::endl; std::exit(1);}

  // Make all normals positive.
  // It would be more efficient to guarantee this by construction but too bad.
  if (normal_z < 0) {
    normal_x = - normal_x;
    normal_y = - normal_y;
    normal_z = - normal_z;
    //std::cout << "Got negative normal" << std::endl;
    //std::exit(1);
  }

  // Write normal
  stl_file->write(reinterpret_cast<const char*>(&normal_x), 4);
  stl_file->write(reinterpret_cast<const char*>(&normal_y), 4);
  stl_file->write(reinterpret_cast<const char*>(&normal_z), 4);

  // Write vertex 1.
  stl_file->write(reinterpret_cast<const char*>(&left_x), 4);
  stl_file->write(reinterpret_cast<const char*>(&left_y), 4);
  stl_file->write(reinterpret_cast<const char*>(&left_z), 4);

  // Write vertex 2.
  stl_file->write(reinterpret_cast<const char*>(&center_x), 4);
  stl_file->write(reinterpret_cast<const char*>(&center_y), 4);
  stl_file->write(reinterpret_cast<const char*>(&center_z), 4);

  // Write vertex 3.
  stl_file->write(reinterpret_cast<const char*>(&right_x), 4);
  stl_file->write(reinterpret_cast<const char*>(&right_y), 4);
  stl_file->write(reinterpret_cast<const char*>(&right_z), 4);

  // Write standard STL zero.
  uint16_t zero = 0;
  stl_file->write(reinterpret_cast<const char*>(&zero), 2);
}

void WriteStl(const int64_t nx, const int64_t ny, const std::vector<double> &image) {
  std::ofstream stl_file("yolo.stl", std::ios::out|std::ios::binary);
  if (!stl_file.is_open()) {
    std::cout << "Failed to open output stl file." << std::endl;
    std::exit(1);
  }

  std::cout << "Counting number of triangles..." << std::endl;

  const float nanf = std::numeric_limits<float>::quiet_NaN();

  // count triangles
  int64_t num_triangles = 0;
  for (int64_t kx = 0; kx < nx; kx++) {
    if (!(kx % 100)) {
      std::cout << "counted " << (100*double(kx)/double(nx)) << " %" << std::endl;
    }
    for (int64_t ky = 0; ky < ny; ky++) {
      const float center = IndexImage(kx, ky, image, nx);
      if (!std::isnan(center)) {
        const float x_minus = (kx == 0) ? nanf : IndexImage(kx - 1, ky, image, nx);
        const float x_plus = (kx == nx - 1) ? nanf : IndexImage(kx + 1, ky, image, nx);
        const float y_minus = (ky == 0) ? nanf : IndexImage(kx, ky - 1, image, nx);
        const float y_plus = (ky == ny - 1) ? nanf : IndexImage(kx, ky + 1, image, nx);

        if (!std::isnan(x_minus) && !std::isnan(y_minus)) {
          num_triangles++;
        }
        if (!std::isnan(x_minus) && !std::isnan(y_plus)) {
          num_triangles++;
        }
        if (!std::isnan(x_plus) && !std::isnan(y_plus)) {
          num_triangles++;
        }
        if (!std::isnan(x_plus) && !std::isnan(y_minus)) {
          num_triangles++;
        }
      }
    }
  }
  std::cout << "Will write " << num_triangles << " triangles." << std::endl;
  const int64_t output_file_size = 80 + 4 + num_triangles*(12*4+2);
  std::cout << "File will be about " << float(output_file_size)/1024/1024/1024 << " GB." << std::endl;

  const char header[80] = {};
  const uint32_t num_triangles_unsigned = static_cast<uint32_t>(num_triangles);
  if (static_cast<int64_t>(num_triangles_unsigned) != num_triangles) {
    std::cout << "Too many triangles!!! (" << num_triangles << ", " << num_triangles_unsigned << ")" << std::endl;
    std::exit(1);
  }

  // Write header.
  stl_file.write(header, 80);
  stl_file.write(reinterpret_cast<const char*>(&num_triangles_unsigned), 4);

  // Write triangles.
  for (int64_t kx = 0; kx < nx; kx++) {
    if (!(kx % 100)) {
      std::cout << "wrote " << (100*double(kx)/double(nx)) << " %" << std::endl;
    }
    for (int64_t ky = 0; ky < ny; ky++) {
      const float center = IndexImage(kx, ky, image, nx);
      if (!std::isnan(center)) {
        const float x_minus = (kx == 0) ? nanf : IndexImage(kx - 1, ky, image, nx);
        const float x_plus = (kx == nx - 1) ? nanf : IndexImage(kx + 1, ky, image, nx);
        const float y_minus = (ky == 0) ? nanf : IndexImage(kx, ky - 1, image, nx);
        const float y_plus = (ky == ny - 1) ? nanf : IndexImage(kx, ky + 1, image, nx);
        // std::cout << "center  " << center << std::endl;
        // std::cout << "x_minus " << x_minus << std::endl;
        // std::cout << "x_plus  " << x_plus << std::endl;
        // std::cout << "y_minus " << y_minus << std::endl;
        // std::cout << "y_plus  " << y_plus << std::endl;

        if (!std::isnan(x_minus) && !std::isnan(y_minus)) {
          WriteTriangle(&stl_file,
                        kx, ky, center,
                        kx-1, ky, x_minus,
                        kx, ky-1, y_minus);
        }
        if (!std::isnan(x_minus) && !std::isnan(y_plus)) {
          WriteTriangle(&stl_file,
                        kx, ky, center,
                        kx-1, ky, x_minus,
                        kx, ky+1, y_plus);
        }
        if (!std::isnan(x_plus) && !std::isnan(y_plus)) {
          WriteTriangle(&stl_file,
                        kx, ky, center,
                        kx+1, ky, x_plus,
                        kx, ky+1, y_plus);
        }
        if (!std::isnan(x_plus) && !std::isnan(y_minus)) {
          WriteTriangle(&stl_file,
                        kx, ky, center,
                        kx+1, ky, x_plus,
                        kx, ky-1, y_minus);
        }
      }
    }
  }
  stl_file.close();
}
