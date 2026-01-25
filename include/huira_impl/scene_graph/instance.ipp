#include "huira/detail/concepts/spectral_concepts.hpp"

#include "huira/assets/mesh.hpp"
#include "huira/assets/lights/light.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    std::string Instance<TSpectral>::get_asset_name() const
    {
        std::string asset_name;
        std::visit([&asset_name](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, Mesh<TSpectral>*>) {
                asset_name += "Mesh[" + std::to_string(arg->id()) + "]";
            }
            else if constexpr (std::is_same_v<T, Light<TSpectral>*>) {
                asset_name += "Light[" + std::to_string(arg->id()) + "]";
            }
            }, asset_);
        return asset_name;
    }

    template <IsSpectral TSpectral>
    std::string Instance<TSpectral>::get_info() const
    {
        std::string info = "Instance[" + std::to_string(this->id()) + "] -> ";
        info += get_asset_name();
        return info;
    }
}
