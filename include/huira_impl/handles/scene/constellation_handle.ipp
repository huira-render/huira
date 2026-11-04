#include <cstddef>
#include <filesystem>
#include <vector>

#include "huira/concepts/spectral_concepts.hpp"

namespace huira {

/// Add satellites, from their element sets, or bring those it holds up to date (see
/// Constellation::add()).
template <IsSpectral TSpectral>
ConstellationReport
ConstellationHandle<TSpectral>::add(const std::vector<SatRec>& element_sets) const
{
    return this->get_()->add(element_sets);
}

/// Add the satellites of a catalog that was read, or bring those it holds up to date (see
/// Constellation::add()).
template <IsSpectral TSpectral>
ConstellationReport ConstellationHandle<TSpectral>::add(const ElementSetCatalog& catalog) const
{
    return this->get_()->add(catalog);
}

/// Read a catalog of element sets, and add its satellites, or bring those it holds up to date (see
/// Constellation::load()).
template <IsSpectral TSpectral>
ConstellationReport ConstellationHandle<TSpectral>::load(const std::filesystem::path& path,
                                                         const ElementSetOptions& options) const
{
    return this->get_()->load(path, options);
}

/// Read a catalog of element sets in the format given, and add its satellites, or bring those it
/// holds up to date (see Constellation::load()).
template <IsSpectral TSpectral>
ConstellationReport ConstellationHandle<TSpectral>::load(const std::filesystem::path& path,
                                                         ElementSetFormat format,
                                                         const ElementSetOptions& options) const
{
    return this->get_()->load(path, format, options);
}

/// @copydoc Constellation::remove()
template <IsSpectral TSpectral>
bool ConstellationHandle<TSpectral>::remove(int catalog_number) const
{
    return this->get_()->remove(catalog_number);
}

/// @copydoc Constellation::clear()
template <IsSpectral TSpectral>
void ConstellationHandle<TSpectral>::clear() const
{
    this->get_()->clear();
}

/// The number of satellites it holds.
template <IsSpectral TSpectral>
std::size_t ConstellationHandle<TSpectral>::size() const
{
    return this->get_()->size();
}

/// @copydoc Constellation::contains()
template <IsSpectral TSpectral>
bool ConstellationHandle<TSpectral>::contains(int catalog_number) const
{
    return this->get_()->contains(catalog_number);
}

/// @copydoc Constellation::element_set()
template <IsSpectral TSpectral>
SatRec ConstellationHandle<TSpectral>::element_set(int catalog_number) const
{
    return this->get_()->element_set(catalog_number);
}

/// @copydoc Constellation::element_sets()
template <IsSpectral TSpectral>
std::vector<SatRec> ConstellationHandle<TSpectral>::element_sets() const
{
    return this->get_()->element_sets();
}

/// @copydoc Constellation::set_unresolved_object()
template <IsSpectral TSpectral>
void ConstellationHandle<TSpectral>::set_unresolved_object(
    const UnresolvedObjectHandle<TSpectral>& unresolved_object) const
{
    this->get_()->set_unresolved_object(unresolved_object.get());
}

/// The unresolved object each satellite is seen as.
template <IsSpectral TSpectral>
UnresolvedObjectHandle<TSpectral> ConstellationHandle<TSpectral>::get_unresolved_object() const
{
    return UnresolvedObjectHandle<TSpectral>{this->get_()->unresolved_object()};
}

/// Show or hide its satellites: a scene view leaves out those of a hidden constellation.
template <IsSpectral TSpectral>
void ConstellationHandle<TSpectral>::set_visible(bool visible) const
{
    this->get_()->set_visible(visible);
}

/// Whether its satellites are shown.
template <IsSpectral TSpectral>
bool ConstellationHandle<TSpectral>::is_visible() const
{
    return this->get_()->is_visible();
}

} // namespace huira
