#pragma once

// An independent reference for the reflectance of diffuse and planetary surfaces, used only by
// tests. It shares no code with huira, so a test that compares a BSDF with it cannot pass because
// both make the same mistake.
//
// Each model is a bidirectional reflectance distribution function (BRDF) f, in 1/sr, written in
// the angles photometry describes a surface's geometry with, in radians: the incidence angle i,
// between the surface normal and the direction to the light; the emission angle e, between the
// normal and the direction to the observer; and either the phase angle g, between the directions
// to the light and to the observer, or the azimuth between those two directions around the normal
// (0 when they are on the same side of it). The radiance factor I/F that planetary photometry
// tabulates is pi f cos i.

#include <algorithm>
#include <cmath>

namespace reference::photometry {

constexpr double PI = 3.141592653589793238462643383279502884;

/// The phase angle g between the directions to the light and to the observer, from their angles i
/// and e from the normal and the azimuth between them: the spherical law of cosines.
inline double phase_angle(double i, double e, double azimuth)
{
    const double cos_g = std::cos(i) * std::cos(e) + std::sin(i) * std::sin(e) * std::cos(azimuth);
    return std::acos(std::clamp(cos_g, -1.0, 1.0));
}

/// Lambert's law, for a perfectly diffuse surface of albedo a: f = a / pi.
inline double lambert(double albedo)
{
    return albedo / PI;
}

/// Oren and Nayar's (1994) qualitative model of a rough diffuse surface of albedo a, made of
/// V-shaped cavities whose facets' slopes have a standard deviation of sigma radians:
///
///   f = (a / pi) (A + B max(0, cos(azimuth)) sin(max(i, e)) tan(min(i, e)))
///   A = 1 - 0.5 sigma^2 / (sigma^2 + 0.33),   B = 0.45 sigma^2 / (sigma^2 + 0.09)
///
/// At sigma = 0 it is Lambert's law.
inline double oren_nayar(double albedo, double sigma, double i, double e, double azimuth)
{
    const double s2 = sigma * sigma;
    const double a = 1.0 - 0.5 * s2 / (s2 + 0.33);
    const double b = 0.45 * s2 / (s2 + 0.09);
    return albedo / PI *
           (a + b * std::max(0.0, std::cos(azimuth)) * std::sin(std::max(i, e)) *
                    std::tan(std::min(i, e)));
}

/// The Lommel-Seeliger law, for light scattered once, isotropically, by particles of single
/// scattering albedo w: f = w / (4 pi (cos i + cos e)).
inline double lommel_seeliger(double w, double i, double e)
{
    return w / (4.0 * PI * (std::cos(i) + std::cos(e)));
}

/// McEwen's (1991) lunar-Lambert function, Lommel-Seeliger's law and Lambert's mixed in the
/// proportion L to 1 - L, for a surface of normal albedo a:
///
///   I/F = a (2 L cos i / (cos i + cos e) + (1 - L) cos i)
///   f   = (a / pi) (2 L / (cos i + cos e) + 1 - L)
///
/// The Lommel-Seeliger part is normalized to equal Lambert's law at i = e = 0, so wherever the
/// light and the observer are both overhead, I/F is a for any L.
inline double lunar_lambert(double albedo, double l, double i, double e)
{
    return albedo / PI * (2.0 * l / (std::cos(i) + std::cos(e)) + 1.0 - l);
}

/// The proportion of Lommel-Seeliger scattering in McEwenBSDF's lunar-Lambert function at phase
/// angle g: L = exp(-g / 60 degrees).
inline double mcewen_l(double g)
{
    return std::exp(-g / (PI / 3.0));
}

/// The parameters of Hapke's model (see hapke()).
struct HapkeParameters {
    double w;  ///< Single scattering albedo
    double h;  ///< Width of the opposition surge
    double b0; ///< Amplitude of the opposition surge
    double b;  ///< Asymmetry of each Henyey-Greenstein lobe, from -1 to 1
    double c;  ///< Weight of the backscattering lobe, from 0 to 1
};

/// A double Henyey-Greenstein phase function of the phase angle g: a lobe of asymmetry b that
/// peaks toward the light (at g = 0, for b > 0), weighted c, and its mirror image, which peaks
/// away from it, weighted 1 - c. Each lobe averages to 1 over the sphere, so their mix does.
inline double double_henyey_greenstein(double b, double c, double g)
{
    const double back = (1.0 - b * b) / std::pow(1.0 - 2.0 * b * std::cos(g) + b * b, 1.5);
    const double forward = (1.0 - b * b) / std::pow(1.0 + 2.0 * b * std::cos(g) + b * b, 1.5);
    return c * back + (1.0 - c) * forward;
}

/// Hapke's (1981) approximation to Chandrasekhar's H function for isotropic scatterers of single
/// scattering albedo w: H(x) = (1 + 2x) / (1 + 2x sqrt(1 - w)).
inline double hapke_h(double x, double w)
{
    return (1.0 + 2.0 * x) / (1.0 + 2.0 * x * std::sqrt(1.0 - w));
}

/// Hapke's shadow-hiding opposition surge: B(g) = B0 / (1 + tan(g / 2) / h), and none for a surge
/// of no width (h = 0).
inline double hapke_opposition(double b0, double h, double g)
{
    if (h == 0.0) {
        return 0.0;
    }
    return b0 / (1.0 + std::tan(g / 2.0) / h);
}

/// Hapke's model of a particulate surface, with an opposition surge, a double Henyey-Greenstein
/// phase function and isotropic multiple scattering, without macroscopic roughness:
///
///   f = w / (4 pi (cos i + cos e)) ((1 + B(g)) p(g) + H(cos i) H(cos e) - 1)
inline double hapke(const HapkeParameters& p, double i, double e, double g)
{
    const double single =
        (1.0 + hapke_opposition(p.b0, p.h, g)) * double_henyey_greenstein(p.b, p.c, g);
    const double multiple = hapke_h(std::cos(i), p.w) * hapke_h(std::cos(e), p.w) - 1.0;
    return lommel_seeliger(p.w, i, e) * (single + multiple);
}

} // namespace reference::photometry
