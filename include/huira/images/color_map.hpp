#pragma once

#include <cstddef>

#include "huira/images/image.hpp"

namespace huira {
    inline Image<huira::RGB> normal_map(Image<Vec3<float>> normals)
    {
        Image<huira::RGB> output(normals.resolution());
        for (std::size_t i = 0; i < normals.size(); ++i) {
            Vec3<float> n = normals[i];
            output[i] = huira::RGB{
                (n.x * 0.5f + 0.5f),
                (n.y * 0.5f + 0.5f),
                (n.z * 0.5f + 0.5f)
            };
        }
        return output;
    }
}
