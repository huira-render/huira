#include <string>

#include "huira/concepts/spectral_concepts.hpp"

#include "huira/assets/lights/light.hpp"
#include "huira/geometry/mesh.hpp"

namespace huira {

    /**
     * @brief Get a descriptive string for this instance, including asset info.
     * @return std::string Info string
     */
    template <IsSpectral TSpectral>
    std::string Instance<TSpectral>::get_info() const {
        return "Instance[" + std::to_string(this->id()) + "]" +
            (this->name().empty() ? "" : " " + this->name()) + " -> " +
            std::visit([](auto* ptr) {
            std::string info = ptr->get_info();
            if constexpr (std::is_same_v<decltype(ptr), Mesh<TSpectral>*>) {
                if (ptr->material()) {
                    info += " -> " + ptr->material()->get_info();
                }
            }
            return info;
                }, asset_);
    }
}
