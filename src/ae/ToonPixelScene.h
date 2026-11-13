#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

template <typename Pixel>
StraightPixel sample_straight(const PF_EffectWorld* world, A_long x, A_long y);

namespace scene {

struct Analysis {
    std::array<float, 5> levels{};
    std::array<float, 4> boundaries{};
    double alpha_weight = 0.0;
    std::size_t samples = 0;
    bool valid = false;
    bool uniform = false;
};

struct WeightedLuma {
    float value;
    float alpha;
};

inline Analysis analyze_samples(const std::vector<WeightedLuma>& samples) {
    std::array<double, 4096> histogram{};
    Analysis result;
    for (const auto& sample : samples) {
        if (!std::isfinite(sample.value) || !std::isfinite(sample.alpha) || sample.alpha <= 0.0f) continue;
        const float value = std::clamp(sample.value, 0.0f, 1.0f);
        const auto bin = static_cast<std::size_t>(value * (histogram.size() - 1));
        histogram[bin] += sample.alpha;
        result.alpha_weight += sample.alpha;
        ++result.samples;
    }
    if (result.samples == 0 || result.alpha_weight <= 0.0) return result;
    constexpr std::array<double, 5> fractions{0.01, 0.25, 0.50, 0.75, 0.99};
    for (std::size_t level = 0; level < fractions.size(); ++level) {
        const double target = result.alpha_weight * fractions[level];
        double previous = 0.0;
        for (std::size_t bin = 0; bin < histogram.size(); ++bin) {
            const double weight = histogram[bin];
            if (weight > 0.0 && previous + weight >= target) {
                const double fraction = std::clamp((target - previous) / weight, 0.0, 1.0);
                result.levels[level] = static_cast<float>((bin + fraction) / histogram.size());
                break;
            }
            previous += weight;
        }
    }
    result.valid = true;
    result.uniform = result.levels.back() - result.levels.front() <= 1.0f / static_cast<float>(histogram.size());
    for (std::size_t index = 0; index < result.boundaries.size(); ++index) {
        result.boundaries[index] = (result.levels[index] + result.levels[index + 1]) * 0.5f;
    }
    return result;
}

template <typename Pixel>
Analysis analyze_world(const PF_EffectWorld* world) {
    if (!world || !world->data || world->width <= 0 || world->height <= 0) return {};
    const auto pixels = static_cast<std::size_t>(world->width) * world->height;
    const auto step = static_cast<A_long>(std::max(1.0, std::ceil(std::sqrt(pixels / 65536.0))));
    std::vector<WeightedLuma> samples;
    samples.reserve((world->width / step + 1) * (world->height / step + 1));
    for (A_long y = 0; y < world->height; y += step) {
        for (A_long x = 0; x < world->width; x += step) {
            const auto pixel = sample_straight<Pixel>(world, x, y);
            samples.push_back({toonpixel::luminance({pixel.red, pixel.green, pixel.blue}), pixel.alpha});
        }
    }
    return analyze_samples(samples);
}

}  // namespace scene
