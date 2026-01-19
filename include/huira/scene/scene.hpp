#pragma once

#include <vector>
#include <memory>
#include <unordered_map>
#include <stdexcept>
#include <string>

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

#include "huira/scene/node.hpp"
#include "huira/handles/frame_handle.hpp"

namespace huira {
	template <IsSpectral TSpectral, IsFloatingPoint TFloat>
	class Scene {
	public:
        Scene(const Scene&) = delete; // Delete the copy constructor
        Scene& operator=(const Scene&) = delete; // Delete the copy assignment operator

        Scene();

        void lock() { locked_ = true; }
        void unlock() { locked_ = false; }
        bool is_locked() const { return locked_; }

        void set_time(const Time& time);
        Time get_time() const { return time_; }


        void set_spice(const std::string& spice_origin, const std::string& spice_frame) { root_node_->set_spice(spice_origin, spice_frame); }
        void set_spice_origin(const std::string& spice_origin) { root_node_->set_spice_origin(spice_origin); }
        void set_spice_frame(const std::string& spice_frame) { root_node_->set_spice_frame(spice_frame); }
        std::string get_spice_origin() const { return root_node_->spice_origin_; }
        std::string get_spice_frame() const { return root_node_->spice_frame_; }


        FrameHandle<TSpectral, TFloat> new_subframe(std::string name = "");
        FrameHandle<TSpectral, TFloat> new_spice_subframe(const std::string& spice_origin, const std::string& spice_frame, std::string name = "");

        FrameHandle<TSpectral, TFloat> get_named_frame(const std::string& name);
        const std::string& name_of(const FrameHandle<TSpectral, TFloat>& handle) const { return handle.name(); }

        void print_graph() const;

	private:
        bool locked_ = false;

        Time time_;

        std::unique_ptr<Node<TSpectral, TFloat>> root_node_;

        std::unordered_map<std::string, std::weak_ptr<Node<TSpectral, TFloat>>> node_names_;


        const std::string& name_of_node_(const Node<TSpectral, TFloat>* node) const;
        void add_node_name_(const std::string& name, std::weak_ptr<Node<TSpectral, TFloat>> node);

        void print_node_(const Node<TSpectral, TFloat>* node, const std::string& prefix, bool is_last) const;
        void print_node_details_(const Node<TSpectral, TFloat>* node) const;

        friend class Node<TSpectral, TFloat>;
	};
}

#include "huira_impl/scene/scene.ipp"

