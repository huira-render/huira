#include <algorithm>

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
        vertex_buffer_(std::move(vertex_buffer)),
        id_(next_id_++)
    {
        const auto c = vertex_buffer_.size();
        if (std::ranges::any_of(index_buffer_, [c](std::uint32_t i) { return i >= c; })) {
            HUIRA_THROW_ERROR("Mesh::Mesh - index_buffer contains out-of-bounds indices.");
        }
    }

    template <IsSpectral TSpectral>
    Mesh<TSpectral>::Mesh(IndexBuffer index_buffer, VertexBuffer<TSpectral> vertex_buffer, TangentBuffer tangent_buffer)
        : index_buffer_(std::move(index_buffer)),
        vertex_buffer_(std::move(vertex_buffer)),
        tangent_buffer_(std::move(tangent_buffer)),
        id_(next_id_++)
    {
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
    Mesh<TSpectral>::~Mesh()
    {
        if (blas_) {
            rtcReleaseScene(blas_);
        }
    }

    template <IsSpectral TSpectral>
    Mesh<TSpectral>::Mesh(Mesh&& other) noexcept
        : index_buffer_(std::move(other.index_buffer_)),
        vertex_buffer_(std::move(other.vertex_buffer_)),
        material_(other.material_),
        tangent_buffer_(std::move(other.tangent_buffer_)),
        device_(other.device_),
        blas_(other.blas_),
        id_(other.id_)
    {
        other.material_ = nullptr;
        other.device_ = nullptr;
        other.blas_ = nullptr;
    }

    template <IsSpectral TSpectral>
    Mesh<TSpectral>& Mesh<TSpectral>::operator=(Mesh&& other) noexcept
    {
        if (this != &other) {
            if (blas_) {
                rtcReleaseScene(blas_);
            }

            index_buffer_ = std::move(other.index_buffer_);
            vertex_buffer_ = std::move(other.vertex_buffer_);
            material_ = other.material_;
            tangent_buffer_ = std::move(other.tangent_buffer_);
            device_ = other.device_;
            blas_ = other.blas_;
            id_ = other.id_;

            other.material_ = nullptr;
            other.device_ = nullptr;
            other.blas_ = nullptr;
        }
        return *this;
    }

    /**
     * @brief Returns the Embree BLAS scene for this mesh, building it if necessary.
     *
     * On first call, constructs the BLAS using shared buffers (zero-copy) over
     * the mesh's vertex and index data. Subsequent calls return the cached BLAS.
     * Requires that set_device() has been called (typically by Scene::add_mesh()).
     *
     * @return The committed RTCScene containing the triangle geometry.
     */
    template <IsSpectral TSpectral>
    RTCScene Mesh<TSpectral>::blas() const
    {
        if (!blas_) {
            if (!device_) {
                HUIRA_THROW_ERROR("Mesh::blas - Cannot build BLAS: no RTCDevice assigned. "
                    "Ensure the mesh has been added to a Scene via add_mesh().");
            }
            build_blas_();
        }
        return blas_;
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
        RTCGeometry geom = rtcNewGeometry(device_, RTC_GEOMETRY_TYPE_TRIANGLE);
        if (!geom) {
            HUIRA_THROW_ERROR("Mesh::build_blas_ - Failed to create Embree geometry (error: "
                + std::to_string(rtcGetDeviceError(device_)) + ").");
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
        RTCError buffer_error = rtcGetDeviceError(device_);
        if (buffer_error != RTC_ERROR_NONE) {
            rtcReleaseGeometry(geom);
            HUIRA_THROW_ERROR("Mesh::build_blas_ - Failed to set shared geometry buffers (error: "
                + std::to_string(buffer_error) + ").");
        }

        rtcCommitGeometry(geom);

        blas_ = rtcNewScene(device_);
        if (!blas_) {
            rtcReleaseGeometry(geom);
            HUIRA_THROW_ERROR("Mesh::build_blas_ - Failed to create Embree BLAS scene (error: "
                + std::to_string(rtcGetDeviceError(device_)) + ").");
        }

        rtcAttachGeometry(blas_, geom);
        rtcReleaseGeometry(geom);

        rtcCommitScene(blas_);

        HUIRA_LOG_INFO("Built BLAS for Mesh " + std::to_string(id_) +
            " (vertices: " + std::to_string(vertex_buffer_.size()) +
            ", triangles: " + std::to_string(index_buffer_.size() / 3) + ")");
    }

    template <IsSpectral TSpectral>
    void Mesh<TSpectral>::set_material(Material<TSpectral>* material)
    {
        material_ = material;
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
