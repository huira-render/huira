#pragma once

#include <vector>
#include <memory>
#include <unordered_map>
#include <stdexcept>
#include <string>

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

#include "huira/scene/node.hpp"
#include "huira/handles/node_handle.hpp"

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


        NodeHandle<TSpectral, TFloat> new_node(std::string name = "");
        void add_node_name(const std::string& name, std::weak_ptr<Node<TSpectral, TFloat>> node);
        NodeHandle<TSpectral, TFloat> get_named_node(const std::string& name);


        const std::string& name_of(const Node<TSpectral, TFloat>* node) const;
        const std::string& name_of(const NodeHandle<TSpectral, TFloat>& handle) const { return handle.name(); }


        void print_graph() const;
        void print_node(const Node<TSpectral, TFloat>* node, const std::string& prefix, bool is_last) const;
        void print_node_details(const Node<TSpectral, TFloat>* node) const;

	private:
        bool locked_ = false;

        std::unique_ptr<Node<TSpectral, TFloat>> root_node_;

        std::unordered_map<std::string, std::weak_ptr<Node<TSpectral, TFloat>>> node_names_;
	};
}

#include "huira_impl/scene/scene.ipp"

