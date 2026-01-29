#include <string>

#include "huira/core/concepts/spectral_concepts.hpp"

#include "huira/assets/mesh.hpp"
#include "huira/assets/lights/light.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    std::string Instance<TSpectral>::get_info() const {
        return "Instance[" + std::to_string(this->id()) + "]" +
            (this->name_.empty() ? "" : " " + this->name_) + " -> " +
            std::visit([](auto* ptr) { return ptr->get_info(); }, asset_);
    }
}
