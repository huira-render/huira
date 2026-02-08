#pragma once

#include <filesystem>

#include "huira/images/image.hpp"
#include "huira/images/io/fits_metadata.hpp"

namespace fs = std::filesystem;

namespace huira {

    // -------------------------------------------------------------------------
    // Read a single-plane FITS image into Image<float>.
    //
    //   • Integer BITPIX (8, 16, 32):
    //       CFITSIO applies BZERO/BSCALE automatically.  The raw ADU values
    //       are then normalised to [0, 1] by dividing by the SATURATE keyword
    //       (if present) or the maximum value for that BITPIX type.
    //       The returned Image has its sensor_bit_depth set if SATURATE is
    //       present in the header.
    //
    //   • Float BITPIX (−32, −64):
    //       Values are returned as-is (physical / flux units).
    //       Consult metadata.bunit for the physical unit.
    //
    // The returned FitsMetadata is populated from every recognised header
    // keyword; unrecognised keywords land in custom_keywords.
    // -------------------------------------------------------------------------
    std::pair<Image<float>, FitsMetadata> read_image_fits(const fs::path& filepath);


    // -------------------------------------------------------------------------
    // Write an Image<float> to a FITS file.
    //
    //   bit_depth    FITS BITPIX value controlling the on-disk format:
    //                  8   →  unsigned  8-bit integer
    //                 16   →  unsigned 16-bit integer  (stored as signed + BZERO)
    //                 32   →  unsigned 32-bit integer  (stored as signed + BZERO)
    //                -32   →  IEEE 754  32-bit float   (default)
    //                -64   →  IEEE 754  64-bit double
    //
    //   metadata     Optional FitsMetadata to embed in the header.
    //
    // For integer BITPIX, the image's sensor_bit_depth() determines how
    // floats in [0, 1] are mapped back to ADU counts:
    //
    //   adu = pixel * (2^sensor_bit_depth - 1)
    //
    // If sensor_bit_depth is 0 (not set), the full BITPIX range is used
    // (e.g. 0–65535 for BITPIX=16).  The SATURATE, DATAMIN, and DATAMAX
    // keywords are written automatically.
    //
    // For float BITPIX, pixel values are written verbatim with no scaling.
    // -------------------------------------------------------------------------
    void write_image_fits(
        const fs::path&      filepath,
        const Image<float>&  image,
        int                  bit_depth = -32,
        const FitsMetadata&  metadata  = {}
    );


    // TODO (multi-band): Read a FITS data cube (NAXIS3 > 1) into separate
    //   per-plane images.  Each plane becomes one Image<float>.
    //
    // std::pair<std::vector<Image<float>>, FitsMetadata>
    //     read_image_fits_cube(const fs::path& filepath);


    // TODO (multi-band): Write an Image<TSpectral> as a FITS data cube.
    //   The number of planes equals ImagePixelTraits<TSpectral>::channels.
    //   For Image<Vec3<float>> that is 3; for Image<SpectralBins<N>> it is N.
    //   band_names / band_wavelengths in FitsMetadata describe each plane.
    //
    // template <IsImagePixel T>
    // void write_image_fits_cube(
    //     const fs::path&      filepath,
    //     const Image<T>&      image,
    //     int                  bit_depth = -32,
    //     const FitsMetadata&  metadata  = {}
    // );

}

#include "huira_impl/images/io/fits_io.ipp"
