#pragma once

#include <cstdint>
#include <string>

namespace huira {
    inline std::uint64_t pack_tycho2_id(std::uint16_t tyc1, std::uint16_t tyc2, std::uint8_t tyc3) {
        return (static_cast<std::uint64_t>(tyc1) << 20) |
            (static_cast<std::uint64_t>(tyc2) << 4) |
            static_cast<std::uint64_t>(tyc3);
    }

    inline void unpack_tycho2_id(std::uint64_t id, std::uint16_t& tyc1, std::uint16_t& tyc2, std::uint8_t& tyc3) {
        tyc1 = static_cast<std::uint16_t>((id >> 20) & 0x3FFF);
        tyc2 = static_cast<std::uint16_t>((id >> 4) & 0xFFFF);
        tyc3 = static_cast<std::uint8_t>(id & 0xF);
    }

    inline std::string format_tycho2_id(std::uint64_t id) {
        std::uint16_t tyc1, tyc2;
        std::uint8_t tyc3;
        unpack_tycho2_id(id, tyc1, tyc2, tyc3);
        return "TYC " +
            std::to_string(static_cast<unsigned int>(tyc1)) + "-" +
            std::to_string(static_cast<unsigned int>(tyc2)) + "-" +
            std::to_string(static_cast<unsigned int>(tyc3));
    }
}
