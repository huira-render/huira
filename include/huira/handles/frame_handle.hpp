#pragma once

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

#include "huira/handles/camera_handle.hpp"
#include "huira/handles/node_handle.hpp"
#include "huira/handles/point_light_handle.hpp"
#include "huira/handles/unresolved_handle.hpp"
#include "huira/handles/instance_handle.hpp"
#include "huira/handles/mesh_handle.hpp"
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

        
        // New Camera:
        CameraHandle<TSpectral> new_camera() const;
        CameraHandle<TSpectral> new_spice_camera(const std::string& spice_origin, const std::string& spice_frame) const;


        // New Mesh Instance:
        InstanceHandle<TSpectral> new_instance(const MeshHandle<TSpectral>& mesh_handle) const
        {
            return InstanceHandle<TSpectral>{ this->get()->new_instance(mesh_handle.get().get()) };
        }

        // New Light Instance:
        InstanceHandle<TSpectral> new_instance(const PointLightHandle<TSpectral>& light_handle) const
        {
            return InstanceHandle<TSpectral>{ this->get()->new_instance(light_handle.get().get()) };
        }
    };
}

#include "huira_impl/handles/frame_handle.ipp"
