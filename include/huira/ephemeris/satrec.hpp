#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "huira/core/time.hpp"
#include "huira/scene/state_callbacks/state_callbacks.hpp"
#include "huira_impl/third_party/sgp4.ipp"

namespace huira {

/**
 * @brief Which of its two behaviors Vallado's SGP4 propagates an element set with.
 *
 * Vallado's reference SGP4 can either reproduce the US Space Force's operational SGP4 (AFSPC), or
 * use his own handling of the one step where he departs from it (IMPROVED). In the revision Huira
 * vendors, that step is in the periodic perturbations by the Sun and Moon, for orbits whose
 * inclination, as they perturb it, is under 0.2 rad (11.46 degrees). Only deep-space orbits (a
 * period of 225 minutes or more) are perturbed so, which leaves those inclined less than about
 * 11.46 degrees, much of the geostationary belt among them: every other element set is propagated
 * identically in either mode. Where the modes differ, they can be a kilometer apart within hours.
 *
 * **Use AFSPC, the default, unless you have a specific reason not to.** TLEs are made by the Space
 * Force fitting observations with its own SGP4, which AFSPC mode reproduces, so they are propagated
 * most faithfully the same way. "Improved" does not mean more accurate for them: it means less
 * consistent with how they were made. Vallado recommends AFSPC mode to match the Space Force's
 * operations, and made his reference output for SGP4 in it.
 *
 * IMPROVED is for reproducing software that uses it: python-sgp4 reads every TLE in it (its
 * Satrec.twoline2rv() has no other mode), and Skyfield reads TLEs through python-sgp4. Choose it to
 * compare Huira's results with theirs exactly, unless a leap second falls between the epoch and the
 * time (see SatRec).
 *
 * The mode is fixed when a SatRec is made, as SGP4 initializes the element set for it.
 */
enum class Sgp4Mode {
    AFSPC,   ///< As the US Space Force's operational SGP4 does (opsmode 'a'). The default.
    IMPROVED ///< Vallado's improved handling (opsmode 'i'), as python-sgp4 and Skyfield use.
};

/**
 * @brief An Orbit Mean-Elements Message's keywords and their values, as text: "EPOCH",
 * "MEAN_MOTION", "BSTAR", and so on (see SatRec::from_omm()).
 *
 * Every OMM format (KVN, XML, JSON or CSV) comes to this: each keyword and its value.
 */
using OmmFields = std::map<std::string, std::string, std::less<>>;

/**
 * @brief A satellite's element set, read from a two-line element set (TLE) or an Orbit
 * Mean-Elements Message (OMM), and ready to be propagated with SGP4.
 *
 * A SatRec is made from a TLE's two lines, or from three with a name line before them, or from an
 * OMM's fields, and checks them as it is made. For a TLE: each line's length, the layout of its
 * columns and its checksum, and that both lines are for the same satellite. For an OMM: that it
 * has the fields SGP4 needs, each a number, an integer or a time as it should be, and that its
 * metadata, if given, is for SGP4 in TEME about the Earth. For both: that the elements are in
 * range, and that SGP4 can propagate them at their epoch. Anything else throws, so a SatRec that
 * exists is good at its epoch.
 *
 * evaluate() gives the satellite's position and velocity at a time, in the TEME frame (true
 * equator, mean equinox) that SGP4 works in, which Huira defines as the SPICE frame
 * HUIRA_EARTH_TEME, centered on the Earth. SGP4 runs with the WGS-72 constants that TLEs are made
 * with, in the mode chosen when the SatRec is made (see Sgp4Mode).
 *
 * The time since epoch that SGP4 is given is elapsed ephemeris time (TDB seconds), as SPICE's own
 * SGP4 (evsgp4_c) takes it, so it counts leap seconds. Software that subtracts UTC Julian dates,
 * as python-sgp4 does, differs from it by a second of motion, several kilometers in low Earth
 * orbit, for each leap second between the epoch and the time.
 *
 * Catalog numbers in the Alpha-5 form, a letter and four digits for the numbers from 100000 to
 * 339999, are read. SGP4-XP element sets (ephemeris type 4) are not: SGP4 cannot propagate them,
 * so they are rejected rather than propagated wrongly.
 *
 * evaluate() keeps no state between calls: it can be called from several threads at once, and what
 * it gives does not depend on what it was called with before. Each call propagates from the epoch,
 * which for the resonant deep-space orbits (periods near 12 and 24 hours) means integrating in
 * steps of 720 minutes, so they cost a little more the further from the epoch they are evaluated.
 */
class SatRec {
  public:
    static SatRec
    from_tle(std::string_view line1, std::string_view line2, Sgp4Mode mode = Sgp4Mode::AFSPC);

    static SatRec from_tle(std::string_view name,
                           std::string_view line1,
                           std::string_view line2,
                           Sgp4Mode mode = Sgp4Mode::AFSPC);

    static SatRec from_omm(const OmmFields& fields, Sgp4Mode mode = Sgp4Mode::AFSPC);

    /// The satellite's NORAD catalog number, decoded from the Alpha-5 form if it is in it.
    [[nodiscard]] int catalog_number() const { return catalog_number_; }

    /// The satellite's name: from a TLE's name line, or an OMM's OBJECT_NAME, or empty if it has
    /// none.
    [[nodiscard]] const std::string& name() const { return name_; }

    /// The satellite's international designator (COSPAR ID) as a TLE writes it, such as "98067A"
    /// (an OMM's OBJECT_ID "1998-067A"), or empty if the element set leaves it blank.
    [[nodiscard]] const std::string& international_designator() const
    {
        return international_designator_;
    }

    /// The epoch of the element set: the time it describes the orbit at.
    [[nodiscard]] const Time& epoch() const { return epoch_; }

    /// The mode SGP4 propagates the element set with.
    [[nodiscard]] Sgp4Mode mode() const { return mode_; }

    [[nodiscard]] std::optional<PositionState> evaluate(const Time& time) const;

    [[nodiscard]] std::string error_message_at(const Time& time) const;

  private:
    SatRec() = default;

    static SatRec
    from_lines_(std::string name, std::string_view line1, std::string_view line2, Sgp4Mode mode);

    static SatRec from_omm_(const OmmFields& fields, Sgp4Mode mode);

    /// Initializes an element set whose elements have been read into the record, and checks it.
    static SatRec from_record_(detail::vallado::elsetrec& record,
                               int catalog_number,
                               std::string name,
                               std::string international_designator,
                               Sgp4Mode mode,
                               const std::string& context);

    /// SGP4's state at a time, in kilometers and kilometers per second: 0 if it has one, or else
    /// its error code (see error_message_at()), or -1 if the state it gave is not finite.
    int propagate_(const Time& time, double (&r)[3], double (&v)[3]) const;

    int catalog_number_ = 0;
    std::string name_;
    std::string international_designator_;
    Time epoch_;
    Sgp4Mode mode_ = Sgp4Mode::AFSPC;

    // Vallado's elsetrec, as SGP4 initialized it. It is in an unnamed namespace (see sgp4.ipp), so
    // a class in a header can't hold one as a member: GCC warns that it then has a different type
    // in every source file (-Wsubobject-linkage). It is trivially copyable, so it is kept as its
    // bytes, which are copied into a local elsetrec to propagate it.
    alignas(detail::vallado::elsetrec)
        std::array<std::byte, sizeof(detail::vallado::elsetrec)> record_{};
};

} // namespace huira

#include "huira_impl/ephemeris/satrec.ipp"
