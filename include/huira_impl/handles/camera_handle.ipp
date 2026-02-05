#include "huira/core/concepts/spectral_concepts.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::set_focal_length(float focal_length) const {
        this->get()->set_focal_length(focal_length);
    }

    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::set_fstop(float fstop) const
    {
        this->get()->set_fstop(fstop);
    }

    template <IsSpectral TSpectral>
    FrameBuffer<TSpectral> CameraModelHandle<TSpectral>::make_frame_buffer() const {
        return this->get()->make_frame_buffer();
    }
}
