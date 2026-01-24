#pragma once

#include <vector>
#include <memory>
#include <unordered_map>
#include <stdexcept>
#include <string>

#include "huira/detail/concepts/spectral_concepts.hpp"

#include "huira/objects/scene_graph/frame_node.hpp"
#include "huira/handles/root_frame_handle.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class Scene {
    private:
        std::shared_ptr<FrameNode<TSpectral>> root_node_;

    public:
        Scene(const Scene&) = delete; // Delete the copy constructor
        Scene& operator=(const Scene&) = delete; // Delete the copy assignment operator

        Scene();

        RootFrameHandle<TSpectral> root;

        void print_graph() const;

    private:
        void print_node_(const Node<TSpectral>* node, const std::string& prefix, bool is_last) const;
        void print_node_details_(const Node<TSpectral>* node) const;

        std::shared_ptr<Node<TSpectral>> find_node_shared_ptr_(const Node<TSpectral>* target) const;
        std::shared_ptr<Node<TSpectral>> find_node_in_tree_(const std::shared_ptr<Node<TSpectral>>& current, const Node<TSpectral>* target) const;

        friend class Node<TSpectral>;
        friend class FrameNode<TSpectral>;
    };
}

#include "huira_impl/core/scene.ipp"
