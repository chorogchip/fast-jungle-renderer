#include "FastJungle/dx12/Shader.hpp"

#include <cstdlib>
#include <cstdint>
#include <fstream>
#include <limits>

namespace fjr::dx {

    void Shader::load(
        const std::filesystem::path& path) {

        std::ifstream file{
            path,
            std::ios::binary |
            std::ios::ate
        };

        if (!file) {
            std::abort();
        }

        const std::streampos end_position =
            file.tellg();

        if (end_position <= 0) {
            std::abort();
        }

        const auto size =
            static_cast<std::uintmax_t>(
                end_position);

        if (size >
            std::numeric_limits<std::size_t>::max()) {
            std::abort();
        }

        bytecode_.resize(
            static_cast<std::size_t>(size));

        file.seekg(
            0,
            std::ios::beg);

        if (!file.read(
            reinterpret_cast<char*>(
                bytecode_.data()),
            static_cast<std::streamsize>(
                bytecode_.size()))) {

            bytecode_.clear();
            std::abort();
        }
    }

} // namespace fjr::dx