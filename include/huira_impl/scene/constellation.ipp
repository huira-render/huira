#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "huira/concepts/spectral_concepts.hpp"
#include "huira/util/logger.hpp"

namespace huira {

/**
 * @brief Construct an empty constellation, whose satellites are seen as an unresolved object.
 *
 * Scene::new_constellation() makes it, and loads Huira's frame kernel, which defines the TEME
 * frame's axes, as Scene::teme_frame() does.
 *
 * @param scene The scene it is in.
 * @param unresolved_object What each satellite is seen as.
 * @throws std::runtime_error if there is no unresolved object.
 */
template <IsSpectral TSpectral>
Constellation<TSpectral>::Constellation(
    Scene<TSpectral>* scene, std::shared_ptr<UnresolvedObject<TSpectral>> unresolved_object)
    : scene_{scene}
{
    set_unresolved_object(std::move(unresolved_object));
}

/**
 * @brief Add satellites, from their element sets, or bring those it holds up to date.
 *
 * Each satellite new to the constellation is added, unless a node or another constellation places
 * it already (it is then left out, and reported as SkipReason::CLAIMED). A satellite it holds has
 * its element set replaced. They are taken in the order given: a later element set for the same
 * satellite replaces an earlier one.
 *
 * @param element_sets The satellites' element sets (see SatRec, and ElementSetCatalog for reading
 *        many).
 * @return The satellites added, those replaced, and those left out.
 */
template <IsSpectral TSpectral>
ConstellationReport Constellation<TSpectral>::add(const std::vector<SatRec>& element_sets)
{
    return add_(element_sets, nullptr);
}

/**
 * @brief Add the satellites of a catalog that was read, or bring those it holds up to date.
 *
 * As add() with its element sets, with the entries the catalog's read left out reported too, and
 * each element set left out identified by where it is in the catalog.
 *
 * @param catalog The catalog read (see ElementSetCatalog::read()).
 * @return The satellites added, those replaced, and those left out, by the read or here.
 */
template <IsSpectral TSpectral>
ConstellationReport Constellation<TSpectral>::add(const ElementSetCatalog& catalog)
{
    ConstellationReport report = add_(catalog.element_sets, &catalog.sources);
    report.skipped.insert(report.skipped.end(), catalog.skipped.begin(), catalog.skipped.end());
    std::stable_sort(
        report.skipped.begin(),
        report.skipped.end(),
        [](const SkippedElementSet& a, const SkippedElementSet& b) { return a.entry < b.entry; });
    return report;
}

/**
 * @brief Read a catalog of element sets from a file, and add its satellites, or bring those it
 * holds up to date.
 *
 * @param path The file, in the format its text is in (see ElementSetCatalog::detect_format()).
 * @param options Which of its element sets to read, and how (see ElementSetOptions).
 * @return The satellites added, those replaced, and those left out, by the read or here.
 * @throws std::runtime_error as ElementSetCatalog::read() does.
 */
template <IsSpectral TSpectral>
ConstellationReport Constellation<TSpectral>::load(const std::filesystem::path& path,
                                                   const ElementSetOptions& options)
{
    return add(ElementSetCatalog::read(path, options));
}

/**
 * @brief Read a catalog of element sets from a file in the format given, and add its satellites,
 * or bring those it holds up to date.
 *
 * @param path The file.
 * @param format The format of its text.
 * @param options Which of its element sets to read, and how (see ElementSetOptions).
 * @return The satellites added, those replaced, and those left out, by the read or here.
 * @throws std::runtime_error as ElementSetCatalog::read() does.
 */
template <IsSpectral TSpectral>
ConstellationReport Constellation<TSpectral>::load(const std::filesystem::path& path,
                                                   ElementSetFormat format,
                                                   const ElementSetOptions& options)
{
    return add(ElementSetCatalog::read(path, format, options));
}

template <IsSpectral TSpectral>
ConstellationReport Constellation<TSpectral>::add_(const std::vector<SatRec>& element_sets,
                                                   const std::vector<ElementSetSource>* sources)
{
    ConstellationReport report;
    for (std::size_t i = 0; i < element_sets.size(); ++i) {
        const SatRec& satrec = element_sets[i];

        const auto found = index_of_.find(satrec.catalog_number());
        if (found != index_of_.end()) {
            entries_[found->second]->satrec = satrec;
            ++report.replaced;
            continue;
        }

        std::string holder;
        std::shared_ptr<TleEntry> entry = scene_->tle_manager_.claim(satrec, *this, holder);
        if (!entry) {
            const bool sourced = sources != nullptr && i < sources->size();
            report.skipped.push_back(
                {sourced ? (*sources)[i].entry : i,
                 sourced ? (*sources)[i].line : 0,
                 satrec.catalog_number(),
                 satrec.name(),
                 SkipReason::CLAIMED,
                 "its satellite is placed by " + holder + " already: a satellite is placed once"});
            continue;
        }
        index_of_.emplace(satrec.catalog_number(), entries_.size());
        entries_.push_back(std::move(entry));
        ++report.added;
    }

    HUIRA_LOG_INFO(this->get_info() + " - Added " + std::to_string(report.added) +
                   " satellites and replaced " + std::to_string(report.replaced) +
                   " element sets, of " + std::to_string(element_sets.size()) + "; it holds " +
                   std::to_string(entries_.size()));
    if (!report.skipped.empty()) {
        HUIRA_LOG_WARNING(this->get_info() + " - Left out " +
                          std::to_string(report.skipped.size()) +
                          " satellites that something else in the scene places already (see the "
                          "report's skipped)");
    }
    return report;
}

/**
 * @brief Remove a satellite, which something else in the scene can then place.
 *
 * @param catalog_number The satellite's NORAD catalog number.
 * @return Whether it held the satellite.
 */
template <IsSpectral TSpectral>
bool Constellation<TSpectral>::remove(int catalog_number)
{
    const auto found = index_of_.find(catalog_number);
    if (found == index_of_.end()) {
        return false;
    }

    // The last satellite takes its place
    const std::size_t index = found->second;
    index_of_.erase(found);
    if (index + 1 != entries_.size()) {
        entries_[index] = std::move(entries_.back());
        index_of_[entries_[index]->satrec.catalog_number()] = index;
    }
    entries_.pop_back();
    return true;
}

/**
 * @brief Remove every satellite.
 */
template <IsSpectral TSpectral>
void Constellation<TSpectral>::clear()
{
    entries_.clear();
    index_of_.clear();
}

/**
 * @brief Whether it holds a satellite.
 * @param catalog_number The satellite's NORAD catalog number.
 */
template <IsSpectral TSpectral>
bool Constellation<TSpectral>::contains(int catalog_number) const
{
    return index_of_.contains(catalog_number);
}

/**
 * @brief The element set it holds for a satellite.
 *
 * @param catalog_number The satellite's NORAD catalog number.
 * @return Its element set.
 * @throws std::runtime_error if it doesn't hold the satellite.
 */
template <IsSpectral TSpectral>
const SatRec& Constellation<TSpectral>::element_set(int catalog_number) const
{
    const auto found = index_of_.find(catalog_number);
    if (found == index_of_.end()) {
        HUIRA_THROW_ERROR(this->get_info() + " - doesn't hold satellite " +
                          std::to_string(catalog_number));
    }
    return entries_[found->second]->satrec;
}

/**
 * @brief The element sets it holds, one for each satellite, in no particular order.
 */
template <IsSpectral TSpectral>
std::vector<SatRec> Constellation<TSpectral>::element_sets() const
{
    std::vector<SatRec> element_sets;
    element_sets.reserve(entries_.size());
    for (const std::shared_ptr<TleEntry>& entry : entries_) {
        element_sets.push_back(entry->satrec);
    }
    return element_sets;
}

/**
 * @brief Set the unresolved object each satellite is seen as.
 *
 * Each satellite is an instance of it, resolved for where that satellite is: a Lambertian sphere
 * lit by the Sun is bright or faint by its phase, and dark in the Earth's shadow, if the Earth is
 * in the scene to cast one. A scene view resolves the satellites in parallel, so the object must
 * be safe to resolve from several threads at once, as Huira's own are.
 *
 * @param unresolved_object What each satellite is seen as.
 * @throws std::runtime_error if there is no unresolved object.
 */
template <IsSpectral TSpectral>
void Constellation<TSpectral>::set_unresolved_object(
    std::shared_ptr<UnresolvedObject<TSpectral>> unresolved_object)
{
    if (!unresolved_object) {
        HUIRA_THROW_ERROR(this->get_info() + " - needs an unresolved object to see its satellites "
                                             "as");
    }
    unresolved_object_ = std::move(unresolved_object);
}

} // namespace huira
