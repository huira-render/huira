#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "huira/core/time.hpp"
#include "huira/ephemeris/satrec.hpp"
#include "huira/units/units.hpp"

namespace huira {

/**
 * @brief The formats a catalog of element sets is read from, as CelesTrak and Space-Track publish
 * them.
 */
enum class ElementSetFormat {
    TLE,      ///< Two-line element sets, each with a name line before it or not (2LE or 3LE).
    OMM_KVN,  ///< OMMs in the CCSDS keyword = value notation, each from its CCSDS_OMM_VERS line.
    OMM_CSV,  ///< OMMs as CSV: a header row of keywords, then a row for each.
    OMM_JSON, ///< OMMs as JSON: an array of objects of keywords and values, or one object.
};

/**
 * @brief Why an entry of a catalog was left out of it.
 */
enum class SkipReason {
    INVALID,      ///< It isn't an element set SGP4 can propagate, or isn't one at all.
    NOT_INCLUDED, ///< Its satellite isn't one of ElementSetOptions::include.
    EXCLUDED,     ///< Its satellite is one of ElementSetOptions::exclude.
    TOO_OLD,      ///< Its epoch is further than ElementSetOptions::max_age from the reference time.
    SUPERSEDED,   ///< Another element set for its satellite was kept (see ElementSetCatalog).
};

/**
 * @brief Which of a catalog's element sets to keep, and how to read them.
 *
 * Its fields can be set by name: `{.include = {25544}, .max_age = units::Day(14)}`.
 */
struct ElementSetOptions {
    /// The mode to propagate them in. Leave it AFSPC unless you have a reason not to: see
    /// Sgp4Mode.
    Sgp4Mode mode = Sgp4Mode::AFSPC;

    /// Keep only the satellites with these catalog numbers, or every satellite if it is empty.
    std::vector<int> include{};

    /// Leave out the satellites with these catalog numbers.
    std::vector<int> exclude{};

    /// When the element sets are to be used. It decides which of a satellite's element sets is
    /// kept (see ElementSetCatalog), and is what max_age is measured from.
    std::optional<Time> reference_time{};

    /// Leave out element sets whose epoch is further than this from reference_time, before or
    /// after it. It needs a reference_time.
    std::optional<units::Day> max_age{};

    /// Throw on the first entry that isn't an element set SGP4 can propagate, rather than skip
    /// it.
    bool strict = false;
};

/**
 * @brief An entry of a catalog that was left out of it, and why.
 */
struct SkippedElementSet {
    /// Which entry of the catalog it was, counted from 0.
    std::size_t entry = 0;

    /// The line its entry starts on, counted from 1, or 0 in a JSON catalog, which has no lines
    /// to speak of.
    std::size_t line = 0;

    /// Its satellite's NORAD catalog number, or 0 if it couldn't be read.
    int catalog_number = 0;

    /// Its satellite's name, or empty if it has none.
    std::string name;

    SkipReason reason = SkipReason::INVALID;

    /// Why, in words.
    std::string message;
};

/**
 * @brief Satellites' element sets read from a catalog of TLEs or OMMs, with a report of the
 * entries left out of it and why.
 *
 * A catalog is read in one of the formats CelesTrak and Space-Track publish (ElementSetFormat):
 * TLEs, two lines each or three with a name line, or OMMs in KVN, CSV or JSON. read() works out
 * which from the text, and parse() is told. Each entry is checked as SatRec::from_tle() or
 * SatRec::from_omm() checks it.
 *
 * An entry that isn't an element set SGP4 can propagate is skipped, and reported in `skipped` with
 * the reason, rather than stopping the read: one bad entry in a catalog of thousands shouldn't
 * cost the rest. ElementSetOptions::strict makes it throw instead. An error that leaves nothing
 * to read, such as a file that can't be opened or JSON that doesn't parse, always throws.
 *
 * A satellite is kept once. Of its element sets, the one whose epoch is nearest
 * ElementSetOptions::reference_time is kept, or with no reference time, the newest; of two as
 * near, or as new, the first. The others are reported as SUPERSEDED. The element sets kept are in
 * the order their satellites first appear in the catalog.
 *
 * ## Example
 * @code
 * // Today's active satellites, from CelesTrak, leaving out anything a fortnight old or older
 * const Time now("2026-10-26T12:00:00");
 * const ElementSetCatalog catalog = ElementSetCatalog::read(
 *     "active.json", {.reference_time = now, .max_age = units::Day(14)});
 * for (const SkippedElementSet& skipped : catalog.skipped) {
 *     std::cout << skipped.catalog_number << ": " << skipped.message << "\n";
 * }
 * @endcode
 */
struct ElementSetCatalog {
    /// The element sets read and kept.
    std::vector<SatRec> element_sets;

    /// The entries left out, and why, in the order of the catalog.
    std::vector<SkippedElementSet> skipped;

    static ElementSetCatalog read(const std::filesystem::path& path,
                                  const ElementSetOptions& options = {});

    static ElementSetCatalog read(const std::filesystem::path& path,
                                  ElementSetFormat format,
                                  const ElementSetOptions& options = {});

    static ElementSetCatalog
    parse(std::string_view text, ElementSetFormat format, const ElementSetOptions& options = {});

    static ElementSetFormat detect_format(std::string_view text);
};

} // namespace huira

#include "huira_impl/ephemeris/element_set_catalog.ipp"
