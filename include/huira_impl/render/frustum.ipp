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
template <IsSpectral TSpectral>
Frustum<TSpectral>::Frustum(const std::vector<Vec3<float>>& plane_normals)
    : plane_normals_(plane_normals)
{
}

/**
 * @brief Test whether a direction vector lies inside the frustum.
 *
 * @param direction The direction to test.
 * @return True if the direction is on the positive side of all planes.
 */
template <IsSpectral TSpectral>
bool Frustum<TSpectral>::contains(const Vec3<float>& direction) const
{
    for (const auto& normal : plane_normals_) {
        if (glm::dot(normal, direction) < 0.0f) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Clip a triangle to the frustum.
 *
 * Uses Sutherland-Hodgman clipping against each frustum plane. The
 * result is a convex polygon (possibly empty if the triangle is entirely
 * outside) with vertices guaranteed to be within the valid frustum region.
 *
 * A triangle clipped against N planes can produce a polygon with at most
 * 3 + N vertices.
 *
 * @param v0 First triangle vertex (in 3D camera-frame coordinates).
 * @param v1 Second triangle vertex.
 * @param v2 Third triangle vertex.
 * @return Vertices of the clipped convex polygon, in winding order.
 *         Empty if the triangle is entirely outside the frustum.
 */
template <IsSpectral TSpectral>
std::vector<Triangle<TSpectral>>
Frustum<TSpectral>::clip_triangle(const Triangle<TSpectral>& triangle) const
{
    bool has_tangents = triangle.tangents.has_value();

    // Build the initial polygon:
    std::vector<ClipVertex> polygon(3);
    for (std::size_t i = 0; i < 3; ++i) {
        polygon[i].vertex = triangle.vertices[i];
        if (has_tangents) {
            polygon[i].tangent = (*triangle.tangents)[i];
        }
    }

    // Clip against each plane:
    for (const auto& normal : plane_normals_) {
        polygon = clip_polygon_by_plane_(polygon, normal);
        if (polygon.empty()) {
            return {};
        }
    }

    // Fan-triangulate and convert back to Triangle:
    std::vector<Triangle<TSpectral>> result;
    result.reserve(polygon.size() - 2);

    for (std::size_t i = 1; i + 1 < polygon.size(); ++i) {
        Triangle<TSpectral> tri;
        tri.vertices = {polygon[0].vertex, polygon[i].vertex, polygon[i + 1].vertex};

        if (has_tangents) {
            tri.tangents = std::array<Tangent, 3>{
                polygon[0].tangent, polygon[i].tangent, polygon[i + 1].tangent};
        }

        result.push_back(std::move(tri));
    }

    return result;
}

/**
 * @brief Clip a trajectory arc against the frustum.
 *
 * For each plane, computes the parameter intervals where the arc is on
 * the positive side (dot >= 0). The visible intervals are the intersection
 * of all per-plane intervals.
 */
template <IsSpectral TSpectral>
std::vector<std::pair<float, float>> Frustum<TSpectral>::clip_arc(const TrajectoryArc& arc) const
{
    if (plane_normals_.empty()) {
        return {{0.0f, 1.0f}};
    }

    std::vector<std::pair<float, float>> result = {{0.0f, 1.0f}};

    for (const auto& normal : plane_normals_) {
        auto plane_intervals = arc_intervals_inside_plane_(arc, normal);
        result = intersect_intervals_(result, plane_intervals);

        if (result.empty()) {
            return {};
        }
    }

    return result;
}

/**
 * @brief Linearly interpolate all attributes between two ClipVertex instances.
 *
 * Position, normal, uv, albedo, tangent, and bitangent are all interpolated.
 * Normals, tangents, and bitangents are NOT renormalized here — the caller
 * can normalize after rasterization if needed.
 */
template <IsSpectral TSpectral>
typename Frustum<TSpectral>::ClipVertex
Frustum<TSpectral>::lerp_clip_vertex_(const ClipVertex& a, const ClipVertex& b, float t)
{
    ClipVertex result;
    result.vertex.position = a.vertex.position + t * (b.vertex.position - a.vertex.position);
    result.vertex.albedo = a.vertex.albedo + t * (b.vertex.albedo - a.vertex.albedo);
    result.vertex.normal = a.vertex.normal + t * (b.vertex.normal - a.vertex.normal);
    result.vertex.uv = a.vertex.uv + t * (b.vertex.uv - a.vertex.uv);
    result.tangent.tangent = a.tangent.tangent + t * (b.tangent.tangent - a.tangent.tangent);
    result.tangent.bitangent =
        a.tangent.bitangent + t * (b.tangent.bitangent - a.tangent.bitangent);
    return result;
}

/**
 * @brief Clip a convex polygon against a single plane through the origin.
 *
 * Implements one pass of the Sutherland-Hodgman algorithm. Vertices on
 * the positive side of the plane (dot(normal, position) >= 0) are kept;
 * edges that cross the plane are split at the intersection point with
 * all attributes interpolated.
 *
 * @param polygon Input polygon vertices.
 * @param plane_normal Inward-pointing normal of the clipping plane.
 * @return Clipped polygon vertices.
 */
template <IsSpectral TSpectral>
std::vector<typename Frustum<TSpectral>::ClipVertex>
Frustum<TSpectral>::clip_polygon_by_plane_(const std::vector<ClipVertex>& polygon,
                                           const Vec3<float>& plane_normal)
{
    if (polygon.empty()) {
        return {};
    }

    std::vector<ClipVertex> output;
    output.reserve(polygon.size() + 1);

    const std::size_t n = polygon.size();
    for (std::size_t i = 0; i < n; ++i) {
        const auto& current = polygon[i];
        const auto& next = polygon[(i + 1) % n];

        float d_current = glm::dot(plane_normal, current.vertex.position);
        float d_next = glm::dot(plane_normal, next.vertex.position);

        bool current_inside = d_current >= 0.0f;
        bool next_inside = d_next >= 0.0f;

        if (current_inside && next_inside) {
            output.push_back(next);
        } else if (current_inside && !next_inside) {
            float t = d_current / (d_current - d_next);
            output.push_back(lerp_clip_vertex_(current, next, t));
        } else if (!current_inside && next_inside) {
            float t = d_current / (d_current - d_next);
            output.push_back(lerp_clip_vertex_(current, next, t));
            output.push_back(next);
        }
    }

    return output;
}

/**
 * @brief Compute the intervals of the arc where dot(normal, arc(t)) >= 0.
 *
 * Finds all roots (plane crossings), then tests the sign between consecutive
 * roots to determine which intervals are on the positive side.
 */
template <IsSpectral TSpectral>
std::vector<std::pair<float, float>>
Frustum<TSpectral>::arc_intervals_inside_plane_(const TrajectoryArc& arc, const Vec3<float>& normal)
{
    constexpr float eps = 1e-6f;

    auto crossings = arc.find_plane_crossings(normal);

    // Build candidate interval boundaries: [0, crossings..., 1]
    std::vector<float> boundaries;
    boundaries.reserve(crossings.size() + 2);
    boundaries.push_back(0.0f);
    for (float t : crossings) {
        if (t > eps && t < 1.0f - eps) {
            boundaries.push_back(t);
        }
    }
    boundaries.push_back(1.0f);

    // Test the midpoint of each interval to determine sign:
    std::vector<std::pair<float, float>> intervals;
    for (std::size_t i = 0; i + 1 < boundaries.size(); ++i) {
        float t_mid = (boundaries[i] + boundaries[i + 1]) / 2.0f;
        Vec3<float> dir = arc.evaluate(t_mid);
        if (glm::dot(normal, dir) >= 0.0f) {
            // Merge with previous interval if contiguous:
            if (!intervals.empty() && std::abs(intervals.back().second - boundaries[i]) < eps) {
                intervals.back().second = boundaries[i + 1];
            } else {
                intervals.emplace_back(boundaries[i], boundaries[i + 1]);
            }
        }
    }

    return intervals;
}

/**
 * @brief Intersect two sorted, non-overlapping interval sets.
 *
 * Standard sweep-line interval intersection.
 */
template <IsSpectral TSpectral>
std::vector<std::pair<float, float>>
Frustum<TSpectral>::intersect_intervals_(const std::vector<std::pair<float, float>>& a,
                                         const std::vector<std::pair<float, float>>& b)
{
    std::vector<std::pair<float, float>> result;
    std::size_t i = 0, j = 0;

    while (i < a.size() && j < b.size()) {
        float lo = std::max(a[i].first, b[j].first);
        float hi = std::min(a[i].second, b[j].second);

        if (lo < hi) {
            result.emplace_back(lo, hi);
        }

        if (a[i].second < b[j].second) {
            ++i;
        } else {
            ++j;
        }
    }

    return result;
}
} // namespace huira
