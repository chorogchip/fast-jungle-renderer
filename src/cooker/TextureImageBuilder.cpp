#include "TextureImageBuilder.hpp"

#include "FastJungle/core/util/Logger.hpp"

#include <DirectXMath.h>

#include <algorithm>
#include <bit>

namespace fjr::cooker {
    namespace {
        [[nodiscard]] DXGI_FORMAT
        working_format(DXGI_FORMAT source,
                       const TextureCompressionPlan& plan) noexcept {
            if (!DirectX::IsCompressed(source)) {
                return DirectX::MakeLinear(source);
            }
            return plan.dxgi_format == DXGI_FORMAT_BC6H_UF16
                       ? DXGI_FORMAT_R32G32B32A32_FLOAT
                       : DXGI_FORMAT_R8G8B8A8_UNORM;
        }

        void make_linear(DirectX::ScratchImage& image) {
            const DXGI_FORMAT source = image.GetMetadata().format;
            const DXGI_FORMAT linear = DirectX::MakeLinear(source);
            if (source != linear && !image.OverrideFormat(linear)) {
                log::Logger::g_logger << log::abrt(
                    "Failed to normalize the texture color-space format.");
            }
        }

        [[nodiscard]] DirectX::XMVECTOR
        select_channel(DirectX::XMVECTOR value,
                       scene::StaticScene::EnumTextureChannel channel) {
            switch (channel) {
            case scene::StaticScene::EnumTextureChannel::R:
                return DirectX::XMVectorSplatX(value);
            case scene::StaticScene::EnumTextureChannel::G:
                return DirectX::XMVectorSplatY(value);
            case scene::StaticScene::EnumTextureChannel::B:
                return DirectX::XMVectorSplatZ(value);
            case scene::StaticScene::EnumTextureChannel::A:
                return DirectX::XMVectorSplatW(value);
            case scene::StaticScene::EnumTextureChannel::RGB:
            case scene::StaticScene::EnumTextureChannel::RGBA:
                return value;
            }
            log::Logger::g_logger
                << log::abrt("Texture source channel is invalid.");
        }

        void isolate_channel(const DirectX::Image& source,
                             const TextureCompressionPlan& plan,
                             DirectX::ScratchImage& result) {
            const HRESULT transform_result = DirectX::TransformImage(
                source,
                [&plan](DirectX::XMVECTOR* output,
                        const DirectX::XMVECTOR* input, std::size_t width,
                        std::size_t) {
                    for (std::size_t x = 0; x < width; ++x) {
                        DirectX::XMVECTOR value = input[x];
                        if (plan.linearize_source_channel) {
                            value = DirectX::XMColorSRGBToRGB(value);
                        }
                        output[x] = select_channel(value, plan.source_channel);
                    }
                },
                result);
            if (FAILED(transform_result)) {
                log::Logger::g_logger << log::abrt(
                    "Failed to isolate the texture source channel.");
            }
        }

        [[nodiscard]] DirectX::TEX_FILTER_FLAGS
        mip_filter(const TextureCompressionPlan& plan, std::size_t width,
                   std::size_t height) noexcept {
            uint32_t result =
                std::has_single_bit(width) && std::has_single_bit(height)
                    ? DirectX::TEX_FILTER_FANT
                    : DirectX::TEX_FILTER_LINEAR;
            if (plan.filter_as_srgb) {
                result |= DirectX::TEX_FILTER_SRGB;
            }
            return static_cast<DirectX::TEX_FILTER_FLAGS>(result);
        }

        [[nodiscard]] DirectX::TEX_COMPRESS_FLAGS
        compression_flags(const TextureCompressionPlan& plan) noexcept {
            uint32_t result = DirectX::TEX_COMPRESS_DEFAULT;
            if (plan.dxgi_format == DXGI_FORMAT_BC7_UNORM) {
                result |= DirectX::TEX_COMPRESS_BC7_QUICK;
            }
            result |= DirectX::TEX_COMPRESS_PARALLEL;
            return static_cast<DirectX::TEX_COMPRESS_FLAGS>(result);
        }

    } // namespace

    void TextureImageBuilder::build(DirectX::ScratchImage& decoded,
                                    const TextureCompressionPlan& plan,
                                    std::string_view source_key,
                                    DirectX::ScratchImage& compressed) {
        if (!plan.use_block_compression) {
            compressed = std::move(decoded);
            return;
        }

        DirectX::ScratchImage decompressed;
        DirectX::ScratchImage coverage_carrier;
        DirectX::ScratchImage isolated;
        DirectX::ScratchImage mip_chain;
        DirectX::ScratchImage coverage_chain;
        DirectX::ScratchImage coverage_scalar_chain;

        DirectX::ScratchImage* source = &decoded;
        if (DirectX::IsCompressed(decoded.GetMetadata().format)) {
            const auto* base = decoded.GetImage(0, 0, 0);
            if (base == nullptr ||
                FAILED(DirectX::Decompress(
                    *base, working_format(decoded.GetMetadata().format, plan),
                    decompressed))) {
                log::Logger::g_logger
                    << log::abrt("Failed to decompress the source texture.");
            }
            source = &decompressed;
        }
        make_linear(*source);

        const DirectX::Image* base = source->GetImage(0, 0, 0);
        if (base == nullptr) {
            log::Logger::g_logger
                << log::abrt("Texture base image is missing.");
        }
        if (plan.preserve_alpha_coverage) {
            if (!plan.isolate_source_channel ||
                plan.dxgi_format != DXGI_FORMAT_BC4_UNORM) {
                log::Logger::g_logger
                    << log::abrt("Alpha coverage preservation requires "
                                 "isolated BC4 output.");
            }
            if (base->format != DXGI_FORMAT_R8G8B8A8_UNORM) {
                if (FAILED(DirectX::Convert(*base, DXGI_FORMAT_R8G8B8A8_UNORM,
                                            DirectX::TEX_FILTER_DEFAULT,
                                            DirectX::TEX_THRESHOLD_DEFAULT,
                                            coverage_carrier))) {
                    log::Logger::g_logger << log::abrt(
                        "Failed to create the alpha coverage working image.");
                }
                base = coverage_carrier.GetImage(0, 0, 0);
                if (base == nullptr) {
                    log::Logger::g_logger << log::abrt(
                        "Alpha coverage working image is missing.");
                }
            }
        }
        if (plan.isolate_source_channel) {
            isolate_channel(*base, plan, isolated);
            base = isolated.GetImage(0, 0, 0);
            if (base == nullptr) {
                log::Logger::g_logger
                    << log::abrt("Isolated texture image is missing.");
            }
        }

        const DirectX::Image* images = base;
        std::size_t image_count = 1;
        DirectX::TexMetadata metadata = plan.isolate_source_channel
                                            ? isolated.GetMetadata()
                                            : source->GetMetadata();
        metadata.mipLevels = 1;
        if (base->width > 1 || base->height > 1) {
            if (FAILED(DirectX::GenerateMipMaps(
                    *base, mip_filter(plan, base->width, base->height), 0,
                    mip_chain))) {
                log::Logger::g_logger << log::abrt(
                    std::string{"Failed to generate texture mipmaps: "} +
                    std::string{source_key});
            }
            images = mip_chain.GetImages();
            image_count = mip_chain.GetImageCount();
            metadata = mip_chain.GetMetadata();
        }

        if (plan.preserve_alpha_coverage && image_count > 1) {
            if (FAILED(coverage_chain.Initialize(metadata)) ||
                FAILED(DirectX::ScaleMipMapsAlphaForCoverage(
                    images, image_count, metadata, 0, plan.alpha_reference,
                    coverage_chain))) {
                log::Logger::g_logger << log::abrt(
                    std::string{"Failed to preserve alpha coverage: "} +
                    std::string{source_key});
            }
            if (FAILED(DirectX::TransformImage(
                    coverage_chain.GetImages(), coverage_chain.GetImageCount(),
                    coverage_chain.GetMetadata(),
                    [](DirectX::XMVECTOR* output,
                       const DirectX::XMVECTOR* input, std::size_t width,
                       std::size_t) {
                        for (std::size_t x = 0; x < width; ++x) {
                            output[x] = DirectX::XMVectorSplatW(input[x]);
                        }
                    },
                    coverage_scalar_chain))) {
                log::Logger::g_logger << log::abrt(
                    std::string{"Failed to move coverage alpha to R: "} +
                    std::string{source_key});
            }
            images = coverage_scalar_chain.GetImages();
            image_count = coverage_scalar_chain.GetImageCount();
            metadata = coverage_scalar_chain.GetMetadata();
        }

        if (FAILED(DirectX::Compress(images, image_count, metadata,
                                     static_cast<DXGI_FORMAT>(plan.dxgi_format),
                                     compression_flags(plan),
                                     DirectX::TEX_THRESHOLD_DEFAULT,
                                     compressed))) {
            log::Logger::g_logger
                << log::abrt(std::string{"Failed to compress the texture: "} +
                             std::string{source_key});
        }
    }

} // namespace fjr::cooker
