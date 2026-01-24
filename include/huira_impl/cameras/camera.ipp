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
}
