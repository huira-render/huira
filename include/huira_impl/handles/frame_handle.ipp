namespace huira {
    template <IsSpectral TSpectral>
    FrameHandle<TSpectral> FrameHandle<TSpectral>::new_subframe() const {
        return FrameHandle<TSpectral>{ this->get()->new_child() };
    }

    template <IsSpectral TSpectral>
    FrameHandle<TSpectral> FrameHandle<TSpectral>::new_spice_subframe(const std::string& spice_origin, const std::string& spice_frame) const {
        FrameHandle<TSpectral> subframe = this->new_subframe();
        subframe.set_spice(spice_origin, spice_frame);
        return subframe;
    }

    template <IsSpectral TSpectral>
    void FrameHandle<TSpectral>::delete_subframe(FrameHandle<TSpectral> subframe) const {
        this->get()->delete_child(std::weak_ptr<Node<TSpectral>>(subframe.get()));
    }



    // Unresolbed Object:
    template <IsSpectral TSpectral>
    UnresolvedObjectHandle<TSpectral> FrameHandle<TSpectral>::new_unresolved_object() const {
        return UnresolvedObjectHandle<TSpectral>{ this->get()->new_unresolved_object() };
    }

    template <IsSpectral TSpectral>
    UnresolvedObjectHandle<TSpectral> FrameHandle<TSpectral>::new_spice_unresolved_object(const std::string& spice_origin) const {
        UnresolvedObjectHandle<TSpectral> child = this->new_unresolved_object();
        child.set_spice_origin(spice_origin);
        return child;
    }



    // New Camera:
    template <IsSpectral TSpectral>
    CameraHandle<TSpectral> FrameHandle<TSpectral>::new_camera() const {
        return CameraHandle<TSpectral>{ this->get()->new_camera() };
    }

    template <IsSpectral TSpectral>
    CameraHandle<TSpectral> FrameHandle<TSpectral>::new_spice_camera(const std::string& spice_origin, const std::string& spice_frame) const {
        CameraHandle<TSpectral> child = this->new_camera();
        child.set_spice(spice_origin, spice_frame);
        return child;
    }


    // New Instance:
    template <IsSpectral TSpectral>
    template <typename THandle>
        requires std::is_constructible_v<Instantiable<TSpectral>, decltype(std::declval<THandle>().get().get())>
    InstanceHandle<TSpectral> FrameHandle<TSpectral>::new_instance(const THandle& asset_handle) const
    {
        auto asset_shared = asset_handle.get();
        auto* asset_raw = asset_shared.get();
        auto instance_weak = this->get()->new_instance(asset_raw);

        return InstanceHandle<TSpectral>(instance_weak);
    }
}
