
#include "huira/math/rotation.hpp"

#include "huira/radiometry/spectral_bins.hpp"

#include "huira/scene/nodes.hpp"
#include "huira/handles/node_handles.hpp"
#include "huira/scene/scene.hpp"

namespace huira {
    using TestSpectral = RGB;
    using TestFloat = float;

    // Explicit instantiations - forces code generation for code coverage purposes
    template class Scene<TestSpectral, TestFloat>;

    template class Node<TestSpectral, TestFloat>;
    template class NodeHandle<TestSpectral, TestFloat>;
    
    template class Rotation<float>;
    

}
