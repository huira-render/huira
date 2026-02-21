#pragma once

#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <vector>

#include "huira/util/logger.hpp"

namespace fs = std::filesystem;

namespace huira {

    /**
     * @brief Reads an entire file into a memory buffer.
     *
     * Utility function used by all image format readers to load file contents
     * before forwarding to the buffer-based decoding implementations.
     *
     * @param filepath Path to the file to read
     * @return Vector containing the raw file bytes
     * @throws std::runtime_error if the file cannot be opened or read
     */
    inline std::vector<unsigned char> read_file_to_buffer(const fs::path& filepath)
    {
#ifdef _MSC_VER
        FILE* fp = nullptr;
        errno_t err = fopen_s(&fp, filepath.string().c_str(), "rb");
        if (err != 0 || !fp) {
            HUIRA_THROW_ERROR("read_file_to_buffer - Failed to open file: " + filepath.string());
        }
#else
        FILE* fp = fopen(filepath.string().c_str(), "rb");
        if (!fp) {
            HUIRA_THROW_ERROR("read_file_to_buffer - Failed to open file: " + filepath.string());
        }
#endif

        fseek(fp, 0, SEEK_END);
        long file_size = ftell(fp);
        if (file_size <= 0) {
            fclose(fp);
            HUIRA_THROW_ERROR("read_file_to_buffer - Failed to determine file size or file is empty: " + filepath.string());
        }
        fseek(fp, 0, SEEK_SET);

        std::vector<unsigned char> buffer(static_cast<std::size_t>(file_size));
        if (fread(buffer.data(), 1, buffer.size(), fp) != buffer.size()) {
            fclose(fp);
            HUIRA_THROW_ERROR("read_file_to_buffer - Failed to read file: " + filepath.string());
        }

        fclose(fp);
        return buffer;
    }

}
