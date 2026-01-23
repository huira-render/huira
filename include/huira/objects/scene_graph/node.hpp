#pragma once

#include <memory>
#include <string>
#include <stdexcept>
#include <cstdint>

#include "huira/core/transform.hpp"

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

namespace huira {
    // Forward declare:
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class Scene;

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class FrameNode;

    enum class TransformSource {
        MANUAL_TRANSFORM,
        SPICE_TRANSFORM
    };

    /**
     * @brief Base class for all scene graph nodes.
     *
     * Node represents a transformable entity in the scene graph. It handles:
     * - Local and global transforms (position, rotation, scale)
     * - SPICE-based transforms for celestial mechanics
     * - Parent-child relationships (being a child)
     *
     * Node itself cannot have children - use FrameNode for nodes that need children.
     * Leaf nodes (lights, unresolved objects, etc.) should derive from Node directly.
     */
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class Node {
    public:
        Node(Scene<TSpectral, TFloat>* scene);
        virtual ~Node() = default;

        // Delete copying:
        Node(const Node&) = delete;
        Node& operator=(const Node&) = delete;

        std::uint64_t id() const { return id_; }

        void set_position(const Vec3<TFloat>& position);
        void set_rotation(const Rotation<TFloat>& rotation);
        void set_scale(const Vec3<TFloat>& scale);

        void set_velocity(const Vec3<TFloat>& velocity);
        void set_angular_velocity(const Vec3<TFloat>& angular_velocity);

        void set_spice_origin(const std::string& spice_origin);
        void set_spice_frame(const std::string& spice_frame);
        void set_spice(const std::string& spice_origin, const std::string& spice_frame);


        virtual std::string get_info() const;
        virtual std::string get_type_name() const { return "Node"; }


        Vec3<TFloat> get_global_position() const { return global_transform_.position; }
        Vec3<TFloat> get_local_position() const { return local_transform_.position; }

        Rotation<TFloat> get_global_rotation() const { return global_transform_.rotation; }
        Rotation<TFloat> get_local_rotation() const { return local_transform_.rotation; }

        Vec3<TFloat> get_global_scale() const { return global_transform_.scale; }
        Vec3<TFloat> get_local_scale() const { return local_transform_.scale; }

        Vec3<TFloat> get_global_velocity() const { return global_transform_.velocity; }
        Vec3<TFloat> get_local_velocity() const { return local_transform_.velocity; }

        Vec3<TFloat> get_global_angular_velocity() const { return global_transform_.angular_velocity; }
        Vec3<TFloat> get_local_angular_velocity() const { return local_transform_.angular_velocity; }


        std::string get_spice_origin() const { return spice_origin_; }
        std::string get_spice_frame() const { return spice_frame_; }


        // Query state relative to arbitrary SPICE frames
        Vec3<TFloat> get_position_in_frame(const std::string& target_origin, const std::string& target_frame) const;
        Vec3<TFloat> get_velocity_in_frame(const std::string& target_origin, const std::string& target_frame) const;

        Rotation<TFloat> get_rotation_in_frame(const std::string& target_frame) const;
        Vec3<TFloat> get_angular_velocity_in_frame(const std::string& target_frame) const;

        std::pair<Vec3<TFloat>, Vec3<TFloat>> get_state_in_frame(const std::string& target_origin, const std::string& target_frame) const;
        std::pair<Rotation<TFloat>, Vec3<TFloat>> get_attitude_in_frame(const std::string& target_frame) const;

    protected:
        Transform<TFloat> local_transform_;
        Transform<TFloat> global_transform_;

        TransformSource position_source_ = TransformSource::MANUAL_TRANSFORM;
        TransformSource rotation_source_ = TransformSource::MANUAL_TRANSFORM;

        std::string spice_origin_ = "";
        std::string spice_frame_ = "";

        std::uint64_t id_ = 0;
        static inline std::uint64_t next_id_ = 0;

        Scene<TSpectral, TFloat>* scene_;
        Node<TSpectral, TFloat>* parent_ = nullptr;

        void set_parent_(Node<TSpectral, TFloat>* parent) { parent_ = parent; }

        
        virtual void on_transform_changed_() {}

        virtual const std::vector<std::shared_ptr<Node<TSpectral, TFloat>>>* get_children_() const { return nullptr; }
        virtual std::shared_ptr<Node<TSpectral, TFloat>> child_spice_origins_() const { return nullptr; }
        virtual std::shared_ptr<Node<TSpectral, TFloat>> child_spice_frames_() const { return nullptr; }

        void update_spice_transform_();
        virtual void update_all_spice_transforms_();
        virtual void update_global_transform_();

        void validate_scene_unlocked_(const std::string function_name);
        void validate_spice_origin_allowed_();
        void validate_spice_frame_allowed_();

        void compute_global_spice_position_();
        void compute_global_spice_rotation_();

        void compute_local_position_from_global_();
        void compute_local_rotation_from_global_();

        void compute_global_position_from_local_();
        void compute_global_rotation_from_local_();

        std::pair<const Node<TSpectral, TFloat>*, Transform<TFloat>> find_spice_origin_ancestor_() const;
        std::pair<const Node<TSpectral, TFloat>*, std::pair<Rotation<TFloat>, Vec3<TFloat>>> find_spice_frame_ancestor_() const;

        friend class Scene<TSpectral, TFloat>;
        friend class FrameNode<TSpectral, TFloat>;
    };
}

#include "huira_impl/objects/scene_graph/node.ipp"
