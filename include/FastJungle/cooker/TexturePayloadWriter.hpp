#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

#include "FastJungle/scene/StaticScene.hpp"

namespace DirectX {
    class ScratchImage;
}

namespace fjr::cooker {

    class TexturePayloadWriter final {
    public:
        explicit TexturePayloadWriter(std::filesystem::path path);

        void append(
            const DirectX::ScratchImage& image,
            scene::StaticScene::Texture& texture,
            std::vector<scene::StaticScene::TextureMip>& mips,
            const std::filesystem::path& source_path);

        [[nodiscard]] std::uint64_t finish();
        void abandon() noexcept;

    private:
        std::filesystem::path path_;
        std::ofstream output_;
        std::uint64_t size_ = 0;
    };

} // namespace fjr::cooker
