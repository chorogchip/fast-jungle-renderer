#pragma once

#include <d3d12.h>

#include <cstddef>
#include <filesystem>
#include <vector>

namespace fjr::dx {

    class Shader {
    public:
        Shader() = default;

        void load(
            const std::filesystem::path& path);

        [[nodiscard]]
        D3D12_SHADER_BYTECODE get_bytecode() const noexcept {
            return {
                .pShaderBytecode = bytecode_.data(),
                .BytecodeLength = bytecode_.size()
            };
        }

        [[nodiscard]]
        explicit operator bool() const noexcept {
            return !bytecode_.empty();
        }

    private:
        std::vector<std::byte> bytecode_;
    };

} // namespace fjr::dx