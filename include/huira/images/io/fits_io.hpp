#pragma once

#include <filesystem>

#include "huira/images/image.hpp"
#include "huira/images/io/fits_metadata.hpp"

namespace fs = std::filesystem;

namespace huira {

// -------------------------------------------------------------------------
// Read a single-plane FITS image into Image<float>.
//
//   CFITSIO applies BZERO/BSCALE, so values are first the file's physical
//   values (a sensor's DN, for a sensor response write_image_fits() wrote).
//   They are then normalised:
//
//   • With ADCBITS (a sensor's bit depth, which write_image_fits() writes
//     for a sensor response), at any BITPIX: divided by 2^ADCBITS - 1, as
//     the sensor response held them, and the returned Image has its
//     sensor_bit_depth set to ADCBITS.
//
//   • Otherwise, integer BITPIX (8, 16, 32): divided by the SATURATE
//     keyword (if present) or the maximum value for that BITPIX type. If
//     SATURATE is 2^bits - 1, the Image's sensor_bit_depth is set to bits.
//
//   • Otherwise, float BITPIX (−32, −64): returned as they are (physical /
//     flux units; consult metadata.bunit for the unit).
//
//   Undefined pixels (BLANK in an integer image, NaN in a float one) are
//   returned as NaN.
//
// The returned FitsMetadata is populated from every recognised header
// keyword; unrecognised keywords land in custom_keywords. Keywords that
// describe how the data are stored (BITPIX, BZERO, BSCALE, BLANK, ADCBITS,
// ...) are not returned as custom keywords.
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
//   scaling      For an image with a sensor bit depth (a sensor response):
//                Counts (default) writes the sensor's digital numbers;
//                FullRange writes it as any other image.
//
// A sensor response (an image whose sensor_bit_depth() is set, as a sensor
// readout sets it) holds DN / (2^bits - 1). Unless scaling is FullRange,
// the file holds the DN themselves, at every BITPIX:
//
//   • Integer BITPIX with at least the sensor's bits: the DN as they are
//     (0–4095 for a 12-bit sensor in a 16-bit file).
//   • Integer BITPIX with fewer bits than the sensor: the lowest bits are
//     dropped, and BSCALE = 2^(sensor bits - BITPIX) scales the stored
//     values back to DN, so every FITS reader sees DN (in steps of BSCALE).
//   • Float BITPIX: the DN as floating-point values.
//
//   SATURATE (2^bits - 1, the ADC ceiling in DN), ADCBITS (the bit depth)
//   and BUNIT = 'adu' (unless metadata gives a unit) are written, so that
//   read_image_fits() returns the sensor response as it was.
//
// Any other image is written, in an integer BITPIX, with its values in
// [0, 1] stretched over the type's range (SATURATE is then the type's
// maximum), and in a float BITPIX as it is.
//
// Undefined (non-finite) pixels are NaN in a float image. In an integer
// image they are marked by BLANK: the type's largest value when the data
// leave it spare (a sensor with fewer bits than the file), otherwise 0, and
// valid pixels are then written as at least one step above it.
//
// The writer sets the keywords that describe the data from the image, and
// ignores them in metadata (which may have come from another file): BZERO,
// BSCALE, BLANK, DATAMIN, DATAMAX (the range of the defined pixels, in
// physical values; not written if there are none), ADCBITS, and SATURATE
// (except for a float image that is not a sensor response, whose SATURATE
// is metadata's).
// -------------------------------------------------------------------------
void write_image_fits(const fs::path& filepath,
                      const Image<float>& image,
                      int bit_depth = -32,
                      const FitsMetadata& metadata = {},
                      PixelScaling scaling = PixelScaling::Counts);

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

} // namespace huira

#include "huira_impl/images/io/fits_io.ipp"
