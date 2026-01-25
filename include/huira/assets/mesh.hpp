#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "huira/core/types.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class Mesh : public std::enable_shared_from_this<Mesh<TSpectral>> {
    public:
        Mesh() : id_(next_id_++) {}
        Mesh(IndexBuffer index_buffer, VertexBuffer<TSpectral> vertex_buffer)
            : index_buffer_(std::move(index_buffer)),
            vertex_buffer_(std::move(vertex_buffer)),
            id_(next_id_++)
        {}

        std::uint64_t id() const noexcept { return id_; }
        void set_name(const std::string& name) { name_ = name; }
        const std::string& name() const noexcept { return name_; }

        std::string get_info() const { return "Mesh[" + std::to_string(id_) + "]" + (name_.empty() ? "" : " " + name_); }

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
        std::string name_;
    };

}

#include "huira_impl/assets/mesh.ipp"
