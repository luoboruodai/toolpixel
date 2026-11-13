#pragma once

#include "ToonPixelGlyphs.h"
#include "ToonPixelSettings.h"
#include "ToonPixelScene.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

namespace studio {

enum class Hit : int {
    none = 0,
    preserve_hue,
    luma_ramp,
    analyze,
    boundary1,
    boundary2,
    boundary3,
    boundary4,
    color1,
    color2,
    color3,
    color4,
    color5,
    softness,
    repeat_down,
    repeat_up,
    palette_mode,
    palette_down,
    palette_up,
    palette1,
    palette2,
    palette3,
    palette4,
    palette5,
    palette6,
    palette7,
    palette8,
    palette9,
    palette10,
    dither_mode,
    dither_matrix,
    dither_threshold,
    tint,
    tint_amount,
    exposure,
    vibrance,
    hue,
    chroma,
};

inline unsigned char channel(float value) {
    return static_cast<unsigned char>(std::lround(
        std::clamp(toonpixel::encode_unmanaged(value), 0.0f, 1.0f) * 255.0f));
}

inline void put_pixel(std::vector<unsigned char>& pixels, int width, int height, int x, int y,
    unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha = 255) {
    if (x < 0 || x >= width || y < 0 || y >= height) return;
    const std::size_t offset = (static_cast<std::size_t>(y) * width + x) * 4;
    pixels[offset] = red;
    pixels[offset + 1] = green;
    pixels[offset + 2] = blue;
    pixels[offset + 3] = alpha;
}

inline void rect(std::vector<unsigned char>& pixels, int width, int height, int left, int top,
    int right, int bottom, unsigned char red, unsigned char green, unsigned char blue) {
    for (int y = std::max(0, top); y < std::min(height, bottom); ++y) {
        for (int x = std::max(0, left); x < std::min(width, right); ++x) {
            put_pixel(pixels, width, height, x, y, red, green, blue);
        }
    }
}

inline Hit ramp_hit(float x, float y) {
    if (y < 0.12f) {
        if (x < 0.50f) return Hit::preserve_hue;
        if (x < 0.75f) return Hit::luma_ramp;
        return Hit::analyze;
    }
    if (y < 0.33f) return Hit::boundary1;
    if (y < 0.41f) return static_cast<Hit>(static_cast<int>(Hit::color1) +
        std::clamp(static_cast<int>(x * 5.0f), 0, 4));
    if (y < 0.55f) {
        if (x < 0.66f) return Hit::softness;
        return x > 0.85f ? Hit::repeat_up : Hit::repeat_down;
    }
    if (y < 0.66f) {
        if (x < 0.70f) return Hit::palette_mode;
        return x > 0.85f ? Hit::palette_up : Hit::palette_down;
    }
    if (y < 0.79f) return static_cast<Hit>(static_cast<int>(Hit::palette1) +
        std::clamp(static_cast<int>(x * 10.0f), 0, 9));
    if (y < 0.88f) return x < 0.68f ? Hit::dither_mode : Hit::dither_matrix;
    return x < 0.70f ? Hit::dither_threshold : Hit::dither_matrix;
}

struct VisualState {
    Hit hit = Hit::none;
    float amount = 0.0f;
};

struct InteractionState {
    Hit requested_hit = Hit::none;
    Hit display_hit = Hit::none;
    bool pressed = false;
    bool reduced_motion = false;
    float from = 0.0f;
    float target = 0.0f;
    double started = 0.0;
    PF_Rect frame{};
    int diagnostic_idle_events = 0;
};

inline float feedback_value(const InteractionState& state, double now) {
    if (state.reduced_motion) return state.target;
    const float time = std::clamp(static_cast<float>((now - state.started) / 0.12), 0.0f, 1.0f);
    if (time == 0.0f) return state.from;
    if (time == 1.0f) return state.target;
    float lower = 0.0f;
    float upper = 1.0f;
    float t = time;
    for (int iteration = 0; iteration < 12; ++iteration) {
        t = (lower + upper) * 0.5f;
        const float inverse = 1.0f - t;
        const float x = 3.0f * inverse * inverse * t * 0.23f +
            3.0f * inverse * t * t * 0.32f + t * t * t;
        if (x < time) lower = t;
        else upper = t;
    }
    const float inverse = 1.0f - t;
    const float eased = 3.0f * inverse * t + t * t * t;
    return state.from + (state.target - state.from) * eased;
}

inline bool retarget_feedback(InteractionState& state, Hit hit, bool pressed, double now, bool reduced) {
    if (state.requested_hit == hit && state.pressed == pressed && state.reduced_motion == reduced) return false;
    const float current = feedback_value(state, now);
    state.requested_hit = hit;
    if (hit != Hit::none) state.display_hit = hit;
    state.pressed = pressed;
    state.reduced_motion = reduced;
    state.from = current;
    state.target = pressed ? 1.0f : hit == Hit::none ? 0.0f : 0.65f;
    state.started = now;
    state.diagnostic_idle_events = 0;
    return true;
}

inline bool needs_feedback_frame(const InteractionState& state, double now) {
    return feedback_value(state, now) != state.target;
}

struct WheelLayout {
    float size;
    float left;
    float top;
};

inline WheelLayout wheel_layout(int width, int height) {
    return {std::max(16.0f, std::min({52.0f, width * 0.30f, height - 52.0f})), 2.0f, 2.0f};
}

inline float slider_track_left(int width) {
    return width * 0.60f;
}

inline Hit wheel_hit(float x, float y, int width, int height) {
    const float px = x * width;
    const float py = y * height;
    const auto layout = wheel_layout(width, height);
    if (px <= layout.left + layout.size + 2.0f && py >= layout.top && py <= layout.top + layout.size) return Hit::tint;
    if (px <= layout.left + layout.size + 2.0f && py > layout.top + layout.size) return Hit::tint_amount;
    if (px > width * 0.38f) {
        const int row = std::clamp(static_cast<int>(py / (height * 0.25f)), 0, 3);
        return static_cast<Hit>(static_cast<int>(Hit::exposure) + row);
    }
    return Hit::none;
}

inline Hit interactive_hit(const RenderSettings& settings, PF_ParamIndex index,
    float x, float y, int width, int height) {
    Hit hit = Hit::none;
    if (index == kRampBar) {
        hit = ramp_hit(x, y);
        if (hit == Hit::boundary1) {
            int boundary = 0;
            float distance = 2.0f;
            for (int candidate = 0; candidate < static_cast<int>(settings.ramp.boundaries.size()); ++candidate) {
                const float candidate_distance = std::fabs(settings.ramp.boundaries[candidate] - x);
                if (candidate_distance < distance) {
                    boundary = candidate;
                    distance = candidate_distance;
                }
            }
            hit = static_cast<Hit>(static_cast<int>(Hit::boundary1) + boundary);
        }
        if (settings.mode == static_cast<int>(OutputMode::pixel_only) &&
            hit >= Hit::preserve_hue && hit <= Hit::repeat_up) return Hit::none;
        if (settings.mode == static_cast<int>(OutputMode::toon_only) &&
            hit >= Hit::palette_mode && hit <= Hit::dither_matrix) return Hit::none;
        if (settings.palette_mode != static_cast<int>(toonpixel::PaletteMode::custom) &&
            (hit == Hit::palette_down || hit == Hit::palette_up)) return Hit::none;
    } else if (index == kColorWheel) {
        hit = wheel_hit(x, y, width, height);
    }
    return hit;
}

inline std::vector<unsigned char> pack_drawbot_pixels(const std::vector<unsigned char>& rgba,
    bool supports_argb, bool supports_bgra) {
    if (rgba.size() % 4 != 0 || (!supports_argb && !supports_bgra)) return {};
    std::vector<unsigned char> packed(rgba.size());
    for (std::size_t index = 0; index < rgba.size(); index += 4) {
        if (supports_argb) {
            packed[index] = rgba[index + 3];
            packed[index + 1] = rgba[index];
            packed[index + 2] = rgba[index + 1];
            packed[index + 3] = rgba[index + 2];
        } else {
            packed[index] = rgba[index + 2];
            packed[index + 1] = rgba[index + 1];
            packed[index + 2] = rgba[index];
            packed[index + 3] = rgba[index + 3];
        }
    }
    return packed;
}

inline void slider(std::vector<unsigned char>& pixels, int width, int height, int left, int top,
    int right, float value, float zero = 0.0f, float emphasis = 0.0f) {
    value = std::clamp(value, 0.0f, 1.0f);
    emphasis = std::clamp(emphasis, 0.0f, 1.0f);
    const auto lifted = [emphasis](float base, float amount) {
        return static_cast<unsigned char>(std::lround(base + amount * emphasis));
    };
    const int track_right = std::max(left + 1, right);
    const int marker = left + static_cast<int>((track_right - left - 1) * value);
    const int origin = left + static_cast<int>((track_right - left - 1) * zero);
    rect(pixels, width, height, left, top, track_right, top + 4,
        lifted(76, 8), lifted(76, 8), lifted(76, 8));
    rect(pixels, width, height, std::min(origin, marker), top, std::max(origin, marker) + 1, top + 4,
        lifted(82, 24), lifted(142, 20), lifted(199, 16));
    rect(pixels, width, height, marker - 1, top - 2, marker + 2, top + 6,
        lifted(232, 23), lifted(232, 23), lifted(232, 23));
}

inline std::vector<unsigned char> ramp_pixels(int width, int height, const RenderSettings& settings,
    bool enabled, const scene::Analysis* analysis = nullptr, const VisualState& feedback = {}) {
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height * 4);
    rect(pixels, width, height, 0, 0, width, height, 28, 28, 28);
    const auto emphasis = [&](Hit hit) { return feedback.hit == hit ? feedback.amount : 0.0f; };
    const bool preserve = settings.toon_model == static_cast<int>(toonpixel::ToonModel::preserve_hue);
    rect(pixels, width, height, 0, 0, static_cast<int>(width * 0.50f), static_cast<int>(height * 0.12f),
        preserve ? 58 : 38, preserve ? 88 : 38, preserve ? 112 : 38);
    rect(pixels, width, height, static_cast<int>(width * 0.50f), 0, static_cast<int>(width * 0.75f),
        static_cast<int>(height * 0.12f), preserve ? 38 : 58, preserve ? 38 : 88, preserve ? 38 : 112);
    rect(pixels, width, height, static_cast<int>(width * 0.75f), 0, width,
        static_cast<int>(height * 0.12f), 43, 43, 43);
    const int tab_bottom = static_cast<int>(height * 0.12f);
    for (const auto& tab : {std::pair<Hit, int>{Hit::preserve_hue, 0}, {Hit::luma_ramp, 1}, {Hit::analyze, 2}}) {
        const float amount = emphasis(tab.first);
        if (amount > 0.0f) {
            const int left = tab.second == 0 ? 0 : tab.second == 1 ? width / 2 : width * 3 / 4;
            const int right = tab.second == 0 ? width / 2 : tab.second == 1 ? width * 3 / 4 : width;
            rect(pixels, width, height, left, tab_bottom - 2, right, tab_bottom,
                static_cast<unsigned char>(std::lround(82 + 24 * amount)),
                static_cast<unsigned char>(std::lround(142 + 20 * amount)),
                static_cast<unsigned char>(std::lround(199 + 16 * amount)));
        }
    }
    toonpixel::RampSettings ramp = settings.ramp;
    ramp.repeat = 1;
    const int strip_top = static_cast<int>(height * 0.14f);
    const int strip_bottom = static_cast<int>(height * 0.29f);
    for (int y = strip_top; y < strip_bottom; ++y) {
        for (int x = 0; x < width; ++x) {
            const float t = width > 1 ? static_cast<float>(x) / (width - 1) : 0.0f;
            auto color = toonpixel::sample_ramp(t, ramp);
            if (settings.toon_model == static_cast<int>(toonpixel::ToonModel::luma_ramp)) {
                color = toonpixel::apply_gamut(toonpixel::apply_color_grade(color, settings.grade),
                    settings.gamut_mode, settings.gamut_margin);
            }
            if (!enabled) color = toonpixel::mix_rgb(color, {0.0f, 0.0f, 0.0f}, 0.65f);
            put_pixel(pixels, width, height, x, y, channel(color.red), channel(color.green), channel(color.blue));
        }
    }
    for (std::size_t index = 0; index < settings.ramp.boundaries.size(); ++index) {
        const float boundary = settings.ramp.boundaries[index];
        const int x = std::clamp(static_cast<int>(std::lround(boundary * (width - 1))), 0, width - 1);
        const float amount = emphasis(static_cast<Hit>(static_cast<int>(Hit::boundary1) + index));
        rect(pixels, width, height, x - 3, strip_bottom, x + 4, strip_bottom + 7,
            static_cast<unsigned char>(std::lround(20 + 40 * amount)),
            static_cast<unsigned char>(std::lround(20 + 70 * amount)),
            static_cast<unsigned char>(std::lround(20 + 100 * amount)));
        rect(pixels, width, height, x - 1, strip_bottom, x + 2, strip_bottom + 5,
            static_cast<unsigned char>(std::lround(235 + 20 * amount)),
            static_cast<unsigned char>(std::lround(235 + 20 * amount)),
            static_cast<unsigned char>(std::lround(235 + 20 * amount)));
    }
    if (analysis && analysis->valid) {
        for (const float level : analysis->levels) {
            const int x = std::clamp(static_cast<int>(std::lround(level * (width - 1))), 0, width - 1);
            rect(pixels, width, height, x, strip_bottom - 3, x + 1, strip_bottom, 255, 255, 255);
        }
    }
    for (std::size_t index = 0; index < settings.ramp.colors.size(); ++index) {
        const int left = static_cast<int>(index) * width / 5 + 2;
        const int right = (static_cast<int>(index) + 1) * width / 5 - 2;
        const auto& color = settings.ramp.colors[index];
        rect(pixels, width, height, left - 1, static_cast<int>(height * 0.33f),
            right + 1, static_cast<int>(height * 0.41f), 85, 85, 85);
        rect(pixels, width, height, left, static_cast<int>(height * 0.33f) + 1,
            right, static_cast<int>(height * 0.41f) - 1, channel(color.red), channel(color.green), channel(color.blue));
    }
    slider(pixels, width, height, 0, static_cast<int>(height * 0.52f),
        static_cast<int>(width * 0.60f), settings.ramp.softness * 2.0f, 0.0f, emphasis(Hit::softness));
    rect(pixels, width, height, static_cast<int>(width * 0.70f), static_cast<int>(height * 0.46f),
        width, static_cast<int>(height * 0.55f), 43, 43, 43);
    const auto palette = settings.palette();
    const bool pixel_enabled = settings.mode != static_cast<int>(OutputMode::toon_only);
    for (int index = 0; index < 10; ++index) {
        const int left = index * width / 10 + 1;
        const int right = (index + 1) * width / 10 - 1;
        if (index < static_cast<int>(palette.size())) {
            auto color = palette[index];
            if (!pixel_enabled) color = toonpixel::mix_rgb(color, {0.0f, 0.0f, 0.0f}, 0.70f);
            rect(pixels, width, height, left, static_cast<int>(height * 0.68f), right,
                static_cast<int>(height * 0.77f), channel(color.red), channel(color.green), channel(color.blue));
        } else {
            rect(pixels, width, height, left, static_cast<int>(height * 0.68f), right,
                static_cast<int>(height * 0.77f), 35, 35, 35);
        }
        if (settings.palette_mode == static_cast<int>(toonpixel::PaletteMode::custom)) {
            rect(pixels, width, height, left, static_cast<int>(height * 0.77f), right,
                static_cast<int>(height * 0.77f) + 1, 85, 85, 85);
        }
    }
    if (settings.dither_mode == static_cast<int>(toonpixel::DitherMode::bayer)) {
        rect(pixels, width, height, static_cast<int>(width * 0.70f), static_cast<int>(height * 0.81f),
            width, static_cast<int>(height * 0.87f), 53, 79, 99);
    }
    slider(pixels, width, height, 0, static_cast<int>(height * 0.95f),
        static_cast<int>(width * 0.62f), settings.dither_threshold, 0.5f, emphasis(Hit::dither_threshold));
    return pixels;
}

inline std::vector<unsigned char> wheel_pixels(int width, int height, const RenderSettings& settings,
    const VisualState& feedback = {}) {
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height * 4);
    rect(pixels, width, height, 0, 0, width, height, 28, 28, 28);
    const auto layout = wheel_layout(width, height);
    const float center = layout.size * 0.5f;
    const float radius = std::max(1.0f, center - 1.5f);
    const auto lab = toonpixel::linear_to_oklab(settings.grade.tint);
    const float tint_chroma = std::hypot(lab.a, lab.b);
    const float tint_angle = std::atan2(lab.b, lab.a);
    const float marker_x = layout.left + center + std::cos(tint_angle) * std::min(1.0f, tint_chroma / 0.25f) * radius;
    const float marker_y = layout.top + center + std::sin(tint_angle) * std::min(1.0f, tint_chroma / 0.25f) * radius;
    for (int y = 0; y < static_cast<int>(layout.size) && layout.top + y < height; ++y) {
        for (int x = 0; x < static_cast<int>(layout.size) && layout.left + x < width; ++x) {
            const float dx = (x - center) / radius;
            const float dy = (y - center) / radius;
            const float r = std::hypot(dx, dy);
            if (r <= 1.0f) {
                const float angle = std::atan2(dy, dx);
                const auto color = toonpixel::oklab_to_linear({
                    0.72f, 0.25f * r * std::cos(angle), 0.25f * r * std::sin(angle)});
                const float edge = std::clamp((1.0f - r) * radius, 0.0f, 1.0f);
                put_pixel(pixels, width, height, static_cast<int>(layout.left + x), static_cast<int>(layout.top + y),
                    channel(color.red), channel(color.green), channel(color.blue),
                    static_cast<unsigned char>(std::lround(edge * 255.0f)));
            }
        }
    }
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        const float distance = std::hypot(x - marker_x, y - marker_y);
        if (distance < 2.5f) put_pixel(pixels, width, height, x, y, 245, 245, 245);
        else if (distance <= 4.0f) put_pixel(pixels, width, height, x, y, 18, 18, 18);
        if (feedback.hit == Hit::tint && feedback.amount > 0.0f && distance > 4.0f && distance <= 5.0f) {
            put_pixel(pixels, width, height, x, y,
                static_cast<unsigned char>(std::lround(82 + 24 * feedback.amount)),
                static_cast<unsigned char>(std::lround(142 + 20 * feedback.amount)),
                static_cast<unsigned char>(std::lround(199 + 16 * feedback.amount)));
        }
    }
    const int swatch_top = static_cast<int>(layout.top + layout.size + 6.0f);
    const auto& tint = settings.grade.tint;
    const int swatch_left = static_cast<int>(layout.left);
    rect(pixels, width, height, swatch_left, swatch_top, swatch_left + 8, swatch_top + 8,
        channel(tint.red), channel(tint.green), channel(tint.blue));
    slider(pixels, width, height, swatch_left, static_cast<int>(layout.top + layout.size + 30.0f),
        static_cast<int>(layout.left + layout.size), settings.grade.tint_amount, 0.0f,
        feedback.hit == Hit::tint_amount ? feedback.amount : 0.0f);
    const int track_left = static_cast<int>(slider_track_left(width));
    const int row_height = height / 4;
    const auto row_emphasis = [&](Hit hit) { return feedback.hit == hit ? feedback.amount : 0.0f; };
    slider(pixels, width, height, track_left, row_height * 0 + 20, width - 2,
        (settings.grade.exposure + 2.0f) / 4.0f, 0.5f, row_emphasis(Hit::exposure));
    slider(pixels, width, height, track_left, row_height * 1 + 20, width - 2,
        (settings.grade.vibrance + 1.0f) * 0.5f, 0.5f, row_emphasis(Hit::vibrance));
    slider(pixels, width, height, track_left, row_height * 2 + 20, width - 2,
        (settings.grade.hue + 180.0f) / 360.0f, 0.5f, row_emphasis(Hit::hue));
    slider(pixels, width, height, track_left, row_height * 3 + 20, width - 2,
        settings.grade.chroma * 0.5f, 0.0f, row_emphasis(Hit::chroma));
    return pixels;
}

inline std::vector<unsigned char> ascii_pixels(int width, int height, const RenderSettings& settings) {
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height * 4, 0);
    const auto ramp = glyphs::active_ramp(settings.ascii_style, settings.ascii_ramp);
    rect(pixels, width, height, 0, 0, width, height, 28, 28, 28);
    const int selected = std::min(settings.ascii_selected, static_cast<int>(ramp.size()) - 1);
    for (std::size_t index = 0; index < ramp.size(); ++index) {
        const int left = static_cast<int>(index) * 12;
        if (left + 10 >= width) break;
        const auto glyph = glyphs::glyph_for(ramp[index], settings.ascii_style);
        for (int y = 0; y < 8; ++y) for (int x = 0; x < 8; ++x) {
            if (glyph[y] & (1 << x)) put_pixel(pixels, width, height, left + x + 1, y + 2, 218, 222, 216);
        }
        if (static_cast<int>(index) == selected) {
            for (int x = 0; x < 11; ++x) {
                put_pixel(pixels, width, height, left + x, 1, 80, 155, 255);
                put_pixel(pixels, width, height, left + x, 11, 80, 155, 255);
            }
            for (int y = 0; y < 12; ++y) {
                put_pixel(pixels, width, height, left, y, 80, 155, 255);
                put_pixel(pixels, width, height, left + 10, y, 80, 155, 255);
            }
        }
    }
    const auto glyph = glyphs::glyph_for(ramp[selected], settings.ascii_style);
    for (int y = 0; y < 24; ++y) for (int x = 0; x < 24; ++x) {
        if (glyph[y / 3] & (1 << (x / 3))) put_pixel(pixels, width, height, x + 5, y + 20, 220, 225, 214);
    }
    return pixels;
}

}  // namespace studio
