#pragma once

#include <vector>

namespace huira {
/**
 * @brief Configuration parameters for the radius lookup table.
 *
 * Controls how the effective PSF radius is determined based on photon count thresholds
 * and minimum radius constraints.
 */
struct RadiusLUTConfig {
    float photon_threshold = 0.1f; // photon count that counts as "visible" (0.1 photons per second)
    int min_radius = 1;            // never go below this
};

/**
 * @brief Entry in the radius lookup table.
 *
 * Maps a PSF radius to the minimum irradiance threshold that requires that radius
 * for accurate rendering.
 */
struct RadiusLUTEntry {
    int radius;
    float min_irradiance;
};

/**
 * @brief Build a lookup table mapping PSF radius to minimum irradiance thresholds.
 *
 * This function analyzes the PSF kernel to determine, for each radius from 1 to full_radius,
 * what minimum irradiance level would produce at least photon_threshold photons per second
 * at the highest-sensitivity pixel within that radius ring. The resulting LUT allows efficient
 * per-star radius culling: stars with low irradiance can use smaller PSF kernels without
 * visible quality loss.
 *
 * @tparam TSpectral Spectral type for the rendering pipeline
 * @param center_kernel The full PSF kernel centered at (0,0) offset
 * @param full_radius_signed The maximum PSF radius in pixels
 * @param area The projected aperture area for photon flux calculations
 * @param photon_energies Per-channel photon energies for flux-to-photon conversion
 * @param config Configuration parameters for LUT generation
 * @return std::vector<RadiusLUTEntry> Lookup table sorted by increasing radius
 */
template <IsSpectral TSpectral>
std::vector<RadiusLUTEntry> build_radius_lut(const Image<TSpectral>& center_kernel,
                                             int full_radius_signed,
                                             float area,
                                             const TSpectral& photon_energies,
                                             const RadiusLUTConfig& config = {})
{
    std::vector<RadiusLUTEntry> lut;
    if (full_radius_signed <= 0) {
        return lut;
    }

    const std::size_t full_radius = static_cast<std::size_t>(full_radius_signed);
    const std::size_t k_w = static_cast<std::size_t>(center_kernel.width());
    const std::size_t k_h = static_cast<std::size_t>(center_kernel.height());

    lut.reserve(full_radius);

    // Precompute per-channel conversion: area / photon_energy[c]
    TSpectral conversion;
    for (std::size_t c = 0; c < TSpectral::size(); ++c) {
        float e = photon_energies[c];
        conversion[c] = (e > 0.0f) ? (area / e) : 0.0f;
    }

    // Helper: accumulate max sensitivity from a single kernel pixel.
    auto scan_pixel = [&](std::size_t kx, std::size_t ky, float& max_sensitivity) {
        if (kx >= k_w || ky >= k_h) {
            return;
        }
        const TSpectral& w = center_kernel(static_cast<int>(kx), static_cast<int>(ky));
        for (std::size_t c = 0; c < TSpectral::size(); ++c) {
            float s = w[c] * conversion[c];
            max_sensitivity = std::max(max_sensitivity, s);
        }
    };

    for (std::size_t r = 1; r <= full_radius; ++r) {
        float max_sensitivity = 0.0f;

        // Absolute kernel coordinates for this ring's bounding box.
        // r <= full_radius, so lo >= 0 and hi <= 2*full_radius.
        const std::size_t lo = full_radius - r;
        const std::size_t hi = full_radius + r;

        // Top & bottom rows: kx spans [lo .. hi]
        for (std::size_t kx = lo; kx <= hi; ++kx) {
            scan_pixel(kx, lo, max_sensitivity);
            scan_pixel(kx, hi, max_sensitivity);
        }

        // Left & right columns (excluding corners): ky spans (lo .. hi)
        for (std::size_t ky = lo + 1; ky < hi; ++ky) {
            scan_pixel(lo, ky, max_sensitivity);
            scan_pixel(hi, ky, max_sensitivity);
        }

        if (max_sensitivity > 0.0f) {
            float min_irradiance = config.photon_threshold / max_sensitivity;
            lut.push_back({static_cast<int>(r), min_irradiance});
        }
    }

    return lut;
}

/**
 * @brief Look up the effective PSF radius for a given irradiance level.
 *
 * Searches the radius LUT to find the smallest radius that can accurately render
 * a point source with the specified maximum irradiance. The effective radius is
 * clamped to be at least min_radius.
 *
 * @param lut The radius lookup table (result of build_radius_lut)
 * @param max_irradiance The maximum irradiance of the point source across all channels
 * @param min_radius Minimum allowed radius (typically 1)
 * @return int The effective PSF radius to use for rendering this point source
 */
inline int lookup_effective_radius(const std::vector<RadiusLUTEntry>& lut,
                                   float max_irradiance,
                                   int min_radius)
{
    if (lut.empty()) {
        return min_radius;
    }

    std::size_t i = lut.size();
    while (i-- > 0) {
        if (max_irradiance >= lut[i].min_irradiance) {
            return std::max(lut[i].radius, min_radius);
        }
    }

    return min_radius;
}
} // namespace huira
