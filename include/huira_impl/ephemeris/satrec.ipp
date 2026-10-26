#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ios>
#include <locale>
#include <numbers>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>

#include "huira/core/spice.hpp"
#include "huira/util/logger.hpp"

namespace huira {
namespace detail::tle {

static_assert(std::is_trivially_copyable_v<vallado::elsetrec>,
              "SatRec keeps SGP4's elsetrec as its bytes");

/// The columns in a TLE line, the last of them its checksum.
inline constexpr std::size_t LINE_LENGTH = 69;

// What each column of a TLE line holds, from the format's definition. A line number, '.' or ' '
// stands for itself, and:
//   d  a digit
//   z  a digit or a space, which SGP4 reads as 0
//   N  part of a right-aligned whole number: spaces, then digits
//   s  a sign: '+', '-' or a space
//   K  the classification: a letter or a space
//   A  part of the international designator: a letter, a digit or a space
//   C  part of the catalog number, which decode_catalog_number() checks
//   t  the ephemeris type: a digit or a space
//   x  the checksum: a digit
inline constexpr std::string_view LINE1_COLUMNS =
    "1 CCCCCK AAAAAAAA ddddd.dddddddd s.dddddddd szzzzzsz sdddddsd t NNNNx";
inline constexpr std::string_view LINE2_COLUMNS =
    "2 CCCCC NNN.dddd NNN.dddd ddddddd NNN.dddd NNN.dddd NN.ddddddddNNNNNx";
static_assert(LINE1_COLUMNS.size() == LINE_LENGTH && LINE2_COLUMNS.size() == LINE_LENGTH);

inline bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

inline bool is_letter(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

inline std::string_view trim(std::string_view text)
{
    constexpr std::string_view whitespace = " \t\r\n\v\f";
    const std::size_t first = text.find_first_not_of(whitespace);
    if (first == std::string_view::npos) {
        return {};
    }
    return text.substr(first, text.find_last_not_of(whitespace) - first + 1);
}

/// What a column of the kind given (see LINE1_COLUMNS) should hold, for an error message.
inline std::string describe_column(char kind)
{
    switch (kind) {
    case 'd':
        return "a digit";
    case 'z':
    case 't':
        return "a digit or a space";
    case 'N':
        return "a digit, or a space before the number's first digit";
    case 's':
        return "'+', '-' or a space";
    case 'K':
        return "a letter or a space";
    case 'A':
        return "a letter, a digit or a space";
    case 'x':
        return "a digit, the line's checksum";
    case ' ':
        return "a space";
    default:
        return std::string("'") + kind + "'";
    }
}

/// A line's checksum: the sum of its digits, before the checksum, with each '-' counted as 1,
/// modulo 10.
inline int checksum(std::string_view line)
{
    int sum = 0;
    for (std::size_t i = 0; i + 1 < LINE_LENGTH; ++i) {
        if (is_digit(line[i])) {
            sum += line[i] - '0';
        } else if (line[i] == '-') {
            sum += 1;
        }
    }
    return sum % 10;
}

/// Throws unless the line has a TLE line's length, the layout its columns give, and a checksum
/// that matches it. `context` starts the message.
inline void
check_line(std::string_view line, std::string_view columns, int number, const std::string& context)
{
    const std::string which = "Line " + std::to_string(number);
    const std::string quoted = ": \"" + std::string(line) + "\"";
    if (line.size() != LINE_LENGTH) {
        HUIRA_THROW_ERROR(context + which + " has " + std::to_string(line.size()) +
                          " characters, where a TLE line has 69" + quoted);
    }
    for (std::size_t i = 0; i < LINE_LENGTH; ++i) {
        const char c = line[i];
        const char kind = columns[i];
        bool ok = true;
        switch (kind) {
        case 'd':
        case 'x':
            ok = is_digit(c);
            break;
        case 'z':
        case 't':
            ok = is_digit(c) || c == ' ';
            break;
        case 'N':
            // A space only before the first digit: the column before is one of the number's
            // spaces, or not part of the number
            ok = is_digit(c) || (c == ' ' && (columns[i - 1] != 'N' || line[i - 1] == ' '));
            break;
        case 's':
            ok = c == ' ' || c == '+' || c == '-';
            break;
        case 'K':
            ok = is_letter(c) || c == ' ';
            break;
        case 'A':
            ok = is_letter(c) || is_digit(c) || c == ' ';
            break;
        case 'C':
            break;
        default:
            ok = c == kind;
            break;
        }
        if (!ok) {
            HUIRA_THROW_ERROR(context + which + ", column " + std::to_string(i + 1) +
                              ", should be " + describe_column(kind) + ", not '" + c + "'" +
                              quoted);
        }
    }
    const int expected = checksum(line);
    if (line[LINE_LENGTH - 1] - '0' != expected) {
        HUIRA_THROW_ERROR(context + which + "'s checksum is " + line[LINE_LENGTH - 1] +
                          ", but its other columns give " + std::to_string(expected) +
                          ": a column is wrong, or the line is incomplete" + quoted);
    }
}

/// The catalog number in a line's five catalog columns, or -1 if they don't hold one. It is
/// digits, after any spaces (an old way of writing numbers below 10000), or in the Alpha-5 form: a
/// letter for its first two digits, then four digits. The letters run from A for 10 to Z for 33,
/// skipping I and O, which look like 1 and 0.
inline int decode_catalog_number(std::string_view field)
{
    std::size_t i = 0;
    while (i < field.size() && field[i] == ' ') {
        ++i;
    }
    if (i == field.size()) {
        return -1;
    }

    int number = 0;
    const char first = field[i];
    if (first >= 'A' && first <= 'Z') {
        if (i != 0 || first == 'I' || first == 'O') {
            return -1;
        }
        number = first - 'A' + 10 - (first > 'I' ? 1 : 0) - (first > 'O' ? 1 : 0);
        ++i;
    }
    for (; i < field.size(); ++i) {
        if (!is_digit(field[i])) {
            return -1;
        }
        number = number * 10 + (field[i] - '0');
    }
    return number;
}

/// A catalog number as five characters, as SGP4 keeps it: digits, with zeros before them, below
/// 100000; the Alpha-5 form (see decode_catalog_number()) to 339999; and empty after that, where
/// five characters can't hold it.
inline std::string encode_catalog_number(int number)
{
    if (number < 0 || number > 339999) {
        return {};
    }
    std::string encoded(5, '0');
    int rest = number;
    for (std::size_t i = 5; i-- > 1;) {
        encoded[i] = static_cast<char>('0' + rest % 10);
        rest /= 10;
    }
    if (rest < 10) {
        encoded[0] = static_cast<char>('0' + rest);
    } else {
        // 10 is A, skipping I and O
        int letter = rest - 10;
        letter += (letter >= 'I' - 'A' ? 1 : 0);
        letter += (letter >= 'O' - 'A' ? 1 : 0);
        encoded[0] = static_cast<char>('A' + letter);
    }
    return encoded;
}

/// The value of a run of columns that hold digits and spaces, with each space read as 0.
inline int whole_number(std::string_view columns)
{
    int number = 0;
    for (const char c : columns) {
        number = number * 10 + (is_digit(c) ? c - '0' : 0);
    }
    return number;
}

/**
 * The value of columns that hold digits and at most one '.', with each space read as 0, read as
 * strtod() would read it, to the bit, but whatever the C locale. The digits, read as a whole
 * number, are exact in a double for every field of a TLE (none has more than 11), as is the power
 * of ten for the digits after the point, and dividing one exact double by another rounds
 * correctly, as strtod() does.
 *
 * @param columns The columns.
 * @param implied_decimals How many digits there are after a decimal point the field leaves out.
 */
inline double decimal(std::string_view columns, int implied_decimals = 0)
{
    constexpr std::array<double, 16> powers_of_ten = {
        1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11, 1e12, 1e13, 1e14, 1e15};
    std::int64_t digits = 0;
    std::size_t decimals = static_cast<std::size_t>(implied_decimals);
    bool after_point = false;
    for (const char c : columns) {
        if (c == '.') {
            after_point = true;
            continue;
        }
        digits = digits * 10 + (is_digit(c) ? c - '0' : 0);
        if (after_point) {
            ++decimals;
        }
    }
    return static_cast<double>(digits) / powers_of_ten.at(decimals);
}

/// The sign in a sign column: -1 for '-', and 1 for '+' or a space.
inline double sign(char column)
{
    return column == '-' ? -1.0 : 1.0;
}

/// The power of ten in a TLE's exponent columns: a sign, then a digit (a space is read as 0).
inline int exponent(std::string_view columns)
{
    const int digit = is_digit(columns[1]) ? columns[1] - '0' : 0;
    return columns[0] == '-' ? -digit : digit;
}

/// What an SGP4 error code (elsetrec::error), or -1 for a state that isn't finite, means.
inline std::string sgp4_error_message(int code)
{
    switch (code) {
    case 1:
        return "SGP4 error 1: the mean eccentricity is outside the range SGP4 allows, -0.001 to 1";
    case 2:
        return "SGP4 error 2: the mean motion is not positive";
    case 3:
        return "SGP4 error 3: the perturbed eccentricity is outside the range 0 to 1";
    case 4:
        return "SGP4 error 4: the semi-latus rectum is negative";
    case 5:
        return "SGP4 error 5: the elements are sub-orbital";
    case 6:
        return "SGP4 error 6: the satellite has decayed, to below the Earth's radius";
    case -1:
        return "SGP4 gave a position or velocity that is not finite";
    default:
        return "SGP4 error " + std::to_string(code);
    }
}

} // namespace detail::tle

namespace detail::omm {

/// A real number as an OMM writes it, such as "15.72125391", "-0.11606E-04" or "1.1606e-05",
/// read as strtod() would, to the bit, but whatever the C locale; or nothing if it isn't one, or
/// isn't finite.
inline std::optional<double> real(std::string_view text)
{
    text = tle::trim(text);
    if (!text.empty() && text.front() == '+') {
        text.remove_prefix(1);
    }
    // Only decimal digits, signs, a point and an exponent: not "inf", "nan" or hexadecimal, which
    // strtod() would read too
    if (text.empty() || text.find_first_not_of("0123456789.+-eE") != std::string_view::npos) {
        return std::nullopt;
    }
    double value = 0.0;
#if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size()) {
        return std::nullopt;
    }
#else
    // Standard libraries without floating-point from_chars(): a stream in the classic locale
    // reads numbers as strtod() does in the C locale
    std::istringstream stream{std::string(text)};
    stream.imbue(std::locale::classic());
    stream >> value;
    if (stream.fail() || stream.peek() != std::istringstream::traits_type::eof()) {
        return std::nullopt;
    }
#endif
    if (!std::isfinite(value)) {
        return std::nullopt;
    }
    return value;
}

/// A non-negative whole number of at most nine digits, or nothing if the text isn't one.
inline std::optional<int> whole_number(std::string_view text)
{
    text = tle::trim(text);
    if (!text.empty() && text.front() == '+') {
        text.remove_prefix(1);
    }
    if (text.empty() || text.size() > 9 || text.find_first_not_of("0123456789") != text.npos) {
        return std::nullopt;
    }
    return tle::whole_number(text);
}

/// A UTC time as an OMM gives its epoch, to the second and its fraction.
struct Epoch {
    int year = 0;
    int month = 0;
    int day = 0;
    int day_of_year = 0;
    int hour = 0;
    int minute = 0;
    double second = 0.0;
};

inline bool is_leap_year(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

/**
 * The epoch in an OMM's EPOCH field: YYYY-MM-DDThh:mm:ss or YYYY-DDDThh:mm:ss, the forms the OMM
 * standard (CCSDS 502.0) gives, each with any fraction of a second, and a Z for UTC or not.
 *
 * @param text The field.
 * @param error Set to why, if it isn't an epoch.
 * @return The epoch, or nothing if the text isn't one from 1900 to 2100, the years SGP4's Julian
 *         dates are good for.
 */
inline std::optional<Epoch> epoch(std::string_view text, std::string& error)
{
    text = tle::trim(text);
    if (!text.empty() && (text.back() == 'Z' || text.back() == 'z')) {
        text.remove_suffix(1);
    }
    const std::size_t t = text.find('T');
    if (t == std::string_view::npos) {
        error = "it has no T between the date and the time";
        return std::nullopt;
    }
    const std::string_view date = text.substr(0, t);
    const std::string_view clock = text.substr(t + 1);

    // A run of digits, as a number, or -1
    const auto digits = [](std::string_view field) {
        if (field.empty() || field.find_first_not_of("0123456789") != field.npos) {
            return -1;
        }
        return tle::whole_number(field);
    };

    constexpr std::array<int, 12> days_in_month = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    Epoch epoch;
    epoch.year = digits(date.substr(0, 4));
    if (date.size() < 5 || date[4] != '-' || epoch.year < 1900 || epoch.year > 2100) {
        error = "its date should start with a year from 1900 to 2100, then '-'";
        return std::nullopt;
    }
    const int february = is_leap_year(epoch.year) ? 29 : 28;
    if (date.size() == 10 && date[7] == '-') {
        epoch.month = digits(date.substr(5, 2));
        epoch.day = digits(date.substr(8, 2));
        if (epoch.month < 1 || epoch.month > 12 || epoch.day < 1 ||
            epoch.day > (epoch.month == 2
                             ? february
                             : days_in_month[static_cast<std::size_t>(epoch.month - 1)])) {
            error = "its date isn't a day of the year";
            return std::nullopt;
        }
        epoch.day_of_year = epoch.day;
        for (int m = 1; m < epoch.month; ++m) {
            epoch.day_of_year += m == 2 ? february : days_in_month[static_cast<std::size_t>(m - 1)];
        }
    } else if (date.size() == 8) {
        epoch.day_of_year = digits(date.substr(5, 3));
        if (epoch.day_of_year < 1 || epoch.day_of_year > 337 + february) {
            error = "its day of the year isn't one";
            return std::nullopt;
        }
        int rest = epoch.day_of_year;
        epoch.month = 1;
        for (int length = 31; rest > length;
             length = epoch.month == 2 ? february
                                       : days_in_month[static_cast<std::size_t>(epoch.month - 1)]) {
            rest -= length;
            ++epoch.month;
        }
        epoch.day = rest;
    } else {
        error = "its date should be YYYY-MM-DD or YYYY-DDD";
        return std::nullopt;
    }

    if (clock.size() < 8 || clock[2] != ':' || clock[5] != ':') {
        error = "its time should be hh:mm:ss, with any fraction of a second";
        return std::nullopt;
    }
    epoch.hour = digits(clock.substr(0, 2));
    epoch.minute = digits(clock.substr(3, 2));
    const int whole_seconds = digits(clock.substr(6, 2));
    const std::string_view fraction = clock.substr(8);
    if (epoch.hour < 0 || epoch.hour > 23 || epoch.minute < 0 || epoch.minute > 59 ||
        whole_seconds < 0) {
        error = "its time should be hh:mm:ss, with any fraction of a second";
        return std::nullopt;
    }
    if (whole_seconds > 59) {
        error = "it is during a leap second, or isn't a time";
        return std::nullopt;
    }
    if (!fraction.empty() &&
        (fraction[0] != '.' || fraction.size() > 13 || digits(fraction.substr(1)) < 0)) {
        error = "its fraction of a second should be a '.', then up to 12 digits";
        return std::nullopt;
    }
    epoch.second = tle::decimal(clock.substr(6));
    return epoch;
}

/// The international designator a TLE writes, such as "98067A", from an OMM's OBJECT_ID, such as
/// "1998-067A"; or empty if it has none in that form, as for "UNKNOWN", which the standard allows.
inline std::string designator(std::string_view object_id)
{
    object_id = tle::trim(object_id);
    const auto all = [](std::string_view text, auto predicate) {
        for (const char c : text) {
            if (!predicate(c)) {
                return false;
            }
        }
        return true;
    };
    if (object_id.size() < 9 || object_id.size() > 11 || object_id[4] != '-' ||
        !all(object_id.substr(0, 4), tle::is_digit) ||
        !all(object_id.substr(5, 3), tle::is_digit) ||
        !all(object_id.substr(8), [](char c) { return c >= 'A' && c <= 'Z'; })) {
        return {};
    }
    return std::string(object_id.substr(2, 2)) + std::string(object_id.substr(5, 3)) +
           std::string(object_id.substr(8));
}

/// Text in upper case, for comparing values the standard's examples write in it.
inline std::string upper(std::string_view text)
{
    std::string result(tle::trim(text));
    for (char& c : result) {
        if (c >= 'a' && c <= 'z') {
            c = static_cast<char>(c - 'a' + 'A');
        }
    }
    return result;
}

} // namespace detail::omm

/**
 * @brief Reads a two-line element set.
 *
 * Each line may have whitespace before or after it, such as a line ending, which is ignored.
 *
 * @param line1 The TLE's first line, which starts "1 ".
 * @param line2 The TLE's second line, which starts "2 ".
 * @param mode The mode to propagate it in. Leave it AFSPC unless you have a reason not to: see
 *             Sgp4Mode.
 * @return The element set, initialized for SGP4.
 * @throws std::runtime_error if a line is not 69 characters long, isn't laid out as a TLE line is,
 *         or fails its checksum; if the lines are for different satellites; if the element set is
 *         for SGP4-XP; or if SGP4 can't propagate it at its epoch.
 */
inline SatRec SatRec::from_tle(std::string_view line1, std::string_view line2, Sgp4Mode mode)
{
    return from_lines_({}, line1, line2, mode);
}

/**
 * @brief Reads a three-line element set: a name line, then a TLE's two lines.
 *
 * The name line may be just the name, as CelesTrak writes it, or start with "0 ", as Space-Track
 * writes it. Whitespace around the name is ignored.
 *
 * @param name The satellite's name line.
 * @param line1 The TLE's first line, which starts "1 ".
 * @param line2 The TLE's second line, which starts "2 ".
 * @param mode The mode to propagate it in. Leave it AFSPC unless you have a reason not to: see
 *             Sgp4Mode.
 * @return The element set, initialized for SGP4.
 * @throws std::runtime_error as the two-line from_tle() does.
 */
inline SatRec SatRec::from_tle(std::string_view name,
                               std::string_view line1,
                               std::string_view line2,
                               Sgp4Mode mode)
{
    name = detail::tle::trim(name);
    if (name.starts_with("0 ")) {
        name = detail::tle::trim(name.substr(2));
    }
    return from_lines_(std::string(name), line1, line2, mode);
}

/**
 * @brief Reads an Orbit Mean-Elements Message (OMM): the CCSDS form of a satellite's mean elements,
 * which CelesTrak and Space-Track publish alongside TLEs, in KVN, XML, JSON and CSV.
 *
 * Every OMM format comes to its keywords and their values, as text. Of those, SGP4 needs:
 *
 * | Keyword | Value |
 * |---|---|
 * | NORAD_CAT_ID | The catalog number, of up to nine digits |
 * | EPOCH | UTC, as YYYY-MM-DDThh:mm:ss.ssssss or YYYY-DDDThh:mm:ss.ssssss |
 * | MEAN_MOTION | Revolutions a day |
 * | ECCENTRICITY | From 0 to below 1 |
 * | INCLINATION | Degrees, from 0 to 180 |
 * | RA_OF_ASC_NODE, ARG_OF_PERICENTER, MEAN_ANOMALY | Degrees |
 * | BSTAR | Per Earth radius |
 * | MEAN_MOTION_DOT | Revolutions a day squared, as a TLE writes it: half the rate |
 * | MEAN_MOTION_DDOT | Revolutions a day cubed, as a TLE writes it: a sixth of the second rate |
 *
 * OBJECT_NAME and OBJECT_ID give the name and international designator. These, if given, are
 * checked: MEAN_ELEMENT_THEORY must be SGP4 (or SGP/SGP4), REF_FRAME TEME, CENTER_NAME EARTH,
 * TIME_SYSTEM UTC, and EPHEMERIS_TYPE not 4. Any other keyword is ignored, so the extra fields
 * Space-Track adds can be passed in as they are.
 *
 * Numbers are read as C++ reads them in the classic locale, so "1.1606E-05" and "0.000011606" are
 * both fine. An element set read from an OMM made from a TLE propagates as that TLE does: for
 * each of Vallado's verification cases, to the bit.
 *
 * @param fields The OMM's keywords and values.
 * @param mode The mode to propagate it in. Leave it AFSPC unless you have a reason not to: see
 *             Sgp4Mode.
 * @return The element set, initialized for SGP4.
 * @throws std::runtime_error if a keyword SGP4 needs is missing, a value isn't the number, whole
 *         number or time it should be, the metadata is for something other than SGP4 in TEME
 *         about the Earth, the element set is for SGP4-XP, an element is out of range, or SGP4
 *         can't propagate it at its epoch.
 */
inline SatRec SatRec::from_omm(const OmmFields& fields, Sgp4Mode mode)
{
    namespace omm = detail::omm;
    namespace tle = detail::tle;
    namespace vallado = detail::vallado;

    const auto find = [&fields](std::string_view keyword) -> std::optional<std::string_view> {
        const auto found = fields.find(keyword);
        if (found == fields.end() || tle::trim(found->second).empty()) {
            return std::nullopt;
        }
        return tle::trim(found->second);
    };

    // Errors name the satellite as far as it is known
    std::string name(find("OBJECT_NAME").value_or(""));
    const std::string quoted_name = name.empty() ? std::string() : "\"" + name + "\"";
    std::string context = "SatRec::from_omm - " + (name.empty() ? "" : quoted_name + ": ");

    const auto required = [&](std::string_view keyword) {
        const std::optional<std::string_view> value = find(keyword);
        if (!value.has_value()) {
            HUIRA_THROW_ERROR(context + "The OMM has no " + std::string(keyword) +
                              ", which SGP4 needs");
        }
        return *value;
    };

    const std::string_view catalog_text = required("NORAD_CAT_ID");
    const std::optional<int> catalog_number = omm::whole_number(catalog_text);
    if (!catalog_number.has_value() || *catalog_number < 1) {
        HUIRA_THROW_ERROR(context + "NORAD_CAT_ID is \"" + std::string(catalog_text) +
                          "\", where a catalog number is a positive whole number of up to nine "
                          "digits");
    }
    context = "SatRec::from_omm - Satellite " + std::to_string(*catalog_number) +
              (name.empty() ? "" : ", " + quoted_name) + ": ";

    // The metadata, where it's given: these elements are SGP4's, in TEME, about the Earth, in UTC
    const auto expect = [&](std::string_view keyword,
                            std::initializer_list<std::string_view> allowed,
                            const std::string& meaning) {
        const std::optional<std::string_view> value = find(keyword);
        if (!value.has_value()) {
            return;
        }
        const std::string given = omm::upper(*value);
        for (const std::string_view ok : allowed) {
            if (given == ok) {
                return;
            }
        }
        HUIRA_THROW_ERROR(context + std::string(keyword) + " is \"" + std::string(*value) +
                          "\": " + meaning);
    };
    const std::optional<std::string_view> theory = find("MEAN_ELEMENT_THEORY");
    if (theory.has_value() && omm::upper(*theory) == "SGP4-XP") {
        HUIRA_THROW_ERROR(context + "The element set is for SGP4-XP (MEAN_ELEMENT_THEORY), which "
                                    "SGP4 can't propagate");
    }
    expect("MEAN_ELEMENT_THEORY", {"SGP4", "SGP/SGP4"}, "SatRec reads SGP4's mean elements");
    expect("REF_FRAME", {"TEME"}, "SGP4's elements are in TEME");
    expect("CENTER_NAME", {"EARTH"}, "SGP4 propagates orbits about the Earth");
    expect("TIME_SYSTEM", {"UTC"}, "SGP4's epochs are UTC");
    if (const std::optional<std::string_view> type = find("EPHEMERIS_TYPE")) {
        const std::optional<int> value = omm::whole_number(*type);
        if (!value.has_value()) {
            HUIRA_THROW_ERROR(context + "EPHEMERIS_TYPE is \"" + std::string(*type) +
                              "\", where it should be a whole number");
        }
        if (*value == 4) {
            HUIRA_THROW_ERROR(context + "The element set is for SGP4-XP (EPHEMERIS_TYPE 4), which "
                                        "SGP4 can't propagate");
        }
    }

    const auto number = [&](std::string_view keyword) {
        const std::string_view text = required(keyword);
        const std::optional<double> value = omm::real(text);
        if (!value.has_value()) {
            HUIRA_THROW_ERROR(context + std::string(keyword) + " is \"" + std::string(text) +
                              "\", which isn't a finite number");
        }
        return *value;
    };
    const double mean_motion = number("MEAN_MOTION");
    const double eccentricity = number("ECCENTRICITY");
    const double inclination = number("INCLINATION");
    const double raan = number("RA_OF_ASC_NODE");
    const double arg_of_pericenter = number("ARG_OF_PERICENTER");
    const double mean_anomaly = number("MEAN_ANOMALY");
    const double bstar = number("BSTAR");
    const double mean_motion_dot = number("MEAN_MOTION_DOT");
    const double mean_motion_ddot = number("MEAN_MOTION_DDOT");

    // The checks the TLE format makes by its columns
    if (!(mean_motion > 0.0)) {
        HUIRA_THROW_ERROR(context + "The mean motion is " + std::string(required("MEAN_MOTION")) +
                          " revolutions a day, where it should be positive");
    }
    if (!(eccentricity >= 0.0 && eccentricity < 1.0)) {
        HUIRA_THROW_ERROR(context + "The eccentricity is " + std::string(required("ECCENTRICITY")) +
                          ", where it should be from 0 to below 1");
    }
    if (!(inclination >= 0.0 && inclination <= 180.0)) {
        HUIRA_THROW_ERROR(context + "The inclination is " + std::string(required("INCLINATION")) +
                          " degrees, where it should be from 0 to 180");
    }

    std::string epoch_error;
    const std::string_view epoch_text = required("EPOCH");
    const std::optional<omm::Epoch> epoch = omm::epoch(epoch_text, epoch_error);
    if (!epoch.has_value()) {
        HUIRA_THROW_ERROR(context + "EPOCH is \"" + std::string(epoch_text) +
                          "\", which isn't a UTC "
                          "time as YYYY-MM-DDThh:mm:ss.ssssss or YYYY-DDDThh:mm:ss.ssssss: " +
                          epoch_error);
    }

    // Converted as from a TLE (see from_lines_())
    constexpr double radians_per_degree = std::numbers::pi / 180.0;
    constexpr double revolutions_per_day = 1440.0 / (2.0 * std::numbers::pi); // In 1 rad/min

    vallado::elsetrec record{};
    record.epochyr = epoch->year % 100;
    record.epochdays = epoch->day_of_year +
                       (epoch->hour * 3600.0 + epoch->minute * 60.0 + epoch->second) / 86400.0;
    record.ndot = mean_motion_dot / (revolutions_per_day * 1440.0);
    record.nddot = mean_motion_ddot / (revolutions_per_day * 1440.0 * 1440.0);
    record.bstar = bstar;
    record.inclo = inclination * radians_per_degree;
    record.nodeo = raan * radians_per_degree;
    record.ecco = eccentricity;
    record.argpo = arg_of_pericenter * radians_per_degree;
    record.mo = mean_anomaly * radians_per_degree;
    record.no_kozai = mean_motion / revolutions_per_day;
    vallado::SGP4Funcs::jday_SGP4(epoch->year,
                                  epoch->month,
                                  epoch->day,
                                  epoch->hour,
                                  epoch->minute,
                                  epoch->second,
                                  record.jdsatepoch,
                                  record.jdsatepochF);

    return from_record_(record,
                        *catalog_number,
                        std::move(name),
                        omm::designator(find("OBJECT_ID").value_or("")),
                        mode,
                        context);
}

inline SatRec
SatRec::from_lines_(std::string name, std::string_view line1, std::string_view line2, Sgp4Mode mode)
{
    namespace tle = detail::tle;
    namespace vallado = detail::vallado;

    // Errors name the satellite as far as it is known
    const std::string quoted_name = name.empty() ? std::string() : "\"" + name + "\"";
    std::string context = "SatRec::from_tle - " + (name.empty() ? "" : quoted_name + ": ");

    line1 = tle::trim(line1);
    line2 = tle::trim(line2);
    tle::check_line(line1, tle::LINE1_COLUMNS, 1, context);
    tle::check_line(line2, tle::LINE2_COLUMNS, 2, context);

    const int catalog_number = tle::decode_catalog_number(line1.substr(2, 5));
    if (catalog_number < 0) {
        HUIRA_THROW_ERROR(context + "The catalog number, \"" + std::string(line1.substr(2, 5)) +
                          "\" in columns 3-7 of line 1, is neither digits nor in the Alpha-5 form "
                          "(a letter other than I or O, then four digits)");
    }
    if (tle::decode_catalog_number(line2.substr(2, 5)) != catalog_number) {
        HUIRA_THROW_ERROR(context + "The lines are for different satellites: line 1 is for " +
                          std::string(line1.substr(2, 5)) + ", and line 2 for " +
                          std::string(line2.substr(2, 5)));
    }
    context = "SatRec::from_tle - Satellite " + std::to_string(catalog_number) +
              (name.empty() ? "" : ", " + quoted_name) + ": ";

    // Checks SGP4 doesn't make. Without them it would propagate garbage: an SGP4-XP element set
    // holds other terms where SGP4 reads B* and the mean motion's second derivative, and a mean
    // motion of 0 gives an infinite semi-major axis.
    if (line1[62] == '4') {
        HUIRA_THROW_ERROR(context +
                          "The element set is for SGP4-XP (ephemeris type 4, in column 63 "
                          "of line 1), which SGP4 can't propagate");
    }
    const int epoch_day = tle::whole_number(line1.substr(20, 3));
    if (epoch_day < 1 || epoch_day > 366) {
        HUIRA_THROW_ERROR(context + "The epoch is on day " + std::to_string(epoch_day) +
                          " of its year, where days run from 1 to 366");
    }
    if (tle::whole_number(line2.substr(8, 3)) * 10000 + tle::whole_number(line2.substr(12, 4)) >
        180 * 10000) {
        HUIRA_THROW_ERROR(context + "The inclination, " +
                          std::string(tle::trim(line2.substr(8, 8))) +
                          " degrees, is more than 180");
    }
    if (line2.substr(52, 11).find_first_of("123456789") == std::string_view::npos) {
        HUIRA_THROW_ERROR(context + "The mean motion is 0");
    }

    // The elements, read and converted as Vallado's twoline2rv() reads and converts them, to the
    // bit (test_satrec checks). It isn't called itself: it reads the lines with sscanf(), whose
    // decimal point is the C locale's, and it passes sgp4init() the catalog number it has just
    // read into the record, which sgp4init() then strcpy()s onto itself, an overlap that
    // AddressSanitizer stops a program for.
    constexpr double radians_per_degree = std::numbers::pi / 180.0;
    constexpr double revolutions_per_day = 1440.0 / (2.0 * std::numbers::pi); // In 1 rad/min

    vallado::elsetrec record{};
    record.epochyr = tle::whole_number(line1.substr(18, 2));
    record.epochdays = tle::decimal(line1.substr(20, 12));
    record.ndot = tle::sign(line1[33]) * tle::decimal(line1.substr(34, 9));
    record.nddot = tle::sign(line1[44]) * tle::decimal(line1.substr(45, 5), 5) *
                   std::pow(10.0, tle::exponent(line1.substr(50, 2)));
    record.bstar = tle::sign(line1[53]) * tle::decimal(line1.substr(54, 5), 5) *
                   std::pow(10.0, tle::exponent(line1.substr(59, 2)));
    record.inclo = tle::decimal(line2.substr(8, 8)) * radians_per_degree;
    record.nodeo = tle::decimal(line2.substr(17, 8)) * radians_per_degree;
    record.ecco = tle::decimal(line2.substr(26, 7), 7);
    record.argpo = tle::decimal(line2.substr(34, 8)) * radians_per_degree;
    record.mo = tle::decimal(line2.substr(43, 8)) * radians_per_degree;
    record.no_kozai = tle::decimal(line2.substr(52, 11)) / revolutions_per_day;
    record.ndot = record.ndot / (revolutions_per_day * 1440.0);
    record.nddot = record.nddot / (revolutions_per_day * 1440.0 * 1440.0);

    // Two-digit years from 1957 to 2056
    const int year = record.epochyr + (record.epochyr < 57 ? 2000 : 1900);
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    double second = 0.0;
    vallado::SGP4Funcs::days2mdhms_SGP4(year, record.epochdays, month, day, hour, minute, second);
    vallado::SGP4Funcs::jday_SGP4(
        year, month, day, hour, minute, second, record.jdsatepoch, record.jdsatepochF);

    return from_record_(record,
                        catalog_number,
                        std::move(name),
                        std::string(tle::trim(line1.substr(9, 8))),
                        mode,
                        context);
}

inline SatRec SatRec::from_record_(detail::vallado::elsetrec& record,
                                   int catalog_number,
                                   std::string name,
                                   std::string international_designator,
                                   Sgp4Mode mode,
                                   const std::string& context)
{
    namespace vallado = detail::vallado;

    // sgp4init() keeps the catalog number as SGP4 reads it from a TLE, as five characters: the
    // Alpha-5 form, with zeros before a shorter number, or nothing for a larger one. It
    // initializes the element set and propagates it to its epoch, which sets record.error if it
    // can't. Its epoch is in days from 1950 January 0.
    const std::string catalog = detail::tle::encode_catalog_number(catalog_number);
    vallado::SGP4Funcs::sgp4init(vallado::wgs72,
                                 mode == Sgp4Mode::IMPROVED ? 'i' : 'a',
                                 catalog.c_str(),
                                 (record.jdsatepoch + record.jdsatepochF) - 2433281.5,
                                 record.bstar,
                                 record.ndot,
                                 record.nddot,
                                 record.ecco,
                                 record.argpo,
                                 record.inclo,
                                 record.mo,
                                 record.no_kozai,
                                 record.nodeo,
                                 record);
    if (record.error != 0) {
        HUIRA_THROW_ERROR(context + "SGP4 can't propagate the element set at its epoch: " +
                          detail::tle::sgp4_error_message(record.error));
    }

    SatRec satrec;
    satrec.catalog_number_ = catalog_number;
    satrec.name_ = std::move(name);
    satrec.international_designator_ = std::move(international_designator);
    satrec.mode_ = mode;
    std::memcpy(satrec.record_.data(), &record, sizeof(record));

    // The epoch is UTC, as a Julian date in two parts, the day and its fraction, which are added
    // here only once J2000 is taken from the day: as one Julian date it would be good to about 40
    // microseconds, 30 cm along a low orbit.
    const double utc_seconds =
        ((record.jdsatepoch - Time::J2000_JD) + record.jdsatepochF) * SECONDS_PER_DAY;
    satrec.epoch_ = Time::from_et(utc_seconds + spice::deltet(utc_seconds, "UTC"));

    double r[3] = {};
    double v[3] = {};
    const int error = satrec.propagate_(satrec.epoch_, r, v);
    if (error != 0) {
        HUIRA_THROW_ERROR(context + "SGP4 can't propagate the element set at its epoch: " +
                          detail::tle::sgp4_error_message(error));
    }
    return satrec;
}

/**
 * @brief The satellite's position and velocity at a time, from SGP4.
 *
 * @param time The time to propagate the element set to.
 * @return Its position, in meters, and velocity, in meters per second, in the TEME frame
 *         (HUIRA_EARTH_TEME, centered on the Earth), or nothing if SGP4 can't propagate it to the
 *         time: most often because it has decayed by then. error_message_at() says why.
 */
inline std::optional<PositionState> SatRec::evaluate(const Time& time) const
{
    double r[3] = {};
    double v[3] = {};
    if (propagate_(time, r, v) != 0) {
        return std::nullopt;
    }
    constexpr double meters_per_kilometer = 1000.0;
    return PositionState{Vec3<double>{r[0], r[1], r[2]} * meters_per_kilometer,
                         Vec3<double>{v[0], v[1], v[2]} * meters_per_kilometer};
}

/**
 * @brief Why SGP4 can't propagate the element set to a time, if it can't.
 *
 * @param time The time evaluate() was given.
 * @return An empty string if evaluate() gives a state at the time, and otherwise the SGP4 error
 *         that stopped it, such as "SGP4 error 6: the satellite has decayed, to below the Earth's
 *         radius".
 */
inline std::string SatRec::error_message_at(const Time& time) const
{
    double r[3] = {};
    double v[3] = {};
    const int error = propagate_(time, r, v);
    return error == 0 ? std::string() : detail::tle::sgp4_error_message(error);
}

inline int SatRec::propagate_(const Time& time, double (&r)[3], double (&v)[3]) const
{
    // SGP4 updates the record as it propagates, so it propagates a copy, from the record as it was
    // initialized: no call sees another's
    detail::vallado::elsetrec record;
    std::memcpy(&record, record_.data(), sizeof(record));

    const double minutes = (time.et() - epoch_.et()) / 60.0;
    if (!detail::vallado::SGP4Funcs::sgp4(record, minutes, r, v)) {
        return record.error != 0 ? record.error : -1;
    }
    for (std::size_t i = 0; i < 3; ++i) {
        if (!std::isfinite(r[i]) || !std::isfinite(v[i])) {
            return -1;
        }
    }
    return 0;
}

} // namespace huira
