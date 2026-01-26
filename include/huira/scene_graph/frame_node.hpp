#pragma once

#include <vector>
#include <memory>
#include <string>
#include <algorithm>
#include <span>

#include "huira/scene_graph/node.hpp"
#include "huira/scene_graph/instance.hpp"

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

namespace huira {
    // Forward declare:
    template <IsSpectral TSpectral>
    class Scene;

    template <IsSpectral TSpectral>
    class SceneView;

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

        FrameNode(const FrameNode&) = delete;
        FrameNode& operator=(const FrameNode&) = delete;


        // Child management:
        std::weak_ptr<FrameNode<TSpectral>> new_child();
        void delete_child(std::weak_ptr<Node<TSpectral>> child);

        // Factory methods for leaf nodes:
        std::weak_ptr<Camera<TSpectral>> new_camera();
        std::weak_ptr<Instance<TSpectral>> new_instance(Mesh<TSpectral>* mesh);
        std::weak_ptr<Instance<TSpectral>> new_instance(Light<TSpectral>* light);
        std::weak_ptr<Instance<TSpectral>> new_instance(UnresolvedObject<TSpectral>* unresolved_object);
        std::weak_ptr<Instance<TSpectral>> new_instance(CameraModel<TSpectral>* camera_model);
        std::weak_ptr<Instance<TSpectral>> new_instance(Model<TSpectral>* model);

        std::string get_info() const override { return "FrameNode[" + std::to_string(this->id()) + "]" + (this->name_.empty() ? "" : " " + this->name_); }

        // Override to check children for SPICE constraints
        std::span<const std::shared_ptr<Node<TSpectral>>> get_children() const override { return children_; }

    protected:
        std::vector<std::shared_ptr<Node<TSpectral>>> children_;

        bool position_can_be_manual_() const override;
        bool rotation_can_be_manual_() const override;

        friend class Scene<TSpectral>;
        friend class SceneView<TSpectral>;
    };
}

#include "huira_impl/scene_graph/frame_node.ipp"
