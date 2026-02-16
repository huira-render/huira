#pragma once

#include <memory>
#include <span>
#include <string>
#include <vector>

#include "huira/core/concepts/spectral_concepts.hpp"

#include "huira/scene/instance.hpp"
#include "huira/scene/node.hpp"


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

    /**
     * @brief Scene graph node that can have children.
     *
     * FrameNode represents a node in the scene graph that can have child nodes and leaf objects.
     * Provides child management and factory methods for leaf node creation.
     *
     * @tparam TSpectral Spectral type (e.g., RGB, Spectral)
     */
    template <IsSpectral TSpectral>
    class FrameNode : public Node<TSpectral> {
    public:
        FrameNode(Scene<TSpectral>* scene);
        ~FrameNode() override = default;

        FrameNode(const FrameNode&) = delete;
        FrameNode& operator=(const FrameNode&) = delete;

        std::weak_ptr<FrameNode<TSpectral>> new_child();

        void delete_child(std::weak_ptr<Node<TSpectral>> child);

        // Factory methods for leaf nodes:
        std::weak_ptr<Camera<TSpectral>> new_camera();
        std::weak_ptr<Instance<TSpectral>> new_instance(Mesh<TSpectral>* mesh);
        std::weak_ptr<Instance<TSpectral>> new_instance(Light<TSpectral>* light);
        std::weak_ptr<Instance<TSpectral>> new_instance(UnresolvedObject<TSpectral>* unresolved_object);
        std::weak_ptr<Instance<TSpectral>> new_instance(CameraModel<TSpectral>* camera_model);
        std::weak_ptr<Instance<TSpectral>> new_instance(Model<TSpectral>* model);


        std::string type() const override { return "FrameNode"; }

        std::span<const std::shared_ptr<Node<TSpectral>>> get_children() const override { return children_; }

    protected:
        std::vector<std::shared_ptr<Node<TSpectral>>> children_;

        bool position_can_be_manual_() const override;
        bool rotation_can_be_manual_() const override;

        friend class Scene<TSpectral>;
        friend class SceneView<TSpectral>;
    };
}

#include "huira_impl/scene/frame_node.ipp"
