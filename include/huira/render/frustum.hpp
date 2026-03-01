#pragma once

#include <vector>
#include <utility>

#include <glm/glm.hpp>

namespace huira {
    /**
     * @brief Frustum defined by planes through the origin for visibility culling.
     *
     * Frustum represents the valid viewing region of a camera, defined as the
     * intersection of half-spaces bounded by planes through the origin. Each plane
     * is specified by an inward-pointing normal: a direction vector d is "inside"
     * the plane if dot(normal, d) >= 0.
     *
     * Typically constructed from the extremal edge pixel directions of a camera
     * model, accounting for lens distortion. The frustum can test whether a
     * direction lies inside the viewing region and can clip a TrajectoryArc to
     * find the parameter intervals that are visible.
     *
     * A boresight plane (normal = boresight direction) can be included to reject
     * points behind the camera.
     */
    class Frustum {
    public:
        Frustum() = default;
        explicit Frustum(const std::vector<glm::vec3>& plane_normals);

        [[nodiscard]] bool contains(const glm::vec3& direction) const;

        /** @brief Number of planes defining this frustum. */
        [[nodiscard]] std::size_t plane_count() const noexcept { return plane_normals_.size(); }

        /** @brief Access the plane normals. */
        [[nodiscard]] const std::vector<glm::vec3>& plane_normals() const noexcept { return plane_normals_; }

    private:
        std::vector<glm::vec3> plane_normals_;
    };

}

#include "huira_impl/render/frustum.ipp"
