namespace huira {
    /**
     * @brief Creates a new subframe as a child of this frame.
     * @return FrameHandle<TSpectral> Handle to the new subframe
     */
    template <IsSpectral TSpectral>
    FrameHandle<TSpectral> FrameHandle<TSpectral>::new_subframe() const {
        return FrameHandle<TSpectral>{ this->get_()->new_child() };
    }

    /**
     * @brief Creates a new subframe and sets its SPICE origin and frame.
     * @param spice_origin SPICE origin string
     * @param spice_frame SPICE frame string
     * @return FrameHandle<TSpectral> Handle to the new SPICE subframe
     */
    template <IsSpectral TSpectral>
    FrameHandle<TSpectral> FrameHandle<TSpectral>::new_spice_subframe(const std::string& spice_origin, const std::string& spice_frame) const {
        FrameHandle<TSpectral> subframe = this->new_subframe();
        subframe.set_spice(spice_origin, spice_frame);
        return subframe;
    }

    /**
     * @brief Deletes a subframe from this frame.
     * @param subframe Handle to the subframe to delete
     */
    template <IsSpectral TSpectral>
    void FrameHandle<TSpectral>::delete_subframe(FrameHandle<TSpectral> subframe) const {
        this->get_()->delete_child(std::weak_ptr<Node<TSpectral>>(subframe.get()));
    }


    /**
     * @brief Creates a new instance of an asset in this frame.
     * @tparam THandle Asset handle type
     * @param asset_handle Handle to the asset to instantiate
     * @return InstanceHandle<TSpectral> Handle to the new instance
     */
    template <IsSpectral TSpectral>
    template <typename THandle>
        requires std::is_constructible_v<Instantiable<TSpectral>, decltype(std::declval<THandle>().get().get())>
    InstanceHandle<TSpectral> FrameHandle<TSpectral>::new_instance(const THandle& asset_handle) const
    {
        auto asset_shared = asset_handle.get();
        auto* asset_raw = asset_shared.get();
        auto instance_weak = this->get_()->new_instance(asset_raw);

        return InstanceHandle<TSpectral>(instance_weak);
    }

    /**
     * @brief Deletes an instance from this frame.
     * @param instance Handle to the instance to delete
     */
    template <IsSpectral TSpectral>
    void FrameHandle<TSpectral>::delete_instance(InstanceHandle<TSpectral> instance) const {
        this->get_()->delete_child(std::weak_ptr<Node<TSpectral>>(instance.get()));
    }
}
