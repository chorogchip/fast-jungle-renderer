#include "FastJungle/dx12/Shader.hpp"

#include "FastJungle/core/util/Logger.hpp"

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
            log::Logger::g_logger
                << "Failed to open shader: " << path
                << log::abrt();
        }

        const std::streampos end_position =
            file.tellg();

        if (end_position <= 0) {
            log::Logger::g_logger
                << "Shader is empty: " << path
                << log::abrt();
        }

        const auto size =
            static_cast<std::uintmax_t>(
                end_position);

        if (size >
            std::numeric_limits<std::size_t>::max()) {
            log::Logger::g_logger
                << "Shader is too large: " << path
                << log::abrt();
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
            log::Logger::g_logger
                << "Failed to read shader: " << path
                << log::abrt();
        }
    }

} // namespace fjr::dx
