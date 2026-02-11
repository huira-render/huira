namespace huira {
    template <IsSpectral TSpectral>
    FrameHandle<TSpectral> FrameHandle<TSpectral>::new_subframe() const {
        return FrameHandle<TSpectral>{ this->get_()->new_child() };
    }

    template <IsSpectral TSpectral>
    FrameHandle<TSpectral> FrameHandle<TSpectral>::new_spice_subframe(const std::string& spice_origin, const std::string& spice_frame) const {
        FrameHandle<TSpectral> subframe = this->new_subframe();
        subframe.set_spice(spice_origin, spice_frame);
        return subframe;
    }

    template <IsSpectral TSpectral>
    void FrameHandle<TSpectral>::delete_subframe(FrameHandle<TSpectral> subframe) const {
        this->get_()->delete_child(std::weak_ptr<Node<TSpectral>>(subframe.get()));
    }


    // New Instance:
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

    template <IsSpectral TSpectral>
    void FrameHandle<TSpectral>::delete_instance(InstanceHandle<TSpectral> instance) const {
        this->get_()->delete_child(std::weak_ptr<Node<TSpectral>>(instance.get()));
    }
}
