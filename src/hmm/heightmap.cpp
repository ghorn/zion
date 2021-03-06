#include "heightmap.h"

#include <fstream>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/normal.hpp>

#include "blur.h"
#include "src/common/heightmap_data.hpp"

Heightmap::Heightmap(const std::string &path, const float zoffset_fraction) :
    m_Width(0),
    m_Height(0)
{
    ReadHeightmapData(path, &m_Width, &m_Height, &m_Data);

    if (zoffset_fraction > 0) {
        bool initialized = false;
        float lo = m_Data[0];
        float hi = m_Data[0];
        for (int i = 0; i < m_Data.size(); i++) {
            const float z = m_Data[i];
            if (!std::isnan(z)) {
                if (!initialized) {
                  lo = z;
                  hi = z;
                  initialized = true;
                }
                lo = std::min(lo, z);
                hi = std::max(hi, z);
            }
        }

        // compute z offset from min/max height
        // relief == max - min
        // zoff / (relief + zoff) == frac
        // zoff == relief * frac / (1 - frac)
        const float z_offset = (hi - lo) * zoffset_fraction / (1 - zoffset_fraction);
        fprintf(stderr, "z offset: %.2f\n", z_offset);

        for (int i = 0; i < m_Data.size(); i++) {
            m_Data[i] = m_Data[i] + z_offset;
        }
    }

    for (int i = 0; i < m_Width*m_Height; i++) {
        if (std::isnan(m_Data[i])) {
          m_Data[i] = 0.0;
        }
    }
}

Heightmap::Heightmap(
    const int width,
    const int height,
    const std::vector<float> &data) :
    m_Width(width),
    m_Height(height),
    m_Data(data)
{}

void Heightmap::Invert() {
    for (int i = 0; i < m_Data.size(); i++) {
        m_Data[i] = 1.f - m_Data[i];
    }
}

void Heightmap::GammaCurve(const float gamma) {
    for (int i = 0; i < m_Data.size(); i++) {
        m_Data[i] = std::pow(m_Data[i], gamma);
    }
}

void Heightmap::AddBorder(const int size, const float z) {
    const int w = m_Width + size * 2;
    const int h = m_Height + size * 2;
    std::vector<float> data(w * h, z);
    int i = 0;
    for (int y = 0; y < m_Height; y++) {
        int j = (y + size) * w + size;
        for (int x = 0; x < m_Width; x++) {
            data[j++] = m_Data[i++];
        }
    }
    m_Width = w;
    m_Height = h;
    m_Data = data;
}

void Heightmap::GaussianBlur(const int r) {
    m_Data = ::GaussianBlur(m_Data, m_Width, m_Height, r);
}

std::vector<glm::vec3> Heightmap::Normalmap(const float zScale) const {
    const int w = m_Width - 1;
    const int h = m_Height - 1;
    std::vector<glm::vec3> result(w * h);
    int i = 0;
    for (int y0 = 0; y0 < h; y0++) {
        const int y1 = y0 + 1;
        const float yc = y0 + 0.5f;
        for (int x0 = 0; x0 < w; x0++) {
            const int x1 = x0 + 1;
            const float xc = x0 + 0.5f;
            const float z00 = At(x0, y0) * -zScale;
            const float z01 = At(x0, y1) * -zScale;
            const float z10 = At(x1, y0) * -zScale;
            const float z11 = At(x1, y1) * -zScale;
            const float zc = (z00 + z01 + z10 + z11) / 4.f;
            const glm::vec3 p00(x0, y0, z00);
            const glm::vec3 p01(x0, y1, z01);
            const glm::vec3 p10(x1, y0, z10);
            const glm::vec3 p11(x1, y1, z11);
            const glm::vec3 pc(xc, yc, zc);
            const glm::vec3 n0 = glm::triangleNormal(pc, p00, p10);
            const glm::vec3 n1 = glm::triangleNormal(pc, p10, p11);
            const glm::vec3 n2 = glm::triangleNormal(pc, p11, p01);
            const glm::vec3 n3 = glm::triangleNormal(pc, p01, p00);
            result[i] = glm::normalize(n0 + n1 + n2 + n3);
            i++;
        }
    }
    return result;
}

std::pair<glm::ivec2, float> Heightmap::FindCandidate(
    const glm::ivec2 p0,
    const glm::ivec2 p1,
    const glm::ivec2 p2) const
{
    const auto edge = [](
        const glm::ivec2 a, const glm::ivec2 b, const glm::ivec2 c)
    {
        return (b.x - c.x) * (a.y - c.y) - (b.y - c.y) * (a.x - c.x);
    };

    // triangle bounding box
    const glm::ivec2 min = glm::min(glm::min(p0, p1), p2);
    const glm::ivec2 max = glm::max(glm::max(p0, p1), p2);

    // forward differencing variables
    int w00 = edge(p1, p2, min);
    int w01 = edge(p2, p0, min);
    int w02 = edge(p0, p1, min);
    const int a01 = p1.y - p0.y;
    const int b01 = p0.x - p1.x;
    const int a12 = p2.y - p1.y;
    const int b12 = p1.x - p2.x;
    const int a20 = p0.y - p2.y;
    const int b20 = p2.x - p0.x;

    // pre-multiplied z values at vertices
    const float a = edge(p0, p1, p2);
    const float z0 = At(p0) / a;
    const float z1 = At(p1) / a;
    const float z2 = At(p2) / a;

    // iterate over pixels in bounding box
    float maxError = 0;
    glm::ivec2 maxPoint(0);
    for (int y = min.y; y <= max.y; y++) {
        // compute starting offset
        int dx = 0;
        if (w00 < 0 && a12 != 0) {
            dx = std::max(dx, -w00 / a12);
        }
        if (w01 < 0 && a20 != 0) {
            dx = std::max(dx, -w01 / a20);
        }
        if (w02 < 0 && a01 != 0) {
            dx = std::max(dx, -w02 / a01);
        }

        int w0 = w00 + a12 * dx;
        int w1 = w01 + a20 * dx;
        int w2 = w02 + a01 * dx;

        bool wasInside = false;

        for (int x = min.x + dx; x <= max.x; x++) {
            // check if inside triangle
            if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                wasInside = true;

                // compute z using barycentric coordinates
                const float z = z0 * w0 + z1 * w1 + z2 * w2;
                const float dz = std::abs(z - At(x, y));
                if (dz > maxError) {
                    maxError = dz;
                    maxPoint = glm::ivec2(x, y);
                }
            } else if (wasInside) {
                break;
            }

            w0 += a12;
            w1 += a20;
            w2 += a01;
        }

        w00 += b12;
        w01 += b20;
        w02 += b01;
    }

    if (maxPoint == p0 || maxPoint == p1 || maxPoint == p2) {
        maxError = 0;
    }

    return std::make_pair(maxPoint, maxError);
}
