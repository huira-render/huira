#pragma once

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

#include "huira/scene/reference_frame.hpp"

#include "huira/scene/handle.hpp"

namespace huira {
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class ReferenceFrameHandle : public Handle<ReferenceFrame<TSpectral, TFloat>> {
    public:
        ReferenceFrameHandle() = delete;
        using Handle<ReferenceFrame<TSpectral, TFloat>>::Handle;

        double get_x() const {
            return this->get()->get_x();
        }

        const std::string& name() const {
            return this->get()->name();
        }
    };

}
