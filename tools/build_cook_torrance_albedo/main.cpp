// build_cook_torrance_albedo: tabulates the directional albedo of CookTorranceBSDF's dielectric
// specular lobe, which the BSDF uses to give its diffuse lobe only the light that the specular
// lobe does not reflect (see CookTorranceBSDF::eval()). Users do not run it; its output is part
// of the source. Build it with -DHUIRA_BUILD_TOOLS=ON (it needs only a C++20 compiler), and run
// it from the repository's root:
//
//   build_cook_torrance_albedo [output.ipp]
//
// The output defaults to include/huira_impl/materials/bsdfs/cook_torrance_albedo.ipp. It takes
// about 4 minutes on two cores, and gives the same file every time.
//
// When to rebuild the table, and what guards it:
//
//   - The table is built from the sources listed in fingerprint.hpp: the lobe's masking and
//     Fresnel terms, f0 and least roughness (cook_torrance_lobe.hpp, which the BSDF evaluates
//     too), how the BSDF reads the table (cook_torrance_albedo_lookup.ipp), and this file. The
//     table records their fingerprint, and test_cook_torrance fails as soon as any of them
//     changes, until the table is rebuilt and committed.
//   - CI's "Generated Tables" workflow, whenever any of them or the table changes, rebuilds the
//     table on ubuntu-24.04 and fails if it differs from the committed one: the table can be
//     neither stale nor edited by hand.
//   - test_cook_torrance also integrates the BSDF's own eval(), independently of this tool, and
//     fails where the table is below the lobe's albedo, or more than 0.005 above it more than
//     half a degree from grazing; and CookTorranceBSDF checks at compile time that the table
//     starts at its least roughness.
//
// The lobe is a GGX microfacet reflection with Smith's height-correlated masking-shadowing and
// Schlick's Fresnel term with f0 = 0.04. Its directional albedo, the light it reflects of a unit
// irradiance from direction wo,
//
//   E(cos_o, roughness) = integral over wi of f_spec(wo, wi) cos_i
//                       = mean over the visible normals h of F(wo.h) G2(wo, wi) / G1(wo)
//
// (zero where the reflection wi is below the surface), is computed in double by Gauss-Legendre
// quadrature over the microfacet normal h: in GGX's sampling variable v, in which
// D(h) cos(theta_h) dw_h = dv dphi / (2 pi), and in the azimuth phi, over only the azimuths at
// which h faces wo and reflects it above the surface, so that the integrand is smooth and the
// quadrature converges fast (to 1e-5 at 128 points each). The quadrature assumes the lobe's
// distribution is GGX: if it changes, so must the quadrature.
//
// The table holds E at nodes evenly spaced in sqrt(cos_o) and in sqrt(roughness), which the BSDF
// interpolates bilinearly, raised so that the interpolation is nowhere below E, and the diffuse
// lobe never gets light the specular lobe reflects: E is sampled on a grid of 4 points per cell
// edge, and, in the cells where interpolating the nodes falls short of it, of 32, plus the
// largest shortfalls, found by climbing from the worst samples; the corners of each cell are
// raised until the interpolation is at least E, plus a margin for the quadrature's error, at
// every sample in it. The tool then checks the table at 20000 random points, and fails if it is
// below E at any.

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <numbers>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "fingerprint.hpp"
#include "huira/materials/bsdfs/cook_torrance_lobe.hpp"

namespace {

constexpr int COS_NODES = 65;       // in sqrt(cos_o), from 0 to 1
constexpr int ROUGHNESS_NODES = 33; // in sqrt(roughness), from sqrt(MIN_ROUGHNESS) to 1
constexpr double MIN_ROUGHNESS = huira::detail::cook_torrance::MIN_ROUGHNESS;
constexpr int SUBDIVISIONS = 4; // samples per cell edge, for the bound, in every cell
constexpr int REFINED = 32;     // and in cells that fall short, or fail the check
constexpr int POINTS = 128;     // Gauss-Legendre points per dimension
constexpr int CHECKS = 20000;   // random points at which the finished table is checked
constexpr double F0 = huira::detail::cook_torrance::DIELECTRIC_F0;

/// Gauss-Legendre nodes and weights on [-1, 1].
struct GaussLegendre {
    std::vector<double> x;
    std::vector<double> w;

    explicit GaussLegendre(int n) : x(static_cast<std::size_t>(n)), w(static_cast<std::size_t>(n))
    {
        for (int i = 0; i < n; ++i) {
            double z = std::cos(std::numbers::pi * (i + 0.75) / (n + 0.5));
            double derivative = 0.0;
            for (int iteration = 0; iteration < 100; ++iteration) {
                double p0 = 1.0;
                double p1 = z;
                for (int k = 2; k <= n; ++k) {
                    const double p2 = ((2.0 * k - 1.0) * z * p1 - (k - 1.0) * p0) / k;
                    p0 = p1;
                    p1 = p2;
                }
                derivative = n * (z * p1 - p0) / (z * z - 1.0);
                const double step = p1 / derivative;
                z -= step;
                if (std::abs(step) < 1e-16) {
                    break;
                }
            }
            x[static_cast<std::size_t>(i)] = z;
            w[static_cast<std::size_t>(i)] = 2.0 / ((1.0 - z * z) * derivative * derivative);
        }
    }
};

using huira::detail::cook_torrance::schlick_weight;
using huira::detail::cook_torrance::smith_lambda;

/// E(cos_o, roughness), by Gauss-Legendre quadrature over the microfacet normal (see the top).
double specular_albedo(double cos_o, double roughness, const GaussLegendre& gl)
{
    // E at cos_o = 0 is its limit from above: the BSDF never shades a viewer on the horizon.
    const double co = std::clamp(cos_o, 1e-9, 1.0);
    const double so = std::sqrt(1.0 - co * co);
    const double alpha = roughness * roughness;
    const double alpha2 = alpha * alpha;
    const double lambda_o = smith_lambda(co, alpha2);

    double total = 0.0;
    for (std::size_t k = 0; k < gl.x.size(); ++k) {
        // v = 1 - (1 - s)^2 for s from 0 to 1, which smooths the integrand where v nears 1:
        const double s = 0.5 * (gl.x[k] + 1.0);
        const double v = 1.0 - (1.0 - s) * (1.0 - s);
        const double dv = 0.5 * gl.w[k] * 2.0 * (1.0 - s);
        const double tan2 = alpha2 * v / (1.0 - v);
        const double ch = 1.0 / std::sqrt(1.0 + tan2);
        const double sh = std::sqrt(tan2) * ch;

        // wo.h = so sh cos(phi) + co ch must exceed co / (2 ch) for the reflection, 2 (wo.h) ch
        // - co, to be above the surface (and so for h to face wo):
        const double needed = co / (2.0 * ch);
        double min_cos_phi = 0.0;
        if (so * sh > 0.0) {
            min_cos_phi = (needed - co * ch) / (so * sh);
        } else {
            min_cos_phi = co * ch > needed ? -2.0 : 2.0;
        }
        if (min_cos_phi >= 1.0) {
            continue;
        }
        const double max_phi = std::acos(std::max(min_cos_phi, -1.0));
        double inner = 0.0;
        for (std::size_t m = 0; m < gl.x.size(); ++m) {
            const double phi = max_phi * gl.x[m];
            const double wo_h = so * sh * std::cos(phi) + co * ch;
            const double wi_z = 2.0 * wo_h * ch - co;
            if (wi_z <= 0.0) {
                continue;
            }
            const double fresnel = F0 + (1.0 - F0) * schlick_weight(wo_h);
            const double g2 = 1.0 / (1.0 + lambda_o + smith_lambda(wi_z, alpha2));
            inner += gl.w[m] * fresnel * g2 * wo_h / (co * ch);
        }
        total += dv * max_phi * inner / (2.0 * std::numbers::pi);
    }
    return total;
}

double roughness_at(double y) // y from 0 to 1 along the roughness axis
{
    const double sqrt_min = std::sqrt(MIN_ROUGHNESS);
    return std::pow(sqrt_min + (1.0 - sqrt_min) * y, 2);
}

/// 2 times the integral of E(mu) mu over mu from 0 to 1, for E linear in x = sqrt(mu) between
/// the nodes of one row: 4 times the integral of E(x) x^3 dx, exactly, segment by segment.
double average_albedo(const double* row)
{
    const double h = 1.0 / (COS_NODES - 1);
    double sum = 0.0;
    for (int i = 0; i + 1 < COS_NODES; ++i) {
        const double x0 = i * h;
        const double x1 = (i + 1) * h;
        const double e0 = row[i];
        const double slope = (row[i + 1] - row[i]) / h;
        // integral of (e0 + slope (x - x0)) x^3 dx from x0 to x1:
        const double p4 = (std::pow(x1, 4) - std::pow(x0, 4)) / 4.0;
        const double p5 = (std::pow(x1, 5) - std::pow(x0, 5)) / 5.0;
        sum += (e0 - slope * x0) * p4 + slope * p5;
    }
    return 4.0 * sum;
}

void write_array(std::FILE* out,
                 const char* name,
                 const char* size,
                 const std::vector<double>& v,
                 std::size_t per_line)
{
    std::fprintf(out, "inline constexpr std::array<float, %s> %s{\n", size, name);
    for (std::size_t k = 0; k < v.size(); ++k) {
        std::fprintf(out,
                     "%s%.9ef,%s",
                     k % per_line == 0 ? "    " : " ",
                     static_cast<double>(static_cast<float>(v[k])),
                     (k + 1) % per_line == 0 || k + 1 == v.size() ? "\n" : "");
    }
    std::fprintf(out, "};\n");
}

/// Index of (i, j) in a row-major grid of n columns.
std::size_t index(int i, int j, int n)
{
    return static_cast<std::size_t>(j) * static_cast<std::size_t>(n) + static_cast<std::size_t>(i);
}

/// Calls work(k) for k from 0 to n - 1, on every core.
template <class F>
void parallel_for(int n, const F& work)
{
    std::atomic<int> next{0};
    std::vector<std::thread> threads;
    const unsigned cores = std::max(1u, std::thread::hardware_concurrency());
    for (unsigned t = 0; t < cores; ++t) {
        threads.emplace_back([&] {
            for (int k = next++; k < n; k = next++) {
                work(k);
            }
        });
    }
    for (std::thread& thread : threads) {
        thread.join();
    }
}

} // namespace

int main(int argc, char** argv)
{
    const std::string out_path =
        argc > 1 ? argv[1] : "include/huira_impl/materials/bsdfs/cook_torrance_albedo.ipp";
    const std::optional<std::uint64_t> fingerprint =
        huira::tools::cook_torrance_albedo_fingerprint(".");
    if (!fingerprint) {
        std::cerr << "Cannot read the sources the table is built from: run this from the "
                     "repository's root.\n";
        return 1;
    }
    const GaussLegendre gl(POINTS);
    const GaussLegendre gl_fine(2 * POINTS);

    // E on a grid of SUBDIVISIONS samples per cell edge, the nodes among them:
    const int fine_cos = (COS_NODES - 1) * SUBDIVISIONS + 1;
    const int fine_roughness = (ROUGHNESS_NODES - 1) * SUBDIVISIONS + 1;
    std::vector<double> fine(index(0, fine_roughness, fine_cos));
    auto at = [&](int i, int j) -> double& { return fine[index(i, j, fine_cos)]; };
    parallel_for(fine_roughness, [&](int j) {
        const double roughness = roughness_at(double(j) / (fine_roughness - 1));
        for (int i = 0; i < fine_cos; ++i) {
            const double x = double(i) / (fine_cos - 1);
            at(i, j) = specular_albedo(x * x, roughness, gl);
        }
    });
    std::vector<double> node(index(0, ROUGHNESS_NODES, COS_NODES));
    auto nd = [&](int i, int j) -> double& { return node[index(i, j, COS_NODES)]; };
    for (int j = 0; j < ROUGHNESS_NODES; ++j) {
        for (int i = 0; i < COS_NODES; ++i) {
            nd(i, j) = at(i * SUBDIVISIONS, j * SUBDIVISIONS);
        }
    }
    auto interpolate_cell = [&](int ci, int cj, double fx, double fy) {
        return (1 - fx) * (1 - fy) * nd(ci, cj) + fx * (1 - fy) * nd(ci + 1, cj) +
               (1 - fx) * fy * nd(ci, cj + 1) + fx * fy * nd(ci + 1, cj + 1);
    };

    // Samples of E in each cell, at which the table must not fall below it, first the grid's:
    const int cells_cos = COS_NODES - 1;
    const int cells = cells_cos * (ROUGHNESS_NODES - 1);
    struct Sample {
        double fx, fy, e; // where in the cell, and E there
    };
    std::vector<std::vector<Sample>> samples(static_cast<std::size_t>(cells));
    std::vector<double> shortfall(static_cast<std::size_t>(cells), 0.0);
    for (int c = 0; c < cells; ++c) {
        const int ci = c % cells_cos;
        const int cj = c / cells_cos;
        auto& cell = samples[static_cast<std::size_t>(c)];
        for (int b = 0; b <= SUBDIVISIONS; ++b) {
            for (int a = 0; a <= SUBDIVISIONS; ++a) {
                const double fx = double(a) / SUBDIVISIONS;
                const double fy = double(b) / SUBDIVISIONS;
                cell.push_back({fx, fy, at(ci * SUBDIVISIONS + a, cj * SUBDIVISIONS + b)});
                shortfall[static_cast<std::size_t>(c)] =
                    std::max(shortfall[static_cast<std::size_t>(c)],
                             cell.back().e - interpolate_cell(ci, cj, fx, fy));
            }
        }
    }

    // Then, in the cells where interpolating the nodes falls short of E there, REFINED samples
    // per edge, and the largest shortfalls, found by climbing from the worst samples (E is
    // smooth, if steep). In the first column, sample toward the horizon at halving distances
    // too: E rises from its value at the horizon over a cos_o of about alpha, which on a smooth
    // surface is far less than a cell.
    std::vector<int> refined;
    for (int c = 0; c < cells; ++c) {
        if (shortfall[static_cast<std::size_t>(c)] > 1e-5) {
            refined.push_back(c);
        }
    }
    parallel_for(static_cast<int>(refined.size()), [&](int k) {
        const int c = refined[static_cast<std::size_t>(k)];
        const int ci = c % cells_cos;
        const int cj = c / cells_cos;
        auto e_at = [&](double fx, double fy) {
            const double x = (ci + fx) / (COS_NODES - 1);
            return specular_albedo(x * x, roughness_at((cj + fy) / (ROUGHNESS_NODES - 1)), gl);
        };
        auto gap = [&](const Sample& sample) {
            return sample.e - interpolate_cell(ci, cj, sample.fx, sample.fy);
        };
        std::vector<double> fractions;
        for (int a = 0; a <= REFINED; ++a) {
            fractions.push_back(double(a) / REFINED);
        }
        if (ci == 0) {
            for (double fx = 0.5 / REFINED; fx > 1e-12; fx *= 0.5) {
                fractions.push_back(fx);
            }
        }
        std::vector<Sample> cell;
        for (int b = 0; b <= REFINED; ++b) {
            const double fy = double(b) / REFINED;
            for (double fx : fractions) {
                cell.push_back({fx, fy, e_at(fx, fy)});
            }
        }
        std::vector<Sample> starts = cell;
        std::partial_sort(starts.begin(),
                          starts.begin() + 3,
                          starts.end(),
                          [&](const Sample& p, const Sample& q) { return gap(p) > gap(q); });
        for (int start = 0; start < 3; ++start) {
            Sample best = starts[static_cast<std::size_t>(start)];
            for (double step = 1.0 / REFINED; step > 1e-9;) {
                bool moved = false;
                for (int dx = -1; dx <= 1; ++dx) {
                    for (int dy = -1; dy <= 1; ++dy) {
                        const double fx = std::clamp(best.fx + dx * step, 0.0, 1.0);
                        const double fy = std::clamp(best.fy + dy * step, 0.0, 1.0);
                        if (fx == best.fx && fy == best.fy) {
                            continue;
                        }
                        const Sample candidate{fx, fy, e_at(fx, fy)};
                        if (gap(candidate) > gap(best)) {
                            best = candidate;
                            moved = true;
                        }
                    }
                }
                if (!moved) {
                    step *= 0.5;
                }
            }
            cell.push_back(best);
        }
        auto& stored = samples[static_cast<std::size_t>(c)];
        stored.insert(stored.end(), cell.begin(), cell.end());
    });

    // The quadrature's error, against twice the points, where the integrand is least smooth
    // (smooth surfaces near grazing) and elsewhere; the margin is twice the worst:
    double quadrature_error = 0.0;
    for (double roughness : {0.01, 0.015, 0.03, 0.1, 0.3, 1.0}) {
        for (double c : {1e-6, 1e-4, 1e-3, 0.01, 0.1, 0.5, 1.0}) {
            quadrature_error = std::max(quadrature_error,
                                        std::abs(specular_albedo(c, roughness, gl) -
                                                 specular_albedo(c, roughness, gl_fine)));
        }
    }
    const double margin = std::max(2.0 * quadrature_error, 1e-6);

    // Raise the corners of each cell until the table, interpolated as the BSDF does, is at
    // least E plus the margin (or 1) at every sample in it. The table cannot exceed 1 (the
    // diffuse lobe gets 1 - E), so where a corner reaches 1, the others rise further for it.
    std::vector<double> table = node;
    auto entry = [&](int i, int j) -> double& { return table[index(i, j, COS_NODES)]; };
    int sweeps = 0;
    for (bool raised = true; raised; ++sweeps) {
        raised = false;
        for (int c = 0; c < cells; ++c) {
            const int ci = c % cells_cos;
            const int cj = c / cells_cos;
            double* corner[4] = {
                &entry(ci, cj), &entry(ci + 1, cj), &entry(ci, cj + 1), &entry(ci + 1, cj + 1)};
            for (const Sample& sample : samples[static_cast<std::size_t>(c)]) {
                const double weight[4] = {(1 - sample.fx) * (1 - sample.fy),
                                          sample.fx * (1 - sample.fy),
                                          (1 - sample.fx) * sample.fy,
                                          sample.fx * sample.fy};
                double interpolated = 0.0;
                double free_weight = 0.0; // of the corners still below 1
                for (int q = 0; q < 4; ++q) {
                    interpolated += weight[q] * *corner[q];
                    free_weight += *corner[q] < 1.0 ? weight[q] : 0.0;
                }
                const double needed = std::min(1.0, sample.e + margin) - interpolated;
                if (needed <= 1e-15 || free_weight <= 0.0) {
                    continue;
                }
                for (double* value : corner) {
                    *value = std::min(1.0, *value + needed / free_weight);
                }
                raised = true;
            }
        }
        if (sweeps > 1000) {
            std::cerr << "Raising the table does not converge.\n";
            return 1;
        }
    }
    double largest_raise = 0.0;
    for (std::size_t k = 0; k < table.size(); ++k) {
        largest_raise = std::max(largest_raise, table[k] - node[k]);
    }

    // Check the table at random points, none of them samples:
    std::vector<double> check_x(CHECKS);
    std::vector<double> check_y(CHECKS);
    std::vector<double> check_e(CHECKS);
    unsigned state = 12345u;
    auto uniform = [&state] {
        state = state * 1664525u + 1013904223u;
        return (state >> 8) / double(1u << 24);
    };
    for (int k = 0; k < CHECKS; ++k) {
        // A third anywhere, a third within 0.6 degrees of grazing, and a third from 1e-12 to 1e-2
        // of the horizon in sqrt(cos_o), evenly in its logarithm:
        const double u = uniform();
        check_x[static_cast<std::size_t>(k)] =
            k % 3 == 0 ? u : (k % 3 == 1 ? 0.1 * u : std::pow(10.0, -12.0 + 10.0 * u));
        check_y[static_cast<std::size_t>(k)] = uniform();
    }
    parallel_for(CHECKS, [&](int k) {
        const auto kk = static_cast<std::size_t>(k);
        check_e[kk] = specular_albedo(check_x[kk] * check_x[kk], roughness_at(check_y[kk]), gl);
    });
    double worst_check = 1.0;
    for (int k = 0; k < CHECKS; ++k) {
        const auto kk = static_cast<std::size_t>(k);
        const double fx = check_x[kk] * (COS_NODES - 1);
        const double fy = check_y[kk] * (ROUGHNESS_NODES - 1);
        const int i = std::min(static_cast<int>(fx), COS_NODES - 2);
        const int j = std::min(static_cast<int>(fy), ROUGHNESS_NODES - 2);
        const double ax = fx - i;
        const double ay = fy - j;
        const double interpolated = (1 - ax) * (1 - ay) * entry(i, j) +
                                    ax * (1 - ay) * entry(i + 1, j) +
                                    (1 - ax) * ay * entry(i, j + 1) + ax * ay * entry(i + 1, j + 1);
        worst_check = std::min(worst_check, interpolated - check_e[kk]);
    }
    std::cerr << refined.size() << " cells refined; quadrature error " << quadrature_error
              << ", margin " << margin << "; largest raise of a node " << largest_raise << ", in "
              << sweeps << " sweeps; the table minus E at " << CHECKS
              << " random points is at least " << worst_check << "\n";
    if (worst_check < 0.0) {
        std::cerr << "The table is below E somewhere: raise REFINED.\n";
        return 1;
    }
    std::vector<double> average(ROUGHNESS_NODES);
    for (int j = 0; j < ROUGHNESS_NODES; ++j) {
        average[index(j, 0, 0)] = average_albedo(&table[index(0, j, COS_NODES)]);
    }

    std::FILE* out = std::fopen(out_path.c_str(), "w");
    if (out == nullptr) {
        std::cerr << "Cannot write " << out_path << "\n";
        return 1;
    }
    std::fprintf(
        out,
        R"(// Generated by tools/build_cook_torrance_albedo; do not edit. See that tool for how.
//
// The directional albedo E(cos_o, roughness) of CookTorranceBSDF's dielectric specular lobe (GGX,
// Smith's height-correlated masking-shadowing, Schlick's Fresnel term with f0 = 0.04): the light
// it reflects of a unit irradiance from a direction at cos_o to the normal. Node (i, j), at
// DIELECTRIC_SPECULAR_ALBEDO[j * DIELECTRIC_ALBEDO_COS_NODES + i], is at
//
//   cos_o = (i / (DIELECTRIC_ALBEDO_COS_NODES - 1))^2
//   roughness = (s + (1 - s) j / (DIELECTRIC_ALBEDO_ROUGHNESS_NODES - 1))^2,
//     s = sqrt(DIELECTRIC_ALBEDO_MIN_ROUGHNESS)
//
// and its value bounds E from above wherever bilinear interpolation uses it. Node j of
// DIELECTRIC_SPECULAR_AVERAGE_ALBEDO is the cosine-weighted average of row j's interpolation,
// 2 times the integral of E(mu) mu over mu from 0 to 1.
//
// Quadrature error %.1e, margin %.1e; largest raise of a node %.1e.
// The table minus E at %d random points is at least %.1e.
//
// DIELECTRIC_ALBEDO_FINGERPRINT is that of the sources the table was built from (see
// tools/build_cook_torrance_albedo/fingerprint.hpp), which test_cook_torrance checks.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace huira::detail {

inline constexpr std::uint64_t DIELECTRIC_ALBEDO_FINGERPRINT = 0x%016llxULL;
inline constexpr std::size_t DIELECTRIC_ALBEDO_COS_NODES = %d;
inline constexpr std::size_t DIELECTRIC_ALBEDO_ROUGHNESS_NODES = %d;
inline constexpr float DIELECTRIC_ALBEDO_MIN_ROUGHNESS = %.9gf;

// clang-format off
)",
        quadrature_error,
        margin,
        largest_raise,
        CHECKS,
        worst_check,
        static_cast<unsigned long long>(*fingerprint),
        COS_NODES,
        ROUGHNESS_NODES,
        MIN_ROUGHNESS);
    write_array(out,
                "DIELECTRIC_SPECULAR_ALBEDO",
                "DIELECTRIC_ALBEDO_COS_NODES * DIELECTRIC_ALBEDO_ROUGHNESS_NODES",
                table,
                5);
    std::fprintf(out, "\n");
    write_array(
        out, "DIELECTRIC_SPECULAR_AVERAGE_ALBEDO", "DIELECTRIC_ALBEDO_ROUGHNESS_NODES", average, 5);
    std::fprintf(out, "// clang-format on\n\n} // namespace huira::detail\n");
    std::fclose(out);
    std::cerr << "Wrote " << out_path << "\n";
    return 0;
}
