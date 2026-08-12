#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

#include "FastJungle/core/util/TemporaryFile.hpp"
#include "FastJungle/scene/StaticScene.hpp"
#include "FastJungle/scene/StaticTexturePayload.hpp"

namespace DirectX {
    class ScratchImage;
}

namespace fjr::cooker {

    class TexturePayloadBuilder final {
    public:
        explicit TexturePayloadBuilder(std::filesystem::path path);

        void append(
            const DirectX::ScratchImage& image,
            scene::StaticScene::Texture& texture,
            std::vector<scene::StaticScene::TextureMip>& mips,
            const std::filesystem::path& source_path);

        [[nodiscard]] scene::StaticTexturePayload finish();

    private:
        util::TemporaryFile file_;
        std::ofstream output_;
        uint64_t size_ = 0;
    };

} // namespace fjr::cooker
