#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/detail/logger.hpp"
#include "huira/render/frame_buffer.hpp"
#include "huira/images/image.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    void SimpleColorSensor<TSpectral>::readout(FrameBuffer<TSpectral>& fb, float exposure_time) const {
        (void)fb;
        (void)exposure_time;
        HUIRA_THROW_ERROR("SimpleColorSensor::readout not yet implemented");
    }
}
