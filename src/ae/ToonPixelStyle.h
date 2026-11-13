#pragma once

#include <utility>

namespace style {

template <typename Pixel>
StraightPixel source_pixel(const PF_EffectWorld* input, A_long x, A_long y, const RenderSettings&) {
    return sample_straight<Pixel>(input, x, y);
}

inline StraightPixel grade_styled(const StraightPixel& pixel, const RenderSettings& settings) {
    auto color = toonpixel::apply_color_grade({pixel.red, pixel.green, pixel.blue}, settings.grade);
    color = toonpixel::apply_gamut(color, settings.gamut_mode, settings.gamut_margin);
    return {pixel.alpha, color.red, color.green, color.blue};
}

template <typename Pixel>
float sample_luminance(const PF_EffectWorld* input, A_long x, A_long y, const RenderSettings& settings) {
    const StraightPixel pixel = source_pixel<Pixel>(input, x, y, settings);
    return toonpixel::luminance({pixel.red, pixel.green, pixel.blue});
}

template <typename Pixel>
bool is_outline(const PF_EffectWorld* input, A_long x, A_long y, const RenderSettings& settings) {
    const float center = sample_luminance<Pixel>(input, x, y, settings);
    const int radius = std::clamp(settings.outline_width, 1, 4);
    const float difference = std::max({
        std::fabs(center - sample_luminance<Pixel>(input, x - radius, y, settings)),
        std::fabs(center - sample_luminance<Pixel>(input, x + radius, y, settings)),
        std::fabs(center - sample_luminance<Pixel>(input, x, y - radius, settings)),
        std::fabs(center - sample_luminance<Pixel>(input, x, y + radius, settings)),
    });
    return difference > settings.edge_threshold;
}

template <typename Pixel>
StraightPixel toon_base(const PF_EffectWorld* input, A_long x, A_long y, const RenderSettings& settings) {
    const StraightPixel pixel = source_pixel<Pixel>(input, x, y, settings);
    const toonpixel::Rgb source{pixel.red, pixel.green, pixel.blue};
    const float luminance = toonpixel::luminance(source);
    if (settings.toon_model == static_cast<int>(toonpixel::ToonModel::luma_ramp)) {
        const auto ramp = toonpixel::sample_ramp(luminance, settings.ramp);
        if (x == 48 && y == 60) {
            TOONPIXEL_LOG("toon_ramp_sample luminance=%.9f bounds=%.9f,%.9f,%.9f,%.9f rgb=%.9f,%.9f,%.9f repeat=%d",
                luminance, settings.ramp.boundaries[0], settings.ramp.boundaries[1],
                settings.ramp.boundaries[2], settings.ramp.boundaries[3], ramp.red, ramp.green, ramp.blue,
                settings.ramp.repeat);
        }
        const auto color = toonpixel::mix_rgb(source, ramp, settings.strength);
        return {pixel.alpha, color.red, color.green, color.blue};
    }
    const float multiplier = 1.0f + (toonpixel::band_multiplier(luminance) - 1.0f) * settings.strength;
    return {pixel.alpha, pixel.red * multiplier, pixel.green * multiplier, pixel.blue * multiplier};
}

struct GridCell {
    StraightPixel mean{};
    toonpixel::Oklab lab{};
    float luma = 0.0f;
    std::size_t palette_index = 0;
    bool legacy_outline = false;
};

struct Grid {
    int left = 0;
    int top = 0;
    int width = 0;
    int height = 0;
    int factor = 1;
    std::vector<GridCell> cells;

    const GridCell& at(int x, int y) const {
        x = std::clamp(x, 0, width - 1);
        y = std::clamp(y, 0, height - 1);
        return cells[static_cast<std::size_t>(y) * width + x];
    }
};

template <typename Pixel>
GridCell block_color(const PF_EffectWorld* input, int left, int top, int right, int bottom,
    const RenderSettings& settings) {
    float alpha = 0.0f;
    float red = 0.0f;
    float green = 0.0f;
    float blue = 0.0f;
    int count = 0;
    bool legacy_outline = false;
    for (int y = top; y < bottom; ++y) {
        for (int x = left; x < right; ++x) {
            const StraightPixel pixel = settings.mode == static_cast<int>(OutputMode::toon_pixel)
                ? toon_base<Pixel>(input, x, y, settings)
                : source_pixel<Pixel>(input, x, y, settings);
            alpha += pixel.alpha;
            red += pixel.red * pixel.alpha;
            green += pixel.green * pixel.alpha;
            blue += pixel.blue * pixel.alpha;
            if (settings.outline && settings.contour_mode == 1 &&
                settings.pixel_outline == static_cast<int>(toonpixel::PixelOutlineMode::legacy_block) &&
                settings.mode == static_cast<int>(OutputMode::toon_pixel)) {
                legacy_outline = legacy_outline || is_outline<Pixel>(input, x, y, settings);
            }
            ++count;
        }
    }
    GridCell result;
    result.mean.alpha = count ? alpha / count : 0.0f;
    if (alpha > 0.0f) {
        result.mean.red = red / alpha;
        result.mean.green = green / alpha;
        result.mean.blue = blue / alpha;
    }
    result.luma = toonpixel::luminance({result.mean.red, result.mean.green, result.mean.blue});
    result.lab = toonpixel::linear_to_oklab({result.mean.red, result.mean.green, result.mean.blue});
    result.legacy_outline = legacy_outline;
    if (top == 60 && (left == 0 || left == 16 || left == 48 || left == 80 || left == 112)) {
        const auto source = source_pixel<Pixel>(input, left, top, settings);
        TOONPIXEL_LOG("ramp_input x=%d luminance=%.9f", left,
            toonpixel::luminance({source.red, source.green, source.blue}));
        TOONPIXEL_LOG("style_cell x=%d source=%.9f,%.9f,%.9f mean=%.9f,%.9f,%.9f luma=%.9f palette=%d model=%d dither=%d ramp0=%.9f,%.9f,%.9f ramp2=%.9f,%.9f,%.9f ramp4=%.9f,%.9f,%.9f",
            left, source.red, source.green, source.blue, result.mean.red, result.mean.green, result.mean.blue,
            result.luma, settings.palette_mode, settings.toon_model, settings.dither_mode,
            settings.ramp.colors[0].red, settings.ramp.colors[0].green, settings.ramp.colors[0].blue,
            settings.ramp.colors[2].red, settings.ramp.colors[2].green, settings.ramp.colors[2].blue,
            settings.ramp.colors[4].red, settings.ramp.colors[4].green, settings.ramp.colors[4].blue);
    }
    return result;
}

inline float decay_at(const RenderSettings& settings, int width, int height, int x, int y) {
    return toonpixel::dither_decay((x + 0.5f) / width, (y + 0.5f) / height,
        static_cast<float>(width) / height, settings.decay);
}

template <typename Pixel>
PF_Err build_grid(const PF_EffectWorld* input, const RenderSettings& settings,
    const std::vector<toonpixel::Rgb>& palette, Grid& grid) {
    const PF_LRect bounds = world_rect(input);
    const GridRect& full = settings.full_rect;
    if (settings.pixel_size <= 0 || full.left < bounds.left || full.top < bounds.top ||
        full.right > bounds.right || full.bottom > bounds.bottom ||
        full.left >= full.right || full.top >= full.bottom) {
        TOONPIXEL_LOG("style_full_world_missing required=%d,%d,%d,%d world=%ld,%ld,%ld,%ld",
            full.left, full.top, full.right, full.bottom,
            static_cast<long>(bounds.left), static_cast<long>(bounds.top),
            static_cast<long>(bounds.right), static_cast<long>(bounds.bottom));
        return PF_Err_BAD_CALLBACK_PARAM;
    }
    const int factor = settings.pixel_size;
    const int width = (full.right - full.left + factor - 1) / factor;
    const int height = (full.bottom - full.top + factor - 1) / factor;
    const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (width <= 0 || height <= 0 || count / static_cast<std::size_t>(width) != static_cast<std::size_t>(height)) {
        return PF_Err_OUT_OF_MEMORY;
    }
    grid = Grid{};
    grid.left = full.left;
    grid.top = full.top;
    grid.width = width;
    grid.height = height;
    grid.factor = factor;
    try {
        grid.cells.resize(count);
        const auto palette_lab = toonpixel::palette_oklab(palette);
        std::vector<toonpixel::Oklab> labs;
        std::vector<float> alpha;
        std::vector<float> decay;
        if (settings.dither_mode == static_cast<int>(toonpixel::DitherMode::floyd_steinberg)) {
            labs.reserve(count);
            alpha.reserve(count);
            decay.reserve(count);
        }
        for (int grid_y = 0; grid_y < height; ++grid_y) {
            const int top = full.top + grid_y * factor;
            const int bottom = std::min(top + factor, full.bottom);
            for (int grid_x = 0; grid_x < width; ++grid_x) {
                const int left = full.left + grid_x * factor;
                const int right = std::min(left + factor, full.right);
                GridCell cell = block_color<Pixel>(input, left, top, right, bottom, settings);
                const float decay_amount = decay_at(settings, width, height, grid_x, grid_y);
                if (settings.decay.shape == 3 && settings.dither_matrix == 8 && grid_y == 1 &&
                    grid_x >= 12 && grid_x <= 16) {
                    const auto palette_index = toonpixel::nearest_palette_ordered(
                        {cell.mean.red, cell.mean.green, cell.mean.blue}, settings.dither * decay_amount,
                        grid_x, grid_y, palette_lab, settings.dither_threshold, settings.dither_matrix);
                    const auto& palette_color = palette[palette_index];
                    TOONPIXEL_LOG("decay_cell x=%d y=%d mean=%.9f bias=%.9f palette=%zu rgb=%.6f,%.6f,%.6f",
                        grid_x, grid_y, cell.mean.red, (toonpixel::ordered_threshold(grid_x, grid_y, 3) +
                            0.5f - settings.dither_threshold) * settings.dither * decay_amount,
                        palette_index, palette_color.red, palette_color.green, palette_color.blue);
                }
                if (settings.dither_mode == static_cast<int>(toonpixel::DitherMode::floyd_steinberg)) {
                    labs.push_back(cell.lab);
                    alpha.push_back(cell.mean.alpha);
                    decay.push_back(decay_amount);
                } else if (settings.dither_mode == static_cast<int>(toonpixel::DitherMode::bayer)) {
                    cell.palette_index = toonpixel::nearest_palette_ordered(
                        {cell.mean.red, cell.mean.green, cell.mean.blue}, settings.dither * decay_amount,
                        grid_x, grid_y, palette_lab, settings.dither_threshold, settings.dither_matrix);
                } else if (settings.dither_mode != static_cast<int>(toonpixel::DitherMode::off)) {
                    return PF_Err_BAD_CALLBACK_PARAM;
                } else {
                    cell.palette_index = toonpixel::nearest_palette_lab(cell.lab, palette_lab);
                }
                grid.cells[static_cast<std::size_t>(grid_y) * width + grid_x] = cell;
            }
        }
        if (settings.dither_mode == static_cast<int>(toonpixel::DitherMode::floyd_steinberg)) {
            const auto indices = toonpixel::floyd_steinberg(std::move(labs), alpha, width, height,
                palette_lab, settings.dither, settings.dither_threshold, decay);
            if (indices.size() != count) return PF_Err_BAD_CALLBACK_PARAM;
            for (std::size_t index = 0; index < count; ++index) grid.cells[index].palette_index = indices[index];
        }
    } catch (const std::bad_alloc&) {
        return PF_Err_OUT_OF_MEMORY;
    }
    return PF_Err_NONE;
}

bool grid_outline(const Grid& grid, int x, int y, const RenderSettings& settings) {
    if (!settings.outline || settings.contour_mode != 1 ||
        settings.mode != static_cast<int>(OutputMode::toon_pixel)) return false;
    const GridCell& cell = grid.at(x, y);
    if (settings.pixel_outline == static_cast<int>(toonpixel::PixelOutlineMode::legacy_block)) {
        return cell.legacy_outline;
    }
    const int radius = std::clamp(settings.outline_width, 1, 4);
    const float difference = std::max({
        std::fabs(cell.luma - grid.at(x - radius, y).luma),
        std::fabs(cell.luma - grid.at(x + radius, y).luma),
        std::fabs(cell.luma - grid.at(x, y - radius).luma),
        std::fabs(cell.luma - grid.at(x, y + radius).luma),
    });
    return difference > settings.edge_threshold;
}

template <typename Pixel>
float drive_value(const PF_EffectWorld* input, const PF_EffectWorld* map, int x, int y,
    const RenderSettings& settings) {
    const auto source = source_pixel<Pixel>(input, x, y, settings);
    float value = toonpixel::luminance({source.red, source.green, source.blue});
    if (settings.ascii_drive == 2 || settings.ascii_drive == 4) {
        value = settings.audio_level * settings.ascii_audio_gain;
    }
    if ((settings.ascii_drive == 3 || settings.ascii_drive == 4) && settings.ascii_map_mode != 1) {
        const auto driver = sample_straight<Pixel>(map ? map : input, x, y);
        float mapped = driver.alpha;
        if (settings.ascii_map_mode == 2) {
            switch (settings.ascii_channel) {
                case 2: mapped = driver.red; break;
                case 3: mapped = driver.green; break;
                case 4: mapped = driver.blue; break;
                case 5: mapped = driver.alpha; break;
                default: mapped = toonpixel::luminance({driver.red, driver.green, driver.blue}); break;
            }
        }
        value = settings.ascii_drive == 3 ? mapped : value * mapped;
        if (y == 60 && (x == 8 || x == 40 || x == 88 || x == 136)) {
            TOONPIXEL_LOG("map_drive x=%d source=%.9f map=%p driver=%.9f,%.9f,%.9f alpha=%.9f channel=%d mode=%d mapped=%.9f value=%.9f",
                x, toonpixel::luminance({source.red, source.green, source.blue}), static_cast<const void*>(map),
                driver.red, driver.green, driver.blue, driver.alpha, settings.ascii_channel,
                settings.ascii_map_mode, mapped, value);
        }
    }
    return std::clamp(value * settings.ascii_amount, 0.0f, 1.0f);
}

template <typename Pixel>
StraightPixel ascii_pixel(const PF_EffectWorld* input, const PF_EffectWorld* map,
    const Grid& grid, const std::vector<toonpixel::Rgb>& palette, A_long x, A_long y,
    const RenderSettings& settings, const std::vector<A_u_short>& ramp) {
    const int size = std::clamp(settings.ascii_cell_size, 4, 32);
    const int row = toonpixel::floor_div(static_cast<int>(y - grid.top), size);
    const int shift = settings.ascii_glitch > 0.0f
        ? static_cast<int>(std::lround((glyphs::noise(row, settings.frame, 8) - 0.5f) * settings.ascii_glitch * 12.0f))
        : 0;
    const int column = toonpixel::floor_div(static_cast<int>(x - grid.left), size) - shift;
    const int left = grid.left + (column + shift) * size;
    const int top = grid.top + row * size;
    const int local_x = static_cast<int>(x) - left;
    const int local_y = static_cast<int>(y) - top;
    const int gx = toonpixel::floor_div(static_cast<int>(x - grid.left), grid.factor);
    const int gy = toonpixel::floor_div(static_cast<int>(y - grid.top), grid.factor);
    const auto& cell = grid.at(gx, gy);
    const float driver = drive_value<Pixel>(input, map, static_cast<int>(x), static_cast<int>(y), settings);
    int glyph_index = std::min(static_cast<int>(ramp.size()) - 1,
        static_cast<int>(driver * static_cast<float>(ramp.size())));
    const float glitch = glyphs::noise(column, row, settings.frame);
    if (glitch < settings.ascii_glitch * 0.35f) {
        glyph_index = static_cast<int>(glyphs::hash(column, row, settings.frame) % ramp.size());
    }
    const auto glyph = glyphs::glyph_for(ramp[glyph_index], settings.ascii_style);
    const float u = static_cast<float>(local_x) / size;
    const float v = static_cast<float>(local_y) / size;
    const float shift_uv = settings.ascii_aberration / size;
    const float red_cover = glyphs::coverage(glyph, u - shift_uv, v);
    const float green_cover = glyphs::coverage(glyph, u, v);
    const float blue_cover = glyphs::coverage(glyph, u + shift_uv, v);
    toonpixel::Rgb foreground = settings.ascii_color;
    if (settings.ascii_color_mode == 2) foreground = {cell.mean.red, cell.mean.green, cell.mean.blue};
    if (settings.ascii_color_mode == 3) foreground = toonpixel::mix_rgb(settings.ascii_color, settings.ascii_color2, driver);
    const float flicker = 1.0f - settings.ascii_flicker * glyphs::noise(column, row, settings.frame);
    foreground.red *= flicker;
    foreground.green *= flicker;
    foreground.blue *= flicker;
    const auto& background = settings.ascii_background;
    StraightPixel result{cell.mean.alpha,
        toonpixel::mix_rgb(background, foreground, red_cover).red,
        toonpixel::mix_rgb(background, foreground, green_cover).green,
        toonpixel::mix_rgb(background, foreground, blue_cover).blue};
    if (glyph_index == 0 && settings.ascii_style >= 9) {
        const auto& source = palette[cell.palette_index];
        result.red = source.red;
        result.green = source.green;
        result.blue = source.blue;
    }
    return result;
}

template <typename Pixel>
StraightPixel apply_contour(const PF_EffectWorld* input, A_long x, A_long y,
    StraightPixel color, const RenderSettings& settings) {
    if (settings.outline && settings.contour_mode == 2 && settings.mode != static_cast<int>(OutputMode::pixel_only)) {
        const float coverage = contour::coverage<Pixel>(input, x, y, settings) * settings.outline_strength;
        if (y == 60 && (x == 20 || x == 40 || x == 110)) {
            const auto source = sample_straight<Pixel>(input, x, y);
            TOONPIXEL_LOG("contour_sample x=%ld y=%ld aa=%d source_mode=%d threshold=%.6f coverage=%.6f alpha=%.9f rgb=%.9f,%.9f,%.9f bg=%.9f,%.9f,%.9f base=%.9f outline=%.9f strength=%.6f",
                static_cast<long>(x), static_cast<long>(y), settings.contour_aa, settings.contour_source,
                settings.edge_threshold, coverage, source.alpha, source.red, source.green, source.blue,
                settings.contour_background.red, settings.contour_background.green, settings.contour_background.blue,
                color.red, settings.outline_color.red, settings.outline_strength);
        }
        const auto outlined = toonpixel::mix_rgb({color.red, color.green, color.blue}, settings.outline_color, coverage);
        color.red = outlined.red;
        color.green = outlined.green;
        color.blue = outlined.blue;
    }
    return color;
}

template <typename Pixel>
PF_Err render_typed(const PF_EffectWorld* input, PF_EffectWorld* output, const PF_LRect& area,
    bool preserve_zero_alpha, PF_ChannelMask channel_mask, const RenderSettings& settings,
    const PF_EffectWorld* map = nullptr) {
    const bool pixel_mode = settings.mode != static_cast<int>(OutputMode::toon_only) || settings.ascii_enabled;
    const auto palette = pixel_mode ? settings.palette() : std::vector<toonpixel::Rgb>{};
    const auto ramp = glyphs::active_ramp(settings.ascii_style, settings.ascii_ramp);
    Grid grid;
    if (pixel_mode) {
        const PF_Err grid_error = build_grid<Pixel>(input, settings, palette, grid);
        if (grid_error) return grid_error;
    }
    for (A_long y = area.top; y < area.bottom; ++y) {
        const auto* source_row = reinterpret_cast<const Pixel*>(
            reinterpret_cast<const unsigned char*>(input->data) + (y - input->origin_y) * input->rowbytes);
        auto* destination_row = reinterpret_cast<Pixel*>(
            reinterpret_cast<unsigned char*>(output->data) + (y - output->origin_y) * output->rowbytes);
        for (A_long x = area.left; x < area.right; ++x) {
            const FloatPixel pixel = PixelTraits<Pixel>::read(source_row[x - input->origin_x]);
            FloatPixel shaded{};
            if (preserve_zero_alpha && pixel.alpha == 0.0f) {
                shaded = pixel;
            } else if (!pixel_mode) {
                if (settings.strength == 0.0f && !settings.outline && settings.gamut_mode == 1 &&
                    toonpixel::is_neutral_grade(settings.grade)) {
                    shaded = pixel;
                } else {
                    const auto base = toon_base<Pixel>(input, x, y, settings);
                    StraightPixel color = grade_styled(base, settings);
                    if (x == 0 && y == 60 && settings.gamut_mode > 1) {
                        const auto graded = toonpixel::apply_color_grade({base.red, base.green, base.blue}, settings.grade);
                        TOONPIXEL_LOG("gamut_sample mode=%d margin=%.6f graded=%.9f,%.9f,%.9f final=%.9f,%.9f,%.9f",
                            settings.gamut_mode, settings.gamut_margin, graded.red, graded.green, graded.blue,
                            color.red, color.green, color.blue);
                    }
                    if (settings.outline && settings.contour_mode == 1 && is_outline<Pixel>(input, x, y, settings)) {
                        const auto outline = toonpixel::mix_rgb({color.red, color.green, color.blue},
                            settings.outline_color, settings.outline_strength);
                        color.red = outline.red;
                        color.green = outline.green;
                        color.blue = outline.blue;
                    }
                    color = apply_contour<Pixel>(input, x, y, color, settings);
                    shaded = encode_pixel(color);
                }
            } else {
                const int grid_x = toonpixel::floor_div(static_cast<int>(x - grid.left), grid.factor);
                const int grid_y = toonpixel::floor_div(static_cast<int>(y - grid.top), grid.factor);
                const GridCell& cell = grid.at(grid_x, grid_y);
                const auto& selected = palette[cell.palette_index];
                const bool direct_ramp = settings.palette_mode == static_cast<int>(toonpixel::PaletteMode::ramp_linked) &&
                    settings.dither_mode == static_cast<int>(toonpixel::DitherMode::off);
                StraightPixel color = direct_ramp ? cell.mean :
                    StraightPixel{cell.mean.alpha, selected.red, selected.green, selected.blue};
                if (!settings.ascii_enabled) color = grade_styled(color, settings);
                if (grid_outline(grid, grid_x, grid_y, settings)) {
                    const auto outline = toonpixel::mix_rgb({color.red, color.green, color.blue},
                        settings.outline_color, settings.outline_strength);
                    color.red = outline.red;
                    color.green = outline.green;
                    color.blue = outline.blue;
                }
                color = apply_contour<Pixel>(input, x, y, color, settings);
                if (settings.ascii_enabled) {
                    color = grade_styled(ascii_pixel<Pixel>(input, map, grid, palette, x, y, settings, ramp), settings);
                    color = apply_contour<Pixel>(input, x, y, color, settings);
                }
                shaded = encode_pixel(color);
            }
            if (!(channel_mask & PF_ChannelMask_ALPHA)) shaded.alpha = pixel.alpha;
            if (!(channel_mask & PF_ChannelMask_RED)) shaded.red = pixel.red;
            if (!(channel_mask & PF_ChannelMask_GREEN)) shaded.green = pixel.green;
            if (!(channel_mask & PF_ChannelMask_BLUE)) shaded.blue = pixel.blue;
            if (y == 60 && (x == 0 || x == 16 || x == 48 || x == 80 || x == 112)) {
                TOONPIXEL_LOG("native_pixel x=%ld shaded=%.9f,%.9f,%.9f alpha=%.9f",
                    static_cast<long>(x), shaded.red, shaded.green, shaded.blue, shaded.alpha);
            }
            destination_row[x - output->origin_x] = PixelTraits<Pixel>::write(shaded);
        }
    }
    return PF_Err_NONE;
}

}  // namespace style
