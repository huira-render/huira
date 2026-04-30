#include <algorithm>

#include "embree4/rtcore.h"

#include "huira/geometry/vertex.hpp"
#include "huira/util/logger.hpp"

namespace huira {

    /**
     * @brief Constructs a Mesh from index and vertex buffers.
     *
     * Creates a new mesh by taking ownership of the provided buffers. The index
     * buffer should contain triangle indices (groups of three) that reference
     * vertices in the vertex buffer. The Embree BLAS is not built here — it is
     * constructed lazily on first call to blas() after a device has been assigned.
     *
     * @param index_buffer Buffer containing triangle vertex indices.
     * @param vertex_buffer Buffer containing vertex data (positions, normals, etc.).
     */
    template <IsSpectral TSpectral>
    Mesh<TSpectral>::Mesh(IndexBuffer index_buffer, VertexBuffer<TSpectral> vertex_buffer)
        : index_buffer_(std::move(index_buffer)),
        vertex_buffer_(std::move(vertex_buffer))
    {
        HUIRA_TRACE_SCOPE("Mesh::Mesh(index_buffer, vertex_buffer)");
        const auto c = vertex_buffer_.size();
        if (std::ranges::any_of(index_buffer_, [c](std::uint32_t i) { return i >= c; })) {
            HUIRA_THROW_ERROR("Mesh::Mesh - index_buffer contains out-of-bounds indices.");
        }
    }

    template <IsSpectral TSpectral>
    Mesh<TSpectral>::Mesh(IndexBuffer index_buffer, VertexBuffer<TSpectral> vertex_buffer, TangentBuffer tangent_buffer)
        : index_buffer_(std::move(index_buffer)),
        vertex_buffer_(std::move(vertex_buffer)),
        tangent_buffer_(std::move(tangent_buffer))
    {
        HUIRA_TRACE_SCOPE("Mesh::Mesh(index_buffer, vertex_buffer, tangent_buffer)");
        const auto vertex_count = vertex_buffer_.size();
        if (std::ranges::any_of(index_buffer_, [vertex_count](std::uint32_t idx) {
            return idx >= vertex_count;
            })) {
            HUIRA_THROW_ERROR("Mesh::Mesh - index_buffer contains out-of-bounds indices.");
        }

        if (tangent_buffer_.size() != 0 && tangent_buffer_.size() != vertex_buffer_.size()) {
            HUIRA_THROW_ERROR("Mesh::Mesh - tangent_buffer must be the same size as vertex_buffer.");
        }
    }

    template <IsSpectral TSpectral>
    void Mesh<TSpectral>::compute_surface_interaction(const HitRecord& hit, Interaction<TSpectral>& isect) const
    {
        std::uint32_t idx0 = index_buffer_[hit.prim_id * 3 + 0];
        std::uint32_t idx1 = index_buffer_[hit.prim_id * 3 + 1];
        std::uint32_t idx2 = index_buffer_[hit.prim_id * 3 + 2];

        float w = 1.0f - hit.u - hit.v;

        isect.position = w * vertex_buffer_[idx0].position
            + hit.u * vertex_buffer_[idx1].position
            + hit.v * vertex_buffer_[idx2].position;

        isect.normal_s = w * vertex_buffer_[idx0].normal
            + hit.u * vertex_buffer_[idx1].normal
            + hit.v * vertex_buffer_[idx2].normal;

        isect.uv = w * vertex_buffer_[idx0].uv
            + hit.u * vertex_buffer_[idx1].uv
            + hit.v * vertex_buffer_[idx2].uv;

        isect.vertex_albedo = w * vertex_buffer_[idx0].albedo
            + hit.u * vertex_buffer_[idx1].albedo
            + hit.v * vertex_buffer_[idx2].albedo;

        // Interpolate tangent frame:
        isect.tangent = Vec3<float>{ 0.0f };
        isect.bitangent = Vec3<float>{ 0.0f };
        if (has_tangents()) {
            isect.tangent = w * tangent_buffer_[idx0].tangent
                + hit.u * tangent_buffer_[idx1].tangent
                + hit.v * tangent_buffer_[idx2].tangent;
            isect.bitangent = w * tangent_buffer_[idx0].bitangent
                + hit.u * tangent_buffer_[idx1].bitangent
                + hit.v * tangent_buffer_[idx2].bitangent;
        }
    }

    template <IsSpectral TSpectral>
    Vec2<float> Mesh<TSpectral>::compute_uv(const HitRecord& hit) const
    {
        std::uint32_t idx0 = index_buffer_[hit.prim_id * 3 + 0];
        std::uint32_t idx1 = index_buffer_[hit.prim_id * 3 + 1];
        std::uint32_t idx2 = index_buffer_[hit.prim_id * 3 + 2];

        float w = 1.0f - hit.u - hit.v;

        return w * vertex_buffer_[idx0].uv
            + hit.u * vertex_buffer_[idx1].uv
            + hit.v * vertex_buffer_[idx2].uv;
    }

    /**
     * @brief Builds the Embree BLAS for this mesh.
     *
     * Creates a single triangle geometry using shared buffers (zero-copy) that
     * point directly into the mesh's vertex and index data. The vertex buffer is
     * shared with a stride of sizeof(Vertex<TSpectral>), so Embree reads the
     * float3 position at the start of each vertex struct and skips over the
     * remaining fields (albedo, normal, uv).
     *
     * The resulting RTCScene is committed and ready to be instanced in a TLAS.
     */
    template <IsSpectral TSpectral>
    void Mesh<TSpectral>::build_blas_() const
    {
        RTCGeometry geom = rtcNewGeometry(this->device_->get(), RTC_GEOMETRY_TYPE_TRIANGLE);
        if (!geom) {
            HUIRA_THROW_ERROR("Mesh::build_blas_ - Failed to create Embree geometry (error: "
                + std::to_string(static_cast<int>(rtcGetDeviceError(this->device_->get()))) + ").");
        }

        rtcSetSharedGeometryBuffer(geom,
            RTC_BUFFER_TYPE_VERTEX, 0,
            RTC_FORMAT_FLOAT3,
            vertex_buffer_.data(),
            0,
            sizeof(Vertex<TSpectral>),
            vertex_buffer_.size());

        rtcSetSharedGeometryBuffer(geom,
            RTC_BUFFER_TYPE_INDEX, 0,
            RTC_FORMAT_UINT3,
            index_buffer_.data(),
            0,
            3 * sizeof(std::uint32_t),
            index_buffer_.size() / 3);

        // Check for errors after setting shared buffers (e.g. invalid stride, null pointer):
        RTCError buffer_error = rtcGetDeviceError(this->device_->get());
        if (buffer_error != RTC_ERROR_NONE) {
            rtcReleaseGeometry(geom);
            HUIRA_THROW_ERROR("Mesh::build_blas_ - Failed to set shared geometry buffers (error: "
                + std::to_string(static_cast<int>(buffer_error)) + ").");
        }

        rtcCommitGeometry(geom);

        this->blas_.reset(rtcNewScene(this->device_->get()));
        if (!this->blas_) {
            rtcReleaseGeometry(geom);
            HUIRA_THROW_ERROR("Mesh::build_blas_ - Failed to create Embree BLAS scene (error: "
                + std::to_string(static_cast<int>(rtcGetDeviceError(this->device_->get()))) + ").");
        }

        rtcAttachGeometry(this->blas_.get(), geom);
        rtcReleaseGeometry(geom);

        rtcCommitScene(this->blas_.get());

        HUIRA_LOG_INFO("Built BLAS for Mesh " + std::to_string(this->id()) +
            " (vertices: " + std::to_string(vertex_buffer_.size()) +
            ", triangles: " + std::to_string(index_buffer_.size() / 3) + ")");
    }

    template <IsSpectral TSpectral>
    std::size_t Mesh<TSpectral>::index_count() const noexcept
    {
        return index_buffer_.size();
    }

    template <IsSpectral TSpectral>
    std::size_t Mesh<TSpectral>::vertex_count() const noexcept {
        return vertex_buffer_.size();
    }

    template <IsSpectral TSpectral>
    std::size_t Mesh<TSpectral>::triangle_count() const noexcept
    {
        return index_buffer_.size() / 3;
    }

    template <IsSpectral TSpectral>
    [[nodiscard]] const IndexBuffer& Mesh<TSpectral>::index_buffer() const noexcept
    {
        return index_buffer_;
    }

    template <IsSpectral TSpectral>
    [[nodiscard]] const VertexBuffer<TSpectral>& Mesh<TSpectral>::vertex_buffer() const noexcept
    {
        return vertex_buffer_;
    }

    template <IsSpectral TSpectral>
    [[nodiscard]] const TangentBuffer& Mesh<TSpectral>::tangent_buffer() const noexcept
    {
        return tangent_buffer_;
    }
}
