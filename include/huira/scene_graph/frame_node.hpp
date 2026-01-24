#pragma once

#include <vector>
#include <memory>
#include <string>
#include <algorithm>

#include "huira/scene_graph/node.hpp"

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

namespace huira {
    // Forward declare:
    template <IsSpectral TSpectral>
    class Scene;

    template <IsSpectral TSpectral>
    class UnresolvedObject;

    template <IsSpectral TSpectral>
    class PointLight;

    template <IsSpectral TSpectral>
    class Camera;

    template <IsSpectral TSpectral>
    class FrameNode : public Node<TSpectral> {
    public:
        FrameNode(Scene<TSpectral>* scene);
        ~FrameNode() override = default;

        // Delete copying:
        FrameNode(const FrameNode&) = delete;
        FrameNode& operator=(const FrameNode&) = delete;


        // Child management:
        std::weak_ptr<FrameNode<TSpectral>> new_child();
        void delete_child(std::weak_ptr<Node<TSpectral>> child);

        // Factory methods for leaf nodes:
        std::weak_ptr<UnresolvedObject<TSpectral>> new_unresolved_object();
        std::weak_ptr<PointLight<TSpectral>> new_point_light(TSpectral spectral_intensity);
        std::weak_ptr<Camera<TSpectral>> new_camera();

        std::string get_type_name() const override { return "FrameNode"; }

    protected:

        // Override to check children for SPICE constraints
        const std::vector<std::shared_ptr<Node<TSpectral>>>* get_children_() const override { return &children_; }
        std::shared_ptr<Node<TSpectral>> child_spice_origins_() const override;
        std::shared_ptr<Node<TSpectral>> child_spice_frames_() const override;

    private:
        std::vector<std::shared_ptr<Node<TSpectral>>> children_;

        friend class Scene<TSpectral>;
    };
}

#include "huira_impl/scene_graph/frame_node.ipp"
