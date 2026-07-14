#include <algorithm>
#include <cmath>
#include <limits>

#include "embree4/rtcore.h"
#include "glm/glm.hpp"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/core/constants.hpp"
#include "huira/core/types.hpp"
#include "huira/util/logger.hpp"

namespace huira {
template <IsSpectral TSpectral>
void Ellipsoid<TSpectral>::compute_surface_interaction(const HitRecord& hit,
                                                       Interaction<TSpectral>& isect) const
{
    // Compute local position
    float phi = hit.u * 2.0f * PI<float>() - PI<float>();
    float theta = hit.v * PI<float>();

    float sin_phi = std::sin(phi);
    float cos_phi = std::cos(phi);
    float sin_theta = std::sin(theta);
    float cos_theta = std::cos(theta);
    isect.position = {
        radii_.x * sin_theta * cos_phi, radii_.y * sin_theta * sin_phi, radii_.z * cos_theta};
    isect.uv = {hit.u, hit.v};

    // Compute Normal
    float r_max = std::max({radii_.x, radii_.y, radii_.z});
    isect.normal_g = glm::normalize(Vec3<float>{(sin_theta * cos_phi) * (r_max / radii_.x),
                                                (sin_theta * sin_phi) * (r_max / radii_.y),
                                                (cos_theta) * (r_max / radii_.z)});
    isect.normal_s = isect.normal_g;

    // Compute Tangent Frame using partials
    Vec3<float> dpdu = {-radii_.x * sin_theta * sin_phi, radii_.y * sin_theta * cos_phi, 0.0f};

    // Handle singularity at the poles
    if (glm::dot(dpdu, dpdu) < 1e-8f) {
        dpdu = {1.0f, 0.0f, 0.0f};
    }

    isect.tangent = glm::normalize(dpdu);
    isect.bitangent = glm::normalize(glm::cross(isect.normal_g, isect.tangent));
}

template <IsSpectral TSpectral>
Vec2<float> Ellipsoid<TSpectral>::compute_uv(const HitRecord& hit) const
{
    return {hit.u, hit.v};
}

template <IsSpectral TSpectral>
void Ellipsoid<TSpectral>::build_blas_() const
{
    RTCGeometry geom = rtcNewGeometry(this->device_->get(), RTC_GEOMETRY_TYPE_USER);

    rtcSetGeometryUserPrimitiveCount(geom, 1);

    rtcSetGeometryUserData(geom, const_cast<Ellipsoid<TSpectral>*>(this));

    rtcSetGeometryBoundsFunction(geom, bounds_callback, nullptr);
    rtcSetGeometryIntersectFunction(geom, intersect_callback);

    rtcCommitGeometry(geom);

    this->blas_.reset(rtcNewScene(this->device_->get()));
    // Robust traversal: required so BVH plane tests cannot drop marginal hits
    // at extreme coordinate ranges (planetary camera-relative scenes).
    rtcSetSceneFlags(this->blas_.get(), RTC_SCENE_FLAG_ROBUST);
    rtcAttachGeometry(this->blas_.get(), geom);
    rtcReleaseGeometry(geom);

    rtcCommitScene(this->blas_.get());
}

template <IsSpectral TSpectral>
void Ellipsoid<TSpectral>::bounds_callback(const RTCBoundsFunctionArguments* args) noexcept
{
    const auto* ellipsoid = static_cast<const Ellipsoid<TSpectral>*>(args->geometryUserPtr);

    Vec3<float> r = ellipsoid->radii_;

    RTCBounds* bounds_o = args->bounds_o;
    bounds_o->lower_x = -r.x;
    bounds_o->lower_y = -r.y;
    bounds_o->lower_z = -r.z;

    bounds_o->upper_x = r.x;
    bounds_o->upper_y = r.y;
    bounds_o->upper_z = r.z;
}

template <IsSpectral TSpectral>
void Ellipsoid<TSpectral>::intersect_callback(const RTCIntersectFunctionNArguments* args) noexcept
{
    int* valid = args->valid;
    if (!valid) {
        return;
    }

    const auto* ellipsoid = static_cast<const Ellipsoid<TSpectral>*>(args->geometryUserPtr);
    Vec3<float> r = ellipsoid->radii_;

    RTCRayHitN* rayhit = args->rayhit;
    RTCRayN* ray = RTCRayHitN_RayN(rayhit, args->N);
    RTCHitN* hit = RTCRayHitN_HitN(rayhit, args->N);

    // The quadratic is solved in DOUBLE precision using the geometric
    // (closest-approach) form of the discriminant. The naive float32
    // B*B - 4*C form cancels catastrophically once the origin is far from
    // the body: at |O|/r ~ 60 (Earth seen from lunar range) it loses ~10
    // bits: near-hit t errors reach ~6 km over the disk and ~57 km at
    // grazing (limb) incidence, corrupting depth output, hit positions,
    // and the u/v texture coordinates derived from them.
    // In double with the stable form, the t error is limited by the float32
    // quantization of the ray itself (~1 ULP of t). Cost is negligible for
    // a single analytic primitive.
    const glm::dvec3 inv_r = {1.0 / static_cast<double>(r.x),
                              1.0 / static_cast<double>(r.y),
                              1.0 / static_cast<double>(r.z)};

    const glm::dvec3 O = {static_cast<double>(RTCRayN_org_x(ray, args->N, 0)),
                          static_cast<double>(RTCRayN_org_y(ray, args->N, 0)),
                          static_cast<double>(RTCRayN_org_z(ray, args->N, 0))};
    const glm::dvec3 D = {static_cast<double>(RTCRayN_dir_x(ray, args->N, 0)),
                          static_cast<double>(RTCRayN_dir_y(ray, args->N, 0)),
                          static_cast<double>(RTCRayN_dir_z(ray, args->N, 0))};

    const glm::dvec3 O_s = O * inv_r;
    const glm::dvec3 D_s = D * inv_r;

    const double L = glm::length(D_s);
    const glm::dvec3 D_u = D_s / L;

    // Signed distance along D_u to the point of closest approach is -b:
    const double b = glm::dot(O_s, D_u);
    // Perpendicular offset of the (unit) sphere center from the ray:
    const glm::dvec3 m = O_s - b * D_u;
    // Stable discriminant: 1 - |m|^2 == (B^2 - 4C)/4 without the cancellation.
    const double discriminant = 1.0 - glm::dot(m, m);
    if (discriminant < 0.0) {
        return;
    }

    const double sqrt_disc = std::sqrt(discriminant);
    // Citardauq split: compute the non-cancelling root directly, the other via C/q.
    const double q = -(b + std::copysign(sqrt_disc, b));
    const double C = glm::dot(O_s, O_s) - 1.0;
    double t0_u = q;
    double t1_u = C / q;

    if (t0_u > t1_u) {
        std::swap(t0_u, t1_u);
    }

    const float t0 = static_cast<float>(t0_u / L);
    const float t1 = static_cast<float>(t1_u / L);

    // A lambda allows us to cleanly test t0, and if rejected by alpha, test t1
    auto test_hit = [&](float t_candidate) -> bool {
        if (t_candidate < RTCRayN_tnear(ray, args->N, 0) ||
            t_candidate > RTCRayN_tfar(ray, args->N, 0)) {
            return false;
        }

        // Save original tfar in case the filter rejects this hit
        float old_tfar = RTCRayN_tfar(ray, args->N, 0);
        RTCRayN_tfar(ray, args->N, 0) = t_candidate;

        const double t_u = static_cast<double>(t_candidate) * L;
        const glm::dvec3 P_s = O_s + t_u * D_u;
        const double r_max =
            std::max({static_cast<double>(r.x), static_cast<double>(r.y),
                      static_cast<double>(r.z)});
        const glm::dvec3 normal = (P_s * inv_r) * r_max;

        RTCHitN_Ng_x(hit, args->N, 0) = static_cast<float>(normal.x);
        RTCHitN_Ng_y(hit, args->N, 0) = static_cast<float>(normal.y);
        RTCHitN_Ng_z(hit, args->N, 0) = static_cast<float>(normal.z);

        const glm::dvec3 P_unit = glm::normalize(P_s);
        const float u = static_cast<float>((std::atan2(P_unit.y, P_unit.x) + PI<double>()) /
                                           (2.0 * PI<double>()));
        const float v = static_cast<float>(std::acos(std::clamp(P_unit.z, -1.0, 1.0)) /
                                           PI<double>());

        RTCHitN_u(hit, args->N, 0) = u;
        RTCHitN_v(hit, args->N, 0) = v;
        RTCHitN_geomID(hit, args->N, 0) = args->geomID;
        RTCHitN_primID(hit, args->N, 0) = args->primID;
        RTCHitN_instID(hit, args->N, 0, 0) = args->context->instID[0];

        int filter_valid = valid[0];

        // Map properties into an Embree filter argument structure
        RTCFilterFunctionNArguments fargs;
        fargs.valid = &filter_valid;
        fargs.geometryUserPtr = args->geometryUserPtr;
        fargs.context = args->context;
        fargs.ray = ray;
        fargs.hit = hit;
        fargs.N = args->N;

        rtcInvokeIntersectFilterFromGeometry(args, &fargs);

        if (filter_valid == 0) {
            RTCRayN_tfar(ray, args->N, 0) = old_tfar; // Restore ray extent
            return false;
        }
        return true;
    };

    // If the ray hits transparency on the front face, let it attempt to hit the back face
    if (!test_hit(t0)) {
        test_hit(t1);
    }
}
} // namespace huira
