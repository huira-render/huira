namespace huira {
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    FrameHandle<TSpectral, TFloat> FrameHandle<TSpectral, TFloat>::new_subframe() const {
        return FrameHandle<TSpectral, TFloat>{ this->get()->new_child() };
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    FrameHandle<TSpectral, TFloat> FrameHandle<TSpectral, TFloat>::new_spice_subframe(const std::string& spice_origin, const std::string& spice_frame) const {
        FrameHandle<TSpectral, TFloat> subframe = this->new_subframe();
        subframe.set_spice(spice_origin, spice_frame);
        return subframe;
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void FrameHandle<TSpectral, TFloat>::delete_subframe(FrameHandle<TSpectral, TFloat> subframe) const {
        this->get()->delete_child(std::weak_ptr<Node<TSpectral, TFloat>>(subframe.get()));
    }



    // Unresolbed Object:
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    UnresolvedObjectHandle<TSpectral, TFloat> FrameHandle<TSpectral, TFloat>::new_unresolved_object() const {
        return UnresolvedObjectHandle<TSpectral, TFloat>{ this->get()->new_unresolved_object() };
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    UnresolvedObjectHandle<TSpectral, TFloat> FrameHandle<TSpectral, TFloat>::new_spice_unresolved_object(const std::string& spice_origin) const {
        UnresolvedObjectHandle<TSpectral, TFloat> child = this->new_unresolved_object();
        child.set_spice_origin(spice_origin);
        return child;
    }




    // New Point Light:
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    PointLightHandle<TSpectral, TFloat> FrameHandle<TSpectral, TFloat>::new_point_light(TSpectral spectral_intensity) const {
        return PointLightHandle<TSpectral, TFloat>{ this->get()->new_point_light(spectral_intensity) };
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    PointLightHandle<TSpectral, TFloat> FrameHandle<TSpectral, TFloat>::new_spice_point_light(const std::string& spice_origin, TSpectral spectral_intensity) const {
        PointLightHandle<TSpectral, TFloat> child = this->new_point_light(spectral_intensity);
        child.set_spice_origin(spice_origin);
        return child;
    }



    // New Camera:
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    CameraHandle<TSpectral, TFloat> FrameHandle<TSpectral, TFloat>::new_camera() const {
        return CameraHandle<TSpectral, TFloat>{ this->get()->new_camera() };
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    CameraHandle<TSpectral, TFloat> FrameHandle<TSpectral, TFloat>::new_spice_camera(const std::string& spice_origin, const std::string& spice_frame) const {
        CameraHandle<TSpectral, TFloat> child = this->new_camera();
        child.set_spice(spice_origin, spice_frame);
        return child;
    }
}
