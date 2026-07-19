#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "glm/glm.hpp"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/core/transform.hpp"
#include "huira/core/types.hpp"
#include "huira/sampling/cone_sampling.hpp"
#include "huira/sampling/sampler.hpp"

namespace huira {
template <IsSpectral TSpectral>
class Primitive;

/**
 * @brief One primitive instance belonging to a designated indirect source.
 *
 * Identifies a single instance placed in the scene view's TLAS. The indices are
 * the source of truth for both the bounding proxy (which reads the instance's
 * BLAS bounds and render transforms) and hit classification (which maps a TLAS
 * geometry back to its owning source).
 *
 * @tparam TSpectral The spectral representation type.
 */
template <IsSpectral TSpectral>
struct IndirectSourceMember {
    std::shared_ptr<Primitive<TSpectral>> primitive;
    std::size_t batch_index = 0;    ///< Index into SceneView's primitive batches
    std::size_t instance_index = 0; ///< Index within the batch's instances
};

/**
 * @brief A designated indirect illumination source (reflector) in a scene view.
 *
 * An indirect source is ordinary primitive geometry (e.g. the Earth or Moon)
 * that has been designated for direct sampling during next event estimation:
 * it does not emit light itself, but its sunlit surface can contribute
 * significant reflected illumination that undirected sampling would rarely find.
 *
 * A source is a *set* of primitive instances: designating an Instance that holds
 * a Primitive yields a single member, while designating an Instance that holds a
 * Model yields one member per Primitive in the model's sub-graph. All members are
 * bounded by one proxy and treated as one illumination source, so a sample landing
 * on any of them counts as a hit on this source.
 *
 * Direction sampling uses a conservative world-space bounding sphere enclosing
 * every member. Samples that miss the actual geometry are known zeros; the PDF is
 * defined over the bounding cone, so the estimator remains unbiased.
 *
 * @tparam TSpectral The spectral representation type.
 */
template <IsSpectral TSpectral>
struct IndirectSourceInstance {
    /// The primitive instances this source is composed of (at least one).
    std::vector<IndirectSourceMember<TSpectral>> members;

    /// Transform of the *designated* instance at N temporal samples. For a Model
    /// this is the model root's placement, not any individual member's transform.
    std::vector<Transform<float>> transforms;

    /// Conservative world-space bounding sphere enclosing every member, one per
    /// temporal sample. Populated by SceneView from the members' BLAS bounds.
    std::vector<Vec3<float>> bounding_centers;
    std::vector<float> bounding_radii;

    /**
     * @brief Samples a direction from p toward this source's bounding sphere.
     *
     * If p lies outside the bounding sphere, the subtended cone is sampled
     * uniformly. If p lies inside it (e.g. a LEO spacecraft inside an
     * AABB-derived Earth bound), the full unit sphere is sampled uniformly
     * instead; this remains unbiased, at reduced efficiency.
     *
     * @param p Shading/reference point (world space).
     * @param time_index Temporal sample index selecting the bounding sphere.
     * @param sampler Random sampler (consumes two 1D samples).
     * @return The sampled direction and its solid-angle PDF.
     */
    SphereConeSample
    sample_toward(const Vec3<float>& p, std::size_t time_index, Sampler<float>& sampler) const
    {
        const Vec3<float>& center = bounding_centers[time_index];
        const float radius = bounding_radii[time_index];

        Vec3<float> wc = center - p;
        if (glm::dot(wc, wc) <= radius * radius) {
            SphereConeSample sample;
            sample.wi = sample_uniform_sphere(sampler);
            sample.pdf = uniform_sphere_pdf();
            sample.distance = std::numeric_limits<float>::infinity();
            return sample;
        }

        // p is outside the sphere, so the cone sample cannot fail:
        return *sample_sphere_cone(p, center, radius, sampler);
    }

    /**
     * @brief Evaluates the PDF of sample_toward() for a given direction.
     *
     * @param p Shading/reference point (world space).
     * @param time_index Temporal sample index selecting the bounding sphere.
     * @param wi Direction to evaluate (normalized).
     * @return The solid-angle PDF of sampling wi from p (0 if wi misses the cone).
     */
    float pdf_toward(const Vec3<float>& p, std::size_t time_index, const Vec3<float>& wi) const
    {
        const Vec3<float>& center = bounding_centers[time_index];
        const float radius = bounding_radii[time_index];

        Vec3<float> wc = center - p;
        if (glm::dot(wc, wc) <= radius * radius) {
            return uniform_sphere_pdf();
        }
        return pdf_sphere_cone(p, center, radius, wi);
    }

    /**
     * @brief Continuous-time sample_toward(): the bounding sphere is linearly
     *        interpolated between temporal samples at normalized time t in [0, 1].
     *
     * The path tracer works in continuous shutter time, so the reflector's proxy
     * must be evaluated at the ray's time rather than a fixed sample. The lerped
     * sphere need not tightly bound the moving geometry between samples: any
     * surface direction it fails to cover is simply left to BSDF sampling
     * (pdf_toward returns 0 there, so MIS gives that arm full weight), which keeps
     * the estimator unbiased at the cost of at most a little efficiency.
     *
     * @param p Shading/reference point (world space).
     * @param t Normalized time in [0, 1] across the temporal samples.
     * @param sampler Random sampler (consumes two 1D samples).
     * @return The sampled direction and its solid-angle PDF.
     */
    SphereConeSample sample_toward(const Vec3<float>& p, float t, Sampler<float>& sampler) const
    {
        const auto [center, radius] = interpolated_bound_(t);

        Vec3<float> wc = center - p;
        if (glm::dot(wc, wc) <= radius * radius) {
            SphereConeSample sample;
            sample.wi = sample_uniform_sphere(sampler);
            sample.pdf = uniform_sphere_pdf();
            sample.distance = std::numeric_limits<float>::infinity();
            return sample;
        }
        return *sample_sphere_cone(p, center, radius, sampler);
    }

    /**
     * @brief Continuous-time pdf_toward(); interpolates the bounding sphere exactly
     *        as the continuous-time sample_toward() does so the two agree for MIS.
     *
     * @param p Shading/reference point (world space).
     * @param t Normalized time in [0, 1] across the temporal samples.
     * @param wi Direction to evaluate (normalized).
     * @return The solid-angle PDF of sampling wi from p (0 if wi misses the cone).
     */
    float pdf_toward(const Vec3<float>& p, float t, const Vec3<float>& wi) const
    {
        const auto [center, radius] = interpolated_bound_(t);

        Vec3<float> wc = center - p;
        if (glm::dot(wc, wc) <= radius * radius) {
            return uniform_sphere_pdf();
        }
        return pdf_sphere_cone(p, center, radius, wi);
    }

  private:
    /// Linearly interpolates (center, radius) at normalized time t in [0, 1],
    /// indexing temporal samples exactly as interpolate_transform() does.
    std::pair<Vec3<float>, float> interpolated_bound_(float t) const
    {
        const std::size_t n = bounding_centers.size();
        if (n == 1) {
            return {bounding_centers[0], bounding_radii[0]};
        }
        const float scaled = t * static_cast<float>(n - 1);
        std::size_t idx = static_cast<std::size_t>(std::floor(scaled));
        idx = std::min(idx, n - 2);
        const float frac = scaled - static_cast<float>(idx);

        const Vec3<float> center =
            bounding_centers[idx] * (1.0f - frac) + bounding_centers[idx + 1] * frac;
        const float radius = bounding_radii[idx] * (1.0f - frac) + bounding_radii[idx + 1] * frac;
        return {center, radius};
    }
};
} // namespace huira
