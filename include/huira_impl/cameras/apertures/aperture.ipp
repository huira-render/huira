

namespace huira {
    template <IsSpectral TSpectral>
    void Aperture<TSpectral>::build_defocus_kernel(
        units::Diopter defocus,
        units::Meter focal_length,
        units::Meter pitch_x,
        units::Meter pitch_y,
        int banks)
    {
        (void)defocus;
        (void)focal_length;
        (void)pitch_x;
        (void)pitch_y;
        (void)banks;
    }

    template <IsSpectral TSpectral>
    const Image<float>& Aperture<TSpectral>::get_defocus_kernel(float u, float v) const
    {
        (void)u;
        (void)v;
        return defocus_cache_.kernels[0];
    }


    template <IsSpectral TSpectral>
    void Aperture<TSpectral>::generate_polyphase_data_()
    {

    }
}
