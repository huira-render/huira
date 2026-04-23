#include <cmath>
#include <limits>

#include "embree4/rtcore.h"
#include "glm/glm.hpp"

#include "huira/core/types.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/util/logger.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    RTCScene Ellipsoid<TSpectral>::blas() const
    {
        if (!blas_) {
            if (!this->device_) {
                HUIRA_THROW_ERROR("Ellipsoid::blas - Cannot build BLAS: no RTCDevice assigned. "
                    "Ensure the geometry has been added to a Scene.");
            }
            build_blas_();
        }
        return blas_;

    }

    template <IsSpectral TSpectral>
    void Ellipsoid<TSpectral>::compute_surface_interaction(const HitRecord& hit, Interaction<TSpectral>& isect) const
    {
        // Compute local position
        float phi = hit.u * 2.0f * std::numbers::pi_v<float> -std::numbers::pi_v<float>;
        float theta = hit.v * std::numbers::pi_v<float>;

        float sin_phi = std::sin(phi);
        float cos_phi = std::cos(phi);
        float sin_theta = std::sin(theta);
        float cos_theta = std::cos(theta);
        isect.position = {
            radii_.x * sin_theta * cos_phi,
            radii_.y * cos_theta,
            radii_.z * sin_theta * sin_phi
        };
        isect.uv = { hit.u, hit.v };

        // Compute Normal
        isect.normal_g = glm::normalize(Vec3<float>{
            isect.position.x / (radii_.x * radii_.x),
                isect.position.y / (radii_.y * radii_.y),
                isect.position.z / (radii_.z * radii_.z)
        });
        isect.normal_s = isect.normal_g;

        // Compute Tangent Frame using partials
        Vec3<float> dpdu = {
            -radii_.x * sin_theta * sin_phi,
             0.0f,
             radii_.z * sin_theta * cos_phi
        };

        // Handle singularity at the poles
        if (glm::dot(dpdu, dpdu) < 1e-8f) {
            dpdu = { 1.0f, 0.0f, 0.0f };
        }

        isect.tangent = glm::normalize(dpdu);

        // Ensure tangent space is orthonormal
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

        rtcSetGeometryUserData(geom, (void*)this);
        
        rtcSetGeometryBoundsFunction(geom, bounds_callback, nullptr);
        rtcSetGeometryIntersectFunction(geom, intersect_callback);
        rtcSetGeometryOccludedFunction(geom, occluded_callback);

        rtcCommitGeometry(geom);
        
        blas_ = rtcNewScene(this->device_->get());
        rtcAttachGeometry(blas_, geom);
        rtcReleaseGeometry(geom);

        rtcCommitScene(blas_);
    }

    template <IsSpectral TSpectral>
    void Ellipsoid<TSpectral>::bounds_callback(const RTCBoundsFunctionArguments* args)
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
    void Ellipsoid<TSpectral>::intersect_callback(const RTCIntersectFunctionNArguments* args)
    {
        int* valid = args->valid;
        if (!valid) {
            return;
        }

        const auto* ellipsoid = static_cast<const Ellipsoid<TSpectral>*>(args->geometryUserPtr);
        Vec3<float> r = ellipsoid->radii_;

        // Cache the inverse radii to scale the ray
        Vec3<float> inv_r = { 1.0f / r.x, 1.0f / r.y, 1.0f / r.z };

        RTCRayHitN* rayhit = args->rayhit;
        RTCRayN* ray = RTCRayHitN_RayN(rayhit, args->N);
        RTCHitN* hit = RTCRayHitN_HitN(rayhit, args->N);

        // Unpack the ray from Embree
        Vec3<float> O = {
            RTCRayN_org_x(ray, args->N, 0),
            RTCRayN_org_y(ray, args->N, 0),
            RTCRayN_org_z(ray, args->N, 0)
        };
        Vec3<float> D = {
            RTCRayN_dir_x(ray, args->N, 0),
            RTCRayN_dir_y(ray, args->N, 0),
            RTCRayN_dir_z(ray, args->N, 0)
        };
        float tnear = RTCRayN_tnear(ray, args->N, 0);
        float tfar = RTCRayN_tfar(ray, args->N, 0);
        
        Vec3<float> O_scaled = O * inv_r;
        Vec3<float> D_scaled = D * inv_r;

        float A = glm::dot(D_scaled, D_scaled);
        float B = 2.0f * glm::dot(O_scaled, D_scaled);
        float C = glm::dot(O_scaled, O_scaled) - 1.0f;
        float discriminant = B * B - 4.0f * A * C;

        if (discriminant < 0.0f) {
            return;
        }

        float sqrt_disc = std::sqrt(discriminant);
        float t0 = (-B - sqrt_disc) / (2.0f * A);
        float t1 = (-B + sqrt_disc) / (2.0f * A);

        float t = t0;
        if (t < tnear || t > tfar) {
            t = t1;
            if (t < tnear || t > tfar) {
                return;
            }
        }

        // Pack the Hit Data back into Embree
        RTCRayN_tfar(ray, args->N, 0) = t;
        Vec3<float> P = O + t * D;
        Vec3<float> normal = {
            P.x * inv_r.x * inv_r.x,
            P.y * inv_r.y * inv_r.y,
            P.z * inv_r.z * inv_r.z
        };

        RTCHitN_Ng_x(hit, args->N, 0) = normal.x;
        RTCHitN_Ng_y(hit, args->N, 0) = normal.y;
        RTCHitN_Ng_z(hit, args->N, 0) = normal.z;

        // Compute UV coordinates using spherical mapping of the unit sphere:
        Vec3<float> P_unit = glm::normalize(O_scaled + t * D_scaled);
        float u = (std::atan2(P_unit.z, P_unit.x) + std::numbers::pi_v<float>) / (2.0f * std::numbers::pi_v<float>);
        float v = std::acos(P_unit.y) / std::numbers::pi_v<float>;
        RTCHitN_u(hit, args->N, 0) = u;
        RTCHitN_v(hit, args->N, 0) = v;

        RTCHitN_geomID(hit, args->N, 0) = args->geomID;
        RTCHitN_primID(hit, args->N, 0) = args->primID;
    }

    template <IsSpectral TSpectral>
    void Ellipsoid<TSpectral>::occluded_callback(const RTCOccludedFunctionNArguments* args)
    {
        int* valid = args->valid;
        if (!valid) {
            return;
        }

        const auto* ellipsoid = static_cast<const Ellipsoid<TSpectral>*>(args->geometryUserPtr);
        Vec3<float> r = ellipsoid->radii_;
        Vec3<float> inv_r = { 1.0f / r.x, 1.0f / r.y, 1.0f / r.z };

        RTCRayN* ray = args->ray;

        Vec3<float> O = {
            RTCRayN_org_x(ray, args->N, 0),
            RTCRayN_org_y(ray, args->N, 0),
            RTCRayN_org_z(ray, args->N, 0)
        };
        Vec3<float> D = {
            RTCRayN_dir_x(ray, args->N, 0),
            RTCRayN_dir_y(ray, args->N, 0),
            RTCRayN_dir_z(ray, args->N, 0)
        };
        float tnear = RTCRayN_tnear(ray, args->N, 0);
        float tfar = RTCRayN_tfar(ray, args->N, 0);

        Vec3<float> O_scaled = O * inv_r;
        Vec3<float> D_scaled = D * inv_r;

        float A = glm::dot(D_scaled, D_scaled);
        float B = 2.0f * glm::dot(O_scaled, D_scaled);
        float C = glm::dot(O_scaled, O_scaled) - 1.0f;
        float discriminant = B * B - 4.0f * A * C;

        if (discriminant < 0.0f) {
            return;
        }

        float sqrt_disc = std::sqrt(discriminant);
        float t0 = (-B - sqrt_disc) / (2.0f * A);
        float t1 = (-B + sqrt_disc) / (2.0f * A);
        
        if ((t0 > tnear && t0 < tfar) || (t1 > tnear && t1 < tfar)) {
            RTCRayN_tfar(ray, args->N, 0) = -std::numeric_limits<float>::infinity();
        }
    }
    
}
