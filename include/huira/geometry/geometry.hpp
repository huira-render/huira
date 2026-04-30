#pragma once

#include <cstdint>
#include <memory>

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/geometry/ray.hpp"
#include "huira/render/interaction.hpp"
#include "huira/scene/embree_device.hpp"
#include "huira/scene/scene_object.hpp"

namespace huira {
    // Forward Declare
    template <IsSpectral TSpectral> class Scene;
    template <IsSpectral TSpectral> class SceneView;

    template <IsSpectral TSpectral>
    class Geometry : public SceneObject<Geometry<TSpectral>> {
    public:
        Geometry() : id_(next_id_++) {}
        virtual ~Geometry() override = default;
        
        Geometry(const Geometry&) = delete;
        Geometry& operator=(const Geometry&) = delete;
        
        Geometry(Geometry&&) noexcept = default;
        Geometry& operator=(Geometry&&) noexcept = default;

        virtual void compute_surface_interaction(const HitRecord& hit, Interaction<TSpectral>& isect) const = 0;
        virtual Vec2<float> compute_uv(const HitRecord& hit) const = 0;

        std::uint64_t id() const override { return id_; }
        std::string type() const override = 0;

    protected:
        std::shared_ptr<EmbreeDevice> device_ = nullptr;
        mutable UniqueRTCScene blas_ = nullptr;
        virtual void build_blas_() const = 0;

        void set_device(std::shared_ptr<EmbreeDevice> device) noexcept { device_ = device; }
        
        [[nodiscard]] virtual RTCScene blas() const
        {
            if (!blas_) {
                if (!this->device_) {
                    HUIRA_THROW_ERROR("Geometry::blas - Cannot build BLAS: no RTCDevice assigned. "
                        "Ensure the geometry has been added to a Scene.");
                }
                build_blas_();
            }
            return blas_.get();
        }

    private:
        std::uint64_t id_ = 0;
        static inline std::uint64_t next_id_ = 0;

        friend class Scene<TSpectral>;
        friend class SceneView<TSpectral>;
    };
}
