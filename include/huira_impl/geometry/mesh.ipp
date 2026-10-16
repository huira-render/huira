#include <algorithm>
#include <cmath>
#include <string>

#include "embree4/rtcore.h"
#include "glm/glm.hpp"
#include "huira/geometry/vertex.hpp"
#include "huira/render/interaction.hpp"
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
    : index_buffer_(std::move(index_buffer)), vertex_buffer_(std::move(vertex_buffer))
{
    HUIRA_TRACE_SCOPE("Mesh::Mesh(index_buffer, vertex_buffer)");
    check_buffers_();
}

/**
 * @brief Throws unless the buffers make triangles: indices in threes, each of a vertex there is,
 * and every vertex finite, with an albedo that is finite and non-negative, and any tangents
 * finite.
 */
template <IsSpectral TSpectral>
void Mesh<TSpectral>::check_buffers_() const
{
    if (index_buffer_.size() % 3 != 0) {
        HUIRA_THROW_ERROR("Mesh::Mesh - index_buffer must hold three indices per triangle, but "
                          "holds " +
                          std::to_string(index_buffer_.size()));
    }
    const auto c = vertex_buffer_.size();
    if (std::ranges::any_of(index_buffer_, [c](std::uint32_t i) { return i >= c; })) {
        HUIRA_THROW_ERROR("Mesh::Mesh - index_buffer contains out-of-bounds indices.");
    }
    const auto finite = [](const auto& v) {
        for (int i = 0; i < v.length(); ++i) {
            if (!std::isfinite(v[i])) {
                return false;
            }
        }
        return true;
    };
    for (std::size_t k = 0; k < vertex_buffer_.size(); ++k) {
        const auto& vertex = vertex_buffer_[k];
        if (!finite(vertex.position) || !finite(vertex.normal) || !finite(vertex.uv) ||
            !vertex.albedo.valid()) {
            HUIRA_THROW_ERROR("Mesh::Mesh - Vertex " + std::to_string(k) +
                              " must have a finite position, normal and uv, and a finite, "
                              "non-negative albedo");
        }
    }
    for (std::size_t k = 0; k < tangent_buffer_.size(); ++k) {
        if (!finite(tangent_buffer_[k].tangent) || !finite(tangent_buffer_[k].bitangent)) {
            HUIRA_THROW_ERROR("Mesh::Mesh - The tangent and bitangent of vertex " +
                              std::to_string(k) + " must be finite");
        }
    }
}

template <IsSpectral TSpectral>
Mesh<TSpectral>::Mesh(IndexBuffer index_buffer,
                      VertexBuffer<TSpectral> vertex_buffer,
                      TangentBuffer tangent_buffer)
    : index_buffer_(std::move(index_buffer)), vertex_buffer_(std::move(vertex_buffer)),
      tangent_buffer_(std::move(tangent_buffer))
{
    HUIRA_TRACE_SCOPE("Mesh::Mesh(index_buffer, vertex_buffer, tangent_buffer)");
    check_buffers_();

    if (tangent_buffer_.size() != 0 && tangent_buffer_.size() != vertex_buffer_.size()) {
        HUIRA_THROW_ERROR("Mesh::Mesh - tangent_buffer must be the same size as vertex_buffer.");
    }
}

/**
 * @brief Fills in the surface at a hit, by interpolating the hit triangle's vertices.
 *
 * The tangent frame is interpolated from the tangent buffer. A mesh without one (made without
 * tangents, or loaded from a model without texture coordinates) gets a frame built around the
 * hit's shading normal instead, so that every hit has one. It is built per hit rather than per
 * vertex: per-vertex frames can point opposite ways at a triangle's corners, as any frame field
 * over a closed surface must somewhere, and interpolate to zero between them.
 *
 * @param hit The hit, on one of this mesh's triangles.
 * @param isect The interaction to fill in, in the mesh's own frame.
 */
template <IsSpectral TSpectral>
void Mesh<TSpectral>::compute_surface_interaction(const HitRecord& hit,
                                                  Interaction<TSpectral>& isect) const
{
    std::uint32_t idx0 = index_buffer_[hit.prim_id * 3 + 0];
    std::uint32_t idx1 = index_buffer_[hit.prim_id * 3 + 1];
    std::uint32_t idx2 = index_buffer_[hit.prim_id * 3 + 2];

    float w = 1.0f - hit.u - hit.v;

    isect.position = w * vertex_buffer_[idx0].position + hit.u * vertex_buffer_[idx1].position +
                     hit.v * vertex_buffer_[idx2].position;

    isect.normal_s = w * vertex_buffer_[idx0].normal + hit.u * vertex_buffer_[idx1].normal +
                     hit.v * vertex_buffer_[idx2].normal;

    isect.uv = w * vertex_buffer_[idx0].uv + hit.u * vertex_buffer_[idx1].uv +
               hit.v * vertex_buffer_[idx2].uv;

    isect.vertex_albedo = w * vertex_buffer_[idx0].albedo + hit.u * vertex_buffer_[idx1].albedo +
                          hit.v * vertex_buffer_[idx2].albedo;

    // Interpolate tangent frame:
    if (has_tangents()) {
        isect.tangent = w * tangent_buffer_[idx0].tangent + hit.u * tangent_buffer_[idx1].tangent +
                        hit.v * tangent_buffer_[idx2].tangent;
        isect.bitangent = w * tangent_buffer_[idx0].bitangent +
                          hit.u * tangent_buffer_[idx1].bitangent +
                          hit.v * tangent_buffer_[idx2].bitangent;
    } else {
        // Without tangents, any frame around the shading normal will do. (Left at zero, the
        // tangent was normalized into NaN when the hit was resolved.)
        build_default_tangent_frame(glm::normalize(isect.normal_s), isect.tangent, isect.bitangent);
    }
}

template <IsSpectral TSpectral>
Vec2<float> Mesh<TSpectral>::compute_uv(const HitRecord& hit) const
{
    std::uint32_t idx0 = index_buffer_[hit.prim_id * 3 + 0];
    std::uint32_t idx1 = index_buffer_[hit.prim_id * 3 + 1];
    std::uint32_t idx2 = index_buffer_[hit.prim_id * 3 + 2];

    float w = 1.0f - hit.u - hit.v;

    return w * vertex_buffer_[idx0].uv + hit.u * vertex_buffer_[idx1].uv +
           hit.v * vertex_buffer_[idx2].uv;
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
        HUIRA_THROW_ERROR(
            "Mesh::build_blas_ - Failed to create Embree geometry (error: " +
            std::to_string(static_cast<int>(rtcGetDeviceError(this->device_->get()))) + ").");
    }

    rtcSetSharedGeometryBuffer(geom,
                               RTC_BUFFER_TYPE_VERTEX,
                               0,
                               RTC_FORMAT_FLOAT3,
                               vertex_buffer_.data(),
                               0,
                               sizeof(Vertex<TSpectral>),
                               vertex_buffer_.size());

    rtcSetSharedGeometryBuffer(geom,
                               RTC_BUFFER_TYPE_INDEX,
                               0,
                               RTC_FORMAT_UINT3,
                               index_buffer_.data(),
                               0,
                               3 * sizeof(std::uint32_t),
                               index_buffer_.size() / 3);

    // Check for errors after setting shared buffers (e.g. invalid stride, null pointer):
    RTCError buffer_error = rtcGetDeviceError(this->device_->get());
    if (buffer_error != RTC_ERROR_NONE) {
        rtcReleaseGeometry(geom);
        HUIRA_THROW_ERROR("Mesh::build_blas_ - Failed to set shared geometry buffers (error: " +
                          std::to_string(static_cast<int>(buffer_error)) + ").");
    }

    rtcCommitGeometry(geom);

    this->blas_.reset(rtcNewScene(this->device_->get()));
    if (!this->blas_) {
        rtcReleaseGeometry(geom);
        HUIRA_THROW_ERROR(
            "Mesh::build_blas_ - Failed to create Embree BLAS scene (error: " +
            std::to_string(static_cast<int>(rtcGetDeviceError(this->device_->get()))) + ").");
    }

    // Robust traversal: required so BVH plane tests cannot drop marginal hits
    // at extreme coordinate ranges (planetary camera-relative scenes).
    rtcSetSceneFlags(this->blas_.get(), RTC_SCENE_FLAG_ROBUST);

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
std::size_t Mesh<TSpectral>::vertex_count() const noexcept
{
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
} // namespace huira
