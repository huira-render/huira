#include <cstddef>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "huira/ephemeris/element_set_catalog.hpp"
#include "third_party/sgp4_verification.hpp"

using namespace huira;
using Catch::Matchers::ContainsSubstring;

namespace {

// The ISS, epoch 2008-09-20 12:25:40.104192 UTC
const std::string ISS_LINE1 =
    "1 25544U 98067A   08264.51782528 -.00002182  00000-0 -11606-4 0  2927";
const std::string ISS_LINE2 =
    "2 25544  51.6416 247.4627 0006703 130.5360 325.0288 15.72125391563537";

// Vallado's case 23599, a deep-space orbit, epoch 2006-06-20 18:22:06.640 UTC
const std::string DEEP_LINE1 =
    "1 23599U 95029B   06171.76535463  .00085586  12891-6  12956-2 0  2905";
const std::string DEEP_LINE2 =
    "2 23599   6.9327   0.2849 5782022 274.4436  25.2425  4.47796565123555";

/// A TLE line with `text` written over it from `column` (counted from 1), and its checksum made to
/// match.
std::string edit(std::string_view line, std::size_t column, std::string_view text)
{
    std::string edited(line.substr(0, 69));
    edited.replace(column - 1, text.size(), text);
    int sum = 0;
    for (std::size_t i = 0; i < 68; ++i) {
        if (edited[i] >= '0' && edited[i] <= '9') {
            sum += edited[i] - '0';
        } else if (edited[i] == '-') {
            sum += 1;
        }
    }
    edited[68] = static_cast<char>('0' + sum % 10);
    return edited;
}

/// The ISS's element set, with its epoch a day later.
const std::string ISS_NEXT_DAY_LINE1 = edit(ISS_LINE1, 19, "08265.51782528");

/// The catalog numbers of the element sets kept, in order.
std::vector<int> numbers(const ElementSetCatalog& catalog)
{
    std::vector<int> result;
    for (const SatRec& satrec : catalog.element_sets) {
        result.push_back(satrec.catalog_number());
    }
    return result;
}

/// Whether two element sets propagate alike, to a millimeter, as the same element set read from a
/// TLE and an OMM made from it do.
bool same_states(const SatRec& a, const SatRec& b)
{
    for (const double minutes : {0.0, 600.0}) {
        const Time time = a.epoch() + units::Second(minutes * 60.0);
        if (glm::length(a.evaluate(time)->position - b.evaluate(time)->position) > 1e-3) {
            return false;
        }
    }
    return true;
}

// The ISS and 23599 as OMMs, in each format, as CelesTrak and Space-Track write them
const std::string KVN = R"(CCSDS_OMM_VERS = 2.0
COMMENT  GENERATED VIA SPACE-TRACK.ORG API
CREATION_DATE = 2008-09-21T00:00:00
ORIGINATOR = 18 SPCS
OBJECT_NAME = ISS (ZARYA)
OBJECT_ID = 1998-067A
CENTER_NAME = EARTH
REF_FRAME = TEME
TIME_SYSTEM = UTC
MEAN_ELEMENT_THEORY = SGP4
EPOCH = 2008-09-20T12:25:40.104192
MEAN_MOTION = 15.72125391 [rev/day]
ECCENTRICITY = 0.0006703
INCLINATION = 51.6416 [deg]
RA_OF_ASC_NODE = 247.4627 [deg]
ARG_OF_PERICENTER = 130.5360 [deg]
MEAN_ANOMALY = 325.0288 [deg]
EPHEMERIS_TYPE = 0
CLASSIFICATION_TYPE = U
NORAD_CAT_ID = 25544
ELEMENT_SET_NO = 292
REV_AT_EPOCH = 56353
BSTAR = -0.11606E-04 [1/ER]
MEAN_MOTION_DOT = -0.00002182 [rev/day**2]
MEAN_MOTION_DDOT = 0.0 [rev/day**3]

CCSDS_OMM_VERS = 2.0
OBJECT_NAME = 23599
OBJECT_ID = 1995-029B
EPOCH = 2006-171T18:22:06.640032
MEAN_MOTION = 4.47796565
ECCENTRICITY = 0.5782022
INCLINATION = 6.9327
RA_OF_ASC_NODE = 0.2849
ARG_OF_PERICENTER = 274.4436
MEAN_ANOMALY = 25.2425
NORAD_CAT_ID = 23599
BSTAR = 0.12956E-02
MEAN_MOTION_DOT = 0.00085586
MEAN_MOTION_DDOT = 0.12891E-06
)";

const std::string CSV =
    "OBJECT_NAME,OBJECT_ID,EPOCH,MEAN_MOTION,ECCENTRICITY,INCLINATION,RA_OF_ASC_NODE,"
    "ARG_OF_PERICENTER,MEAN_ANOMALY,EPHEMERIS_TYPE,CLASSIFICATION_TYPE,NORAD_CAT_ID,"
    "ELEMENT_SET_NO,REV_AT_EPOCH,BSTAR,MEAN_MOTION_DOT,MEAN_MOTION_DDOT\r\n"
    "\"ISS (ZARYA), \"\"THE STATION\"\"\",1998-067A,2008-09-20T12:25:40.104192,15.72125391,"
    ".0006703,51.6416,247.4627,130.536,325.0288,0,U,25544,292,56353,-.11606E-4,-.2182E-4,0\r\n"
    "23599,1995-029B,2006-06-20T18:22:06.640032,4.47796565,.5782022,6.9327,.2849,274.4436,"
    "25.2425,0,U,23599,290,12355,.12956E-2,.85586E-3,.12891E-6\r\n";

// CelesTrak writes numbers as numbers, and Space-Track as text, with nulls for what it doesn't have
const std::string JSON = R"json([
  {"OBJECT_NAME": "ISS (ZARYA)",
                  "OBJECT_ID" : "1998-067A",
                  "EPOCH" : "2008-09-20T12:25:40.104192",
                  "MEAN_MOTION" : 15.72125391,
                  "ECCENTRICITY" : 0.0006703,
                  "INCLINATION" : 51.6416,
                  "RA_OF_ASC_NODE" : 247.4627,
                  "ARG_OF_PERICENTER" : 130.536,
                  "MEAN_ANOMALY" : 325.0288,
                  "EPHEMERIS_TYPE" : 0,
                  "CLASSIFICATION_TYPE" : "U",
                  "NORAD_CAT_ID" : 25544,
                  "ELEMENT_SET_NO" : 292,
                  "REV_AT_EPOCH" : 56353,
                  "BSTAR" : -1.1606e-05,
                  "MEAN_MOTION_DOT" : -2.182e-05,
                  "MEAN_MOTION_DDOT" : 0
},
  {"CCSDS_OMM_VERS": "2.0", "OBJECT_NAME": "23599", "OBJECT_ID": "1995-029B",
   "CENTER_NAME": "EARTH", "REF_FRAME": "TEME", "TIME_SYSTEM": "UTC",
   "MEAN_ELEMENT_THEORY": "SGP4", "EPOCH": "2006-06-20T18:22:06.640032",
   "MEAN_MOTION": "4.47796565", "ECCENTRICITY": "0.5782022", "INCLINATION": "6.9327",
   "RA_OF_ASC_NODE": "0.2849", "ARG_OF_PERICENTER": "274.4436", "MEAN_ANOMALY": "25.2425",
   "EPHEMERIS_TYPE": "0", "NORAD_CAT_ID": "23599", "BSTAR": "0.0012956",
   "MEAN_MOTION_DOT": "0.00085586", "MEAN_MOTION_DDOT": "0.00000012891",
   "DECAY_DATE": null, "SEMIMAJOR_AXIS": "11830.531"}
])json";

/// A file in the system's temporary directory, deleted when it goes.
class TemporaryFile {
  public:
    explicit TemporaryFile(const std::string& text)
    {
        std::random_device random;
        path_ = std::filesystem::temp_directory_path() /
                ("huira_test_catalog_" + std::to_string(random()) + ".txt");
        std::ofstream(path_, std::ios::binary) << text;
    }
    TemporaryFile(const TemporaryFile&) = delete;
    TemporaryFile& operator=(const TemporaryFile&) = delete;
    ~TemporaryFile()
    {
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
    }
    const std::filesystem::path& path() const { return path_; }

  private:
    std::filesystem::path path_;
};

} // namespace

TEST_CASE("ElementSetCatalog: Reads TLEs, of two lines or three", "[catalog]")
{
    // As CelesTrak writes names, padded, and as Space-Track does, after "0 "; and a TLE with no
    // name; with Windows line endings, a blank line and a comment
    const std::string text = "ISS (ZARYA)             \r\n" + ISS_LINE1 + "\r\n" + ISS_LINE2 +
                             "\r\n\r\n# A comment\r\n" + "0 23599\r\n" + DEEP_LINE1 + "\r\n" +
                             DEEP_LINE2 + "\r\n" + edit(ISS_LINE1, 3, "00005") + "\r\n" +
                             edit(ISS_LINE2, 3, "00005");
    const ElementSetCatalog catalog = ElementSetCatalog::parse(text, ElementSetFormat::TLE);
    CHECK(catalog.skipped.empty());
    REQUIRE(numbers(catalog) == std::vector<int>{25544, 23599, 5});
    CHECK(catalog.element_sets[0].name() == "ISS (ZARYA)");
    CHECK(catalog.element_sets[1].name() == "23599");
    CHECK(catalog.element_sets[2].name().empty());
    CHECK(same_states(catalog.element_sets[0], SatRec::from_tle(ISS_LINE1, ISS_LINE2)));
    CHECK(catalog.element_sets[0].mode() == Sgp4Mode::AFSPC);

    // In the mode asked for
    const ElementSetCatalog improved =
        ElementSetCatalog::parse(text, ElementSetFormat::TLE, {.mode = Sgp4Mode::IMPROVED});
    CHECK(improved.element_sets[1].mode() == Sgp4Mode::IMPROVED);
}

TEST_CASE("ElementSetCatalog: Reads OMMs in KVN, CSV and JSON", "[catalog]")
{
    const SatRec iss = SatRec::from_tle(ISS_LINE1, ISS_LINE2);
    const SatRec deep = SatRec::from_tle(DEEP_LINE1, DEEP_LINE2);

    for (const auto& [format, text] : {std::pair{ElementSetFormat::OMM_KVN, KVN},
                                       std::pair{ElementSetFormat::OMM_CSV, CSV},
                                       std::pair{ElementSetFormat::OMM_JSON, JSON}}) {
        INFO("Format " << static_cast<int>(format));
        const ElementSetCatalog catalog = ElementSetCatalog::parse(text, format);
        CHECK(catalog.skipped.empty());
        REQUIRE(numbers(catalog) == std::vector<int>{25544, 23599});
        CHECK(same_states(catalog.element_sets[0], iss));
        CHECK(same_states(catalog.element_sets[1], deep));
        CHECK(catalog.element_sets[0].international_designator() == "98067A");
        CHECK(catalog.element_sets[1].name() == "23599");
    }

    // A name in quotes, with a comma and quotes of its own
    CHECK(ElementSetCatalog::parse(CSV, ElementSetFormat::OMM_CSV).element_sets[0].name() ==
          "ISS (ZARYA), \"THE STATION\"");

    // One JSON object, not in an array
    const std::string one = JSON.substr(JSON.find('{'), JSON.find("},") - JSON.find('{') + 1);
    CHECK(numbers(ElementSetCatalog::parse(one, ElementSetFormat::OMM_JSON)) ==
          std::vector<int>{25544});
}

TEST_CASE("ElementSetCatalog: Works out a catalog's format from its text", "[catalog]")
{
    CHECK(ElementSetCatalog::detect_format(ISS_LINE1 + "\n" + ISS_LINE2) == ElementSetFormat::TLE);
    CHECK(ElementSetCatalog::detect_format("ISS (ZARYA)\n" + ISS_LINE1 + "\n" + ISS_LINE2) ==
          ElementSetFormat::TLE);
    CHECK(ElementSetCatalog::detect_format(KVN) == ElementSetFormat::OMM_KVN);
    CHECK(ElementSetCatalog::detect_format("\nOBJECT_NAME = ISS\n") == ElementSetFormat::OMM_KVN);
    CHECK(ElementSetCatalog::detect_format("\xEF\xBB\xBF" + CSV) == ElementSetFormat::OMM_CSV);
    CHECK(ElementSetCatalog::detect_format("  " + JSON) == ElementSetFormat::OMM_JSON);
    CHECK(ElementSetCatalog::detect_format("{}") == ElementSetFormat::OMM_JSON);

    // As read() does, from a file
    for (const std::string& text : {KVN, CSV, JSON}) {
        const TemporaryFile file(text);
        CHECK(numbers(ElementSetCatalog::read(file.path())) == std::vector<int>{25544, 23599});
    }
    const TemporaryFile tle(ISS_LINE1 + "\n" + ISS_LINE2 + "\n");
    CHECK(numbers(ElementSetCatalog::read(tle.path())) == std::vector<int>{25544});
    CHECK(numbers(ElementSetCatalog::read(tle.path(), ElementSetFormat::TLE)) ==
          std::vector<int>{25544});
}

TEST_CASE("ElementSetCatalog: Skips entries that aren't element sets, and says why", "[catalog]")
{
    SECTION("TLEs")
    {
        std::string bad_checksum = ISS_LINE1;
        bad_checksum[68] = '8';
        const std::string text =
            "GOOD\n" + DEEP_LINE1 + "\n" + DEEP_LINE2 + "\n" +            // 0, lines 1-3
            "BAD CHECKSUM\n" + bad_checksum + "\n" + ISS_LINE2 + "\n" +   // 1, 4-6
            "NO TLE AFTER ME\n" +                                         // 2, 7
            "XP\n" + edit(ISS_LINE1, 63, "4") + "\n" + ISS_LINE2 + "\n" + // 3, 8-10
            ISS_LINE1 + "\n\n" +                                          // 4, 11
            ISS_LINE2 + "\n" +                                            // 5, 13
            "LAST NAME\n";                                                // 6, 14
        const ElementSetCatalog catalog = ElementSetCatalog::parse(text, ElementSetFormat::TLE);
        CHECK(numbers(catalog) == std::vector<int>{23599});
        REQUIRE(catalog.skipped.size() == 6);
        for (const SkippedElementSet& skipped : catalog.skipped) {
            CHECK(skipped.reason == SkipReason::INVALID);
        }

        const SkippedElementSet& checksum = catalog.skipped[0];
        CHECK(checksum.entry == 1);
        CHECK(checksum.line == 4);
        CHECK(checksum.catalog_number == 25544);
        CHECK(checksum.name == "BAD CHECKSUM");
        CHECK_THAT(checksum.message, ContainsSubstring("Line 1's checksum is 8"));
        CHECK_THAT(checksum.message, ContainsSubstring("\"BAD CHECKSUM\""));

        CHECK(catalog.skipped[1].line == 7);
        CHECK(catalog.skipped[1].name == "NO TLE AFTER ME");
        CHECK_THAT(catalog.skipped[1].message, ContainsSubstring("has no TLE after it"));
        CHECK(catalog.skipped[2].line == 8);
        CHECK(catalog.skipped[2].name == "XP");
        CHECK_THAT(catalog.skipped[2].message, ContainsSubstring("SGP4-XP"));
        CHECK_THAT(catalog.skipped[3].message,
                   ContainsSubstring("line 11 is a TLE's line 1, with no line 2 after it"));
        CHECK_THAT(catalog.skipped[4].message,
                   ContainsSubstring("line 13 is a TLE's line 2, with no line 1 before it"));
        CHECK(catalog.skipped[5].line == 14);
        CHECK(catalog.skipped[5].catalog_number == 0);
    }

    SECTION("OMMs in KVN")
    {
        const std::string text = KVN +
                                 "CCSDS_OMM_VERS = 2.0\nNORAD_CAT_ID = 5\nNORAD_CAT_ID = 6\n" +
                                 "CCSDS_OMM_VERS = 2.0\nNOT A KEYWORD AND VALUE\n";
        const ElementSetCatalog catalog = ElementSetCatalog::parse(text, ElementSetFormat::OMM_KVN);
        CHECK(numbers(catalog) == std::vector<int>{25544, 23599});
        REQUIRE(catalog.skipped.size() == 2);
        CHECK_THAT(catalog.skipped[0].message,
                   ContainsSubstring("gives NORAD_CAT_ID again: is a CCSDS_OMM_VERS line missing"));
        CHECK_THAT(catalog.skipped[1].message, ContainsSubstring("isn't KEYWORD = value"));
    }

    SECTION("OMMs in CSV")
    {
        std::string text = CSV + "23599,1995-029B\r\n\"Unclosed,1995-029B\r\n";
        // A row SGP4 can't use: its mean motion is 0
        const std::string deep_row = CSV.substr(CSV.rfind("23599,1995"));
        text += std::string(deep_row).replace(deep_row.find("4.47796565"), 10, "0.00000000");
        const ElementSetCatalog catalog = ElementSetCatalog::parse(text, ElementSetFormat::OMM_CSV);
        CHECK(numbers(catalog) == std::vector<int>{25544, 23599});
        REQUIRE(catalog.skipped.size() == 3);
        CHECK_THAT(catalog.skipped[0].message,
                   ContainsSubstring("has 2 fields, where the header, on line 1, has 17"));
        CHECK_THAT(catalog.skipped[1].message, ContainsSubstring("a quote that isn't closed"));
        CHECK(catalog.skipped[2].catalog_number == 23599);
        CHECK_THAT(catalog.skipped[2].message, ContainsSubstring("where it should be positive"));
    }

    SECTION("OMMs in JSON")
    {
        const std::string text = JSON.substr(0, JSON.rfind(']')) +
                                 R"(, 42, {"NORAD_CAT_ID": [1, 2]}, {"OBJECT_NAME": "X"}])";
        const ElementSetCatalog catalog =
            ElementSetCatalog::parse(text, ElementSetFormat::OMM_JSON);
        CHECK(numbers(catalog) == std::vector<int>{25544, 23599});
        REQUIRE(catalog.skipped.size() == 3);
        CHECK(catalog.skipped[0].entry == 2);
        CHECK(catalog.skipped[0].line == 0);
        CHECK_THAT(catalog.skipped[0].message, ContainsSubstring("isn't an object"));
        CHECK_THAT(catalog.skipped[1].message, ContainsSubstring("is an array or object"));
        CHECK(catalog.skipped[2].name == "X");
        CHECK_THAT(catalog.skipped[2].message, ContainsSubstring("The OMM has no NORAD_CAT_ID"));
    }
}

TEST_CASE("ElementSetCatalog: Strictly, throws on the first entry that isn't an element set",
          "[catalog]")
{
    std::string bad = ISS_LINE1;
    bad[68] = '8';
    const std::string text =
        DEEP_LINE1 + "\n" + DEEP_LINE2 + "\n\n" + bad + "\n" + ISS_LINE2 + "\n";
    CHECK_THROWS_WITH(ElementSetCatalog::parse(text, ElementSetFormat::TLE, {.strict = true}),
                      ContainsSubstring("Entry 1, on line 4: Line 1's checksum is 8"));
    CHECK_NOTHROW(ElementSetCatalog::parse(
        DEEP_LINE1 + "\n" + DEEP_LINE2, ElementSetFormat::TLE, {.strict = true}));
}

TEST_CASE("ElementSetCatalog: Throws when there is nothing to read", "[catalog]")
{
    CHECK_THROWS_WITH(ElementSetCatalog::parse("[{\"NORAD_CAT_ID\": 1", ElementSetFormat::OMM_JSON),
                      ContainsSubstring("The JSON doesn't parse"));
    CHECK_THROWS_WITH(ElementSetCatalog::parse("\"42\"", ElementSetFormat::OMM_JSON),
                      ContainsSubstring("neither an array of OMMs nor one OMM"));
    CHECK_THROWS_WITH(
        ElementSetCatalog::parse("\"OBJECT_NAME,NORAD_CAT_ID\n", ElementSetFormat::OMM_CSV),
        ContainsSubstring("The CSV header, on line 1, has a quote that isn't closed"));
    CHECK_THROWS_WITH(ElementSetCatalog::read("no/such/catalog.txt"),
                      ContainsSubstring("Can't open"));

    // An empty catalog is a catalog with nothing in it
    for (const ElementSetFormat format : {ElementSetFormat::TLE,
                                          ElementSetFormat::OMM_KVN,
                                          ElementSetFormat::OMM_CSV,
                                          ElementSetFormat::OMM_JSON}) {
        const ElementSetCatalog empty =
            ElementSetCatalog::parse(format == ElementSetFormat::OMM_JSON ? "[]" : "\n", format);
        CHECK(empty.element_sets.empty());
        CHECK(empty.skipped.empty());
    }
}

TEST_CASE("ElementSetCatalog: Keeps the satellites asked for", "[catalog]")
{
    // An entry for a satellite left out isn't read, so an error in it isn't reported as one
    std::string bad_deep = DEEP_LINE1;
    bad_deep[68] = bad_deep[68] == '0' ? '1' : '0';
    const std::string text =
        ISS_LINE1 + "\n" + ISS_LINE2 + "\n" + bad_deep + "\n" + DEEP_LINE2 + "\n";

    const ElementSetCatalog included =
        ElementSetCatalog::parse(text, ElementSetFormat::TLE, {.include = {25544}});
    CHECK(numbers(included) == std::vector<int>{25544});
    REQUIRE(included.skipped.size() == 1);
    CHECK(included.skipped[0].reason == SkipReason::NOT_INCLUDED);
    CHECK(included.skipped[0].catalog_number == 23599);

    const ElementSetCatalog excluded =
        ElementSetCatalog::parse(text, ElementSetFormat::TLE, {.exclude = {23599}});
    CHECK(numbers(excluded) == std::vector<int>{25544});
    REQUIRE(excluded.skipped.size() == 1);
    CHECK(excluded.skipped[0].reason == SkipReason::EXCLUDED);

    // Excluding wins over including
    CHECK(ElementSetCatalog::parse(
              text, ElementSetFormat::TLE, {.include = {25544}, .exclude = {25544}})
              .element_sets.empty());
}

TEST_CASE("ElementSetCatalog: Leaves out element sets too far from the reference time", "[catalog]")
{
    const std::string text =
        ISS_LINE1 + "\n" + ISS_LINE2 + "\n" + DEEP_LINE1 + "\n" + DEEP_LINE2 + "\n";
    const Time reference("2008-09-25T00:00:00");

    const ElementSetCatalog catalog = ElementSetCatalog::parse(
        text, ElementSetFormat::TLE, {.reference_time = reference, .max_age = units::Day(14)});
    CHECK(numbers(catalog) == std::vector<int>{25544});
    REQUIRE(catalog.skipped.size() == 1);
    CHECK(catalog.skipped[0].reason == SkipReason::TOO_OLD);
    CHECK_THAT(catalog.skipped[0].message, ContainsSubstring("827.2 days from the reference time"));

    // Before the reference time or after it
    CHECK(ElementSetCatalog::parse(
              text,
              ElementSetFormat::TLE,
              {.reference_time = Time("2008-09-15T00:00:00"), .max_age = units::Day(14)})
              .element_sets.size() == 1);

    CHECK_THROWS_WITH(
        ElementSetCatalog::parse(text, ElementSetFormat::TLE, {.max_age = units::Day(14)}),
        ContainsSubstring("max_age is measured from reference_time, which isn't given"));
    CHECK_THROWS_WITH(
        ElementSetCatalog::parse(
            text, ElementSetFormat::TLE, {.reference_time = reference, .max_age = units::Day(0)}),
        ContainsSubstring("max_age must be positive"));
}

TEST_CASE("ElementSetCatalog: Keeps one element set for each satellite", "[catalog]")
{
    const SatRec older = SatRec::from_tle(ISS_LINE1, ISS_LINE2);
    const SatRec newer = SatRec::from_tle(ISS_NEXT_DAY_LINE1, ISS_LINE2);
    const std::string text = ISS_LINE1 + "\n" + ISS_LINE2 + "\n" + DEEP_LINE1 + "\n" + DEEP_LINE2 +
                             "\n" + ISS_NEXT_DAY_LINE1 + "\n" + ISS_LINE2 + "\n";

    SECTION("The newest, with no reference time")
    {
        const ElementSetCatalog catalog = ElementSetCatalog::parse(text, ElementSetFormat::TLE);
        // In the order the satellites first appear
        REQUIRE(numbers(catalog) == std::vector<int>{25544, 23599});
        CHECK(catalog.element_sets[0].epoch() == newer.epoch());
        REQUIRE(catalog.skipped.size() == 1);
        CHECK(catalog.skipped[0].reason == SkipReason::SUPERSEDED);
        CHECK(catalog.skipped[0].entry == 0);
        CHECK(catalog.skipped[0].line == 1);
        CHECK_THAT(catalog.skipped[0].message, ContainsSubstring("entry 2 on line 5"));
    }

    SECTION("The nearest the reference time")
    {
        const ElementSetCatalog catalog = ElementSetCatalog::parse(
            text, ElementSetFormat::TLE, {.reference_time = older.epoch() + units::Second(3600.0)});
        CHECK(catalog.element_sets[0].epoch() == older.epoch());
        REQUIRE(catalog.skipped.size() == 1);
        CHECK(catalog.skipped[0].entry == 2);
        CHECK_THAT(catalog.skipped[0].message, ContainsSubstring("entry 0 on line 1"));
    }

    SECTION("The first, of two the same")
    {
        const ElementSetCatalog catalog = ElementSetCatalog::parse(
            ISS_LINE1 + "\n" + ISS_LINE2 + "\n" + ISS_LINE1 + "\n" + ISS_LINE2 + "\n",
            ElementSetFormat::TLE);
        CHECK(numbers(catalog) == std::vector<int>{25544});
        REQUIRE(catalog.skipped.size() == 1);
        CHECK(catalog.skipped[0].entry == 1);
    }
}

TEST_CASE("ElementSetCatalog: Reads Vallado's verification cases as one catalog", "[catalog]")
{
    // His cases, cut to their 69 columns, with the checksums he left stale made to match
    std::string text;
    for (const test::VerificationCase& verification_case : test::load_verification_cases()) {
        text += edit(verification_case.line1, 1, "1") + "\n" +
                edit(verification_case.line2, 1, "2") + "\n";
    }
    const ElementSetCatalog catalog = ElementSetCatalog::parse(text, ElementSetFormat::TLE);

    // 33 cases, of 32 satellites: 20413 is there twice, and 33334 can't be propagated at its epoch
    CHECK(catalog.element_sets.size() == 31);
    REQUIRE(catalog.skipped.size() == 2);
    CHECK(catalog.skipped[0].catalog_number == 33334);
    CHECK(catalog.skipped[0].reason == SkipReason::INVALID);
    CHECK_THAT(catalog.skipped[0].message, ContainsSubstring("SGP4 error 3"));
    CHECK(catalog.skipped[1].catalog_number == 20413);
    CHECK(catalog.skipped[1].reason == SkipReason::SUPERSEDED);
}
