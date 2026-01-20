#pragma once

#include <vector>
#include <memory>
#include <unordered_map>
#include <stdexcept>
#include <string>

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

#include "huira/scene/frame_node.hpp"
#include "huira/handles/root_frame_handle.hpp"

namespace huira {
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class Scene {
    private:
        std::shared_ptr<FrameNode<TSpectral, TFloat>> root_node_;

    public:
        Scene(const Scene&) = delete; // Delete the copy constructor
        Scene& operator=(const Scene&) = delete; // Delete the copy assignment operator

        Scene();

        void lock() { locked_ = true; }
        void unlock() { locked_ = false; }
        bool is_locked() const { return locked_; }

        void set_time(const Time& time);
        Time get_time() const { return time_; }

        RootFrameHandle<TSpectral, TFloat> root;

        void print_graph() const;

    private:
        bool locked_ = false;

        Time time_;

        void print_node_(const Node<TSpectral, TFloat>* node, const std::string& prefix, bool is_last) const;
        void print_node_details_(const Node<TSpectral, TFloat>* node) const;

        friend class Node<TSpectral, TFloat>;
        friend class FrameNode<TSpectral, TFloat>;
    };
}

#include "huira_impl/scene/scene.ipp"
