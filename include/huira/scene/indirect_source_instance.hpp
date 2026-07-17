#pragma once

#include <cstddef>
#include <limits>
#include <memory>
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
 * @brief A designated indirect illumination source (reflector) in a scene view.
 *
 * An indirect source is ordinary primitive geometry (e.g. the Earth or Moon)
 * that has been designated for direct sampling during next event estimation:
 * it does not emit light itself, but its sunlit surface can contribute
 * significant reflected illumination that undirected sampling would rarely find.
 *
 * Direction sampling uses a conservative world-space bounding sphere derived
 * from the primitive's BLAS bounds. Samples that miss the actual geometry are
 * known zeros; the PDF is defined over the bounding cone, so the estimator
 * remains unbiased.
 *
 * @tparam TSpectral The spectral representation type.
 */
template <IsSpectral TSpectral>
struct IndirectSourceInstance {
    std::shared_ptr<Primitive<TSpectral>> primitive;
    std::size_t batch_index = 0;    ///< Index into SceneView's primitive batches
    std::size_t instance_index = 0; ///< Index within the batch's instances

    std::vector<Transform<float>> transforms; ///< Transform at N temporal samples

    /// Conservative world-space bounding sphere, one per temporal sample.
    /// Populated by SceneView from the primitive's BLAS bounds.
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
};
} // namespace huira
