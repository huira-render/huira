#pragma once

#include <vector>
#include <memory>
#include <unordered_map>
#include <stdexcept>
#include <string>

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

#include "huira/scene/nodes.hpp"
#include "huira/handles/node_handles.hpp"

namespace huira {
	template <IsSpectral TSpectral, IsFloatingPoint TFloat>
	class Scene {
	public:
        Scene(const Scene&) = delete; // Delete the copy constructor
        Scene& operator=(const Scene&) = delete; // Delete the copy assignment operator

        Scene()
        {
            root_node_ = std::make_unique<Node<TSpectral, TFloat>>(this);
        };

        void lock() { locked_ = true; }
        void unlock() { locked_ = false; }
        bool is_locked() const { return locked_; }

        NodeHandle<TSpectral, TFloat> new_node(std::string name = "")
        {
            std::weak_ptr<Node<TSpectral, TFloat>> node = this->root_node_->new_child(name);
            return NodeHandle<TSpectral, TFloat>{ node, &locked_ };
        }

        void add_node_name(const std::string& name, std::weak_ptr<Node<TSpectral, TFloat>> node)
        {
            if (name.empty()) {
                return;
            }

            if (node_names_.contains(name)) {
                throw std::runtime_error("A Node already exists with the name: " + name);
            }
            node_names_[name] = node;
        };

        NodeHandle<TSpectral, TFloat> get_named_node(const std::string& name)
        {
            if (auto it = node_names_.find(name); it != node_names_.end()) {
                return NodeHandle<TSpectral, TFloat>{ it->second, &locked_ };
            }
            throw std::runtime_error("No Node named: " + name);
        }

        const std::string& name_of(const Node<TSpectral, TFloat>* node) const {
            for (const auto& [name, ptr] : node_names_) {
                if (auto sptr = ptr.lock()) {
                    if (sptr.get() == node) {
                        return name;
                    }
                }
            }
            return "";
        }

        const std::string& name_of(const NodeHandle<TSpectral, TFloat>& handle) const {
            return handle.name();
        }

	private:
        bool locked_ = false;

        std::unique_ptr<Node<TSpectral, TFloat>> root_node_;

        std::unordered_map<std::string, std::weak_ptr<Node<TSpectral, TFloat>>> node_names_;
	};
}

#include "huira_impl/scene/scene.ipp"

