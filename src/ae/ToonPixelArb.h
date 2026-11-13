#pragma once

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace arbitrary {

constexpr std::size_t kMaxGlyphs = 64;
struct RampData {
    A_u_long version = 1;
    A_u_long count = 10;
    A_u_long selected = 0;
    A_u_short codepoints[kMaxGlyphs]{32, 46, 58, 45, 61, 43, 42, 35, 37, 64};
};

inline const RampData* get(PF_ArbitraryH handle) {
    return handle ? reinterpret_cast<const RampData*>(*handle) : nullptr;
}

inline RampData* get_mutable(PF_ArbitraryH handle) {
    return handle ? reinterpret_cast<RampData*>(*handle) : nullptr;
}

inline PF_Err validate(const RampData* data) {
    return data && data->version == 1 && data->count >= 1 && data->count <= kMaxGlyphs &&
        data->selected < data->count ? PF_Err_NONE : PF_Err_BAD_CALLBACK_PARAM;
}

inline PF_Err create(const RampData& source, PF_ArbitraryH* output) {
    if (!output) return PF_Err_BAD_CALLBACK_PARAM;
#ifdef _WIN32
    auto* holder = static_cast<PF_ArbitraryH>(std::malloc(sizeof(PF_ArbitraryH)));
    if (!holder) return PF_Err_OUT_OF_MEMORY;
    *holder = std::malloc(sizeof(RampData));
    if (!*holder) {
        std::free(holder);
        return PF_Err_OUT_OF_MEMORY;
    }
    *output = holder;
#else
    *output = NewHandle(sizeof(RampData));
    if (!*output) return PF_Err_OUT_OF_MEMORY;
#endif
    std::memcpy(**output, &source, sizeof(RampData));
    return PF_Err_NONE;
}

inline PF_Err create_with_host(PF_InData* in_data, const RampData& source, PF_ArbitraryH* output) {
    if (!output) return PF_Err_BAD_CALLBACK_PARAM;
    if (!in_data || !in_data->utils) return create(source, output);
    *output = PF_NEW_HANDLE(sizeof(RampData));
    if (!*output) return PF_Err_OUT_OF_MEMORY;
    std::memcpy(**output, &source, sizeof(RampData));
    return PF_Err_NONE;
}

inline void release(PF_InData* in_data, PF_ArbitraryH handle) {
    if (!handle) return;
    if (in_data && in_data->utils) {
        PF_DISPOSE_HANDLE(handle);
        return;
    }
#ifdef _WIN32
    std::free(*handle);
    std::free(handle);
#else
    DisposeHandle(handle);
#endif
}

inline std::vector<A_u_short> codepoints(PF_ArbitraryH handle) {
    const auto* data = get(handle);
    if (validate(data) != PF_Err_NONE) return {32, 46, 58, 45, 61, 43, 42, 35, 37, 64};
    return {data->codepoints, data->codepoints + data->count};
}

inline PF_Err replace(PF_ParamDef* parameter, const std::vector<A_u_short>& cps, std::size_t selected,
    PF_InData* in_data = nullptr) {
    if (!parameter || cps.empty() || cps.size() > kMaxGlyphs) return PF_Err_BAD_CALLBACK_PARAM;
    RampData data;
    data.count = static_cast<A_u_long>(cps.size());
    data.selected = static_cast<A_u_long>(std::min(selected, cps.size() - 1));
    std::copy(cps.begin(), cps.end(), data.codepoints);
    PF_ArbitraryH replacement = nullptr;
    const PF_Err error = create_with_host(in_data, data, &replacement);
    if (error) return error;
    release(in_data, parameter->u.arb_d.value);
    parameter->u.arb_d.value = replacement;
    parameter->flags |= PF_ChangeFlag_CHANGED_VALUE;
    return PF_Err_NONE;
}

inline PF_Err callback(PF_InData* in_data, PF_ArbParamsExtra* extra) {
    if (!extra) return PF_Err_BAD_CALLBACK_PARAM;
    switch (extra->which_function) {
        case PF_Arbitrary_NEW_FUNC: {
            if (!extra->u.new_func_params.arbPH) return PF_Err_BAD_CALLBACK_PARAM;
            return create_with_host(in_data, RampData{}, extra->u.new_func_params.arbPH);
        }
        case PF_Arbitrary_DISPOSE_FUNC:
            release(in_data, extra->u.dispose_func_params.arbH);
            return PF_Err_NONE;
        case PF_Arbitrary_COPY_FUNC: {
            const auto* source = get(extra->u.copy_func_params.src_arbH);
            if (validate(source) != PF_Err_NONE) return PF_Err_BAD_CALLBACK_PARAM;
            return create_with_host(in_data, *source, extra->u.copy_func_params.dst_arbPH);
        }
        case PF_Arbitrary_FLAT_SIZE_FUNC:
            if (!extra->u.flat_size_func_params.flat_data_sizePLu || validate(get(extra->u.flat_size_func_params.arbH))) {
                return PF_Err_BAD_CALLBACK_PARAM;
            }
            *extra->u.flat_size_func_params.flat_data_sizePLu = sizeof(RampData);
            return PF_Err_NONE;
        case PF_Arbitrary_FLATTEN_FUNC: {
            const auto& params = extra->u.flatten_func_params;
            const auto* source = get(params.arbH);
            if (validate(source) != PF_Err_NONE || !params.flat_dataPV || params.buf_sizeLu < sizeof(RampData)) {
                return PF_Err_BAD_CALLBACK_PARAM;
            }
            std::memcpy(params.flat_dataPV, source, sizeof(RampData));
            return PF_Err_NONE;
        }
        case PF_Arbitrary_UNFLATTEN_FUNC: {
            const auto& params = extra->u.unflatten_func_params;
            if (!params.flat_dataPV || params.buf_sizeLu != sizeof(RampData)) return PF_Err_BAD_CALLBACK_PARAM;
            RampData data;
            std::memcpy(&data, params.flat_dataPV, sizeof(RampData));
            const PF_Err valid = validate(&data);
            if (valid) return valid;
            return create_with_host(in_data, data, params.arbPH);
        }
        case PF_Arbitrary_INTERP_FUNC: {
            const auto& params = extra->u.interp_func_params;
            const auto* source = get(params.tF < 0.5 ? params.left_arbH : params.right_arbH);
            if (validate(source) != PF_Err_NONE) return PF_Err_BAD_CALLBACK_PARAM;
            return create_with_host(in_data, *source, params.interpPH);
        }
        case PF_Arbitrary_COMPARE_FUNC: {
            const auto& params = extra->u.compare_func_params;
            const auto* first = get(params.a_arbH);
            const auto* second = get(params.b_arbH);
            if (!params.compareP || validate(first) || validate(second)) return PF_Err_BAD_CALLBACK_PARAM;
            *params.compareP = std::memcmp(first, second, sizeof(RampData)) == 0 ? PF_ArbCompare_EQUAL : PF_ArbCompare_NOT_EQUAL;
            return PF_Err_NONE;
        }
        case PF_Arbitrary_PRINT_SIZE_FUNC:
            if (!extra->u.print_size_func_params.print_sizePLu) return PF_Err_BAD_CALLBACK_PARAM;
            *extra->u.print_size_func_params.print_sizePLu = kMaxGlyphs * 8 + 32;
            return PF_Err_NONE;
        case PF_Arbitrary_PRINT_FUNC: {
            const auto& params = extra->u.print_func_params;
            const auto* data = get(params.arbH);
            if (validate(data) != PF_Err_NONE || !params.print_bufferPC) return PF_Err_BAD_CALLBACK_PARAM;
            std::string text;
            for (A_u_long index = 0; index < data->count; ++index) {
                char buffer[16];
                std::snprintf(buffer, sizeof(buffer), index ? " %04X" : "%04X", data->codepoints[index]);
                text += buffer;
            }
            if (text.size() + 1 > params.print_sizeLu) return PF_Err_BAD_CALLBACK_PARAM;
            std::memcpy(params.print_bufferPC, text.c_str(), text.size() + 1);
            return PF_Err_NONE;
        }
        case PF_Arbitrary_SCAN_FUNC: {
            const auto& params = extra->u.scan_func_params;
            if (!params.bufPC || params.bytes_to_scanLu > kMaxGlyphs * 6) return PF_Err_BAD_CALLBACK_PARAM;
            std::vector<A_u_short> values;
            std::size_t offset = 0;
            while (offset + 4 <= params.bytes_to_scanLu && values.size() < kMaxGlyphs) {
                unsigned value = 0;
                if (std::sscanf(params.bufPC + offset, "%4x", &value) != 1 || value > 0xFFFF) break;
                values.push_back(static_cast<A_u_short>(value));
                offset += 4;
                while (offset < params.bytes_to_scanLu && params.bufPC[offset] == ' ') ++offset;
            }
            if (values.empty()) return PF_Err_BAD_CALLBACK_PARAM;
            RampData data;
            data.count = static_cast<A_u_long>(values.size());
            std::copy(values.begin(), values.end(), data.codepoints);
            return create_with_host(in_data, data, params.arbPH);
        }
        default:
            return PF_Err_BAD_CALLBACK_PARAM;
    }
}

}  // namespace arbitrary
