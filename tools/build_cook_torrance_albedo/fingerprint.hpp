#pragma once

// The fingerprint of the sources CookTorranceBSDF's albedo table is built from. The tool records
// it in the table it writes, and test_cook_torrance fails when the sources no longer match it: a
// change to any of them, even to a comment, means the table must be rebuilt.
//
// It is FNV-1a (64 bits) of each source's path and bytes, carriage returns left out, so that a
// checkout with Windows line endings has the same fingerprint.

#include <array>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>

namespace huira::tools {

/// The sources the table is built from, relative to the repository's root: the lobe it
/// integrates, how the BSDF reads it, and how the tool builds it.
inline constexpr std::array<const char*, 3> COOK_TORRANCE_ALBEDO_SOURCES{
    "include/huira/materials/bsdfs/cook_torrance_lobe.hpp",
    "include/huira_impl/materials/bsdfs/cook_torrance_albedo_lookup.ipp",
    "tools/build_cook_torrance_albedo/main.cpp",
};

/// The fingerprint of the sources under the repository's root, or nothing if one cannot be read.
inline std::optional<std::uint64_t> cook_torrance_albedo_fingerprint(const std::string& root)
{
    std::uint64_t hash = 0xcbf29ce484222325ULL;
    const auto add = [&hash](unsigned char byte) {
        hash ^= byte;
        hash *= 0x100000001b3ULL;
    };
    for (const char* path : COOK_TORRANCE_ALBEDO_SOURCES) {
        std::ifstream file(root + "/" + path, std::ios::binary);
        if (!file) {
            return std::nullopt;
        }
        for (const char* c = path; *c != '\0'; ++c) {
            add(static_cast<unsigned char>(*c));
        }
        add(0);
        for (std::istreambuf_iterator<char> it(file), end; it != end; ++it) {
            if (*it != '\r') {
                add(static_cast<unsigned char>(*it));
            }
        }
        add(0);
    }
    return hash;
}

} // namespace huira::tools
