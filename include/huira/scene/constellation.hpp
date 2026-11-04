#pragma once

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "huira/assets/unresolved/unresolved_object.hpp"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/ephemeris/element_set_catalog.hpp"
#include "huira/ephemeris/satrec.hpp"
#include "huira/scene/scene_object.hpp"
#include "huira/scene/state_callbacks/tle_position.hpp"

namespace huira {
// Forward declare:
template <IsSpectral TSpectral>
class Scene;

template <IsSpectral TSpectral>
class SceneView;

/**
 * @brief What adding element sets to a constellation did.
 */
struct ConstellationReport {
    /// The satellites added to it.
    std::size_t added = 0;

    /// The satellites it already held, whose element sets were replaced.
    std::size_t replaced = 0;

    /// The element sets left out, and why, in the order they were given: those a catalog's read
    /// left out (see ElementSetCatalog), and those for satellites something else in the scene
    /// places already (SkipReason::CLAIMED). An element set given to add() as one of a list is
    /// identified by its place in the list, as its entry, with no line.
    std::vector<SkippedElementSet> skipped;
};

/**
 * @brief Satellites, many of them, each placed by SGP4 from its element set and seen as a point.
 *
 * A constellation is a scene's way of rendering a population of satellites, such as a catalog of
 * every object in Earth orbit from CelesTrak or Space-Track, without a node for each. Each
 * satellite is placed as a node in the scene's TEME frame would be by Node::set_tle_position(), and
 * seen as an instance of one unresolved object, the same for all: a Lambertian sphere lit by the
 * Sun, say, or a fixed brightness. A scene view places them all at once, in parallel, at as many
 * temporal samples as each one's motion needs (see SceneView).
 *
 * A constellation holds one element set for each satellite, as the scene does: a satellite is
 * placed once, by a node or as one of a constellation's. add() and load() leave out, and report,
 * satellites that something else in the scene places already, and replace the element sets of
 * those the constellation holds itself, so that loading a newer catalog brings it up to date. A
 * satellite remove()d, or a constellation deleted, can then be placed by something else.
 *
 * Its satellites are placed by SPICE's ephemeris of the Earth, as the TEME frame is: rendering
 * them needs an SPK with it, such as de440s.bsp. A satellite SGP4 can't place during an exposure
 * (one that has decayed, most often) is left out of that scene view, with a warning, rather than
 * stopping it (see SceneView::skipped_satellites()).
 *
 * Make one with Scene::new_constellation(), and use it through a ConstellationHandle.
 *
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 */
template <IsSpectral TSpectral>
class Constellation final : public SceneObject<Constellation<TSpectral>> {
  public:
    Constellation(Scene<TSpectral>* scene,
                  std::shared_ptr<UnresolvedObject<TSpectral>> unresolved_object);

    Constellation(const Constellation&) = delete;
    Constellation& operator=(const Constellation&) = delete;

    ConstellationReport add(const std::vector<SatRec>& element_sets);
    ConstellationReport add(const ElementSetCatalog& catalog);

    ConstellationReport load(const std::filesystem::path& path,
                             const ElementSetOptions& options = {});
    ConstellationReport load(const std::filesystem::path& path,
                             ElementSetFormat format,
                             const ElementSetOptions& options = {});

    bool remove(int catalog_number);
    void clear();

    /// The number of satellites it holds.
    [[nodiscard]] std::size_t size() const { return entries_.size(); }

    [[nodiscard]] bool contains(int catalog_number) const;
    [[nodiscard]] const SatRec& element_set(int catalog_number) const;
    [[nodiscard]] std::vector<SatRec> element_sets() const;

    void set_unresolved_object(std::shared_ptr<UnresolvedObject<TSpectral>> unresolved_object);

    /// The unresolved object each satellite is seen as.
    [[nodiscard]] const std::shared_ptr<UnresolvedObject<TSpectral>>& unresolved_object() const
    {
        return unresolved_object_;
    }

    /// Show or hide its satellites: a scene view leaves out those of a hidden constellation.
    void set_visible(bool visible) { visible_ = visible; }

    /// Whether its satellites are shown.
    [[nodiscard]] bool is_visible() const { return visible_; }

    std::string type() const override { return "Constellation"; }

  private:
    ConstellationReport add_(const std::vector<SatRec>& element_sets,
                             const std::vector<ElementSetSource>* sources);

    Scene<TSpectral>* scene_;
    std::shared_ptr<UnresolvedObject<TSpectral>> unresolved_object_;
    bool visible_ = true;

    /// The element sets it holds, one for each satellite, which keep its claims on them (see
    /// TleManager), and where each satellite's is.
    std::vector<std::shared_ptr<TleEntry>> entries_;
    std::unordered_map<int, std::size_t> index_of_;

    friend class Scene<TSpectral>;
    friend class SceneView<TSpectral>;
};

} // namespace huira

#include "huira_impl/scene/constellation.ipp"
