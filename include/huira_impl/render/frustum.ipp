#include <algorithm>

namespace huira {
    /**
     * @brief Construct a frustum from a set of inward-pointing plane normals.
     *
     * Each plane passes through the origin. A direction d is considered
     * inside the frustum if dot(normal, d) >= 0 for all planes.
     *
     * @param plane_normals Inward-pointing normals defining the frustum planes.
     */
    Frustum::Frustum(const std::vector<glm::vec3>& plane_normals)
        : plane_normals_(plane_normals)
    {

    }

    /**
     * @brief Test whether a direction vector lies inside the frustum.
     *
     * @param direction The direction to test.
     * @return True if the direction is on the positive side of all planes.
     */
    bool Frustum::contains(const glm::vec3& direction) const
    {
        for (const auto& normal : plane_normals_) {
            if (glm::dot(normal, direction) < 0.0f) {
                return false;
            }
        }
        return true;
    }
}
