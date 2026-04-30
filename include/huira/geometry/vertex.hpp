#pragma once

#include <array>
#include <optional>
#include <vector>

#include "huira/core/types.hpp"
#include "huira/concepts/spectral_concepts.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    struct Vertex {
        Vec3<float> position{};
        TSpectral albedo{ 1 };
        Vec3<float> normal{ 0 };
        Vec2<float> uv{ 0 };

        bool operator==(const Vertex& other) const {
            return position == other.position &&
                albedo == other.albedo &&
                normal == other.normal &&
                uv == other.uv;
        }
    };

    typedef std::vector<std::uint32_t> IndexBuffer;

    template <IsSpectral TSpectral>
    using VertexBuffer = std::vector<Vertex<TSpectral>>;

    struct Tangent {
        Vec3<float> tangent{ 0,0,0 };
        Vec3<float> bitangent{ 0,0,0 };
    };
    typedef std::vector<Tangent> TangentBuffer;

    template <IsSpectral TSpectral>
    struct Triangle {
        std::array<Vertex<TSpectral>, 3> vertices;
        std::optional<std::array<Tangent, 3>> tangents;
    };
}