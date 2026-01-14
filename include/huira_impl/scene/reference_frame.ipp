namespace huira {
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    const std::string& ReferenceFrame<TSpectral, TFloat>::name() const {
        return scene_->name_of(this);
    }
}
