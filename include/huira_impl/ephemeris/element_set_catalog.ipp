#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "huira/util/logger.hpp"
#include "nlohmann/json.hpp"

namespace huira {
namespace detail {

/// Reads element sets with SatRec's factories that don't log their errors, for a catalog to skip
/// bad entries quietly and report them.
struct ElementSetReader {
    static SatRec
    tle(std::string_view name, std::string_view line1, std::string_view line2, Sgp4Mode mode)
    {
        return SatRec::from_lines_(std::string(tle::name_from_line(name)), line1, line2, mode);
    }

    static SatRec omm(const OmmFields& fields, Sgp4Mode mode)
    {
        return SatRec::from_omm_(fields, mode);
    }
};

namespace catalog {

/// One entry of a catalog, as its text is split into entries, before it is read as an element
/// set: a TLE's lines, or an OMM's fields, or why its text isn't an entry at all.
struct Entry {
    std::size_t line = 0; // Counted from 1, or 0 for JSON
    std::string name;
    std::string line1;
    std::string line2;
    std::optional<OmmFields> fields;
    std::string error;
};

/// A line of text, with the number it is, counted from 1.
struct Line {
    std::size_t number = 0;
    std::string_view text;
};

/// Text without the byte-order mark some programs start UTF-8 with, as one saving a CSV might.
inline std::string_view without_bom(std::string_view text)
{
    constexpr std::string_view bom = "\xEF\xBB\xBF";
    return text.starts_with(bom) ? text.substr(bom.size()) : text;
}

/// The text's lines, ended by "\n", "\r\n" or "\r".
inline std::vector<Line> lines_of(std::string_view text)
{
    std::vector<Line> lines;
    std::size_t start = 0;
    std::size_t number = 1;
    while (start <= text.size()) {
        const std::size_t end = text.find_first_of("\r\n", start);
        if (end == std::string_view::npos) {
            if (start < text.size()) {
                lines.push_back({number, text.substr(start)});
            }
            break;
        }
        lines.push_back({number++, text.substr(start, end - start)});
        start = end + (text[end] == '\r' && end + 1 < text.size() && text[end + 1] == '\n' ? 2 : 1);
    }
    return lines;
}

/// Whether a line is a TLE's line 1 or 2: the line number, then a space.
inline bool is_tle_line(std::string_view line, char number)
{
    line = tle::trim(line);
    return line.size() >= 2 && line[0] == number && line[1] == ' ';
}

/**
 * A file of TLEs, as CelesTrak and Space-Track write them: each a line 1 and a line 2, with a name
 * line before them or not. Blank lines, and lines that start with '#', are passed over.
 */
inline std::vector<Entry> split_tle(std::string_view text)
{
    const std::vector<Line> lines = lines_of(text);
    std::vector<Entry> entries;
    std::optional<Line> name;
    const auto name_without_tle = [&entries](const Line& line) {
        entries.push_back({line.number,
                           std::string(tle::trim(line.text)),
                           {},
                           {},
                           {},
                           "a name line has no TLE after it"});
    };

    for (std::size_t i = 0; i < lines.size(); ++i) {
        const Line& line = lines[i];
        const std::string_view trimmed = tle::trim(line.text);
        if (trimmed.empty() || trimmed.front() == '#') {
            continue;
        }
        if (is_tle_line(trimmed, '1')) {
            Entry entry;
            entry.line = name ? name->number : line.number;
            entry.name = name ? std::string(name->text) : std::string();
            name.reset();
            if (i + 1 < lines.size() && is_tle_line(lines[i + 1].text, '2')) {
                entry.line1 = std::string(line.text);
                entry.line2 = std::string(lines[i + 1].text);
                ++i;
            } else {
                entry.line1 = std::string(line.text);
                entry.error = "line " + std::to_string(line.number) +
                              " is a TLE's line 1, with no line 2 after it";
            }
            entries.push_back(std::move(entry));
            continue;
        }
        if (is_tle_line(trimmed, '2')) {
            if (name) {
                name_without_tle(*name);
                name.reset();
            }
            entries.push_back({line.number,
                               {},
                               {},
                               std::string(line.text),
                               {},
                               "line " + std::to_string(line.number) +
                                   " is a TLE's line 2, with no line 1 before it"});
            continue;
        }

        // A name line, for the TLE after it
        if (name) {
            name_without_tle(*name);
        }
        name = line;
    }
    if (name) {
        name_without_tle(*name);
    }
    return entries;
}

/**
 * A file of OMMs in KVN, the CCSDS keyword = value notation: each message from its CCSDS_OMM_VERS
 * line. Units in brackets after a value, as in "MEAN_MOTION = 15.5 [rev/day]", are dropped, and
 * COMMENT lines and blank lines passed over.
 */
inline std::vector<Entry> split_kvn(std::string_view text)
{
    std::vector<Entry> entries;
    for (const Line& line : lines_of(text)) {
        const std::string_view trimmed = tle::trim(line.text);
        if (trimmed.empty() || trimmed == "COMMENT" || trimmed.starts_with("COMMENT ") ||
            trimmed.starts_with("COMMENT\t")) {
            continue;
        }

        const std::size_t equals = trimmed.find('=');
        const std::string keyword(tle::trim(trimmed.substr(0, std::min(equals, trimmed.size()))));
        if (entries.empty() || keyword == "CCSDS_OMM_VERS") {
            entries.push_back({line.number, {}, {}, {}, OmmFields{}, {}});
        }
        Entry& entry = entries.back();
        if (!entry.error.empty()) {
            continue;
        }
        if (equals == std::string_view::npos || keyword.empty()) {
            entry.error = "line " + std::to_string(line.number) + " isn't KEYWORD = value";
            continue;
        }

        std::string_view value = tle::trim(trimmed.substr(equals + 1));
        const std::size_t bracket = value.rfind('[');
        if (!value.empty() && value.back() == ']' && bracket != std::string_view::npos) {
            value = tle::trim(value.substr(0, bracket));
        }
        if (!entry.fields->emplace(keyword, std::string(value)).second) {
            entry.error = "line " + std::to_string(line.number) + " gives " + keyword +
                          " again: is a CCSDS_OMM_VERS line missing before it?";
            continue;
        }
        if (keyword == "OBJECT_NAME") {
            entry.name = std::string(value);
        }
    }
    return entries;
}

/// A CSV row's fields, as RFC 4180 has them: separated by commas, and in double quotes if they
/// hold a comma or a quote, which is then doubled. Nothing, if a quote isn't closed.
inline std::optional<std::vector<std::string>> csv_fields(std::string_view row)
{
    std::vector<std::string> fields(1);
    bool quoted = false;
    for (std::size_t i = 0; i < row.size(); ++i) {
        const char c = row[i];
        if (quoted) {
            if (c == '"' && i + 1 < row.size() && row[i + 1] == '"') {
                fields.back() += '"';
                ++i;
            } else if (c == '"') {
                quoted = false;
            } else {
                fields.back() += c;
            }
        } else if (c == '"') {
            quoted = true;
        } else if (c == ',') {
            fields.emplace_back();
        } else {
            fields.back() += c;
        }
    }
    if (quoted) {
        return std::nullopt;
    }
    return fields;
}

/// A file of OMMs as CSV: a header row of keywords, then a row of values for each.
inline std::vector<Entry> split_csv(std::string_view text)
{
    std::vector<Entry> entries;
    std::optional<std::vector<std::string>> header;
    std::size_t header_line = 0;
    for (const Line& line : lines_of(text)) {
        if (tle::trim(line.text).empty()) {
            continue;
        }
        const std::optional<std::vector<std::string>> fields = csv_fields(line.text);
        if (!header) {
            if (!fields) {
                HUIRA_THROW_ERROR("ElementSetCatalog - The CSV header, on line " +
                                  std::to_string(line.number) + ", has a quote that isn't closed");
            }
            header = *fields;
            header_line = line.number;
            for (std::string& keyword : *header) {
                keyword = std::string(tle::trim(keyword));
            }
            continue;
        }

        Entry entry;
        entry.line = line.number;
        if (!fields) {
            entry.error = "line " + std::to_string(line.number) + " has a quote that isn't closed";
        } else if (fields->size() != header->size()) {
            entry.error = "line " + std::to_string(line.number) + " has " +
                          std::to_string(fields->size()) + " fields, where the header, on line " +
                          std::to_string(header_line) + ", has " + std::to_string(header->size());
        } else {
            entry.fields = OmmFields{};
            for (std::size_t i = 0; i < fields->size(); ++i) {
                entry.fields->emplace((*header)[i], (*fields)[i]);
            }
            const auto name = entry.fields->find("OBJECT_NAME");
            if (name != entry.fields->end()) {
                entry.name = std::string(tle::trim(name->second));
            }
        }
        entries.push_back(std::move(entry));
    }
    return entries;
}

/// A file of OMMs as JSON: an array of objects, each of an OMM's keywords and their values, or one
/// such object. A value may be text or a number; one that is null is left out.
inline std::vector<Entry> split_json(std::string_view text)
{
    nlohmann::json document;
    try {
        document = nlohmann::json::parse(text.begin(), text.end());
    } catch (const nlohmann::json::parse_error& error) {
        HUIRA_THROW_ERROR(std::string("ElementSetCatalog - The JSON doesn't parse: ") +
                          error.what());
    }
    if (document.is_object()) {
        document = nlohmann::json::array({std::move(document)});
    }
    if (!document.is_array()) {
        HUIRA_THROW_ERROR("ElementSetCatalog - The JSON is neither an array of OMMs nor one OMM");
    }

    std::vector<Entry> entries;
    for (const nlohmann::json& element : document) {
        Entry entry;
        if (!element.is_object()) {
            entry.error = "it isn't an object of keywords and values";
            entries.push_back(std::move(entry));
            continue;
        }
        entry.fields = OmmFields{};
        for (const auto& [keyword, value] : element.items()) {
            if (value.is_null()) {
                continue;
            }
            if (value.is_string()) {
                entry.fields->emplace(keyword, value.get<std::string>());
            } else if (value.is_number() || value.is_boolean()) {
                // A number's shortest text that reads back as the same number
                entry.fields->emplace(keyword, value.dump());
            } else {
                entry.error = keyword + "'s value is an array or object, not text or a number";
                break;
            }
        }
        const auto name = entry.fields->find("OBJECT_NAME");
        if (name != entry.fields->end()) {
            entry.name = std::string(tle::trim(name->second));
        }
        entries.push_back(std::move(entry));
    }
    return entries;
}

/// An entry's satellite's catalog number, read without reading the rest of it, to skip the
/// satellites left out without reading them; or 0 if it can't be.
inline int catalog_number_of(const Entry& entry)
{
    if (entry.fields) {
        const auto found = entry.fields->find("NORAD_CAT_ID");
        return found == entry.fields->end() ? 0 : omm::whole_number(found->second).value_or(0);
    }
    const std::string_view line1 = tle::trim(entry.line1);
    if (line1.size() >= 7) {
        return std::max(tle::decode_catalog_number(line1.substr(2, 5)), 0);
    }
    return 0;
}

/// A file's text, whole.
inline std::string read_text(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        HUIRA_THROW_ERROR("ElementSetCatalog::read - Can't open " + path.string());
    }
    HUIRA_LOG_INFO("ElementSetCatalog::read - " + path.string());
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

} // namespace catalog
} // namespace detail

/**
 * @brief Reads a catalog of element sets from a file, in the format its text is in (see
 * detect_format()).
 *
 * @param path The file.
 * @param options Which element sets to keep, and how.
 * @return The element sets kept, and the entries left out.
 * @throws std::runtime_error if the file can't be read, its text can't be split into entries, or
 *         options.strict is set and an entry isn't an element set SGP4 can propagate.
 */
inline ElementSetCatalog ElementSetCatalog::read(const std::filesystem::path& path,
                                                 const ElementSetOptions& options)
{
    const std::string text = detail::catalog::read_text(path);
    return parse(text, detect_format(text), options);
}

/**
 * @brief Reads a catalog of element sets from a file in the format given.
 *
 * @param path The file.
 * @param format The format of its text.
 * @param options Which element sets to keep, and how.
 * @return The element sets kept, and the entries left out.
 * @throws std::runtime_error as the other read() does.
 */
inline ElementSetCatalog ElementSetCatalog::read(const std::filesystem::path& path,
                                                 ElementSetFormat format,
                                                 const ElementSetOptions& options)
{
    return parse(detail::catalog::read_text(path), format, options);
}

/**
 * @brief The format a catalog's text is in, from how it starts.
 *
 * JSON starts with '[' or '{'. KVN starts with a CCSDS_OMM_VERS or COMMENT line, or another
 * KEYWORD = value line. CSV starts with a header row of keywords, with NORAD_CAT_ID among them.
 * Anything else is taken to be TLEs.
 *
 * @param text The catalog's text.
 * @return Its format.
 */
inline ElementSetFormat ElementSetCatalog::detect_format(std::string_view text)
{
    namespace catalog = detail::catalog;
    text = catalog::without_bom(text);
    for (const catalog::Line& line : catalog::lines_of(text)) {
        const std::string_view trimmed = detail::tle::trim(line.text);
        if (trimmed.empty()) {
            continue;
        }
        if (trimmed.front() == '[' || trimmed.front() == '{') {
            return ElementSetFormat::OMM_JSON;
        }
        if (trimmed.starts_with("CCSDS_OMM_VERS") || trimmed.starts_with("COMMENT")) {
            return ElementSetFormat::OMM_KVN;
        }
        if (trimmed.find(',') != std::string_view::npos &&
            trimmed.find("NORAD_CAT_ID") != std::string_view::npos) {
            return ElementSetFormat::OMM_CSV;
        }
        const std::size_t equals = trimmed.find('=');
        if (equals != std::string_view::npos && equals > 0) {
            const std::string_view keyword = detail::tle::trim(trimmed.substr(0, equals));
            if (!keyword.empty() &&
                keyword.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") ==
                    std::string_view::npos) {
                return ElementSetFormat::OMM_KVN;
            }
        }
        return ElementSetFormat::TLE;
    }
    return ElementSetFormat::TLE;
}

/**
 * @brief Reads a catalog of element sets from its text.
 *
 * @param text The catalog's text.
 * @param format The format it is in.
 * @param options Which element sets to keep, and how.
 * @return The element sets kept, and the entries left out.
 * @throws std::runtime_error if the text can't be split into entries (JSON that doesn't parse, or
 *         CSV whose header has a quote that isn't closed), if options.max_age is given without a
 *         reference_time or isn't positive, or if options.strict is set and an entry isn't an
 *         element set SGP4 can propagate.
 */
inline ElementSetCatalog ElementSetCatalog::parse(std::string_view text,
                                                  ElementSetFormat format,
                                                  const ElementSetOptions& options)
{
    namespace catalog = detail::catalog;

    if (options.max_age && !options.reference_time) {
        HUIRA_THROW_ERROR("ElementSetCatalog::parse - max_age is measured from reference_time, "
                          "which isn't given");
    }
    if (options.max_age && !(options.max_age->to_si() > 0.0)) {
        HUIRA_THROW_ERROR("ElementSetCatalog::parse - max_age must be positive: " +
                          std::to_string(options.max_age->value()) + " days");
    }

    text = catalog::without_bom(text);
    std::vector<catalog::Entry> entries;
    switch (format) {
    case ElementSetFormat::TLE:
        entries = catalog::split_tle(text);
        break;
    case ElementSetFormat::OMM_KVN:
        entries = catalog::split_kvn(text);
        break;
    case ElementSetFormat::OMM_CSV:
        entries = catalog::split_csv(text);
        break;
    case ElementSetFormat::OMM_JSON:
        entries = catalog::split_json(text);
        break;
    default:
        HUIRA_THROW_ERROR("ElementSetCatalog::parse - Not a format: " +
                          std::to_string(static_cast<int>(format)));
    }

    const std::unordered_set<int> include(options.include.begin(), options.include.end());
    const std::unordered_set<int> exclude(options.exclude.begin(), options.exclude.end());

    ElementSetCatalog result;
    // Where each element set kept came from, and which satellite's is where
    struct Kept {
        std::size_t entry = 0;
        std::size_t line = 0;
    };
    std::vector<Kept> kept;
    std::unordered_map<int, std::size_t> index_of;

    std::size_t invalid = 0;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const catalog::Entry& entry = entries[i];
        SkippedElementSet skip{
            i, entry.line, catalog::catalog_number_of(entry), entry.name, {}, {}};
        const auto skip_for = [&](SkipReason reason, std::string message) {
            skip.reason = reason;
            skip.message = std::move(message);
            result.skipped.push_back(skip);
        };

        // Satellites left out by number aren't read: an error in one isn't one the reader asked
        // about
        if (skip.catalog_number != 0 && !include.empty() &&
            !include.contains(skip.catalog_number)) {
            skip_for(SkipReason::NOT_INCLUDED, "its satellite isn't one of those to include");
            continue;
        }
        if (skip.catalog_number != 0 && exclude.contains(skip.catalog_number)) {
            skip_for(SkipReason::EXCLUDED, "its satellite is one of those to exclude");
            continue;
        }

        std::optional<SatRec> satrec;
        std::string error = entry.error;
        if (error.empty()) {
            try {
                satrec = entry.fields ? detail::ElementSetReader::omm(*entry.fields, options.mode)
                                      : detail::ElementSetReader::tle(
                                            entry.name, entry.line1, entry.line2, options.mode);
            } catch (const detail::tle::InvalidElementSet& invalid_element_set) {
                error = invalid_element_set.what();
            }
        }
        if (!satrec) {
            const std::string where =
                "Entry " + std::to_string(i) +
                (entry.line != 0 ? ", on line " + std::to_string(entry.line) : std::string()) +
                ": ";
            if (options.strict) {
                HUIRA_THROW_ERROR("ElementSetCatalog::parse - " + where + error);
            }
            ++invalid;
            skip_for(SkipReason::INVALID, error);
            continue;
        }
        skip.catalog_number = satrec->catalog_number();
        skip.name = satrec->name();

        // The checks by number again, for an entry whose number couldn't be read without reading it
        if (!include.empty() && !include.contains(skip.catalog_number)) {
            skip_for(SkipReason::NOT_INCLUDED, "its satellite isn't one of those to include");
            continue;
        }
        if (exclude.contains(skip.catalog_number)) {
            skip_for(SkipReason::EXCLUDED, "its satellite is one of those to exclude");
            continue;
        }

        const auto distance = [&options](const SatRec& element_set) {
            return std::abs(element_set.epoch().et() - options.reference_time->et());
        };
        if (options.max_age && distance(*satrec) > options.max_age->to_si()) {
            std::ostringstream days;
            days.precision(1);
            days << std::fixed << distance(*satrec) / 86400.0;
            skip_for(SkipReason::TOO_OLD,
                     "its epoch, " + satrec->epoch().to_utc_string() + ", is " + days.str() +
                         " days from the reference time, more than the maximum age");
            continue;
        }

        // A satellite already kept: keep the nearer to the reference time, or the newer
        const auto found = index_of.find(skip.catalog_number);
        if (found == index_of.end()) {
            index_of.emplace(skip.catalog_number, result.element_sets.size());
            result.element_sets.push_back(std::move(*satrec));
            kept.push_back({i, entry.line});
            continue;
        }
        SatRec& other = result.element_sets[found->second];
        const bool replaces = options.reference_time ? distance(*satrec) < distance(other)
                                                     : satrec->epoch() > other.epoch();
        const auto superseded_by = [](const SatRec& winner, const Kept& where) {
            return "another element set for its satellite, entry " + std::to_string(where.entry) +
                   (where.line != 0 ? " on line " + std::to_string(where.line) : std::string()) +
                   ", with its epoch at " + winner.epoch().to_utc_string() + ", was kept";
        };
        if (!replaces) {
            skip_for(SkipReason::SUPERSEDED, superseded_by(other, kept[found->second]));
            continue;
        }
        SkippedElementSet replaced{kept[found->second].entry,
                                   kept[found->second].line,
                                   other.catalog_number(),
                                   other.name(),
                                   SkipReason::SUPERSEDED,
                                   superseded_by(*satrec, {i, entry.line})};
        result.skipped.push_back(std::move(replaced));
        other = std::move(*satrec);
        kept[found->second] = {i, entry.line};
    }

    // The skipped in the order of the catalog, as one superseded later is reported late
    std::stable_sort(
        result.skipped.begin(),
        result.skipped.end(),
        [](const SkippedElementSet& a, const SkippedElementSet& b) { return a.entry < b.entry; });

    const std::string summary = "ElementSetCatalog - Kept " +
                                std::to_string(result.element_sets.size()) + " of " +
                                std::to_string(entries.size()) + " entries";
    if (invalid > 0) {
        HUIRA_LOG_WARNING(summary + "; " + std::to_string(invalid) +
                          " weren't element sets SGP4 can propagate, and were skipped (see the "
                          "catalog's skipped entries)");
    } else {
        HUIRA_LOG_INFO(summary);
    }
    return result;
}

} // namespace huira
