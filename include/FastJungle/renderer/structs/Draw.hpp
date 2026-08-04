#pragma once

#include <cstdint>

namespace fjr::render {

	struct Draw {
        static constexpr inline uint32_t INVALID_INDEX = UINT32_MAX;

        enum class EnumDrawCpuFlag {
            DEFAULT = 0,
            ALPHA_TESTED = 1,
            ALPHA_BLENDED = 2,
            DOUBLE_SIDED = 4,
            POINT_INSTNACED = 8,
        };

        struct DrawDataCpu {

            struct RootConstants {
                uint32_t offset_instance = INVALID_INDEX;
                uint32_t offset_material = INVALID_INDEX;
                uint32_t instnace_kind = 0;
            };
            static constexpr inline uint32_t ROOT_CONSTANTS_COUNT =
                sizeof(RootConstants) / sizeof(uint32_t);

            RootConstants constants;
            EnumDrawCpuFlag flags = EnumDrawCpuFlag::DEFAULT;
            uint32_t offset_cbuf_transform = INVALID_INDEX;
            uint32_t offset_index = INVALID_INDEX;
            uint32_t offset_vertex = INVALID_INDEX;
            uint32_t count_index = 0;
            uint32_t count_instance = 0;
		};
	};
}