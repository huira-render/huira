#pragma once

#include <string>
#include <variant>
#include <vector>
#include <optional>

namespace huira {

    // -------------------------------------------------------------------------
    // A single custom FITS header keyword.
    //
    // FITS keywords are up to 8 ASCII characters (uppercase letters, digits,
    // hyphen, underscore).  The value can be a string (up to 68 chars),
    // a double, an integer, or a boolean.
    // -------------------------------------------------------------------------
    struct FitsKeyword {
        std::string key;        // max 8 characters, uppercase
        std::variant<std::string, double, int, bool> value;
        std::string comment;    // optional annotation (max ~47 chars)
    };

    // -------------------------------------------------------------------------
    // Common FITS metadata for astronomical images.
    //
    // All fields are optional (empty strings / zero values / nullopt are
    // simply not written to the FITS header).  The struct covers:
    //
    //   1. Observation description  (OBJECT, TELESCOP, INSTRUME, …)
    //   2. Exposure / photometric   (EXPTIME, FILTER, BUNIT, …)
    //   3. Data range               (DATAMIN, DATAMAX, SATURATE)
    //   4. Basic tangent-plane WCS  (CRPIXn, CRVALn, CDELTn, CTYPEn, …)
    //   5. Free-form text           (COMMENT, HISTORY)
    //   6. Arbitrary user keywords  (custom_keywords)
    //
    // Note: sensor bit depth is NOT here — it is a property of the Image
    // itself (Image::sensor_bit_depth()), since it describes how the pixel
    // data was quantised, not metadata about the observation.
    // -------------------------------------------------------------------------
    struct FitsMetadata {

        // -- Observation --------------------------------------------------

        std::string object;         // OBJECT   – target name (e.g. "M31")
        std::string telescop;       // TELESCOP – telescope / system name
        std::string instrume;       // INSTRUME – instrument name
        std::string observer;       // OBSERVER – observer / author
        std::string date_obs;       // DATE-OBS – ISO 8601  YYYY-MM-DDThh:mm:ss[.sss]
        std::string origin;         // ORIGIN   – organisation / software that created the file

        // -- Exposure / photometric ---------------------------------------

        float  exptime        = 0.f;    // EXPTIME  – exposure time in seconds
        std::string filter;              // FILTER   – filter name (e.g. "V", "Ha")

        /// Physical unit of the pixel values (e.g. "adu", "W/m2/sr/nm").
        /// For integer-BITPIX files this will typically be "adu".
        std::string bunit;               // BUNIT

        // -- Data range ---------------------------------------------------
        //
        // These are standard FITS keywords written by real instruments.
        // DATAMIN / DATAMAX are the actual min/max pixel values in the file
        // (in stored units, i.e. ADU for integer BITPIX).
        // SATURATE is the saturation level of the detector (e.g. 4095 for
        // a 12-bit ADC).  The FITS writer populates these automatically.

        std::optional<double> datamin;   // DATAMIN  – minimum pixel value
        std::optional<double> datamax;   // DATAMAX  – maximum pixel value
        std::optional<double> saturate;  // SATURATE – detector saturation level

        // -- WCS (basic tangent-plane projection) -------------------------
        //
        // Sufficient for a simple pinhole-camera model:
        //   CRPIX  = reference pixel  (typically image center)
        //   CRVAL  = RA / Dec at that pixel  (degrees)
        //   CDELT  = plate scale  (degrees / pixel;  CDELT1 usually negative)
        //   CTYPE  = projection   ("RA---TAN", "DEC--TAN")
        //   EQUINOX / RADESYS  = coordinate frame
        //
        // For more complex WCS (SIP distortion, CD matrix, etc.) use
        // custom_keywords.

        std::optional<double> crpix1;   // CRPIX1
        std::optional<double> crpix2;   // CRPIX2
        std::optional<double> crval1;   // CRVAL1  – RA  (degrees)
        std::optional<double> crval2;   // CRVAL2  – Dec (degrees)
        std::optional<double> cdelt1;   // CDELT1  – deg/pixel (typically < 0)
        std::optional<double> cdelt2;   // CDELT2  – deg/pixel
        std::string ctype1;             // CTYPE1  – e.g. "RA---TAN"
        std::string ctype2;             // CTYPE2  – e.g. "DEC--TAN"
        double equinox       = 2000.0;  // EQUINOX
        std::string radesys  = "ICRS";  // RADESYS

        // -- Free-form text -----------------------------------------------

        std::vector<std::string> comments;  // COMMENT  (one entry per card)
        std::vector<std::string> history;   // HISTORY  (one entry per card)

        // -- Arbitrary user keywords --------------------------------------

        std::vector<FitsKeyword> custom_keywords;

        // -----------------------------------------------------------------
        // Helpers
        // -----------------------------------------------------------------

        /// Returns true if any WCS field has been set.
        [[nodiscard]] bool has_wcs() const noexcept
        {
            return crpix1.has_value()
                || crval1.has_value()
                || cdelt1.has_value()
                || !ctype1.empty();
        }

        // TODO (multi-band):
        //   When writing multi-band FITS cubes (NAXIS3 > 1), each plane
        //   may correspond to a different filter or wavelength.  Add:
        //
        //     std::vector<std::string> band_names;       // one per plane
        //     std::vector<double>      band_wavelengths;  // central λ (nm)
        //
        //   These would be stored as indexed keywords (e.g. BAND1, BAND2, …)
        //   or in a FITS binary-table extension.
    };

} // namespace huira
