#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "embree4/rtcore.h"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"
#include "huira/geometry/geometry.hpp"
#include "huira/geometry/ray.hpp"
#include "huira/geometry/vertex.hpp"
#include "huira/materials/material.hpp"
#include "huira/render/interaction.hpp"

namespace huira {
/**
 * @brief Represents a 3D triangle mesh with vertex and index data.
 *
 * Mesh stores geometry data as indexed triangles, with each triangle defined
 * by three indices into a vertex buffer. The vertex buffer contains positions,
 * normals, and spectral properties. Meshes are movable but not copyable.
 *
 * Each Mesh owns an Embree BLAS (bottom-level acceleration structure) built
 * over its triangle data. The BLAS is constructed lazily on first access via
 * blas(), using shared buffers (zero-copy). The RTCDevice must be assigned
 * (via set_device()) before the BLAS can be built — this is handled
 * automatically by Scene::add_mesh().
 *
 * @tparam TSpectral The spectral representation type.
 */
template <IsSpectral TSpectral>
class Mesh : public Geometry<TSpectral> {
  public:
    Mesh() = default;
    Mesh(IndexBuffer index_buffer, VertexBuffer<TSpectral> vertex_buffer);
    Mesh(IndexBuffer index_buffer,
         VertexBuffer<TSpectral> vertex_buffer,
         TangentBuffer tangent_buffer);
    ~Mesh() override = default;

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    Mesh(Mesh&& other) = default;
    Mesh& operator=(Mesh&& other) = default;

    // Mesh specific data
    std::size_t index_count() const noexcept;
    std::size_t vertex_count() const noexcept;
    std::size_t triangle_count() const noexcept;

    [[nodiscard]] const IndexBuffer& index_buffer() const noexcept;
    [[nodiscard]] const VertexBuffer<TSpectral>& vertex_buffer() const noexcept;
    [[nodiscard]] const TangentBuffer& tangent_buffer() const noexcept;

    [[nodiscard]] bool has_tangents() const noexcept { return !tangent_buffer_.empty(); }

    // Geometry overrides
    void compute_surface_interaction(const HitRecord& hit,
                                     Interaction<TSpectral>& isect) const override;
    Vec2<float> compute_uv(const HitRecord& hit) const override;
    std::string type() const override { return "Mesh"; }

  private:
    void build_blas_() const override;

    IndexBuffer index_buffer_;
    VertexBuffer<TSpectral> vertex_buffer_;
    TangentBuffer tangent_buffer_;
};

} // namespace huira

#include "huira_impl/geometry/mesh.ipp"
