#pragma once

#include <array>
#include <utility>
#include <vector>

#include "huira/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"
#include "huira/geometry/vertex.hpp"
#include "huira/render/trajectory_arc.hpp"

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
 *
 * @tparam TSpectral The spectral type (e.g., RGB, Visible8)
 */
template <IsSpectral TSpectral>
class Frustum {
  public:
    Frustum() = default;
    explicit Frustum(const std::vector<Vec3<float>>& plane_normals);

    [[nodiscard]] bool contains(const Vec3<float>& direction) const;

    [[nodiscard]] std::vector<Triangle<TSpectral>>
    clip_triangle(const Triangle<TSpectral>& triangle) const;

    [[nodiscard]] std::vector<std::pair<float, float>> clip_arc(const TrajectoryArc& arc) const;

    /**
     * @brief Reusable working storage for clip_arc().
     *
     * clip_arc() otherwise allocates several small vectors per call, which dominates
     * its cost when it is invoked once per catalogue star per frame. Callers that
     * clip many arcs should keep one Scratch alive (one per thread) and pass it in;
     * after the first few calls its buffers stop reallocating entirely.
     */
    struct ClipScratch {
        std::vector<float> crossings;
        std::vector<float> boundaries;
        std::vector<std::pair<float, float>> plane_intervals;
        std::vector<std::pair<float, float>> result;
        std::vector<std::pair<float, float>> merged;
    };

    [[nodiscard]] const std::vector<std::pair<float, float>>& clip_arc(const TrajectoryArc& arc,
                                                                       ClipScratch& scratch) const;

    /// Number of planes defining this frustum.
    [[nodiscard]] std::size_t plane_count() const noexcept { return plane_normals_.size(); }

    /// Access the plane normals.
    [[nodiscard]] const std::vector<Vec3<float>>& plane_normals() const noexcept
    {
        return plane_normals_;
    }

  private:
    std::vector<Vec3<float>> plane_normals_;

    /**
     * @brief Internal polygon vertex used during clipping.
     *
     * Bundles a Vertex with an optional Tangent so that both are
     * interpolated together during Sutherland-Hodgman passes.
     */
    struct ClipVertex {
        Vertex<TSpectral> vertex;
        Tangent tangent;
    };

    [[nodiscard]] static ClipVertex
    lerp_clip_vertex_(const ClipVertex& a, const ClipVertex& b, float t);

    [[nodiscard]] static std::vector<ClipVertex>
    clip_polygon_by_plane_(const std::vector<ClipVertex>& polygon, const Vec3<float>& plane_normal);

    [[nodiscard]] static std::vector<std::pair<float, float>>
    arc_intervals_inside_plane_(const TrajectoryArc& arc, const Vec3<float>& normal);

    static void arc_intervals_inside_plane_(const TrajectoryArc& arc,
                                            const Vec3<float>& normal,
                                            std::vector<float>& crossings,
                                            std::vector<float>& boundaries,
                                            std::vector<std::pair<float, float>>& intervals);

    [[nodiscard]] static std::vector<std::pair<float, float>>
    intersect_intervals_(const std::vector<std::pair<float, float>>& a,
                         const std::vector<std::pair<float, float>>& b);

    static void intersect_intervals_(const std::vector<std::pair<float, float>>& a,
                                     const std::vector<std::pair<float, float>>& b,
                                     std::vector<std::pair<float, float>>& out);
};

} // namespace huira

#include "huira_impl/render/frustum.ipp"
