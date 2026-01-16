
#include "huira/core/rotation.hpp"
#include "huira/core/transform.hpp"

#include "huira/handles/node_handle.hpp"

#include "huira/scene/node.hpp"
#include "huira/scene/scene.hpp"

#include "huira/spectral/spectral_bins.hpp"

namespace huira {
    using TestSpectral = RGB;
    using TestFloat = float;

    // Explicit instantiations - forces code generation for code coverage purposes
    template class Rotation<TestFloat>;
    template struct Transform<TestFloat>;

    template class NodeHandle<TestSpectral, TestFloat>;

    template class Scene<TestSpectral, TestFloat>;
    template class Node<TestSpectral, TestFloat>;
    
}
