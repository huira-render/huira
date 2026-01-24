#pragma once

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

#include "huira/handles/camera_handle.hpp"
#include "huira/handles/node_handle.hpp"
#include "huira/handles/point_light_handle.hpp"
#include "huira/handles/unresolved_handle.hpp"
#include "huira/scene_graph/frame_node.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class FrameHandle : public NodeHandle<TSpectral, FrameNode<TSpectral>> {
    public:
        FrameHandle() = delete;
        using NodeHandle<TSpectral, FrameNode<TSpectral>>::NodeHandle;

       

        // New Subframe
        FrameHandle<TSpectral> new_subframe() const;
        FrameHandle<TSpectral> new_spice_subframe(const std::string& spice_origin, const std::string& spice_frame) const;
        void delete_subframe(FrameHandle<TSpectral> subframe) const;


        // Unresolbed Object
        UnresolvedObjectHandle<TSpectral> new_unresolved_object() const;
        UnresolvedObjectHandle<TSpectral> new_spice_unresolved_object(const std::string& spice_origin) const;


        // New Point Light:
        //PointLightHandle<TSpectral> new_point_light(TSpectral spectral_intensity) const;
        //PointLightHandle<TSpectral> new_spice_point_light(const std::string& spice_origin, TSpectral spectral_intensity) const;

        
        // New Camera:
        CameraHandle<TSpectral> new_camera() const;
        CameraHandle<TSpectral> new_spice_camera(const std::string& spice_origin, const std::string& spice_frame) const;
    };
}

#include "huira_impl/handles/frame_handle.ipp"
