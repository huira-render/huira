#pragma once

#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>

#include "huira/core/spectral_bins.hpp"
#include "huira/images/image.hpp"

namespace huira {
inline Image<huira::RGB> normal_map(Image<Vec3<float>> normals)
{
    Image<huira::RGB> output(normals.resolution());
    for (std::size_t i = 0; i < normals.size(); ++i) {
        Vec3<float> n = normals[i];
        output[i] = huira::RGB{(n.x * 0.5f + 0.5f), (n.y * 0.5f + 0.5f), (n.z * 0.5f + 0.5f)};
    }
    return output;
}

inline Image<huira::RGB> depth_map(Image<float> depth)
{
    Image<huira::RGB> output(depth.resolution());
    float min_depth = std::numeric_limits<float>::infinity();
    float max_depth = 0.f;
    for (std::size_t i = 0; i < depth.size(); ++i) {
        float d = depth[i];
        if (std::isinf(d)) {
            continue;
        }
        if (d < min_depth) {
            min_depth = d;
        }
        if (d > max_depth) {
            max_depth = d;
        }
    }

    for (std::size_t i = 0; i < depth.size(); ++i) {
        if (std::isinf(depth[i])) {
            output[i] = huira::RGB{1, 1, 1};
            continue;
        }
        float d = 0.8f * (depth[i] - min_depth) / (max_depth - min_depth);
        output[i] = huira::RGB{d, d, d};
    }
    return output;
}

template <IsSpectral TSpectral>
Image<TSpectral> rgb_to_spectral(
    Image<RGB> rgb_image,
    std::function<TSpectral(RGB)> spectral_conversion = convert_rgb_to_spectral<TSpectral>)
{
    if constexpr (std::same_as<TSpectral, RGB>) {
        return rgb_image;
    } else {
        Image<TSpectral> output(rgb_image.resolution());
        for (std::size_t i = 0; i < rgb_image.size(); ++i) {
            output[i] = spectral_conversion(rgb_image[i]);
        }
        return output;
    }
}
} // namespace huira
