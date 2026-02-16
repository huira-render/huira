#include <string>

#include "huira/core/concepts/spectral_concepts.hpp"

#include "huira/assets/lights/light.hpp"
#include "huira/assets/mesh.hpp"

namespace huira {

    /**
     * @brief Get a descriptive string for this instance, including asset info.
     * @return std::string Info string
     */
    template <IsSpectral TSpectral>
    std::string Instance<TSpectral>::get_info() const {
        return "Instance[" + std::to_string(this->id()) + "]" +
            (this->name().empty() ? "" : " " + this->name()) + " -> " +
            std::visit([](auto* ptr) { return ptr->get_info(); }, asset_);
    }
}
