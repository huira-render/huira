
#include "huira/math/rotation.hpp"

#include "huira/radiometry/spectral_bins.hpp"

#include "huira/scene/reference_frame.hpp"
#include "huira/handles/reference_frame_handle.hpp"
#include "huira/scene/scene.hpp"

namespace huira {
    using TestSpectral = RGB;
    using TestFloat = float;

    // Explicit instantiations - forces code generation for code coverage purposes
    template class Scene<TestSpectral, TestFloat>;

    template class ReferenceFrame<TestSpectral, TestFloat>;
    template class ReferenceFrameHandle<TestSpectral, TestFloat>;
    
    template class Rotation<float>;
    

}
