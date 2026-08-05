#pragma once

#include "FastJungle/scene/StaticScene.hpp"

#include <compare>
#include <cstdint>
#include <type_traits>

namespace fjr::cooker::internal {

    template<typename Tag>
    class SceneHandle final {
    public:
        constexpr SceneHandle() noexcept = default;

        explicit constexpr SceneHandle(std::uint32_t value) noexcept
            : value_(value) {}

        [[nodiscard]] constexpr std::uint32_t value() const noexcept {
            return value_;
        }

        [[nodiscard]] constexpr bool valid() const noexcept {
            return value_ != scene::StaticScene::INVALID_INDEX;
        }

        auto operator<=>(const SceneHandle&) const = default;

    private:
        std::uint32_t value_ = scene::StaticScene::INVALID_INDEX;
    };

    struct StringOffsetTag;
    struct SamplerIdTag;
    struct TextureIdTag;
    struct TextureBindingIdTag;
    struct MaterialIdTag;
    struct MeshIdTag;
    struct DefinitionIdTag;
    struct PointInstanceIdTag;
    struct PointBatchIdTag;
    struct StaticInstanceIdTag;

    using StringOffset = SceneHandle<StringOffsetTag>;
    using SamplerId = SceneHandle<SamplerIdTag>;
    using TextureId = SceneHandle<TextureIdTag>;
    using TextureBindingId = SceneHandle<TextureBindingIdTag>;
    using MaterialId = SceneHandle<MaterialIdTag>;
    using MeshId = SceneHandle<MeshIdTag>;
    using DefinitionId = SceneHandle<DefinitionIdTag>;
    using PointInstanceId = SceneHandle<PointInstanceIdTag>;
    using PointBatchId = SceneHandle<PointBatchIdTag>;
    using StaticInstanceId = SceneHandle<StaticInstanceIdTag>;

    template<typename Handle>
    struct SceneRange final {
        static_assert(std::is_class_v<Handle>);

        Handle first;
        std::uint32_t count = 0;

        [[nodiscard]] constexpr scene::StaticScene::IndexRange
        serialized() const noexcept {
            return {.offset = first.value(), .count = count};
        }
    };

} // namespace fjr::cooker::internal
