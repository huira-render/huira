#pragma once

#include <vector>
#include <memory>
#include <string>
#include <stdexcept>
#include <cstdint>

#include "huira/core/transform.hpp"
#include "huira/core/time.hpp"

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

namespace huira {
    // Forward declare:
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class Scene;

    enum class TransformSource {
        Manual,
        Spice
    };

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class Node {
    public:
        Node(Scene<TSpectral, TFloat>* scene);
        virtual ~Node() = default;

        const std::string& name() const { return scene_->name_of_node_(this); }
        std::uint64_t id() const { return id_; }

        void set_position(const Vec3<TFloat>& position);
        void set_rotation(const Rotation<TFloat>& rotation);
        void set_scale(const Vec3<TFloat>& scale);

        void set_velocity(const Vec3<TFloat>& velocity);
        void set_angular_velocity(const Vec3<TFloat>& angular_velocity);

        void set_spice_origin(const std::string& spice_origin);
        void set_spice_frame(const std::string& spice_frame);
        void set_spice(const std::string& spice_origin, const std::string& spice_frame);


        std::weak_ptr<Node<TSpectral, TFloat>> new_child(std::string name = "");
        void delete_child(std::weak_ptr<Node<TSpectral, TFloat>> child);
        void change_parent(std::weak_ptr<Node<TSpectral, TFloat>> self_weak, Node<TSpectral, TFloat>* new_parent);

    protected:
        Transform<TFloat> local_transform_;
        Transform<TFloat> global_transform_;
        
        TransformSource position_source_ = TransformSource::Manual;
        TransformSource rotation_source_ = TransformSource::Manual;

        std::string spice_origin_ = "";
        std::string spice_frame_ = "";

        std::uint64_t id_ = 0;
        static inline std::uint64_t next_id_ = 0;

        void set_parent_(Node<TSpectral, TFloat>* parent) { parent_ = parent; }

        std::shared_ptr<Node<TSpectral, TFloat>> child_spice_origins_() const;
        std::shared_ptr<Node<TSpectral, TFloat>> child_spice_frames_() const;

        void update_spice_transform_();
        void update_all_spice_transforms_();

        virtual void update_global_transform_();

        virtual std::string get_info_();

    private:
        Scene<TSpectral, TFloat>* scene_;
        Node<TSpectral, TFloat>* parent_ = nullptr;

        std::vector<std::shared_ptr<Node<TSpectral, TFloat>>> children_;

        friend class Scene<TSpectral, TFloat>;
    };
}

#include "huira_impl/scene/node.ipp"
