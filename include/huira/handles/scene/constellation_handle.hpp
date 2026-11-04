#pragma once

#include <cstddef>
#include <filesystem>
#include <vector>

#include "huira/concepts/spectral_concepts.hpp"
#include "huira/ephemeris/element_set_catalog.hpp"
#include "huira/ephemeris/satrec.hpp"
#include "huira/handles/assets/unresolved_handle.hpp"
#include "huira/handles/handle.hpp"
#include "huira/scene/constellation.hpp"

namespace huira {

/**
 * @brief Handle for a constellation: satellites placed by SGP4 and seen as points (see
 * Constellation).
 *
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 */
template <IsSpectral TSpectral>
class ConstellationHandle : public Handle<Constellation<TSpectral>> {
  public:
    ConstellationHandle() = delete;
    using Handle<Constellation<TSpectral>>::Handle;

    ConstellationReport add(const std::vector<SatRec>& element_sets) const;
    ConstellationReport add(const ElementSetCatalog& catalog) const;

    ConstellationReport load(const std::filesystem::path& path,
                             const ElementSetOptions& options = {}) const;
    ConstellationReport load(const std::filesystem::path& path,
                             ElementSetFormat format,
                             const ElementSetOptions& options = {}) const;

    bool remove(int catalog_number) const;
    void clear() const;

    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] bool contains(int catalog_number) const;
    [[nodiscard]] SatRec element_set(int catalog_number) const;
    [[nodiscard]] std::vector<SatRec> element_sets() const;

    void set_unresolved_object(const UnresolvedObjectHandle<TSpectral>& unresolved_object) const;
    [[nodiscard]] UnresolvedObjectHandle<TSpectral> get_unresolved_object() const;

    void set_visible(bool visible) const;
    [[nodiscard]] bool is_visible() const;
};

} // namespace huira

#include "huira_impl/handles/scene/constellation_handle.ipp"
