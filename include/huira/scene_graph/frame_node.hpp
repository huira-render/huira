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
        std::weak_ptr<Instance<TSpectral>> new_instance(Mesh<TSpectral>* mesh)
        {
            auto child = std::make_shared<Instance<TSpectral>>(this->scene_, mesh);
            child->set_parent_(this);

            HUIRA_LOG_INFO(this->get_info() + " - new Mesh Instance added: " + child->get_info());

            children_.push_back(child);
            return child;
        }
        std::weak_ptr<Instance<TSpectral>> new_instance(Light<TSpectral>* light)
        {
            auto child = std::make_shared<Instance<TSpectral>>(this->scene_, light);
            child->set_parent_(this);

            HUIRA_LOG_INFO(this->get_info() + " - new Light Instance added: " + child->get_info());

            children_.push_back(child);
            return child;
        }

        std::string get_type_name() const override { return "FrameNode"; }

        // Override to check children for SPICE constraints
        std::span<const std::shared_ptr<Node<TSpectral>>> get_children() const override { return children_; }

    protected:
        std::vector<std::shared_ptr<Node<TSpectral>>> children_;

        bool position_can_be_manual_() const override;
        bool rotation_can_be_manual_() const override;

        friend class Scene<TSpectral>;
    };
}

#include "huira_impl/scene_graph/frame_node.ipp"
