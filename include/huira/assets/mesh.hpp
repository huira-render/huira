#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <embree4/rtcore.h>

#include "huira/core/types.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/materials/material.hpp"
#include "huira/scene/scene_object.hpp"

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
    class Mesh : public SceneObject<Mesh<TSpectral>> {
    public:
        Mesh() : id_(next_id_++) {}
        Mesh(IndexBuffer index_buffer, VertexBuffer<TSpectral> vertex_buffer);
        Mesh(IndexBuffer index_buffer, VertexBuffer<TSpectral> vertex_buffer, TangentBuffer tangent_buffer);
        ~Mesh() override;

        Mesh(const Mesh&) = delete;
        Mesh& operator=(const Mesh&) = delete;

        Mesh(Mesh&& other) noexcept;
        Mesh& operator=(Mesh&& other) noexcept;

        void set_device(RTCDevice device) noexcept { device_ = device; }

        void set_material(Material<TSpectral>* material);
        [[nodiscard]] const Material<TSpectral>* material() const noexcept { return material_; }

        std::size_t index_count() const noexcept;
        std::size_t vertex_count() const noexcept;
        std::size_t triangle_count() const noexcept;

        [[nodiscard]] const IndexBuffer& index_buffer() const noexcept;
        [[nodiscard]] const VertexBuffer<TSpectral>& vertex_buffer() const noexcept;
        [[nodiscard]] const TangentBuffer& tangent_buffer() const noexcept;

        [[nodiscard]] bool has_tangents() const noexcept { return !tangent_buffer_.empty(); }

        [[nodiscard]] RTCScene blas() const;

        std::uint64_t id() const override { return id_; }
        std::string type() const override { return "Mesh"; }

    private:
        void build_blas_() const;

        IndexBuffer index_buffer_;
        VertexBuffer<TSpectral> vertex_buffer_;

        Material<TSpectral>* material_ = nullptr;
        TangentBuffer tangent_buffer_;

        RTCDevice device_ = nullptr;
        mutable RTCScene blas_ = nullptr;

        std::uint64_t id_ = 0;
        static inline std::uint64_t next_id_ = 0;
    };

}

#include "huira_impl/assets/mesh.ipp"
