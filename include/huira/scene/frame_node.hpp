#pragma once

#include <vector>
#include <memory>
#include <string>
#include <algorithm>

#include "huira/scene/node.hpp"

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

namespace huira {
    // Forward declare:
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class Scene;

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class UnresolvedObject;

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class PointLight;

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class Camera;

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class FrameNode : public Node<TSpectral, TFloat> {
    public:
        FrameNode(Scene<TSpectral, TFloat>* scene);
        ~FrameNode() override = default;

        // Delete copying:
        FrameNode(const FrameNode&) = delete;
        FrameNode& operator=(const FrameNode&) = delete;


        // Child management:
        std::weak_ptr<FrameNode<TSpectral, TFloat>> new_child();
        void delete_child(std::weak_ptr<Node<TSpectral, TFloat>> child);

        // Factory methods for leaf nodes:
        std::weak_ptr<UnresolvedObject<TSpectral, TFloat>> new_unresolved_object();
        std::weak_ptr<PointLight<TSpectral, TFloat>> new_point_light(TSpectral spectral_intensity);
        std::weak_ptr<Camera<TSpectral, TFloat>> new_camera();

    protected:
        std::string get_type_name_() const override { return "FrameNode"; }

        // Override to propagate transform changes to children
        void on_transform_changed_() override;

        // Override to check children for SPICE constraints
        const std::vector<std::shared_ptr<Node<TSpectral, TFloat>>>* get_children_() const override { return &children_; }
        std::shared_ptr<Node<TSpectral, TFloat>> child_spice_origins_() const override;
        std::shared_ptr<Node<TSpectral, TFloat>> child_spice_frames_() const override;

        // Override to propagate SPICE updates to children
        void update_all_spice_transforms_() override;

    private:
        std::vector<std::shared_ptr<Node<TSpectral, TFloat>>> children_;

        friend class Scene<TSpectral, TFloat>;
    };
}

#include "huira_impl/scene/frame_node.ipp"
