namespace huira {
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Camera<TSpectral, TFloat>::set_focal_length(TFloat focal_length) {
        focal_length_ = focal_length;
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    template <IsDistortion TDistortion, typename... Args>
    void Camera<TSpectral, TFloat>::set_distortion(Args&&... args) {
        distortion_ = std::make_unique<TDistortion>(std::forward<Args>(args)...);
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Camera<TSpectral, TFloat>::look_at(const Vec3<TFloat>& target_position, Vec3<TFloat> up)
    {
        Vec3<TFloat> camera_position = this->get_global_position();
        Vec3<TFloat> forward = glm::normalize(target_position - camera_position);

        // Assume up vector is +Y
        Vec3<TFloat> right = glm::normalize(glm::cross(up, forward));
        up = glm::cross(forward, right);
        Rotation<TFloat> rotation = Rotation<TFloat>(right, up, forward);
        this->set_rotation(rotation);
    }
}
