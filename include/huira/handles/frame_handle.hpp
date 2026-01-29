#pragma once

#include "huira/core/concepts/numeric_concepts.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"

#include "huira/handles/camera_handle.hpp"
#include "huira/handles/node_handle.hpp"
#include "huira/handles/point_light_handle.hpp"
#include "huira/handles/unresolved_handle.hpp"
#include "huira/handles/instance_handle.hpp"
#include "huira/handles/mesh_handle.hpp"
#include "huira/scene/frame_node.hpp"

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


        // New Instance
        template <typename THandle>
            requires std::is_constructible_v<Instantiable<TSpectral>, decltype(std::declval<THandle>().get().get())>
        InstanceHandle<TSpectral> new_instance(const THandle& asset_handle) const;

        void delete_instance(InstanceHandle<TSpectral> instance) const;
    };
}

#include "huira_impl/handles/frame_handle.ipp"
