#pragma once

#include <d3d12.h>
#include <wrl.h>

#include <source_location>
#include <type_traits>
#include <vector>

namespace fjr::dx {

    class RootSignatureBuilder {
    private:
        struct DescriptorRangeDesc {
            D3D12_DESCRIPTOR_RANGE_TYPE type =
                D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

            UINT shader_register = 0;
            UINT count = 0;
            UINT register_space = 0;

            D3D12_DESCRIPTOR_RANGE_FLAGS flags =
                D3D12_DESCRIPTOR_RANGE_FLAG_NONE;

            UINT offset =
                D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        };

    public:
        class ParameterProxy {
        public:
            ParameterProxy(const ParameterProxy&) = delete;
            ParameterProxy& operator=(const ParameterProxy&) = delete;
            ParameterProxy(ParameterProxy&&) = delete;
            ParameterProxy& operator=(ParameterProxy&&) = delete;

            ParameterProxy& reg(UINT shader_register) noexcept;
            ParameterProxy& count(UINT count) noexcept;
            ParameterProxy& space(UINT register_space) noexcept;

            ParameterProxy& vis_all() noexcept;
            ParameterProxy& vis_vertex() noexcept;
            ParameterProxy& vis_pixel() noexcept;

            RootSignatureBuilder& add();

        private:
            friend class RootSignatureBuilder;

            ParameterProxy(
                RootSignatureBuilder& owner,
                UINT index,
                D3D12_ROOT_PARAMETER_TYPE root_type) noexcept;

            RootSignatureBuilder& owner_;

            UINT index_ = 0;

            D3D12_ROOT_PARAMETER_TYPE root_type_ =
                D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;

            UINT shader_register_ = 0;
            UINT count_ = 0;
            UINT register_space_ = 0;

            D3D12_SHADER_VISIBILITY visibility_ =
                D3D12_SHADER_VISIBILITY_ALL;
        };

        class TableProxy {
        public:
            class RangeProxy {
            public:
                RangeProxy(const RangeProxy&) = delete;
                RangeProxy& operator=(const RangeProxy&) = delete;
                RangeProxy(RangeProxy&&) = delete;
                RangeProxy& operator=(RangeProxy&&) = delete;

                RangeProxy& reg(UINT shader_register) noexcept;
                RangeProxy& count(UINT count) noexcept;
                RangeProxy& space(UINT register_space) noexcept;

                RangeProxy& flags(
                    D3D12_DESCRIPTOR_RANGE_FLAGS flags) noexcept;

                RangeProxy& offset(UINT offset) noexcept;

                TableProxy& add_range();

            private:
                friend class TableProxy;

                RangeProxy(
                    TableProxy& owner,
                    D3D12_DESCRIPTOR_RANGE_TYPE type) noexcept;

                TableProxy& owner_;
                DescriptorRangeDesc description_{};
            };

            TableProxy(const TableProxy&) = delete;
            TableProxy& operator=(const TableProxy&) = delete;
            TableProxy(TableProxy&&) = delete;
            TableProxy& operator=(TableProxy&&) = delete;

            [[nodiscard]] RangeProxy cbv();
            [[nodiscard]] RangeProxy srv();
            [[nodiscard]] RangeProxy uav();
            [[nodiscard]] RangeProxy sampler();

            TableProxy& vis_all() noexcept;
            TableProxy& vis_vertex() noexcept;
            TableProxy& vis_pixel() noexcept;

            RootSignatureBuilder& add();

        private:
            friend class RootSignatureBuilder;

            TableProxy(
                RootSignatureBuilder& owner,
                UINT index,
                bool sampler_table) noexcept;

            RootSignatureBuilder& owner_;

            UINT index_ = 0;
            bool sampler_table_ = false;

            D3D12_SHADER_VISIBILITY visibility_ =
                D3D12_SHADER_VISIBILITY_ALL;

            std::vector<DescriptorRangeDesc> ranges_;
        };

        void init(UINT parameter_count);

        template<typename Enum>
            requires std::is_enum_v<Enum>
        void init(Enum parameter_count) {
            init(static_cast<UINT>(parameter_count));
        }

        RootSignatureBuilder& reset() noexcept;

        RootSignatureBuilder& set_flags(
            D3D12_ROOT_SIGNATURE_FLAGS flags) noexcept;

        [[nodiscard]] ParameterProxy set_constants(UINT index);
        [[nodiscard]] ParameterProxy set_root_cbv(UINT index);
        [[nodiscard]] ParameterProxy set_root_srv(UINT index);
        [[nodiscard]] ParameterProxy set_root_uav(UINT index);

        [[nodiscard]] TableProxy set_resource_table(UINT index);
        [[nodiscard]] TableProxy set_sampler_table(UINT index);

        template<typename Enum>
            requires std::is_enum_v<Enum>
        [[nodiscard]] ParameterProxy set_constants(Enum index) {
            return set_constants(static_cast<UINT>(index));
        }

        template<typename Enum>
            requires std::is_enum_v<Enum>
        [[nodiscard]] ParameterProxy set_root_cbv(Enum index) {
            return set_root_cbv(static_cast<UINT>(index));
        }

        template<typename Enum>
            requires std::is_enum_v<Enum>
        [[nodiscard]] ParameterProxy set_root_srv(Enum index) {
            return set_root_srv(static_cast<UINT>(index));
        }

        template<typename Enum>
            requires std::is_enum_v<Enum>
        [[nodiscard]] ParameterProxy set_root_uav(Enum index) {
            return set_root_uav(static_cast<UINT>(index));
        }

        template<typename Enum>
            requires std::is_enum_v<Enum>
        [[nodiscard]] TableProxy set_resource_table(Enum index) {
            return set_resource_table(static_cast<UINT>(index));
        }

        template<typename Enum>
            requires std::is_enum_v<Enum>
        [[nodiscard]] TableProxy set_sampler_table(Enum index) {
            return set_sampler_table(static_cast<UINT>(index));
        }

        [[nodiscard]]
        Microsoft::WRL::ComPtr<ID3D12RootSignature> build(
            ID3D12Device* device,
            std::source_location loc =
                std::source_location::current()) const;

    private:
        enum class ParameterKind {
            constants,
            root_descriptor,
            descriptor_table
        };

        struct ParameterDesc {
            ParameterKind kind = ParameterKind::constants;

            D3D12_ROOT_PARAMETER_TYPE root_type =
                D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;

            UINT shader_register = 0;
            UINT register_space = 0;
            UINT count = 0;

            D3D12_SHADER_VISIBILITY visibility =
                D3D12_SHADER_VISIBILITY_ALL;

            std::vector<DescriptorRangeDesc> ranges;
        };

        RootSignatureBuilder& add_constants(
            UINT index,
            UINT shader_register,
            UINT value_count,
            UINT register_space,
            D3D12_SHADER_VISIBILITY visibility);

        RootSignatureBuilder& add_root_descriptor(
            UINT index,
            D3D12_ROOT_PARAMETER_TYPE type,
            UINT shader_register,
            UINT register_space,
            D3D12_SHADER_VISIBILITY visibility);

        RootSignatureBuilder& add_descriptor_table(
            UINT index,
            std::vector<DescriptorRangeDesc> ranges,
            D3D12_SHADER_VISIBILITY visibility);

        RootSignatureBuilder& set_parameter(
            UINT index,
            ParameterDesc description);

        D3D12_ROOT_SIGNATURE_FLAGS flags_ =
            D3D12_ROOT_SIGNATURE_FLAG_NONE;

        std::vector<ParameterDesc> parameters_;
        std::vector<bool> initialized_;
    };

} // namespace fjr::dx
