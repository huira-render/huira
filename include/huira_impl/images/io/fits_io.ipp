#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include <fitsio.h>

#include "huira/util/logger.hpp"
#include "huira/util/paths.hpp"

namespace huira {

// =====================================================================
//  Internal helpers
// =====================================================================
namespace detail {

// ---------------- CFITSIO error → exception ----------------------

inline void fits_check(int status, const std::string& context)
{
    if (status == 0) {
        return;
    }

    char err_text[FLEN_STATUS];
    fits_get_errstatus(status, err_text);

    HUIRA_THROW_ERROR("fits_check - FITS I/O error (" + context + "): " + std::string(err_text) +
                      " [status " + std::to_string(status) + "]");
}

// RAII wrapper so the fitsfile* is always closed on scope exit.
struct FitsFile {
    fitsfile* fptr = nullptr;

    ~FitsFile()
    {
        if (fptr) {
            int status = 0;
            fits_close_file(fptr, &status);
        }
    }

    FitsFile() = default;
    FitsFile(const FitsFile&) = delete;
    FitsFile& operator=(const FitsFile&) = delete;
};

// -------------- Write helpers ------------------------------------

inline void write_string_key(
    fitsfile* fptr, const char* key, const std::string& value, const char* comment, int& status)
{
    if (value.empty()) {
        return;
    }
    fits_update_key_str(fptr, key, value.c_str(), comment, &status);
}

inline void
write_double_key(fitsfile* fptr, const char* key, double value, const char* comment, int& status)
{
    fits_update_key_dbl(fptr, key, value, 10, comment, &status);
}

inline void
write_float_key(fitsfile* fptr, const char* key, float value, const char* comment, int& status)
{
    fits_update_key_flt(fptr, key, value, 6, comment, &status);
}

inline void
write_int_key(fitsfile* fptr, const char* key, int value, const char* comment, int& status)
{
    long lval = value;
    fits_update_key_lng(fptr, key, lval, comment, &status);
}

inline void
write_bool_key(fitsfile* fptr, const char* key, bool value, const char* comment, int& status)
{
    int ival = value ? 1 : 0;
    fits_update_key_log(fptr, key, ival, comment, &status);
}

inline void write_custom_keyword(fitsfile* fptr, const FitsKeyword& kw, int& status)
{
    const char* comment = kw.comment.empty() ? nullptr : kw.comment.c_str();

    std::visit(
        [&](auto&& val) {
            using V = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<V, std::string>) {
                write_string_key(fptr, kw.key.c_str(), val, comment, status);
            } else if constexpr (std::is_same_v<V, double>) {
                write_double_key(fptr, kw.key.c_str(), val, comment, status);
            } else if constexpr (std::is_same_v<V, int>) {
                write_int_key(fptr, kw.key.c_str(), val, comment, status);
            } else if constexpr (std::is_same_v<V, bool>) {
                write_bool_key(fptr, kw.key.c_str(), val, comment, status);
            }
        },
        kw.value);
}

// Keywords that describe how a file's data are stored, or the data themselves, which
// write_image_fits() sets from the image it writes: never copied from metadata, which may have
// been read from another file.
inline bool is_writer_owned_keyword(const std::string& key)
{
    static constexpr const char* const owned[] = {"SIMPLE",
                                                  "BITPIX",
                                                  "NAXIS",
                                                  "NAXIS1",
                                                  "NAXIS2",
                                                  "NAXIS3",
                                                  "EXTEND",
                                                  "BZERO",
                                                  "BSCALE",
                                                  "BLANK",
                                                  "DATAMIN",
                                                  "DATAMAX",
                                                  "SATURATE",
                                                  "ADCBITS",
                                                  "END"};
    for (const char* k : owned) {
        if (key == k) {
            return true;
        }
    }
    return false;
}

// Serialize the entire FitsMetadata into FITS header keywords.
inline void write_fits_metadata(fitsfile* fptr, const FitsMetadata& m, int& status)
{
    // -- Observation --
    write_string_key(fptr, "OBJECT", m.object, "Target name", status);
    write_string_key(fptr, "TELESCOP", m.telescop, "Telescope / system", status);
    write_string_key(fptr, "INSTRUME", m.instrume, "Instrument", status);
    write_string_key(fptr, "OBSERVER", m.observer, "Observer / author", status);
    write_string_key(fptr, "DATE-OBS", m.date_obs, "Observation date (ISO)", status);
    write_string_key(fptr, "ORIGIN", m.origin, "File origin / software", status);

    // -- Exposure / photometric --
    if (m.exptime > 0.f) {
        write_float_key(fptr, "EXPTIME", m.exptime, "[s] Exposure time", status);
    }

    write_string_key(fptr, "FILTER", m.filter, "Filter name", status);
    write_string_key(fptr, "BUNIT", m.bunit, "Physical unit of pixel values", status);

    // -- Data range --
    if (m.datamin) {
        write_double_key(fptr, "DATAMIN", *m.datamin, "Minimum pixel value", status);
    }
    if (m.datamax) {
        write_double_key(fptr, "DATAMAX", *m.datamax, "Maximum pixel value", status);
    }
    if (m.saturate) {
        write_double_key(fptr, "SATURATE", *m.saturate, "[adu] Detector saturation level", status);
    }

    // -- WCS --
    if (m.has_wcs()) {
        if (m.crpix1) {
            write_double_key(fptr, "CRPIX1", *m.crpix1, "[pixel] Reference pixel X", status);
        }
        if (m.crpix2) {
            write_double_key(fptr, "CRPIX2", *m.crpix2, "[pixel] Reference pixel Y", status);
        }
        if (m.crval1) {
            write_double_key(fptr, "CRVAL1", *m.crval1, "[deg] RA  at ref pixel", status);
        }
        if (m.crval2) {
            write_double_key(fptr, "CRVAL2", *m.crval2, "[deg] Dec at ref pixel", status);
        }
        if (m.cdelt1) {
            write_double_key(fptr, "CDELT1", *m.cdelt1, "[deg/pixel] Plate scale X", status);
        }
        if (m.cdelt2) {
            write_double_key(fptr, "CDELT2", *m.cdelt2, "[deg/pixel] Plate scale Y", status);
        }

        write_string_key(fptr, "CTYPE1", m.ctype1, "Coordinate type axis 1", status);
        write_string_key(fptr, "CTYPE2", m.ctype2, "Coordinate type axis 2", status);

        write_double_key(fptr, "EQUINOX", m.equinox, "Equinox of coordinates", status);
        write_string_key(fptr, "RADESYS", m.radesys, "Reference frame", status);
    }

    // -- COMMENT / HISTORY --
    for (const auto& c : m.comments) {
        fits_write_comment(fptr, c.c_str(), &status);
    }

    for (const auto& h : m.history) {
        fits_write_history(fptr, h.c_str(), &status);
    }

    // -- Custom keywords --
    for (const auto& kw : m.custom_keywords) {
        if (is_writer_owned_keyword(kw.key)) {
            HUIRA_LOG_DEBUG("write_image_fits - Custom keyword " + kw.key +
                            " ignored: the writer sets it from the image it writes.");
            continue;
        }
        write_custom_keyword(fptr, kw, status);
    }
}

// -------------- Read helpers -------------------------------------

inline std::string read_string_key(fitsfile* fptr, const char* key)
{
    char value[FLEN_VALUE] = {};
    int status = 0;
    fits_read_key_str(fptr, key, value, nullptr, &status);
    if (status == KEY_NO_EXIST) {
        return {};
    }
    fits_check(status, std::string("read key ") + key);
    return std::string(value);
}

inline std::optional<double> read_double_key(fitsfile* fptr, const char* key)
{
    double value = 0.0;
    int status = 0;
    fits_read_key_dbl(fptr, key, &value, nullptr, &status);
    if (status == KEY_NO_EXIST) {
        return std::nullopt;
    }
    fits_check(status, std::string("read key ") + key);
    return value;
}

inline float read_float_key(fitsfile* fptr, const char* key)
{
    float value = 0.f;
    int status = 0;
    fits_read_key_flt(fptr, key, &value, nullptr, &status);
    if (status == KEY_NO_EXIST) {
        return 0.f;
    }
    fits_check(status, std::string("read key ") + key);
    return value;
}

inline int read_int_key(fitsfile* fptr, const char* key)
{
    long value = 0;
    int status = 0;
    fits_read_key_lng(fptr, key, &value, nullptr, &status);
    if (status == KEY_NO_EXIST) {
        return 0;
    }
    fits_check(status, std::string("read key ") + key);
    return static_cast<int>(value);
}

// Populate FitsMetadata from the current HDU header.
inline FitsMetadata read_fits_metadata(fitsfile* fptr)
{
    FitsMetadata m;

    // -- Observation --
    m.object = read_string_key(fptr, "OBJECT");
    m.telescop = read_string_key(fptr, "TELESCOP");
    m.instrume = read_string_key(fptr, "INSTRUME");
    m.observer = read_string_key(fptr, "OBSERVER");
    m.date_obs = read_string_key(fptr, "DATE-OBS");
    m.origin = read_string_key(fptr, "ORIGIN");

    // -- Exposure / photometric --
    m.exptime = read_float_key(fptr, "EXPTIME");
    m.filter = read_string_key(fptr, "FILTER");
    m.bunit = read_string_key(fptr, "BUNIT");

    // -- Data range --
    m.datamin = read_double_key(fptr, "DATAMIN");
    m.datamax = read_double_key(fptr, "DATAMAX");
    m.saturate = read_double_key(fptr, "SATURATE");

    // -- WCS --
    m.crpix1 = read_double_key(fptr, "CRPIX1");
    m.crpix2 = read_double_key(fptr, "CRPIX2");
    m.crval1 = read_double_key(fptr, "CRVAL1");
    m.crval2 = read_double_key(fptr, "CRVAL2");
    m.cdelt1 = read_double_key(fptr, "CDELT1");
    m.cdelt2 = read_double_key(fptr, "CDELT2");
    m.ctype1 = read_string_key(fptr, "CTYPE1");
    m.ctype2 = read_string_key(fptr, "CTYPE2");

    auto eq = read_double_key(fptr, "EQUINOX");
    if (eq) {
        m.equinox = *eq;
    }

    std::string rs = read_string_key(fptr, "RADESYS");
    if (!rs.empty()) {
        m.radesys = rs;
    }

    // -- COMMENT, HISTORY, and unknown keywords --
    {
        int num_keys = 0;
        int status = 0;
        fits_get_hdrspace(fptr, &num_keys, nullptr, &status);
        fits_check(status, "get header space");

        static constexpr const char* const known[] = {
            "SIMPLE",   "BITPIX", "NAXIS",   "NAXIS1",  "NAXIS2",   "NAXIS3",   "EXTEND",
            "BZERO",    "BSCALE", "END",     "OBJECT",  "TELESCOP", "INSTRUME", "OBSERVER",
            "DATE-OBS", "ORIGIN", "EXPTIME", "FILTER",  "BUNIT",    "DATAMIN",  "DATAMAX",
            "SATURATE", "CRPIX1", "CRPIX2",  "CRVAL1",  "CRVAL2",   "CDELT1",   "CDELT2",
            "CTYPE1",   "CTYPE2", "EQUINOX", "RADESYS", "BLANK",    "ADCBITS",  nullptr};

        auto is_known = [&](const char* key) -> bool {
            for (const char* const* k = known; *k; ++k) {
                if (std::strcmp(*k, key) == 0) {
                    return true;
                }
            }
            return false;
        };

        for (int i = 1; i <= num_keys; ++i) {
            char card[FLEN_CARD] = {};
            status = 0;
            fits_read_record(fptr, i, card, &status);
            if (status) {
                continue;
            }

            char keyname[FLEN_KEYWORD] = {};
            char value_str[FLEN_VALUE] = {};
            char comment_str[FLEN_COMMENT] = {};
            int key_length = 0;

            status = 0;
            fits_get_keyname(card, keyname, &key_length, &status);
            if (status) {
                continue;
            }

            std::string kn(keyname);

            if (kn == "COMMENT") {
                if (std::strlen(card) > 8) {
                    m.comments.emplace_back(card + 8);
                }
                continue;
            }
            if (kn == "HISTORY") {
                if (std::strlen(card) > 8) {
                    m.history.emplace_back(card + 8);
                }
                continue;
            }

            if (kn.empty() || is_known(kn.c_str())) {
                continue;
            }

            // Unknown keyword → custom_keywords with type detection.
            status = 0;
            char dtype = 0;
            fits_parse_value(card, value_str, comment_str, &status);
            if (status) {
                continue;
            }

            status = 0;
            fits_get_keytype(value_str, &dtype, &status);
            if (status) {
                status = 0;
                continue;
            }

            FitsKeyword kw;
            kw.key = kn;
            kw.comment = comment_str;

            switch (dtype) {
            case 'I': {
                long lv = 0;
                status = 0;
                fits_read_key_lng(fptr, kn.c_str(), &lv, nullptr, &status);
                if (status == 0) {
                    kw.value = static_cast<int>(lv);
                }
                break;
            }
            case 'F': {
                double dv = 0.0;
                status = 0;
                fits_read_key_dbl(fptr, kn.c_str(), &dv, nullptr, &status);
                if (status == 0) {
                    kw.value = dv;
                }
                break;
            }
            case 'L': {
                int bv = 0;
                status = 0;
                fits_read_key_log(fptr, kn.c_str(), &bv, nullptr, &status);
                if (status == 0) {
                    kw.value = (bv != 0);
                }
                break;
            }
            case 'C':
            default: {
                kw.value = read_string_key(fptr, kn.c_str());
                break;
            }
            }
            status = 0;
            m.custom_keywords.push_back(std::move(kw));
        }
    }

    return m;
}

// The CFITSIO image type written for a bit_depth given to write_image_fits(): integers are
// unsigned (16 and 32 bits are stored signed, offset by BZERO, which CFITSIO writes).
inline int fits_image_type(int bit_depth)
{
    switch (bit_depth) {
    case 8:
        return BYTE_IMG;
    case 16:
        return USHORT_IMG;
    case 32:
        return ULONG_IMG;
    case -64:
        return DOUBLE_IMG;
    case -32:
    default:
        return FLOAT_IMG;
    }
}

// Largest value of an integer image of the given type: a CFITSIO image type, as
// fits_image_type() and fits_get_img_equivtype() give, so that unsigned types stored with BZERO
// are told apart from signed ones.
inline double fits_type_max(int image_type)
{
    switch (image_type) {
    case BYTE_IMG:
        return 255.0;
    case SBYTE_IMG:
        return 127.0;
    case SHORT_IMG:
        return 32767.0;
    case USHORT_IMG:
        return 65535.0;
    case LONG_IMG:
        return 2147483647.0;
    case ULONG_IMG:
        return 4294967295.0;
    case LONGLONG_IMG:
        return 9223372036854775807.0;
    default:
        return 1.0;
    }
}

// BZERO of an unsigned integer image type as CFITSIO stores it (signed, offset by BZERO), with
// BSCALE 1.
inline double fits_unsigned_offset(int image_type)
{
    switch (image_type) {
    case USHORT_IMG:
        return 32768.0;
    case ULONG_IMG:
        return 2147483648.0;
    case BYTE_IMG:
    default:
        return 0.0;
    }
}
} // namespace detail

// =====================================================================
//  read_image_fits
// =====================================================================
inline std::pair<Image<float>, FitsMetadata> read_image_fits(const fs::path& filepath)
{
    detail::FitsFile ff;
    int status = 0;

    fits_open_file(&ff.fptr, filepath.string().c_str(), READONLY, &status);
    detail::fits_check(status, "open " + filepath.string());

    // --- Image dimensions & type ---
    int bitpix = 0;
    int naxis = 0;
    long naxes[3] = {0, 0, 0};

    fits_get_img_param(ff.fptr, 3, &bitpix, &naxis, naxes, &status);
    detail::fits_check(status, "get image params");

    if (naxis < 2) {
        HUIRA_THROW_ERROR("FITS file has NAXIS=" + std::to_string(naxis) +
                          "; expected at least 2 for an image.");
    }

    // TODO (multi-band):
    //   If naxis == 3, naxes[2] gives the number of planes.
    //   read_image_fits_cube() would iterate:
    //
    //     for (long plane = 0; plane < naxes[2]; ++plane) {
    //         long fpixel[3] = {1, 1, plane + 1};
    //         fits_read_pix(fptr, TFLOAT, fpixel, w*h, ...);
    //     }
    if (naxis >= 3 && naxes[2] > 1) {
        HUIRA_THROW_ERROR("FITS file is a data cube (NAXIS3=" + std::to_string(naxes[2]) +
                          ").  Use read_image_fits_cube() for multi-band images.");
    }

    const int width = static_cast<int>(naxes[0]);
    const int height = static_cast<int>(naxes[1]);

    // --- Read metadata ---
    FitsMetadata metadata = detail::read_fits_metadata(ff.fptr);

    // --- Read pixel data ---
    // CFITSIO converts any on-disk type to double, applies BZERO/BSCALE, and returns undefined
    // (BLANK) pixels as NaN. Double, so that 32-bit integers keep every bit until normalized.
    const long npixels = static_cast<long>(width) * height;
    std::vector<double> buffer(static_cast<std::size_t>(npixels));

    long fpixel[2] = {1, 1};
    int anynul = 0;
    double null_value = std::numeric_limits<double>::quiet_NaN();

    fits_read_pix(ff.fptr, TDOUBLE, fpixel, npixels, &null_value, buffer.data(), &anynul, &status);
    detail::fits_check(status, "read pixels");

    // The type the stored values represent once BZERO is applied (unsigned 16-bit for BITPIX 16
    // with BZERO 32768, for example):
    int equivalent_type = bitpix;
    fits_get_img_equivtype(ff.fptr, &equivalent_type, &status);
    detail::fits_check(status, "get image type");

    // --- Normalise to [0, 1] ---
    //
    // After CFITSIO applies BZERO/BSCALE, the buffer holds the physical values: a sensor's DN
    // for an image write_image_fits() wrote from a sensor response (e.g. 0-4095 for a 12-bit
    // sensor, in a 16-bit or a float file). They are normalised as the sensor response held
    // them, by the sensor's largest DN, 2^ADCBITS - 1, whatever the BITPIX.
    //
    // Without ADCBITS, an integer image is normalised by SATURATE if present (the ADC ceiling),
    // otherwise by its type's range, and a float image is returned as it is.
    const bool is_integer_bitpix = (bitpix > 0);
    const int adc_bits = detail::read_int_key(ff.fptr, "ADCBITS");
    int sensor_bits = 0;
    double divisor = 1.0;

    if (adc_bits > 0 && adc_bits <= 53) {
        sensor_bits = adc_bits;
        divisor = std::ldexp(1.0, adc_bits) - 1.0;
    } else if (is_integer_bitpix) {
        if (metadata.saturate && *metadata.saturate > 0.0) {
            divisor = *metadata.saturate;
            // The bit depth, if SATURATE is an ADC's ceiling (e.g. 4095: 12 bits):
            const double bits = std::round(std::log2(divisor + 1.0));
            if (bits >= 1.0 && bits <= 53.0 &&
                std::ldexp(1.0, static_cast<int>(bits)) - 1.0 == divisor) {
                sensor_bits = static_cast<int>(bits);
            }
        } else {
            divisor = detail::fits_type_max(equivalent_type);
        }
    }
    if (divisor != 1.0) {
        for (auto& px : buffer) {
            px /= divisor;
        }
    }

    // --- Build Image (flip from FITS bottom-up to our top-down) ---
    Image<float> image(width, height);

    if (sensor_bits > 0) {
        image.set_sensor_bit_depth(sensor_bits);
    }

    for (int y = 0; y < height; ++y) {
        const int fits_row = height - 1 - y;
        const double* src = buffer.data() + static_cast<std::size_t>(fits_row * width);
        for (int x = 0; x < width; ++x) {
            image(x, y) = static_cast<float>(src[x]);
        }
    }

    return {std::move(image), std::move(metadata)};
}

// =====================================================================
//  write_image_fits
// =====================================================================
inline void write_image_fits(const fs::path& filepath,
                             const Image<float>& image,
                             int bit_depth,
                             const FitsMetadata& metadata,
                             PixelScaling scaling)
{
    if (image.empty()) {
        HUIRA_THROW_ERROR("write_image_fits - Cannot write an empty image to FITS.");
    }

    switch (bit_depth) {
    case 8:
    case 16:
    case 32:
    case -32:
    case -64:
        break;
    default:
        HUIRA_THROW_ERROR("write_image_fits - Invalid FITS bit_depth: " +
                          std::to_string(bit_depth) + ".  Must be 8, 16, 32, -32, or -64.");
    }

    // A sensor response is written as the sensor's digital numbers (DN), unless FullRange:
    const int sensor_bits = (scaling == PixelScaling::Counts) ? image.sensor_bit_depth() : 0;
    const bool counts = sensor_bits > 0;
    if (counts && sensor_bits > 32) {
        HUIRA_THROW_ERROR("write_image_fits - The image's sensor bit depth, " +
                          std::to_string(sensor_bits) +
                          ", is more than the 32 bits FITS can hold as integers.");
    }
    const bool is_integer = (bit_depth > 0);
    const double sensor_max = counts ? std::ldexp(1.0, sensor_bits) - 1.0 : 0.0;

    make_path(filepath);

    detail::FitsFile ff;
    int status = 0;

    const std::string cfitsio_path = "!" + filepath.string();
    fits_create_file(&ff.fptr, cfitsio_path.c_str(), &status);
    detail::fits_check(status, "create " + filepath.string());

    // --- Create image HDU ---
    const int w = image.width();
    const int h = image.height();
    long naxes[2] = {w, h};

    // TODO (multi-band):
    //   long naxes[3] = { w, h, num_bands };
    //   fits_create_img(fptr, bit_depth, 3, naxes, &status);
    //   Then write each band plane separately:
    //     for (int b = 0; b < num_bands; ++b) {
    //         long fpixel[3] = {1, 1, b + 1};
    //         fits_write_pix(fptr, TFLOAT, fpixel, w*h, plane_data, &status);
    //     }

    const int image_type = detail::fits_image_type(bit_depth);
    fits_create_img(ff.fptr, image_type, 2, naxes, &status);
    detail::fits_check(status, "create image HDU");

    // --- How values are stored ---
    //
    // Pixels are given to CFITSIO as physical values, which it stores as
    // (physical - BZERO) / BSCALE. Integer images are unsigned: 16 and 32 bits are stored signed,
    // offset by BZERO, which CFITSIO writes. Physical values are:
    //   - a sensor response (counts): the DN. With fewer bits in the file than the sensor has,
    //     the lowest bits are dropped (the DN rounded down to a multiple of BSCALE =
    //     2^(sensor bits - file bits)), and BSCALE scales the stored values back to DN.
    //   - another image, in an integer file: its values in [0, 1] stretched over the file's
    //     range; in a float file: its values as they are.
    const double file_max = is_integer ? std::ldexp(1.0, bit_depth) - 1.0 : 0.0;
    const int shift = (is_integer && counts) ? std::max(sensor_bits - bit_depth, 0) : 0;
    const double bscale = std::ldexp(1.0, shift);
    const double bzero = detail::fits_unsigned_offset(image_type) * bscale;
    if (shift > 0) {
        double key_value = bscale;
        fits_update_key(ff.fptr,
                        TDOUBLE,
                        "BSCALE",
                        &key_value,
                        "Stored values to DN (low bits dropped)",
                        &status);
        key_value = bzero;
        fits_update_key(ff.fptr, TDOUBLE, "BZERO", &key_value, "Offset of stored values", &status);
        fits_set_bscale(ff.fptr, bscale, bzero, &status);
        detail::fits_check(status, "write BSCALE/BZERO");
    }

    // Undefined (non-finite) pixels: NaN in a float image. An integer image marks them with
    // BLANK: a value the data never take, which the sensor's DN leave spare when they have fewer
    // bits than the file (the file's largest value), and otherwise 0, which valid pixels are then
    // kept above (by one stored step). Without undefined pixels every value is used.
    bool has_blank = false;
    if (is_integer) {
        for (std::size_t i = 0; i < image.size() && !has_blank; ++i) {
            has_blank = !std::isfinite(image[i]);
        }
    }
    const bool spare_code = counts && shift == 0 && sensor_bits < bit_depth;
    const double blank_physical = spare_code ? file_max : 0.0;
    const double lowest_valid = (has_blank && !spare_code) ? bscale : 0.0;

    // The physical value of a finite pixel:
    auto physical = [&](float pixel) -> double {
        const double v = static_cast<double>(pixel);
        if (counts) {
            double dn = std::clamp(std::round(v * sensor_max), 0.0, sensor_max);
            if (shift > 0) {
                dn = std::floor(dn / bscale) * bscale; // the low bits dropped
            }
            return is_integer ? std::max(dn, lowest_valid) : dn;
        }
        if (is_integer) {
            return std::max(std::round(std::clamp(v, 0.0, 1.0) * file_max), lowest_valid);
        }
        return v;
    };

    // --- Prepare pixel buffer (flip to FITS bottom-up order) ---
    // Double, so that 32-bit integers are written exactly.
    const long npixels = static_cast<long>(w) * h;
    std::vector<double> buffer(static_cast<std::size_t>(npixels));

    double data_min = std::numeric_limits<double>::infinity();
    double data_max = -std::numeric_limits<double>::infinity();

    for (int y = 0; y < h; ++y) {
        const int fits_row = h - 1 - y;
        double* dst = buffer.data() + static_cast<std::size_t>(fits_row * w);
        for (int x = 0; x < w; ++x) {
            const float pixel = image(x, y);
            if (!std::isfinite(pixel)) {
                // A float image keeps the value (NaN marks it undefined); an integer image has
                // BLANK.
                dst[x] = is_integer ? blank_physical : static_cast<double>(pixel);
                continue;
            }
            const double value = physical(pixel);
            dst[x] = value;
            data_min = std::min(data_min, value);
            data_max = std::max(data_max, value);
        }
    }

    // --- Write metadata ---
    // The keywords that describe the data are set here, from the image, whatever metadata holds
    // (it may have come from another file): DATAMIN and DATAMAX, SATURATE (except for a float
    // image that is not a sensor response, where it is metadata's), ADCBITS and BLANK.
    FitsMetadata meta_copy = metadata;
    meta_copy.datamin.reset();
    meta_copy.datamax.reset();
    if (counts) {
        meta_copy.saturate = sensor_max; // the sensor's ADC ceiling, in DN
    } else if (is_integer) {
        meta_copy.saturate = file_max;
    }
    if ((counts || is_integer) && meta_copy.bunit.empty()) {
        meta_copy.bunit = "adu";
    }

    detail::write_fits_metadata(ff.fptr, meta_copy, status);
    detail::fits_check(status, "write metadata");

    // DATAMIN / DATAMAX: the range of the defined pixels, as physical values (none if every
    // pixel is undefined).
    if (data_min <= data_max) {
        detail::write_double_key(ff.fptr, "DATAMIN", data_min, "Minimum pixel value", status);
        detail::write_double_key(ff.fptr, "DATAMAX", data_max, "Maximum pixel value", status);
        detail::fits_check(status, "write DATAMIN/DATAMAX");
    }

    if (counts) {
        detail::write_int_key(ff.fptr,
                              "ADCBITS",
                              sensor_bits,
                              "[bit] Sensor ADC bit depth; DN max = 2^ADCBITS-1",
                              status);
        detail::fits_check(status, "write ADCBITS keyword");
    }

    if (is_integer && has_blank) {
        // BLANK is in stored units: (physical - BZERO) / BSCALE.
        LONGLONG blank_stored =
            static_cast<LONGLONG>(std::llround((blank_physical - bzero) / bscale));
        fits_update_key(ff.fptr,
                        TLONGLONG,
                        "BLANK",
                        &blank_stored,
                        "Value representing undefined pixels",
                        &status);
        detail::fits_check(status, "write BLANK keyword");
    }

    // --- Write pixels ---
    // CFITSIO converts to the on-disk type, applying BZERO and BSCALE.
    long fpixel[2] = {1, 1};
    fits_write_pix(ff.fptr, TDOUBLE, fpixel, npixels, buffer.data(), &status);
    detail::fits_check(status, "write pixels");
}

} // namespace huira
