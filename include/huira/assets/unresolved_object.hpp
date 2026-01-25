#pragma once

#include <string>
#include <cstdint>

#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/scene_graph/node.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class UnresolvedObject : public std::enable_shared_from_this<UnresolvedObject<TSpectral>> {
    public:
        UnresolvedObject(TSpectral irradiance = TSpectral{ 0 })
            : id_(next_id_++), irradiance_(irradiance) {}

        UnresolvedObject(const UnresolvedObject&) = delete;
        UnresolvedObject& operator=(const UnresolvedObject&) = delete;

        std::uint64_t id() const noexcept { return id_; }
        void set_name(const std::string& name) { name_ = name; }
        const std::string& name() const noexcept { return name_; }

        void set_irradiance(TSpectral irradiance) { irradiance_ = irradiance; }
        TSpectral get_irradiance() const { return irradiance_; }
        
        std::string get_info() const { return "UnresolvedObject[" + std::to_string(id_) + "]" + (this->name_.empty() ? "" : " " + this->name_); }

    protected:
        std::uint64_t id_ = 0;
        static inline std::uint64_t next_id_ = 0;
        std::string name_ = "";

        TSpectral irradiance_{ 0 };
    };
}

#include "huira_impl/assets/unresolved_object.ipp"
