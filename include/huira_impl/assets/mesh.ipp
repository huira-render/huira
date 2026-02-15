namespace huira {
    /**
     * @brief Constructs a Mesh from index and vertex buffers.
     * 
     * Creates a new mesh by taking ownership of the provided buffers. The index
     * buffer should contain triangle indices (groups of three) that reference
     * vertices in the vertex buffer.
     * 
     * @param index_buffer Buffer containing triangle vertex indices.
     * @param vertex_buffer Buffer containing vertex data (positions, normals, etc.).
     */
    template <IsSpectral TSpectral>
    Mesh<TSpectral>::Mesh(IndexBuffer index_buffer, VertexBuffer<TSpectral> vertex_buffer) noexcept
            : index_buffer_(std::move(index_buffer)),
            vertex_buffer_(std::move(vertex_buffer)),
            id_(next_id_++)
    {
        
    }

    /**
     * @brief Returns the number of indices in the mesh.
     * 
     * @return The total number of indices (3 times the triangle count).
     */
    template <IsSpectral TSpectral>
    std::size_t Mesh<TSpectral>::index_count() const noexcept
    {
        return index_buffer_.size();
    }
    
    /**
     * @brief Returns the number of vertices in the mesh.
     * 
     * @return The total number of vertices in the vertex buffer.
     */
    template <IsSpectral TSpectral>
    std::size_t Mesh<TSpectral>::vertex_count() const noexcept {
        return vertex_buffer_.size();
    }
    
    
    /**
     * @brief Returns the number of triangles in the mesh.
     * 
     * @return The number of triangles (index count divided by 3).
     */
    template <IsSpectral TSpectral>
    std::size_t Mesh<TSpectral>::triangle_count() const noexcept 
    { 
        return index_buffer_.size() / 3;
    }

    /**
     * @brief Returns a const reference to the index buffer.
     * 
     * @return Const reference to the index buffer containing triangle indices.
     */
    template <IsSpectral TSpectral>
    [[nodiscard]] const IndexBuffer& Mesh<TSpectral>::index_buffer() const noexcept 
    { 
        return index_buffer_;
    }

    /**
     * @brief Returns a const reference to the vertex buffer.
     * 
     * @return Const reference to the vertex buffer containing vertex data.
     */
    template <IsSpectral TSpectral>
    [[nodiscard]] const VertexBuffer<TSpectral>& Mesh<TSpectral>::vertex_buffer() const noexcept 
    { 
        return vertex_buffer_;
    }
}
