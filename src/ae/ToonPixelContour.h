#pragma once

namespace contour {

constexpr float directions[8][2]{{1,0},{-1,0},{0,1},{0,-1},{0.70710678f,0.70710678f},{-0.70710678f,0.70710678f},{0.70710678f,-0.70710678f},{-0.70710678f,-0.70710678f}};

inline bool inside(float alpha, const toonpixel::Rgb& color, const RenderSettings& settings) {
    if (settings.contour_source == 1) return alpha >= 0.5f;
    if (settings.contour_source != 2 && alpha < 0.999f) return alpha >= 0.5f;
    const auto& background = settings.contour_background;
    const float dr = color.red - background.red;
    const float dg = color.green - background.green;
    const float db = color.blue - background.blue;
    const float distance = std::sqrt(dr * dr + dg * dg + db * db) / 1.73205080757f;
    return distance > settings.edge_threshold;
}

template <typename Pixel>
bool inside_at(const PF_EffectWorld* input, float x, float y, const RenderSettings& settings) {
    const auto pixel = sample_straight<Pixel>(input, static_cast<A_long>(std::floor(x)),
        static_cast<A_long>(std::floor(y)));
    return inside(pixel.alpha, {pixel.red, pixel.green, pixel.blue}, settings);
}

inline float edge_coverage(float distance, bool inside_shape, float width) {
    const float signed_distance = inside_shape ? distance : -distance;
    return std::clamp(width * 0.5f + 0.5f - std::fabs(signed_distance), 0.0f, 1.0f);
}

template <typename Pixel>
float coverage_at(const PF_EffectWorld* input, float x, float y, const RenderSettings& settings) {
    const bool center = inside_at<Pixel>(input, x, y, settings);
    const float search_limit = settings.contour_width + 1.5f;
    float nearest = search_limit;
    for (const auto& direction : directions) {
        float previous = 0.0f;
        for (float distance = 0.5f; distance <= search_limit; distance += 0.5f) {
            if (inside_at<Pixel>(input, x + direction[0] * distance, y + direction[1] * distance, settings) != center) {
                float low = previous;
                float high = distance;
                for (int iteration = 0; iteration < 5; ++iteration) {
                    const float mid = (low + high) * 0.5f;
                    if (inside_at<Pixel>(input, x + direction[0] * mid, y + direction[1] * mid, settings) == center) low = mid;
                    else high = mid;
                }
                nearest = std::min(nearest, high);
                break;
            }
            previous = distance;
        }
    }
    if (nearest > settings.contour_width + 1.0f) return 0.0f;
    const float coverage = edge_coverage(nearest, center, settings.contour_width);
    return settings.contour_aa == 1 ? (coverage >= 0.5f ? 1.0f : 0.0f) : coverage;
}

template <typename Pixel>
float coverage(const PF_EffectWorld* input, A_long x, A_long y, const RenderSettings& settings) {
    const int samples = settings.contour_aa == 1 ? 1 : (settings.contour_aa == 2 ? 2 : 4);
    float total = 0.0f;
    for (int sy = 0; sy < samples; ++sy) for (int sx = 0; sx < samples; ++sx) {
        const float ox = (sx + 0.5f) / samples;
        const float oy = (sy + 0.5f) / samples;
        total += coverage_at<Pixel>(input, x + ox, y + oy, settings);
    }
    return total / (samples * samples);
}

}  // namespace contour
