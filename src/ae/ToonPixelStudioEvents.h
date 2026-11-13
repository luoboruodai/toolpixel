#pragma once

inline void write_color(PF_ParamDef* parameter, const toonpixel::Rgb& color) {
    parameter->u.cd.value.red = studio::channel(color.red);
    parameter->u.cd.value.green = studio::channel(color.green);
    parameter->u.cd.value.blue = studio::channel(color.blue);
    parameter->u.cd.value.alpha = 255;
}

inline PF_Err handle_studio_event(PF_InData* in_data, PF_OutData* out_data,
    PF_ParamDef* params[], PF_EventExtra* event, PF_ParamIndex index,
    const RenderSettings& settings, float x, float y) {
    const bool click = event->e_type == PF_Event_DO_CLICK;
    if (click) {
        if (index == kRampBar || index == kColorWheel) {
            Suites suites(in_data);
            PF_Err error = suites.error();
            PF_Point mouse{};
            if (!error) error = suites.app->PF_GetMouse(&mouse);
            if (error) return error;
            event->u.do_click.continue_refcon[2] = mouse.h;
            event->u.do_click.continue_refcon[3] = mouse.v;
        } else {
            event->u.do_click.continue_refcon[2] = event->u.do_click.screen_point.h;
            event->u.do_click.continue_refcon[3] = event->u.do_click.screen_point.v;
        }
    }
    studio::Hit hit = click ? (index == kRampBar ? studio::ramp_hit(x, y) :
        studio::wheel_hit(x, y, event->effect_win.current_frame.right - event->effect_win.current_frame.left,
            event->effect_win.current_frame.bottom - event->effect_win.current_frame.top)) :
        static_cast<studio::Hit>(event->u.do_click.continue_refcon[1]);
    if (click) event->u.do_click.continue_refcon[1] = static_cast<A_intptr_t>(hit);
    if (index == kRampBar && hit == studio::Hit::analyze) {
        return click ? analyze_current_frame(in_data, out_data, params, event) : PF_Err_NONE;
    }
    if (index == kRampBar && hit >= studio::Hit::preserve_hue && hit <= studio::Hit::repeat_up &&
        settings.mode == static_cast<int>(OutputMode::pixel_only)) {
        event->evt_out_flags |= PF_EO_HANDLED_EVENT;
        return PF_Err_NONE;
    }
    if (index == kRampBar && hit >= studio::Hit::palette_mode && hit <= studio::Hit::dither_matrix &&
        settings.mode == static_cast<int>(OutputMode::toon_only)) {
        event->evt_out_flags |= PF_EO_HANDLED_EVENT;
        return PF_Err_NONE;
    }
    if (index == kRampBar && (hit == studio::Hit::palette_down || hit == studio::Hit::palette_up) &&
        settings.palette_mode != static_cast<int>(toonpixel::PaletteMode::custom)) {
        event->evt_out_flags |= PF_EO_HANDLED_EVENT;
        return PF_Err_NONE;
    }
    PF_ParamIndex changed = 0;
    bool continuous = false;
    bool needs_luma = false;
    bool needs_custom = false;
    bool needs_bayer = false;
    bool on_handle = false;
    if (index == kRampBar && (hit == studio::Hit::preserve_hue || hit == studio::Hit::luma_ramp)) {
        if (click) {
            changed = kToonModel;
            params[changed]->u.pd.value = hit == studio::Hit::luma_ramp ? 2 : 1;
        }
    } else if (index == kRampBar && hit >= studio::Hit::boundary1 && hit <= studio::Hit::boundary4) {
        int boundary = static_cast<int>(hit) - static_cast<int>(studio::Hit::boundary1);
        if (click) {
            float distance = 2.0f;
            for (int candidate = 0; candidate < static_cast<int>(settings.ramp.boundaries.size()); ++candidate) {
                const float candidate_distance = std::fabs(settings.ramp.boundaries[candidate] - x);
                if (candidate_distance < distance) {
                    boundary = candidate;
                    distance = candidate_distance;
                }
            }
            hit = static_cast<studio::Hit>(static_cast<int>(studio::Hit::boundary1) + boundary);
            event->u.do_click.continue_refcon[1] = static_cast<A_intptr_t>(hit);
            on_handle = distance * (event->effect_win.current_frame.right - event->effect_win.current_frame.left) <= 6.0f;
        }
        changed = kRampBoundaryParameters[boundary];
        const float lower = boundary > 0 ? settings.ramp.boundaries[boundary - 1] + 0.001f : 0.0f;
        const float upper = boundary + 1 < static_cast<int>(settings.ramp.boundaries.size()) ?
            settings.ramp.boundaries[boundary + 1] - 0.001f : 1.0f;
        const float requested = click ? x : settings.ramp.boundaries[boundary] + x;
        if (on_handle) {
            params[changed]->u.fs_d.value = settings.ramp.boundaries[boundary];
        } else {
            params[changed]->u.fs_d.value = lower <= upper ? std::clamp(requested, lower, upper) : (lower + upper) * 0.5f;
        }
        continuous = true;
        needs_luma = true;
    } else if (index == kRampBar && hit >= studio::Hit::color1 && hit <= studio::Hit::color5) {
        if (click) {
            const int color_index = static_cast<int>(hit) - static_cast<int>(studio::Hit::color1);
            auto color = settings.ramp.colors[color_index];
            bool accepted = false;
            const PF_Err error = pick_color(in_data, "Ramp Color", color, accepted);
            if (error) return error;
            if (accepted) {
                changed = kRampColorParameters[color_index];
                write_color(params[changed], color);
                needs_luma = true;
            }
        }
    } else if (index == kRampBar && hit == studio::Hit::softness) {
        changed = kRampSoftness;
        params[changed]->u.fs_d.value = click ? std::clamp(x / 0.60f, 0.0f, 1.0f) * 0.5f :
            std::clamp(settings.ramp.softness + x / 0.60f, 0.0f, 0.5f);
        continuous = true;
        needs_luma = true;
    } else if (index == kRampBar && (hit == studio::Hit::repeat_down || hit == studio::Hit::repeat_up)) {
        if (click) {
            changed = kRampRepeat;
            const int delta = hit == studio::Hit::repeat_up ? 1 : -1;
            params[changed]->u.fs_d.value = std::clamp(settings.ramp.repeat + delta, 1, 8);
            needs_luma = true;
        }
    } else if (index == kRampBar && hit == studio::Hit::palette_mode) {
        if (click) {
            changed = kPaletteMode;
            const int palette_mode = settings.palette_mode == static_cast<int>(toonpixel::PaletteMode::ramp_linked)
                ? static_cast<int>(toonpixel::PaletteMode::style10) : settings.palette_mode;
            params[changed]->u.pd.value = palette_mode % 3 + 1;
        }
    } else if (index == kRampBar && (hit == studio::Hit::palette_down || hit == studio::Hit::palette_up)) {
        if (click) {
            changed = kPaletteCount;
            params[changed]->u.fs_d.value = std::clamp(settings.palette_count +
                (hit == studio::Hit::palette_up ? 1 : -1), 1, 10);
        }
    } else if (index == kRampBar && hit >= studio::Hit::palette1 && hit <= studio::Hit::palette10) {
        const int color_index = static_cast<int>(hit) - static_cast<int>(studio::Hit::palette1);
        if (click && (settings.palette_mode != static_cast<int>(toonpixel::PaletteMode::custom) ||
            color_index < settings.palette_count)) {
            auto color = settings.custom_colors[color_index];
            bool accepted = false;
            const PF_Err error = pick_color(in_data, "Palette Color", color, accepted);
            if (error) return error;
            if (accepted) {
                changed = static_cast<PF_ParamIndex>(kCustomColor1 + color_index);
                write_color(params[changed], color);
                needs_custom = true;
            }
        }
    } else if (index == kRampBar && hit == studio::Hit::dither_mode) {
        if (click) {
            changed = kDitherMode;
            params[changed]->u.pd.value = settings.dither_mode % 3 + 1;
        }
    } else if (index == kRampBar && hit == studio::Hit::dither_matrix) {
        if (click) {
            changed = kDitherMatrix;
            params[changed]->u.pd.value = settings.dither_matrix == 4 ? 2 : 1;
            needs_bayer = settings.dither_mode != static_cast<int>(toonpixel::DitherMode::bayer);
        }
    } else if (index == kRampBar && hit == studio::Hit::dither_threshold) {
        changed = kDitherThreshold;
        params[changed]->u.fs_d.value = click ? std::clamp(x / 0.62f, 0.0f, 1.0f) :
            std::clamp(settings.dither_threshold + x / 0.62f, 0.0f, 1.0f);
        continuous = true;
        needs_bayer = settings.dither_mode == static_cast<int>(toonpixel::DitherMode::off);
    } else if (hit == studio::Hit::tint) {
        if (click && event->u.do_click.num_clicks >= 2) {
            auto color = settings.grade.tint;
            bool accepted = false;
            const PF_Err error = pick_color(in_data, "Oklch Tint", color, accepted);
            if (error) return error;
            if (accepted) {
                changed = kTintColor;
                write_color(params[changed], color);
            }
        } else {
            const int width = event->effect_win.current_frame.right - event->effect_win.current_frame.left;
            const int height = event->effect_win.current_frame.bottom - event->effect_win.current_frame.top;
            float dx = 0.0f;
            float dy = 0.0f;
            if (click) {
                const auto layout = studio::wheel_layout(width, height);
                const float radius = std::max(1.0f, layout.size * 0.5f - 1.5f);
                dx = (x * width - layout.left - layout.size * 0.5f) / radius;
                dy = (y * height - layout.top - layout.size * 0.5f) / radius;
            } else {
                const auto lab = toonpixel::linear_to_oklab(settings.grade.tint);
                dx = lab.a / 0.25f + x;
                dy = lab.b / 0.25f + y;
            }
            const float angle = std::atan2(dy, dx);
            const float chroma = std::min(1.0f, std::hypot(dx, dy)) * 0.25f;
            const auto color = toonpixel::oklab_to_linear({
                0.72f, chroma * std::cos(angle), chroma * std::sin(angle)});
            changed = kTintColor;
            write_color(params[changed], color);
            continuous = true;
        }
    } else if (hit == studio::Hit::tint_amount) {
        const int width = event->effect_win.current_frame.right - event->effect_win.current_frame.left;
        const int height = event->effect_win.current_frame.bottom - event->effect_win.current_frame.top;
        const auto layout = studio::wheel_layout(width, height);
        changed = kTintAmount;
        params[changed]->u.fs_d.value = click ? std::clamp((x * width - layout.left) / layout.size, 0.0f, 1.0f) :
            std::clamp(settings.grade.tint_amount + x, 0.0f, 1.0f);
        continuous = true;
    } else if (hit >= studio::Hit::exposure && hit <= studio::Hit::chroma) {
        const int width = event->effect_win.current_frame.right - event->effect_win.current_frame.left;
        const float track_left = studio::slider_track_left(width);
        const float value = std::clamp((x * width - track_left) / std::max(1.0f, width - 2.0f - track_left), 0.0f, 1.0f);
        switch (hit) {
            case studio::Hit::exposure:
                changed = kExposure;
                params[changed]->u.fs_d.value = click ? value * 4.0f - 2.0f :
                    std::clamp(settings.grade.exposure + x * 4.0f, -2.0f, 2.0f);
                break;
            case studio::Hit::vibrance:
                changed = kVibrance;
                params[changed]->u.fs_d.value = click ? value * 2.0f - 1.0f :
                    std::clamp(settings.grade.vibrance + x * 2.0f, -1.0f, 1.0f);
                break;
            case studio::Hit::hue:
                changed = kHue;
                params[changed]->u.fs_d.value = click ? value * 360.0f - 180.0f :
                    std::clamp(settings.grade.hue + x * 360.0f, -180.0f, 180.0f);
                break;
            case studio::Hit::chroma:
                changed = kChroma;
                params[changed]->u.fs_d.value = click ? value * 2.0f :
                    std::clamp(settings.grade.chroma + x * 2.0f, 0.0f, 2.0f);
                break;
            default:
                break;
        }
        continuous = true;
    }
    if (changed == 0 || (!click && !continuous)) {
        event->evt_out_flags |= PF_EO_HANDLED_EVENT;
        return PF_Err_NONE;
    }
    PF_Err error = PF_Err_NONE;
    if (needs_luma && settings.toon_model != static_cast<int>(toonpixel::ToonModel::luma_ramp)) {
        params[kToonModel]->u.pd.value = static_cast<A_long>(toonpixel::ToonModel::luma_ramp);
        error = update_parameter(in_data, kToonModel, params);
    }
    if (!error && needs_custom) {
        params[kPaletteMode]->u.pd.value = static_cast<A_long>(toonpixel::PaletteMode::custom);
        error = update_parameter(in_data, kPaletteMode, params);
    }
    if (!error && needs_bayer) {
        params[kDitherMode]->u.pd.value = static_cast<A_long>(toonpixel::DitherMode::bayer);
        error = update_parameter(in_data, kDitherMode, params);
    }
    if (!error) error = update_parameter(in_data, changed, params);
    if (error) return error;
    if (!click && continuous) {
        Suites suites(in_data);
        PF_Point mouse{};
        const PF_Err mouse_error = suites.error() ? suites.error() : suites.app->PF_GetMouse(&mouse);
        if (mouse_error) return mouse_error;
        event->u.do_click.continue_refcon[2] = mouse.h;
        event->u.do_click.continue_refcon[3] = mouse.v;
    }
    if (click && continuous) {
        event->u.do_click.continue_refcon[0] = static_cast<A_intptr_t>(index);
        event->u.do_click.send_drag = true;
    }
    event->evt_out_flags |= PF_EO_HANDLED_EVENT | PF_EO_UPDATE_NOW;
    out_data->out_flags |= PF_OutFlag_REFRESH_UI | PF_OutFlag_FORCE_RERENDER;
    TOONPIXEL_LOG("studio_event type=%ld index=%ld hit=%ld changed=%ld x=%.4f y=%.4f last=%d",
        static_cast<long>(event->e_type), static_cast<long>(index), static_cast<int>(hit),
        static_cast<long>(changed), x, y, event->u.do_click.last_time ? 1 : 0);
#if TOONPIXEL_DEBUG_LOG
    if (index == kRampBar && hit >= studio::Hit::boundary1 && hit <= studio::Hit::boundary4) {
        TOONPIXEL_LOG("studio_boundary changed=%ld value=%.9f on_handle=%d",
            static_cast<long>(changed), params[changed]->u.fs_d.value, on_handle ? 1 : 0);
    }
#endif
    return PF_Err_NONE;
}
