namespace huira {
    template <IsSpectral TSpectral>
    void Camera<TSpectral>::set_focal_length(double focal_length) {
        focal_length_ = focal_length;
    }

    template <IsSpectral TSpectral>
    template <IsDistortion TDistortion, typename... Args>
    void Camera<TSpectral>::set_distortion(Args&&... args) {
        distortion_ = std::make_unique<TDistortion>(std::forward<Args>(args)...);
    }

    template <IsSpectral TSpectral>
    void Camera<TSpectral>::look_at(const Vec3<double>& target_position, Vec3<double> up)
    {
        Vec3<double> camera_position = this->get_global_position();
        Vec3<double> forward = glm::normalize(target_position - camera_position);

        // Assume up vector is +Y
        Vec3<double> right = glm::normalize(glm::cross(up, forward));
        up = glm::cross(forward, right);
        Rotation<double> rotation = Rotation<double>(right, up, forward);
        this->set_rotation(rotation);
    }
}
