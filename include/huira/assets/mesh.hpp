#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "huira/core/types.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/scene/scene_object.hpp"

namespace huira {
    /**
     * @brief Represents a 3D triangle mesh with vertex and index data.
     * 
     * Mesh stores geometry data as indexed triangles, with each triangle defined
     * by three indices into a vertex buffer. The vertex buffer contains positions,
     * normals, and spectral properties. Meshes are movable but not copyable.
     * 
     * @tparam TSpectral The spectral representation type.
     */
    template <IsSpectral TSpectral>
    class Mesh : public SceneObject<Mesh<TSpectral>, TSpectral> {
    public:
        Mesh() : id_(next_id_++) {}
        Mesh(IndexBuffer index_buffer, VertexBuffer<TSpectral> vertex_buffer) noexcept;
        ~Mesh() override = default;

        Mesh(const Mesh&) = delete;
        Mesh& operator=(const Mesh&) = delete;

        Mesh(Mesh&&) = default;
        Mesh& operator=(Mesh&&) = default;

        std::size_t index_count() const noexcept;
        std::size_t vertex_count() const noexcept;
        std::size_t triangle_count() const noexcept;

        [[nodiscard]] const IndexBuffer& index_buffer() const noexcept;
        [[nodiscard]] const VertexBuffer<TSpectral>& vertex_buffer() const noexcept;

        std::uint64_t id() const override { return id_; }
        std::string type() const override { return "Mesh"; }

    private:
        IndexBuffer index_buffer_;
        VertexBuffer<TSpectral> vertex_buffer_;

        std::uint64_t id_ = 0;
        static inline std::uint64_t next_id_ = 0;
    };

}

#include "huira_impl/assets/mesh.ipp"
