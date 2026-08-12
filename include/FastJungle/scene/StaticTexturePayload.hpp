#pragma once

#include <cstdint>
#include <utility>

#include "FastJungle/core/util/TemporaryFile.hpp"

namespace fjr::scene {

    struct StaticTexturePayload final {
        StaticTexturePayload(util::TemporaryFile file, uint64_t size)
            : file(std::move(file)), size(size) {}

        util::TemporaryFile file;
        uint64_t size = 0;
    };

} // namespace fjr::scene
