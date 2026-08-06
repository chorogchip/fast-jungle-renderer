#include "FastJungle/dx12/RootSignatureBuilder.hpp"

#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/dx12/WindowsUtils.hpp"

#include <algorithm>
#include <utility>

namespace fjr::dx {

    RootSignatureBuilder::ParameterProxy::ParameterProxy(
        RootSignatureBuilder& owner,
        UINT index,
        D3D12_ROOT_PARAMETER_TYPE root_type) noexcept
        : owner_(owner),
        index_(index),
        root_type_(root_type) {}

    RootSignatureBuilder::ParameterProxy&
        RootSignatureBuilder::ParameterProxy::reg(
            UINT shader_register) noexcept {

        shader_register_ = shader_register;
        return *this;
    }

    RootSignatureBuilder::ParameterProxy&
        RootSignatureBuilder::ParameterProxy::count(
            UINT count) noexcept {

        count_ = count;
        return *this;
    }

    RootSignatureBuilder::ParameterProxy&
        RootSignatureBuilder::ParameterProxy::space(
            UINT register_space) noexcept {

        register_space_ = register_space;
        return *this;
    }

    RootSignatureBuilder::ParameterProxy&
        RootSignatureBuilder::ParameterProxy::vis_all() noexcept {
        visibility_ = D3D12_SHADER_VISIBILITY_ALL;
        return *this;
    }

    RootSignatureBuilder::ParameterProxy&
        RootSignatureBuilder::ParameterProxy::vis_vertex() noexcept {
        visibility_ = D3D12_SHADER_VISIBILITY_VERTEX;
        return *this;
    }

    RootSignatureBuilder::ParameterProxy&
        RootSignatureBuilder::ParameterProxy::vis_pixel() noexcept {
        visibility_ = D3D12_SHADER_VISIBILITY_PIXEL;
        return *this;
    }

    RootSignatureBuilder&
        RootSignatureBuilder::ParameterProxy::add() {
        switch (root_type_) {
        case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
            if (count_ == 0) {
                log::Logger::g_logger << log::abrt(
                    "Root constant count cannot be zero.");
            }

            return owner_.add_constants(
                index_,
                shader_register_,
                count_,
                register_space_,
                visibility_);

        case D3D12_ROOT_PARAMETER_TYPE_CBV:
        case D3D12_ROOT_PARAMETER_TYPE_SRV:
        case D3D12_ROOT_PARAMETER_TYPE_UAV:
            return owner_.add_root_descriptor(
                index_,
                root_type_,
                shader_register_,
                register_space_,
                visibility_);

        default:
            log::Logger::g_logger << log::abrt(
                "Root parameter type is invalid.");
        }
    }

    RootSignatureBuilder::TableProxy::RangeProxy::RangeProxy(
        TableProxy& owner,
        D3D12_DESCRIPTOR_RANGE_TYPE type) noexcept
        : owner_(owner) {

        description_.type = type;
    }

    RootSignatureBuilder::TableProxy::RangeProxy&
        RootSignatureBuilder::TableProxy::RangeProxy::reg(
            UINT shader_register) noexcept {

        description_.shader_register = shader_register;
        return *this;
    }

    RootSignatureBuilder::TableProxy::RangeProxy&
        RootSignatureBuilder::TableProxy::RangeProxy::count(
            UINT count) noexcept {

        description_.count = count;
        return *this;
    }

    RootSignatureBuilder::TableProxy::RangeProxy&
        RootSignatureBuilder::TableProxy::RangeProxy::space(
            UINT register_space) noexcept {

        description_.register_space = register_space;
        return *this;
    }

    RootSignatureBuilder::TableProxy::RangeProxy&
        RootSignatureBuilder::TableProxy::RangeProxy::flags(
            D3D12_DESCRIPTOR_RANGE_FLAGS flags) noexcept {

        description_.flags = flags;
        return *this;
    }

    RootSignatureBuilder::TableProxy::RangeProxy&
        RootSignatureBuilder::TableProxy::RangeProxy::offset(
            UINT offset) noexcept {

        description_.offset = offset;
        return *this;
    }

    RootSignatureBuilder::TableProxy&
        RootSignatureBuilder::TableProxy::RangeProxy::add_range() {
        if (description_.count == 0) {
            log::Logger::g_logger << log::abrt(
                "Descriptor range count cannot be zero.");
        }

        const bool is_sampler =
            description_.type ==
            D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;

        if (is_sampler != owner_.sampler_table_) {
            log::Logger::g_logger << log::abrt(
                "Descriptor range type does not match its table.");
        }

        owner_.ranges_.push_back(description_);
        return owner_;
    }

    RootSignatureBuilder::TableProxy::TableProxy(
        RootSignatureBuilder& owner,
        UINT index,
        bool sampler_table) noexcept
        : owner_(owner),
        index_(index),
        sampler_table_(sampler_table) {}

    RootSignatureBuilder::TableProxy::RangeProxy
        RootSignatureBuilder::TableProxy::cbv() {
        return RangeProxy{
            *this,
            D3D12_DESCRIPTOR_RANGE_TYPE_CBV
        };
    }

    RootSignatureBuilder::TableProxy::RangeProxy
        RootSignatureBuilder::TableProxy::srv() {
        return RangeProxy{
            *this,
            D3D12_DESCRIPTOR_RANGE_TYPE_SRV
        };
    }

    RootSignatureBuilder::TableProxy::RangeProxy
        RootSignatureBuilder::TableProxy::uav() {
        return RangeProxy{
            *this,
            D3D12_DESCRIPTOR_RANGE_TYPE_UAV
        };
    }

    RootSignatureBuilder::TableProxy::RangeProxy
        RootSignatureBuilder::TableProxy::sampler() {
        return RangeProxy{
            *this,
            D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER
        };
    }

    RootSignatureBuilder::TableProxy&
        RootSignatureBuilder::TableProxy::vis_all() noexcept {
        visibility_ = D3D12_SHADER_VISIBILITY_ALL;
        return *this;
    }

    RootSignatureBuilder::TableProxy&
        RootSignatureBuilder::TableProxy::vis_vertex() noexcept {
        visibility_ = D3D12_SHADER_VISIBILITY_VERTEX;
        return *this;
    }

    RootSignatureBuilder::TableProxy&
        RootSignatureBuilder::TableProxy::vis_pixel() noexcept {
        visibility_ = D3D12_SHADER_VISIBILITY_PIXEL;
        return *this;
    }

    RootSignatureBuilder&
        RootSignatureBuilder::TableProxy::add() {
        if (ranges_.empty()) {
            log::Logger::g_logger << log::abrt(
                "Descriptor table cannot be empty.");
        }

        return owner_.add_descriptor_table(
            index_,
            std::move(ranges_),
            visibility_);
    }

    void RootSignatureBuilder::init(UINT parameter_count) {
        parameters_.assign(parameter_count, {});
        initialized_.assign(parameter_count, false);

        flags_ = D3D12_ROOT_SIGNATURE_FLAG_NONE;
    }

    RootSignatureBuilder&
        RootSignatureBuilder::reset() noexcept {
        parameters_.clear();
        initialized_.clear();

        flags_ = D3D12_ROOT_SIGNATURE_FLAG_NONE;

        return *this;
    }

    RootSignatureBuilder&
        RootSignatureBuilder::set_flags(
            D3D12_ROOT_SIGNATURE_FLAGS flags) noexcept {

        flags_ = flags;
        return *this;
    }

    RootSignatureBuilder::ParameterProxy
        RootSignatureBuilder::set_constants(UINT index) {
        return ParameterProxy{
            *this,
            index,
            D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS
        };
    }

    RootSignatureBuilder::ParameterProxy
        RootSignatureBuilder::set_root_cbv(UINT index) {
        return ParameterProxy{
            *this,
            index,
            D3D12_ROOT_PARAMETER_TYPE_CBV
        };
    }

    RootSignatureBuilder::ParameterProxy
        RootSignatureBuilder::set_root_srv(UINT index) {
        return ParameterProxy{
            *this,
            index,
            D3D12_ROOT_PARAMETER_TYPE_SRV
        };
    }

    RootSignatureBuilder::ParameterProxy
        RootSignatureBuilder::set_root_uav(UINT index) {
        return ParameterProxy{
            *this,
            index,
            D3D12_ROOT_PARAMETER_TYPE_UAV
        };
    }

    RootSignatureBuilder::TableProxy
        RootSignatureBuilder::set_resource_table(UINT index) {
        return TableProxy{
            *this,
            index,
            false
        };
    }

    RootSignatureBuilder::TableProxy
        RootSignatureBuilder::set_sampler_table(UINT index) {
        return TableProxy{
            *this,
            index,
            true
        };
    }

    RootSignatureBuilder&
        RootSignatureBuilder::add_constants(
            UINT index,
            UINT shader_register,
            UINT value_count,
            UINT register_space,
            D3D12_SHADER_VISIBILITY visibility) {

        ParameterDesc description{};
        description.kind = ParameterKind::constants;
        description.root_type =
            D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        description.shader_register = shader_register;
        description.register_space = register_space;
        description.count = value_count;
        description.visibility = visibility;

        return set_parameter(
            index,
            std::move(description));
    }

    RootSignatureBuilder&
        RootSignatureBuilder::add_root_descriptor(
            UINT index,
            D3D12_ROOT_PARAMETER_TYPE type,
            UINT shader_register,
            UINT register_space,
            D3D12_SHADER_VISIBILITY visibility) {

        ParameterDesc description{};
        description.kind = ParameterKind::root_descriptor;
        description.root_type = type;
        description.shader_register = shader_register;
        description.register_space = register_space;
        description.visibility = visibility;

        return set_parameter(
            index,
            std::move(description));
    }

    RootSignatureBuilder&
        RootSignatureBuilder::add_descriptor_table(
            UINT index,
            std::vector<DescriptorRangeDesc> ranges,
            D3D12_SHADER_VISIBILITY visibility) {

        ParameterDesc description{};
        description.kind = ParameterKind::descriptor_table;
        description.root_type =
            D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        description.visibility = visibility;
        description.ranges = std::move(ranges);

        return set_parameter(
            index,
            std::move(description));
    }

    RootSignatureBuilder&
        RootSignatureBuilder::set_parameter(
            UINT index,
        ParameterDesc description) {

        if (index >= parameters_.size()) {
            log::Logger::g_logger << log::abrt(
                "Root parameter index is out of range.");
        }

        if (initialized_[index]) {
            log::Logger::g_logger << log::abrt(
                "Root parameter is already initialized.");
        }

        parameters_[index] = std::move(description);
        initialized_[index] = true;

        return *this;
    }

    Microsoft::WRL::ComPtr<ID3D12RootSignature>
        RootSignatureBuilder::build(
            ID3D12Device* device) const {

        const bool all_initialized = std::all_of(
            initialized_.begin(),
            initialized_.end(),
            [](bool initialized) {
                return initialized;
            });

        if (!all_initialized) {
            log::Logger::g_logger << log::abrt(
                "Every root parameter must be initialized.");
        }

        std::vector<D3D12_ROOT_PARAMETER1> root_parameters(
            parameters_.size());

        std::vector<std::vector<D3D12_DESCRIPTOR_RANGE1>>
            descriptor_ranges(parameters_.size());

        for (UINT index = 0;
            index < static_cast<UINT>(parameters_.size());
            ++index) {

            const ParameterDesc& source = parameters_[index];

            D3D12_ROOT_PARAMETER1& destination =
                root_parameters[index];

            destination.ParameterType = source.root_type;
            destination.ShaderVisibility = source.visibility;

            switch (source.kind) {
            case ParameterKind::constants:
                destination.Constants.ShaderRegister =
                    source.shader_register;
                destination.Constants.RegisterSpace =
                    source.register_space;
                destination.Constants.Num32BitValues =
                    source.count;
                break;

            case ParameterKind::root_descriptor:
                destination.Descriptor.ShaderRegister =
                    source.shader_register;
                destination.Descriptor.RegisterSpace =
                    source.register_space;
                destination.Descriptor.Flags =
                    D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
                break;

            case ParameterKind::descriptor_table: {
                auto& ranges = descriptor_ranges[index];

                ranges.reserve(source.ranges.size());

                for (const DescriptorRangeDesc& source_range :
                    source.ranges) {

                    D3D12_DESCRIPTOR_RANGE1 range{};

                    range.RangeType = source_range.type;
                    range.NumDescriptors = source_range.count;
                    range.BaseShaderRegister =
                        source_range.shader_register;
                    range.RegisterSpace =
                        source_range.register_space;
                    range.Flags = source_range.flags;
                    range.OffsetInDescriptorsFromTableStart =
                        source_range.offset;

                    ranges.push_back(range);
                }

                destination.DescriptorTable.NumDescriptorRanges =
                    static_cast<UINT>(ranges.size());

                destination.DescriptorTable.pDescriptorRanges =
                    ranges.data();

                break;
            }

            default:
                log::Logger::g_logger << log::abrt(
                    "Root parameter kind is invalid.");
            }
        }

        D3D12_VERSIONED_ROOT_SIGNATURE_DESC description{};
        description.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;

        description.Desc_1_1.NumParameters =
            static_cast<UINT>(root_parameters.size());

        description.Desc_1_1.pParameters =
            root_parameters.empty()
            ? nullptr
            : root_parameters.data();

        description.Desc_1_1.NumStaticSamplers = 0;
        description.Desc_1_1.pStaticSamplers = nullptr;
        description.Desc_1_1.Flags = flags_;

        Microsoft::WRL::ComPtr<ID3DBlob> serialized;
        Microsoft::WRL::ComPtr<ID3DBlob> errors;

        abort_failed(D3D12SerializeVersionedRootSignature(
            &description,
            serialized.ReleaseAndGetAddressOf(),
            errors.ReleaseAndGetAddressOf()));

        Microsoft::WRL::ComPtr<ID3D12RootSignature>
            root_signature;

        abort_failed(device->CreateRootSignature(
            0,
            serialized->GetBufferPointer(),
            serialized->GetBufferSize(),
            IID_PPV_ARGS(
                root_signature.ReleaseAndGetAddressOf())));

        return root_signature;
    }

} // namespace fjr::dx
