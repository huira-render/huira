#pragma once

#include <vector>
#include <memory>
#include <unordered_map>
#include <stdexcept>
#include <string>

#include "huira/detail/concepts/spectral_concepts.hpp"

#include "huira/assets/mesh.hpp"
#include "huira/handles/root_frame_handle.hpp"
#include "huira/handles/mesh_handle.hpp"

namespace huira {
    // Forward declare:
    template <IsSpectral TSpectral>
    class SceneView;


    template <IsSpectral TSpectral>
    class Scene {
    private:
        std::shared_ptr<FrameNode<TSpectral>> root_node_;

    public:
        Scene(const Scene&) = delete; // Delete the copy constructor
        Scene& operator=(const Scene&) = delete; // Delete the copy assignment operator

        Scene();

        RootFrameHandle<TSpectral> root;
        
        MeshHandle<TSpectral> add_mesh(Mesh<TSpectral>&& mesh);
        void delete_mesh(const MeshHandle<TSpectral>& mesh_handle);

        PointLightHandle<TSpectral> new_point_light(TSpectral intensity);
        void delete_light(const PointLightHandle<TSpectral>& light_handle);

        void print_meshes() const;
        void print_lights() const;
        void print_graph() const;
        void print_contents() const;

    private:
        // Assets:
        std::vector<std::shared_ptr<Mesh<TSpectral>>> meshes_;
        std::vector<std::shared_ptr<Light<TSpectral>>> lights_;

        void print_node_(const Node<TSpectral>* node, const std::string& prefix, bool is_last) const;
        void print_node_details_(const Node<TSpectral>* node) const;

        template <typename TAssetPtr>
        void prune_graph_references_(TAssetPtr target_ptr);

        std::shared_ptr<Node<TSpectral>> find_node_shared_ptr_(const Node<TSpectral>* target) const;
        std::shared_ptr<Node<TSpectral>> find_node_in_tree_(const std::shared_ptr<Node<TSpectral>>& current, const Node<TSpectral>* target) const;

        friend class Node<TSpectral>;
        friend class FrameNode<TSpectral>;
        friend class SceneView<TSpectral>;
    };
}

#include "huira_impl/core/scene.ipp"
