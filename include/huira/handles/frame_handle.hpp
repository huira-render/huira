#pragma once

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

#include "huira/handles/camera_handle.hpp"
#include "huira/handles/node_handle.hpp"
#include "huira/handles/point_light_handle.hpp"
#include "huira/handles/unresolved_handle.hpp"
#include "huira/scene/frame_node.hpp"

namespace huira {
    /**
     * @brief Handle for interacting with FrameNode instances.
     *
     * Provides a safe, user-friendly interface to FrameNode.
     * Includes methods for:
     * - Setting transforms (position, rotation, scale, velocities)
     * - SPICE integration
     * - Creating child frames and leaf nodes
     */
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class FrameHandle : public NodeHandle<TSpectral, TFloat, FrameNode<TSpectral, TFloat>> {
    public:
        FrameHandle() = delete;
        using NodeHandle<TSpectral, TFloat, FrameNode<TSpectral, TFloat>>::NodeHandle;

       

        // New Subframe
        FrameHandle<TSpectral, TFloat> new_subframe() const {
            return FrameHandle<TSpectral, TFloat>{ this->get()->new_child() };
        }

        FrameHandle<TSpectral, TFloat> new_spice_subframe(const std::string& spice_origin, const std::string& spice_frame) const {
            FrameHandle<TSpectral, TFloat> subframe = this->new_subframe();
            subframe.set_spice(spice_origin, spice_frame);
            return subframe;
        }

        void delete_subframe(FrameHandle<TSpectral, TFloat> subframe) const {
            this->get()->delete_child(std::weak_ptr<Node<TSpectral, TFloat>>(subframe.get()));
        }



        // Unresolbed Object
        UnresolvedObjectHandle<TSpectral, TFloat> new_unresolved_object() const {
            return UnresolvedObjectHandle<TSpectral, TFloat>{ this->get()->new_unresolved_object() };
        }

        UnresolvedObjectHandle<TSpectral, TFloat> new_spice_unresolved_object(const std::string& spice_origin) const {
            UnresolvedObjectHandle<TSpectral, TFloat> child = this->new_unresolved_object();
            child.set_spice_origin(spice_origin);
            return child;
        }




        // New Point Light:
        PointLightHandle<TSpectral, TFloat> new_point_light(TSpectral spectral_intensity) const {
            return PointLightHandle<TSpectral, TFloat>{ this->get()->new_point_light(spectral_intensity) };
        }

        PointLightHandle<TSpectral, TFloat> new_spice_point_light(const std::string& spice_origin, TSpectral spectral_intensity) const {
            PointLightHandle<TSpectral, TFloat> child = this->new_point_light(spectral_intensity);
            child.set_spice_origin(spice_origin);
            return child;
        }



        // New Camera:
        CameraHandle<TSpectral, TFloat> new_camera() const {
            return CameraHandle<TSpectral, TFloat>{ this->get()->new_camera() };
        }

        CameraHandle<TSpectral, TFloat> new_spice_camera(const std::string& spice_origin, const std::string& spice_frame) const {
            CameraHandle<TSpectral, TFloat> child = this->new_camera();
            child.set_spice(spice_origin, spice_frame);
            return child;
        }
    };
}
