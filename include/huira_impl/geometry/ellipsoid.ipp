#include <cmath>
#include <limits>

#include "embree4/rtcore.h"
#include "glm/glm.hpp"

#include "huira/core/types.hpp"
#include "huira/core/constants.hpp"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/util/logger.hpp"

namespace huira {    
    template <IsSpectral TSpectral>
    void Ellipsoid<TSpectral>::compute_surface_interaction(const HitRecord& hit, Interaction<TSpectral>& isect) const
    {
        // Compute local position
        float phi = hit.u * 2.0f * PI<float>() - PI<float>();
        float theta = hit.v * PI<float>();

        float sin_phi = std::sin(phi);
        float cos_phi = std::cos(phi);
        float sin_theta = std::sin(theta);
        float cos_theta = std::cos(theta);
        isect.position = {
            radii_.x * sin_theta * cos_phi,
            radii_.y * sin_theta * sin_phi,
            radii_.z * cos_theta
        };
        isect.uv = { hit.u, hit.v };

        // Compute Normal
        float r_max = std::max({ radii_.x, radii_.y, radii_.z });
        isect.normal_g = glm::normalize(Vec3<float>{
            (sin_theta* cos_phi)* (r_max / radii_.x),
                (sin_theta* sin_phi)* (r_max / radii_.y),
                (cos_theta)* (r_max / radii_.z)
        });
        isect.normal_s = isect.normal_g;

        // Compute Tangent Frame using partials
        Vec3<float> dpdu = {
            -radii_.x * sin_theta * sin_phi,
             radii_.y * sin_theta * cos_phi,
             0.0f
        };

        // Handle singularity at the poles
        if (glm::dot(dpdu, dpdu) < 1e-8f) {
            dpdu = { 1.0f, 0.0f, 0.0f };
        }

        isect.tangent = glm::normalize(dpdu);
        isect.bitangent = glm::normalize(glm::cross(isect.normal_g, isect.tangent));
    }

    template <IsSpectral TSpectral>
    Vec2<float> Ellipsoid<TSpectral>::compute_uv(const HitRecord& hit) const
    {
        return { hit.u, hit.v };
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
        Vec3<float> inv_r = { 1.0f / r.x, 1.0f / r.y, 1.0f / r.z };

        RTCRayHitN* rayhit = args->rayhit;
        RTCRayN* ray = RTCRayHitN_RayN(rayhit, args->N);
        RTCHitN* hit = RTCRayHitN_HitN(rayhit, args->N);
        
        Vec3<float> O = { RTCRayN_org_x(ray, args->N, 0), RTCRayN_org_y(ray, args->N, 0), RTCRayN_org_z(ray, args->N, 0) };
        Vec3<float> D = { RTCRayN_dir_x(ray, args->N, 0), RTCRayN_dir_y(ray, args->N, 0), RTCRayN_dir_z(ray, args->N, 0) };

        Vec3<float> O_s = O * inv_r;
        Vec3<float> D_s = D * inv_r;

        float L = glm::length(D_s);
        Vec3<float> D_u = D_s / L;

        float B = 2.0f * glm::dot(O_s, D_u);
        float C = glm::dot(O_s, O_s) - 1.0f;

        float discriminant = B * B - 4.0f * C;
        if (discriminant < 0.0f) {
            return;
        }

        float sqrt_disc = std::sqrt(discriminant);
        float q = -0.5f * (B + std::copysign(sqrt_disc, B));
        float t0_u = q;
        float t1_u = C / q;

        if (t0_u > t1_u) {
            std::swap(t0_u, t1_u);
        }

        float t0 = t0_u / L;
        float t1 = t1_u / L;

        // A lambda allows us to cleanly test t0, and if rejected by alpha, test t1
        auto test_hit = [&](float t_candidate) -> bool {
            if (t_candidate < RTCRayN_tnear(ray, args->N, 0) || t_candidate > RTCRayN_tfar(ray, args->N, 0)) {
                return false;
            }

            // Save original tfar in case the filter rejects this hit
            float old_tfar = RTCRayN_tfar(ray, args->N, 0);
            RTCRayN_tfar(ray, args->N, 0) = t_candidate;

            float t_u = t_candidate * L;
            Vec3<float> P_s = O_s + t_u * D_u;
            float r_max = std::max({ r.x, r.y, r.z });
            Vec3<float> normal = (P_s * inv_r) * r_max;
            
            RTCHitN_Ng_x(hit, args->N, 0) = normal.x;
            RTCHitN_Ng_y(hit, args->N, 0) = normal.y;
            RTCHitN_Ng_z(hit, args->N, 0) = normal.z;

            Vec3<float> P_unit = glm::normalize(O_s + t_u * D_u);
            float u = (std::atan2(P_unit.y, P_unit.x) + PI<float>()) / (2.0f * PI<float>());
            float v = std::acos(P_unit.z) / PI<float>();
            
            RTCHitN_u(hit, args->N, 0) = u;
            RTCHitN_v(hit, args->N, 0) = v;
            RTCHitN_geomID(hit, args->N, 0) = args->geomID;
            RTCHitN_primID(hit, args->N, 0) = args->primID;
            RTCHitN_instID(hit, args->N, 0, 0) = args->context->instID[0];
            
            int filter_valid = valid[0];

            // Map properties into an Embree filter argument structure
            RTCFilterFunctionNArguments fargs;
            fargs.valid           = &filter_valid;
            fargs.geometryUserPtr = args->geometryUserPtr;
            fargs.context         = args->context;
            fargs.ray             = ray;
            fargs.hit             = hit;
            fargs.N               = args->N;

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
}
