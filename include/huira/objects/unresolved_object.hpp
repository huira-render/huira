#pragma once

#include <string>
#include <memory>

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/objects/scene_graph/node.hpp"

namespace huira {
    // Forward declare:
    template <IsSpectral TSpectral>
    class Scene;

    template <IsSpectral TSpectral>
    class FrameNode;



    template <IsSpectral TSpectral>
    class UnresolvedObject : public Node<TSpectral> {
    public:
        UnresolvedObject(Scene<TSpectral>* scene)
            : Node<TSpectral>(scene)
        {

        }

        // Delete copying:
        UnresolvedObject(const UnresolvedObject&) = delete;
        UnresolvedObject& operator=(const UnresolvedObject&) = delete;

        void set_irradiance(TSpectral irradiance) { irradiance_ = irradiance; }
        TSpectral get_irradiance() const { return irradiance_; }


        std::string get_type_name() const override { return "UnresolvedObject"; }

        // Explicitly delete methods that don't make sense for a point-like object:
        void set_rotation(const Rotation<double> & rotation) = delete;
        void set_scale(const Vec3<double>& scale) = delete;
        void set_angular_velocity(const Vec3<double>& angular_velocity) = delete;
        void set_spice_frame(const std::string& spice_frame) = delete;
        void set_spice(const std::string& spice_origin, const std::string& spice_frame) = delete;
        std::string get_spice_frame() = delete;

    protected:
        TSpectral irradiance_{ 0 };

        friend class Scene<TSpectral>;
        friend class FrameNode<TSpectral>;
    };
}

#include "huira_impl/objects/unresolved_object.ipp"
