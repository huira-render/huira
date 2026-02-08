#pragma once

#include <fitsio.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace huira {

    // =====================================================================
    //  Internal helpers
    // =====================================================================
    namespace detail {

        // ---------------- CFITSIO error → exception ----------------------

        inline void fits_check(int status, const std::string& context)
        {
            if (status == 0) return;

            char err_text[FLEN_STATUS];
            fits_get_errstatus(status, err_text);

            HUIRA_THROW_ERROR(
                "fits_check - FITS I/O error (" + context + "): " + std::string(err_text)
                + " [status " + std::to_string(status) + "]"
            );
        }

        // RAII wrapper so the fitsfile* is always closed on scope exit.
        struct FitsFile {
            fitsfile* fptr = nullptr;

            ~FitsFile() {
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

        inline void write_string_key(fitsfile* fptr, const char* key,
                                     const std::string& value, const char* comment,
                                     int& status)
        {
            if (value.empty()) return;
            fits_update_key_str(fptr, key, value.c_str(), comment, &status);
        }

        inline void write_double_key(fitsfile* fptr, const char* key,
                                     double value, const char* comment,
                                     int& status)
        {
            fits_update_key_dbl(fptr, key, value, 10, comment, &status);
        }

        inline void write_float_key(fitsfile* fptr, const char* key,
                                    float value, const char* comment,
                                    int& status)
        {
            fits_update_key_flt(fptr, key, value, 6, comment, &status);
        }

        inline void write_int_key(fitsfile* fptr, const char* key,
                                  int value, const char* comment,
                                  int& status)
        {
            long lval = value;
            fits_update_key_lng(fptr, key, lval, comment, &status);
        }

        inline void write_bool_key(fitsfile* fptr, const char* key,
                                   bool value, const char* comment,
                                   int& status)
        {
            int ival = value ? 1 : 0;
            fits_update_key_log(fptr, key, ival, comment, &status);
        }

        inline void write_custom_keyword(fitsfile* fptr, const FitsKeyword& kw,
                                         int& status)
        {
            const char* comment = kw.comment.empty() ? nullptr : kw.comment.c_str();

            std::visit([&](auto&& val) {
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
            }, kw.value);
        }

        // Serialize the entire FitsMetadata into FITS header keywords.
        inline void write_fits_metadata(fitsfile* fptr, const FitsMetadata& m,
                                        int& status)
        {
            // -- Observation --
            write_string_key(fptr, "OBJECT",   m.object,    "Target name",               status);
            write_string_key(fptr, "TELESCOP", m.telescop,  "Telescope / system",        status);
            write_string_key(fptr, "INSTRUME", m.instrume,  "Instrument",                status);
            write_string_key(fptr, "OBSERVER", m.observer,  "Observer / author",         status);
            write_string_key(fptr, "DATE-OBS", m.date_obs,  "Observation date (ISO)",    status);
            write_string_key(fptr, "ORIGIN",   m.origin,    "File origin / software",    status);

            // -- Exposure / photometric --
            if (m.exptime > 0.f)
                write_float_key(fptr, "EXPTIME", m.exptime, "[s] Exposure time",         status);

            write_string_key(fptr, "FILTER", m.filter, "Filter name",                    status);
            write_string_key(fptr, "BUNIT",  m.bunit,  "Physical unit of pixel values",  status);

            // -- Data range --
            if (m.datamin)  write_double_key(fptr, "DATAMIN",  *m.datamin,  "Minimum pixel value",           status);
            if (m.datamax)  write_double_key(fptr, "DATAMAX",  *m.datamax,  "Maximum pixel value",           status);
            if (m.saturate) write_double_key(fptr, "SATURATE", *m.saturate, "[adu] Detector saturation level", status);

            // -- WCS --
            if (m.has_wcs()) {
                if (m.crpix1) write_double_key(fptr, "CRPIX1", *m.crpix1, "[pixel] Reference pixel X", status);
                if (m.crpix2) write_double_key(fptr, "CRPIX2", *m.crpix2, "[pixel] Reference pixel Y", status);
                if (m.crval1) write_double_key(fptr, "CRVAL1", *m.crval1, "[deg] RA  at ref pixel",    status);
                if (m.crval2) write_double_key(fptr, "CRVAL2", *m.crval2, "[deg] Dec at ref pixel",    status);
                if (m.cdelt1) write_double_key(fptr, "CDELT1", *m.cdelt1, "[deg/pixel] Plate scale X", status);
                if (m.cdelt2) write_double_key(fptr, "CDELT2", *m.cdelt2, "[deg/pixel] Plate scale Y", status);

                write_string_key(fptr, "CTYPE1", m.ctype1, "Coordinate type axis 1",     status);
                write_string_key(fptr, "CTYPE2", m.ctype2, "Coordinate type axis 2",     status);

                write_double_key(fptr, "EQUINOX", m.equinox, "Equinox of coordinates",   status);
                write_string_key(fptr, "RADESYS", m.radesys, "Reference frame",          status);
            }

            // -- COMMENT / HISTORY --
            for (const auto& c : m.comments)
                fits_write_comment(fptr, c.c_str(), &status);

            for (const auto& h : m.history)
                fits_write_history(fptr, h.c_str(), &status);

            // -- Custom keywords --
            for (const auto& kw : m.custom_keywords)
                write_custom_keyword(fptr, kw, status);
        }

        // -------------- Read helpers -------------------------------------

        inline std::string read_string_key(fitsfile* fptr, const char* key)
        {
            char value[FLEN_VALUE] = {};
            int status = 0;
            fits_read_key_str(fptr, key, value, nullptr, &status);
            if (status == KEY_NO_EXIST) return {};
            fits_check(status, std::string("read key ") + key);
            return std::string(value);
        }

        inline std::optional<double> read_double_key(fitsfile* fptr, const char* key)
        {
            double value = 0.0;
            int status = 0;
            fits_read_key_dbl(fptr, key, &value, nullptr, &status);
            if (status == KEY_NO_EXIST) return std::nullopt;
            fits_check(status, std::string("read key ") + key);
            return value;
        }

        inline float read_float_key(fitsfile* fptr, const char* key)
        {
            float value = 0.f;
            int status = 0;
            fits_read_key_flt(fptr, key, &value, nullptr, &status);
            if (status == KEY_NO_EXIST) return 0.f;
            fits_check(status, std::string("read key ") + key);
            return value;
        }

        inline int read_int_key(fitsfile* fptr, const char* key)
        {
            long value = 0;
            int status = 0;
            fits_read_key_lng(fptr, key, &value, nullptr, &status);
            if (status == KEY_NO_EXIST) return 0;
            fits_check(status, std::string("read key ") + key);
            return static_cast<int>(value);
        }

        // Populate FitsMetadata from the current HDU header.
        inline FitsMetadata read_fits_metadata(fitsfile* fptr)
        {
            FitsMetadata m;

            // -- Observation --
            m.object   = read_string_key(fptr, "OBJECT");
            m.telescop = read_string_key(fptr, "TELESCOP");
            m.instrume = read_string_key(fptr, "INSTRUME");
            m.observer = read_string_key(fptr, "OBSERVER");
            m.date_obs = read_string_key(fptr, "DATE-OBS");
            m.origin   = read_string_key(fptr, "ORIGIN");

            // -- Exposure / photometric --
            m.exptime = read_float_key(fptr, "EXPTIME");
            m.filter  = read_string_key(fptr, "FILTER");
            m.bunit   = read_string_key(fptr, "BUNIT");

            // -- Data range --
            m.datamin  = read_double_key(fptr, "DATAMIN");
            m.datamax  = read_double_key(fptr, "DATAMAX");
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
            if (eq) m.equinox = *eq;

            std::string rs = read_string_key(fptr, "RADESYS");
            if (!rs.empty()) m.radesys = rs;

            // -- COMMENT, HISTORY, and unknown keywords --
            {
                int num_keys = 0;
                int status   = 0;
                fits_get_hdrspace(fptr, &num_keys, nullptr, &status);
                fits_check(status, "get header space");

                static const char* known[] = {
                    "SIMPLE", "BITPIX", "NAXIS", "NAXIS1", "NAXIS2", "NAXIS3",
                    "EXTEND", "BZERO", "BSCALE", "END",
                    "OBJECT", "TELESCOP", "INSTRUME", "OBSERVER", "DATE-OBS", "ORIGIN",
                    "EXPTIME", "FILTER", "BUNIT",
                    "DATAMIN", "DATAMAX", "SATURATE",
                    "CRPIX1", "CRPIX2", "CRVAL1", "CRVAL2",
                    "CDELT1", "CDELT2", "CTYPE1", "CTYPE2",
                    "EQUINOX", "RADESYS",
                    nullptr
                };

                auto is_known = [&](const char* key) -> bool {
                    for (const char** k = known; *k; ++k)
                        if (std::strcmp(*k, key) == 0) return true;
                    return false;
                };

                for (int i = 1; i <= num_keys; ++i) {
                    char card[FLEN_CARD] = {};
                    status = 0;
                    fits_read_record(fptr, i, card, &status);
                    if (status) continue;

                    char keyname[FLEN_KEYWORD] = {};
                    char value_str[FLEN_VALUE] = {};
                    char comment_str[FLEN_COMMENT] = {};
                    int  key_length = 0;

                    status = 0;
                    fits_get_keyname(card, keyname, &key_length, &status);
                    if (status) continue;

                    std::string kn(keyname);

                    if (kn == "COMMENT") {
                        if (std::strlen(card) > 8)
                            m.comments.emplace_back(card + 8);
                        continue;
                    }
                    if (kn == "HISTORY") {
                        if (std::strlen(card) > 8)
                            m.history.emplace_back(card + 8);
                        continue;
                    }

                    if (kn.empty() || is_known(kn.c_str())) continue;

                    // Unknown keyword → custom_keywords with type detection.
                    status = 0;
                    char dtype = 0;
                    fits_parse_value(card, value_str, comment_str, &status);
                    if (status) continue;

                    status = 0;
                    fits_get_keytype(value_str, &dtype, &status);
                    if (status) { status = 0; continue; }

                    FitsKeyword kw;
                    kw.key     = kn;
                    kw.comment = comment_str;

                    switch (dtype) {
                        case 'I': {
                            long lv = 0;
                            status = 0;
                            fits_read_key_lng(fptr, kn.c_str(), &lv, nullptr, &status);
                            if (status == 0) kw.value = static_cast<int>(lv);
                            break;
                        }
                        case 'F': {
                            double dv = 0.0;
                            status = 0;
                            fits_read_key_dbl(fptr, kn.c_str(), &dv, nullptr, &status);
                            if (status == 0) kw.value = dv;
                            break;
                        }
                        case 'L': {
                            int bv = 0;
                            status = 0;
                            fits_read_key_log(fptr, kn.c_str(), &bv, nullptr, &status);
                            if (status == 0) kw.value = (bv != 0);
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

        // Maximum ADU value for the full range of a given BITPIX.
        inline double bitpix_max(int bitpix)
        {
            switch (bitpix) {
                case  8:  return 255.0;
                case 16:  return 65535.0;
                case 32:  return 4294967295.0;
                default:  return 1.0;
            }
        }

    } // namespace detail


    // =====================================================================
    //  read_image_fits
    // =====================================================================
    inline std::pair<Image<float>, FitsMetadata>
    read_image_fits(const fs::path& filepath)
    {
        detail::FitsFile ff;
        int status = 0;

        fits_open_file(&ff.fptr, filepath.string().c_str(), READONLY, &status);
        detail::fits_check(status, "open " + filepath.string());

        // --- Image dimensions & type ---
        int bitpix = 0;
        int naxis  = 0;
        long naxes[3] = {0, 0, 0};

        fits_get_img_param(ff.fptr, 3, &bitpix, &naxis, naxes, &status);
        detail::fits_check(status, "get image params");

        if (naxis < 2) {
            throw std::runtime_error(
                "FITS file has NAXIS=" + std::to_string(naxis)
                + "; expected at least 2 for an image."
            );
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
            throw std::runtime_error(
                "FITS file is a data cube (NAXIS3=" + std::to_string(naxes[2])
                + ").  Use read_image_fits_cube() for multi-band images."
            );
        }

        const int width  = static_cast<int>(naxes[0]);
        const int height = static_cast<int>(naxes[1]);

        // --- Read metadata ---
        FitsMetadata metadata = detail::read_fits_metadata(ff.fptr);

        // --- Read pixel data ---
        // CFITSIO converts any on-disk type to TFLOAT and applies BZERO/BSCALE.
        const long npixels = static_cast<long>(width) * height;
        std::vector<float> buffer(static_cast<std::size_t>(npixels));

        long fpixel[2] = {1, 1};
        int anynul = 0;

        fits_read_pix(ff.fptr, TFLOAT, fpixel, npixels,
                      nullptr, buffer.data(), &anynul, &status);
        detail::fits_check(status, "read pixels");

        // --- Normalise integer data to [0, 1] ---
        //
        // After CFITSIO applies BZERO/BSCALE, the buffer contains ADU values
        // (e.g. 0–4095 for a 12-bit sensor stored in 16-bit FITS).
        //
        // We normalise by SATURATE if present (the true ADC ceiling),
        // otherwise by the full BITPIX range.
        const bool is_integer_bitpix = (bitpix > 0);
        int inferred_sensor_bits = 0;

        if (is_integer_bitpix) {
            double divisor;
            if (metadata.saturate && *metadata.saturate > 0.0) {
                divisor = *metadata.saturate;
                // Infer sensor bit depth from SATURATE  (e.g. 4095 → 12)
                inferred_sensor_bits = static_cast<int>(
                    std::round(std::log2(divisor + 1.0))
                );
            } else {
                divisor = detail::bitpix_max(bitpix);
            }

            const float inv = 1.0f / static_cast<float>(divisor);
            for (auto& px : buffer)
                px *= inv;
        }
        // Float BITPIX (−32, −64): pass through as-is.

        // --- Build Image (flip from FITS bottom-up to our top-down) ---
        Image<float> image(width, height);

        // If we inferred a sensor bit depth, store it on the image.
        if (inferred_sensor_bits > 0) {
            image.set_sensor_bit_depth(inferred_sensor_bits);
        }

        for (int y = 0; y < height; ++y) {
            const int fits_row = height - 1 - y;
            const float* src = buffer.data() + static_cast<std::size_t>(fits_row * width);
            for (int x = 0; x < width; ++x) {
                image(x, y) = src[x];
            }
        }

        return {std::move(image), std::move(metadata)};
    }


    // =====================================================================
    //  write_image_fits
    // =====================================================================
    inline void write_image_fits(
        const fs::path&      filepath,
        const Image<float>&  image,
        int                  bit_depth,
        const FitsMetadata&  metadata)
    {
        if (image.empty()) {
            HUIRA_THROW_ERROR("write_image_fits - Cannot write an empty image to FITS.");
        }

        switch (bit_depth) {
            case 8: case 16: case 32: case -32: case -64: break;
            default:
                HUIRA_THROW_ERROR(
                    "write_image_fits - Invalid FITS bit_depth: " + std::to_string(bit_depth)
                    + ".  Must be 8, 16, 32, -32, or -64."
                );
        }

        make_path(filepath);

        detail::FitsFile ff;
        int status = 0;

        const std::string cfitsio_path = "!" + filepath.string();
        fits_create_file(&ff.fptr, cfitsio_path.c_str(), &status);
        detail::fits_check(status, "create " + filepath.string());

        // --- Create image HDU ---
        const int w = image.width();
        const int h = image.height();
        long naxes[2] = { w, h };

        // TODO (multi-band):
        //   long naxes[3] = { w, h, num_bands };
        //   fits_create_img(fptr, bit_depth, 3, naxes, &status);
        //   Then write each band plane separately:
        //     for (int b = 0; b < num_bands; ++b) {
        //         long fpixel[3] = {1, 1, b + 1};
        //         fits_write_pix(fptr, TFLOAT, fpixel, w*h, plane_data, &status);
        //     }

        fits_create_img(ff.fptr, bit_depth, 2, naxes, &status);
        detail::fits_check(status, "create image HDU");

        // --- Determine ADU scaling for integer BITPIX ---
        const bool is_integer = (bit_depth > 0);
        const int sensor_bits = image.sensor_bit_depth();

        // The ADC ceiling: how many counts "1.0" maps to.
        //   • If sensor_bit_depth is set → (2^bits − 1), e.g. 4095 for 12-bit
        //   • Otherwise → full BITPIX range, e.g. 65535 for BITPIX=16
        double adc_max = 0.0;
        if (is_integer) {
            adc_max = (sensor_bits > 0)
                ? (std::pow(2.0, sensor_bits) - 1.0)
                : detail::bitpix_max(bit_depth);
        }

        // --- Write metadata ---
        FitsMetadata meta_copy = metadata;

        if (is_integer) {
            // Default BUNIT to "adu" for integer images.
            if (meta_copy.bunit.empty())
                meta_copy.bunit = "adu";

            // SATURATE = the ADC ceiling (so readers know the dynamic range).
            if (!meta_copy.saturate)
                meta_copy.saturate = adc_max;
        }

        detail::write_fits_metadata(ff.fptr, meta_copy, status);
        detail::fits_check(status, "write metadata");

        // --- Prepare pixel buffer (flip to FITS bottom-up order) ---
        const long npixels = static_cast<long>(w) * h;
        std::vector<float> buffer(static_cast<std::size_t>(npixels));

        float actual_min =  std::numeric_limits<float>::max();
        float actual_max = -std::numeric_limits<float>::max();

        for (int y = 0; y < h; ++y) {
            const int fits_row = h - 1 - y;
            float* dst = buffer.data() + static_cast<std::size_t>(fits_row * w);
            for (int x = 0; x < w; ++x) {
                float px = image(x, y);

                if (is_integer) {
                    // Map [0, 1] → [0, adc_max] and clamp to the BITPIX container.
                    px = std::clamp(
                        px * static_cast<float>(adc_max),
                        0.f,
                        static_cast<float>(detail::bitpix_max(bit_depth))
                    );
                }

                dst[x] = px;
                actual_min = std::min(actual_min, px);
                actual_max = std::max(actual_max, px);
            }
        }

        // --- Write DATAMIN / DATAMAX (actual values in the file) ---
        {
            if (!meta_copy.datamin)
                detail::write_double_key(ff.fptr, "DATAMIN", actual_min,
                                         "Minimum pixel value", status);
            if (!meta_copy.datamax)
                detail::write_double_key(ff.fptr, "DATAMAX", actual_max,
                                         "Maximum pixel value", status);
            detail::fits_check(status, "write DATAMIN/DATAMAX");
        }

        // --- Write pixels ---
        // CFITSIO converts TFLOAT → on-disk BITPIX, quantising as needed.
        long fpixel[2] = {1, 1};
        fits_write_pix(ff.fptr, TFLOAT, fpixel, npixels,
                       buffer.data(), &status);
        detail::fits_check(status, "write pixels");
    }

}
