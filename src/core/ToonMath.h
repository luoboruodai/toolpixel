#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace toonpixel {

constexpr float kRedLuma = 0.2126f;
constexpr float kGreenLuma = 0.7152f;
constexpr float kBlueLuma = 0.0722f;
constexpr float kUnmanagedGamma = 2.4f;

struct Rgb {
    float red;
    float green;
    float blue;
};

struct Oklab {
    float l;
    float a;
    float b;
};

enum class ToonModel {
    preserve_hue = 1,
    luma_ramp = 2,
};

enum class PaletteMode {
    original8 = 1,
    style10 = 2,
    custom = 3,
    ramp_linked = 4,
};

enum class DitherMode {
    off = 1,
    bayer = 2,
    floyd_steinberg = 3,
};

enum class PixelOutlineMode {
    grid_edge = 1,
    legacy_block = 2,
};

inline float ordered_threshold(int x, int y, int order);

constexpr std::array<Rgb, 8> kPalette = {{
    {0.012f, 0.016f, 0.024f},
    {0.055f, 0.045f, 0.075f},
    {0.16f, 0.065f, 0.055f},
    {0.42f, 0.19f, 0.12f},
    {0.055f, 0.20f, 0.32f},
    {0.12f, 0.43f, 0.58f},
    {0.56f, 0.58f, 0.52f},
    {0.92f, 0.86f, 0.68f},
}};

constexpr std::array<Rgb, 10> kStylePalette = {{
    {0.012f, 0.016f, 0.024f},
    {0.055f, 0.045f, 0.075f},
    {0.16f, 0.065f, 0.055f},
    {0.42f, 0.19f, 0.12f},
    {0.055f, 0.20f, 0.32f},
    {0.12f, 0.43f, 0.58f},
    {0.56f, 0.58f, 0.52f},
    {0.92f, 0.86f, 0.68f},
    {0.08f, 0.085f, 0.09f},
    {0.24f, 0.24f, 0.24f},
}};

struct GradeSettings {
    float exposure = 0.0f;
    float vibrance = 0.0f;
    float hue = 0.0f;
    float chroma = 1.0f;
    Rgb tint{1.0f, 1.0f, 1.0f};
    float tint_amount = 0.0f;
};

struct RampSettings {
    std::array<float, 4> boundaries{0.08f, 0.22f, 0.45f, 0.72f};
    float softness = 0.0f;
    int repeat = 1;
    std::array<Rgb, 5> colors{{
        {0.012f, 0.016f, 0.024f},
        {0.16f, 0.065f, 0.055f},
        {0.055f, 0.20f, 0.32f},
        {0.56f, 0.58f, 0.52f},
        {0.92f, 0.86f, 0.68f},
    }};
};

constexpr std::array<int, 16> kBayer4 = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};

inline float band_multiplier(float luminance) {
    if (luminance < 0.08f) {
        return 0.30f;
    }
    if (luminance < 0.22f) {
        return 0.65f;
    }
    if (luminance < 0.45f) {
        return 1.00f;
    }
    return 1.25f;
}

inline float luminance(const Rgb& color) {
    return kRedLuma * color.red + kGreenLuma * color.green + kBlueLuma * color.blue;
}

inline float decode_unmanaged(float value) {
    return value <= 0.0f ? 0.0f : std::pow(value, kUnmanagedGamma);
}

inline float encode_unmanaged(float value) {
    return value <= 0.0f ? 0.0f : std::pow(value, 1.0f / kUnmanagedGamma);
}

inline Oklab linear_to_oklab(Rgb color) {
    const float l = 0.4122214708f * color.red + 0.5363325363f * color.green + 0.0514459929f * color.blue;
    const float m = 0.2119034982f * color.red + 0.6806995451f * color.green + 0.1073969566f * color.blue;
    const float s = 0.0883024619f * color.red + 0.2817188376f * color.green + 0.6299787005f * color.blue;
    const float l_root = std::cbrt(l);
    const float m_root = std::cbrt(m);
    const float s_root = std::cbrt(s);
    return {
        0.2104542553f * l_root + 0.7936377774f * m_root - 0.0040720468f * s_root,
        1.9779984951f * l_root - 2.4285922050f * m_root + 0.4505937099f * s_root,
        0.0259040371f * l_root + 0.7827717662f * m_root - 0.8086757660f * s_root,
    };
}

inline std::array<Oklab, 8> palette_oklab() {
    std::array<Oklab, 8> result{};
    for (std::size_t index = 0; index < kPalette.size(); ++index) {
        result[index] = linear_to_oklab(kPalette[index]);
    }
    return result;
}

inline int floor_div(int value, int factor) {
    return value >= 0 ? value / factor : -((-value + factor - 1) / factor);
}

inline int floor_mod(int value, int factor) {
    const int remainder = value % factor;
    return remainder < 0 ? remainder + factor : remainder;
}

inline float bayer_threshold(int x, int y) {
    const int index = kBayer4[floor_mod(y, 4) * 4 + floor_mod(x, 4)];
    return (static_cast<float>(index) + 0.5f) / 16.0f - 0.5f;
}

inline std::size_t nearest_palette(const Rgb& color, float dither, int x, int y,
    const std::array<Oklab, 8>& palette) {
    Oklab lab = linear_to_oklab(color);
    lab.l += bayer_threshold(x, y) * dither;
    std::size_t best = 0;
    float best_distance = 1.0e30f;
    for (std::size_t index = 0; index < palette.size(); ++index) {
        const float dl = lab.l - palette[index].l;
        const float da = lab.a - palette[index].a;
        const float db = lab.b - palette[index].b;
        const float distance = dl * dl + da * da + db * db;
        if (distance < best_distance) {
            best_distance = distance;
            best = index;
        }
    }
    return best;
}

inline Rgb oklab_to_linear(const Oklab& lab) {
    const float l = std::pow(lab.l + 0.3963377774f * lab.a + 0.2158037573f * lab.b, 3.0f);
    const float m = std::pow(lab.l - 0.1055613458f * lab.a - 0.0638541728f * lab.b, 3.0f);
    const float s = std::pow(lab.l - 0.0894841775f * lab.a - 1.2914855480f * lab.b, 3.0f);
    return {
        std::clamp(4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s, 0.0f, 1.0f),
        std::clamp(-1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s, 0.0f, 1.0f),
        std::clamp(-0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s, 0.0f, 1.0f),
    };
}

inline Rgb mix_rgb(const Rgb& first, const Rgb& second, float amount) {
    const float t = std::clamp(amount, 0.0f, 1.0f);
    return {
        first.red + (second.red - first.red) * t,
        first.green + (second.green - first.green) * t,
        first.blue + (second.blue - first.blue) * t,
    };
}

inline bool is_neutral_grade(const GradeSettings& settings) {
    return settings.exposure == 0.0f && settings.vibrance == 0.0f && settings.hue == 0.0f &&
        settings.chroma == 1.0f && settings.tint_amount == 0.0f;
}

inline Rgb oklab_to_linear_unclamped(const Oklab& lab);

inline Rgb apply_color_grade(Rgb color, const GradeSettings& settings) {
    if (is_neutral_grade(settings)) {
        return color;
    }
    const float exposure = std::exp2(std::clamp(settings.exposure, -2.0f, 2.0f));
    color.red *= exposure;
    color.green *= exposure;
    color.blue *= exposure;
    const bool needs_oklab = settings.vibrance != 0.0f || settings.hue != 0.0f || settings.chroma != 1.0f;
    if (std::max({color.red, color.green, color.blue}) - std::min({color.red, color.green, color.blue}) <= 0.000001f) {
        color = {color.red, color.red, color.red};
    } else if (needs_oklab) {
        Oklab lab = linear_to_oklab(color);
        const float radians = std::clamp(settings.hue, -180.0f, 180.0f) * 3.14159265358979323846f / 180.0f;
        const float cosine = std::cos(radians);
        const float sine = std::sin(radians);
        const float chroma = std::sqrt(lab.a * lab.a + lab.b * lab.b);
        const float vibrance = std::max(0.0f,
            1.0f + std::clamp(settings.vibrance, -1.0f, 1.0f) * (1.0f - std::min(chroma / 0.25f, 1.0f)));
        const float factor = vibrance * std::max(0.0f, settings.chroma);
        if (chroma <= 0.00001f) {
            lab.a = 0.0f;
            lab.b = 0.0f;
        } else {
            const float a = (lab.a * cosine - lab.b * sine) * factor;
            const float b = (lab.a * sine + lab.b * cosine) * factor;
            lab.a = a;
            lab.b = b;
        }
        color = oklab_to_linear_unclamped(lab);
    }
    const Rgb tinted{
        color.red * std::clamp(settings.tint.red, 0.0f, 1.0f),
        color.green * std::clamp(settings.tint.green, 0.0f, 1.0f),
        color.blue * std::clamp(settings.tint.blue, 0.0f, 1.0f),
    };
    return mix_rgb(color, tinted, settings.tint_amount);
}

inline Rgb sample_ramp(float luminance, const RampSettings& settings) {
    auto boundaries = settings.boundaries;
    for (float& boundary : boundaries) {
        boundary = std::clamp(boundary, 0.0f, 1.0f);
    }
    std::sort(boundaries.begin(), boundaries.end());
    const int repeat = std::clamp(settings.repeat, 1, 8);
    const float luminance01 = std::clamp(luminance, 0.0f, 1.0f);
    const float scaled = luminance01 * static_cast<float>(repeat);
    const int cycle = luminance01 >= 1.0f ? repeat - 1 : static_cast<int>(std::floor(scaled));
    const float local = luminance01 >= 1.0f ? 1.0f : scaled - static_cast<float>(cycle);
    const float softness = std::clamp(settings.softness, 0.0f, 0.5f);
    for (std::size_t index = 0; index < boundaries.size(); ++index) {
        if (softness > 0.0f) {
            const float start = boundaries[index] - softness * 0.5f;
            const float end = boundaries[index] + softness * 0.5f;
            if (local >= start && local <= end) {
                const float t = std::clamp((local - start) / (end - start), 0.0f, 1.0f);
                return mix_rgb(settings.colors[index], settings.colors[index + 1], t * t * (3.0f - 2.0f * t));
            }
        }
        if (local < boundaries[index]) {
            return settings.colors[index];
        }
    }
    return settings.colors.back();
}

inline std::vector<Oklab> palette_oklab(const std::vector<Rgb>& palette) {
    std::vector<Oklab> result;
    result.reserve(palette.size());
    for (const Rgb& color : palette) {
        result.push_back(linear_to_oklab(color));
    }
    return result;
}

inline std::size_t nearest_palette_lab(const Oklab& lab, const std::vector<Oklab>& palette) {
    std::size_t best = 0;
    float best_distance = 1.0e30f;
    for (std::size_t index = 0; index < palette.size(); ++index) {
        const float dl = lab.l - palette[index].l;
        const float da = lab.a - palette[index].a;
        const float db = lab.b - palette[index].b;
        const float distance = dl * dl + da * da + db * db;
        if (distance < best_distance) {
            best_distance = distance;
            best = index;
        }
    }
    return best;
}

inline std::size_t nearest_palette(const Rgb& color, float dither, int x, int y,
    const std::vector<Oklab>& palette, float threshold = 0.5f) {
    Oklab lab = linear_to_oklab(color);
    lab.l += (bayer_threshold(x, y) + (0.5f - std::clamp(threshold, 0.0f, 1.0f))) * dither;
    return nearest_palette_lab(lab, palette);
}

inline std::size_t nearest_palette_ordered(const Rgb& color, float dither, int x, int y,
    const std::vector<Oklab>& palette, float threshold, int matrix) {
    Oklab lab = linear_to_oklab(color);
    lab.l += (ordered_threshold(x, y, matrix == 8 ? 3 : 2) +
        (0.5f - std::clamp(threshold, 0.0f, 1.0f))) * dither;
    return nearest_palette_lab(lab, palette);
}

inline std::vector<std::size_t> floyd_steinberg(
    std::vector<Oklab> cells,
    const std::vector<float>& alpha,
    int width,
    int height,
    const std::vector<Oklab>& palette,
    float strength,
    float threshold = 0.5f,
    const std::vector<float>& decay = {}) {
    if (width <= 0 || height <= 0 || palette.empty() ||
        cells.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height) ||
        alpha.size() != cells.size() || (!decay.empty() && decay.size() != cells.size())) {
        return {};
    }
    const float base_amount = std::clamp(strength, 0.0f, 1.0f);
    std::vector<std::size_t> indices(cells.size(), 0);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t index = static_cast<std::size_t>(y) * width + x;
            const float amount = base_amount * (decay.empty() ? 1.0f : decay[index]);
            const float bias = (0.5f - std::clamp(threshold, 0.0f, 1.0f)) * amount;
            Oklab current = cells[index];
            current.l += bias;
            const std::size_t selected = nearest_palette_lab(current, palette);
            indices[index] = selected;
            if (alpha[index] <= 0.0f || amount == 0.0f) {
                continue;
            }
            const Oklab error{
                (current.l - palette[selected].l) * amount,
                (current.a - palette[selected].a) * amount,
                (current.b - palette[selected].b) * amount,
            };
            const auto diffuse = [&](int target_x, int target_y, float weight) {
                if (target_x < 0 || target_x >= width || target_y >= height) {
                    return;
                }
                Oklab& target = cells[static_cast<std::size_t>(target_y) * width + target_x];
                target.l += error.l * weight;
                target.a += error.a * weight;
                target.b += error.b * weight;
            };
            diffuse(x + 1, y, 7.0f / 16.0f);
            if (y + 1 < height) {
                diffuse(x - 1, y + 1, 3.0f / 16.0f);
                diffuse(x, y + 1, 5.0f / 16.0f);
                diffuse(x + 1, y + 1, 1.0f / 16.0f);
            }
        }
    }
    return indices;
}

inline Rgb oklab_to_linear_unclamped(const Oklab& lab) {
    const float l = std::pow(lab.l + 0.3963377774f * lab.a + 0.2158037573f * lab.b, 3.0f);
    const float m = std::pow(lab.l - 0.1055613458f * lab.a - 0.0638541728f * lab.b, 3.0f);
    const float s = std::pow(lab.l - 0.0894841775f * lab.a - 1.2914855480f * lab.b, 3.0f);
    return {
        4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s,
        -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s,
        -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s,
    };
}

inline bool in_gamut(const Rgb& color, float epsilon = 0.00001f) {
    return color.red >= -epsilon && color.red <= 1.0f + epsilon &&
        color.green >= -epsilon && color.green <= 1.0f + epsilon &&
        color.blue >= -epsilon && color.blue <= 1.0f + epsilon;
}

inline float gamut_chroma(const Oklab& lab, float margin = 0.0f) {
    const float chroma = std::hypot(lab.a, lab.b);
    if (chroma < 0.000001f) {
        return 0.0f;
    }
    const Oklab target{lab.l, lab.a * (1.0f - std::clamp(margin, 0.0f, 0.25f)),
        lab.b * (1.0f - std::clamp(margin, 0.0f, 0.25f))};
    if (in_gamut(oklab_to_linear_unclamped(target))) {
        return std::hypot(target.a, target.b);
    }
    float low = 0.0f;
    float high = 1.0f;
    for (int iteration = 0; iteration < 24; ++iteration) {
        const float factor = (low + high) * 0.5f;
        if (in_gamut(oklab_to_linear_unclamped({target.l, target.a * factor, target.b * factor}))) {
            low = factor;
        } else {
            high = factor;
        }
    }
    return chroma * low * (1.0f - std::clamp(margin, 0.0f, 0.25f));
}

inline Rgb apply_gamut(Rgb color, int mode, float margin = 0.0f) {
    if (mode <= 1) {
        return color;
    }
    const Oklab lab = linear_to_oklab(color);
    if (mode == 5) {
        return in_gamut(color) ? Rgb{lab.l, lab.l, lab.l} : Rgb{1.0f, 0.0f, 0.65f};
    }
    if (mode == 4) {
        return {std::clamp(color.red, 0.0f, 1.0f), std::clamp(color.green, 0.0f, 1.0f),
            std::clamp(color.blue, 0.0f, 1.0f)};
    }
    if (in_gamut(color) && margin == 0.0f) {
        return color;
    }
    const float chroma = std::hypot(lab.a, lab.b);
    if (chroma < 0.000001f) {
        return oklab_to_linear(lab);
    }
    const float angle = std::atan2(lab.b, lab.a);
    if (mode == 3) {
        const float limited = gamut_chroma(lab, margin);
        return oklab_to_linear({lab.l, limited * std::cos(angle), limited * std::sin(angle)});
    }
    float best_chroma = gamut_chroma(lab, margin);
    float best_angle = angle;
    float best_cost = (chroma - best_chroma) / std::max(chroma, 0.0001f);
    for (int offset = 1; offset <= 12; ++offset) {
        for (const float direction : {-1.0f, 1.0f}) {
            const float candidate_angle = angle + direction * static_cast<float>(offset) *
                (1.25f * 3.14159265358979323846f / 180.0f);
            const Oklab candidate{lab.l, chroma * std::cos(candidate_angle), chroma * std::sin(candidate_angle)};
            const float available = gamut_chroma(candidate, margin);
            const float loss = (chroma - available) / std::max(chroma, 0.0001f);
            const float cost = loss + static_cast<float>(offset) * 0.008f;
            if (available > best_chroma && cost < best_cost) {
                best_chroma = available;
                best_angle = candidate_angle;
                best_cost = cost;
            }
        }
    }
    return oklab_to_linear({lab.l, best_chroma * std::cos(best_angle), best_chroma * std::sin(best_angle)});
}

inline int bayer_index(int x, int y, int order) {
    int result = 0;
    for (int level = 0; level < order; ++level) {
        constexpr int base[4] = {0, 2, 3, 1};
        result = result * 4 + base[floor_mod(y, 2) * 2 + floor_mod(x, 2)];
        x = floor_div(x, 2);
        y = floor_div(y, 2);
    }
    return result;
}

inline float ordered_threshold(int x, int y, int order = 2) {
    order = std::clamp(order, 1, 3);
    const int count = 1 << (2 * order);
    return (static_cast<float>(bayer_index(x, y, order)) + 0.5f) / count - 0.5f;
}

struct DecaySettings {
    int shape = 1;
    float amount = 0.0f;
    float center_x = 0.5f;
    float center_y = 0.5f;
    float scale = 0.75f;
    float angle = 0.0f;
};

inline float dither_decay(float x, float y, float aspect, const DecaySettings& settings) {
    if (settings.shape == 1 || settings.amount <= 0.0f) {
        return 1.0f;
    }
    const float dx = (x - settings.center_x) * std::max(aspect, 0.0001f);
    const float dy = y - settings.center_y;
    const float radians = settings.angle * 3.14159265358979323846f / 180.0f;
    const float distance = settings.shape == 3
        ? std::hypot(dx, dy)
        : std::fabs(dx * std::cos(radians) + dy * std::sin(radians));
    const float progress = std::clamp(distance / std::max(settings.scale, 0.01f), 0.0f, 1.0f);
    return 1.0f - std::clamp(settings.amount, 0.0f, 1.0f) * progress;
}

}  // namespace toonpixel
