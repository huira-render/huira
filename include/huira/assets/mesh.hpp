#pragma once

#include <cstdint>

#include "huira/core/types.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class Mesh {
    public:
        Mesh() : id_(next_id_++) {}
        Mesh(IndexBuffer index_buffer, VertexBuffer<TSpectral> vertex_buffer)
            : index_buffer_(std::move(index_buffer)),
            vertex_buffer_(std::move(vertex_buffer)),
            id_(next_id_++)
        {}

        std::uint64_t id() const noexcept { return id_; }

        std::size_t get_index_count() const noexcept { return index_buffer_.size(); }
        std::size_t get_vertex_count() const noexcept { return vertex_buffer_.size(); }
        std::size_t get_triangle_count() const noexcept { return index_buffer_.size() / 3; }

        [[nodiscard]] const IndexBuffer& get_index_buffer() const noexcept { return index_buffer_; }
        [[nodiscard]] const VertexBuffer<TSpectral>& get_vertex_buffer() const noexcept { return vertex_buffer_; }

    private:
        IndexBuffer index_buffer_;
        VertexBuffer<TSpectral> vertex_buffer_;

        std::uint64_t id_ = 0;
        static inline std::uint64_t next_id_ = 0;
    };

}

#include "huira_impl/assets/mesh.ipp"
