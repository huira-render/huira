#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "huira/core/types.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/scene/scene_object.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class Mesh : public SceneObject<Mesh<TSpectral>, TSpectral> {
    public:
        Mesh() : id_(next_id_++) {}
        Mesh(IndexBuffer index_buffer, VertexBuffer<TSpectral> vertex_buffer) noexcept
            : index_buffer_(std::move(index_buffer)),
            vertex_buffer_(std::move(vertex_buffer)),
            id_(next_id_++)
        {}
        ~Mesh() = default;

        Mesh(const Mesh&) = delete;
        Mesh& operator=(const Mesh&) = delete;

        Mesh(Mesh&&) = default;
        Mesh& operator=(Mesh&&) = default;

        std::uint64_t id() const override { return id_; }
        std::string type() const override { return "Mesh"; }

        std::size_t index_count() const noexcept { return index_buffer_.size(); }
        std::size_t vertex_count() const noexcept { return vertex_buffer_.size(); }
        std::size_t triangle_count() const noexcept { return index_buffer_.size() / 3; }

        [[nodiscard]] const IndexBuffer& index_buffer() const noexcept { return index_buffer_; }
        [[nodiscard]] const VertexBuffer<TSpectral>& vertex_buffer() const noexcept { return vertex_buffer_; }

    private:
        IndexBuffer index_buffer_;
        VertexBuffer<TSpectral> vertex_buffer_;

        std::uint64_t id_ = 0;
        static inline std::uint64_t next_id_ = 0;
    };

}

#include "huira_impl/assets/mesh.ipp"
