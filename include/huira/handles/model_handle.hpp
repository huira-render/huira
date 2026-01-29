#pragma once

#include "huira/assets/model.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/handles/handle.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class Scene;

    template <IsSpectral TSpectral>
    class FrameHandle;

    template <IsSpectral TSpectral>
    class ModelHandle : public Handle<Model<TSpectral>> {
    public:
        ModelHandle() = delete;
        using Handle<Model<TSpectral>>::Handle;

        void print_graph() const {
            this->get()->print_graph();
        }

    private:
        friend class Scene<TSpectral>;
        friend class FrameHandle<TSpectral>;
    };

}
