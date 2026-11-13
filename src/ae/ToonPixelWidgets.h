#pragma once

#include "AE_EffectUI.h"
#include "AE_EffectSuites.h"
#include "adobesdk/DrawbotSuite.h"
#include "ToonPixelStudio.h"
#include "ToonPixelLocalization.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

#if defined(__APPLE__) && defined(__MACH__)
#include <CoreFoundation/CoreFoundation.h>
#ifdef TOONPIXEL_HOST
#include "ToonPixelColorPicker.h"
#endif
#endif

namespace widgets {

class Suites {
public:
    explicit Suites(PF_InData* in_data) : basic_(in_data ? in_data->pica_basicP : nullptr) {
        if (!basic_) {
            error_ = PF_Err_INVALID_CALLBACK;
            return;
        }
        acquire(kPFEffectCustomUISuite, kPFEffectCustomUISuiteVersion1, custom);
        acquire(kDRAWBOT_DrawSuite, kDRAWBOT_DrawSuite_Version1, drawbot);
        acquire(kDRAWBOT_SupplierSuite, kDRAWBOT_SupplierSuite_Version1, supplier);
        acquire(kDRAWBOT_SurfaceSuite, kDRAWBOT_SurfaceSuite_Version1, surface);
        acquire(kPFAppSuite, kPFAppSuiteVersion4, app);
    }

    ~Suites() {
        release(kPFAppSuite, kPFAppSuiteVersion4, app);
        release(kDRAWBOT_SurfaceSuite, kDRAWBOT_SurfaceSuite_Version1, surface);
        release(kDRAWBOT_SupplierSuite, kDRAWBOT_SupplierSuite_Version1, supplier);
        release(kDRAWBOT_DrawSuite, kDRAWBOT_DrawSuite_Version1, drawbot);
        release(kPFEffectCustomUISuite, kPFEffectCustomUISuiteVersion1, custom);
    }

    PF_Err error() const { return error_; }

    const PF_EffectCustomUISuite1* custom = nullptr;
    const DRAWBOT_DrawbotSuite1* drawbot = nullptr;
    const DRAWBOT_SupplierSuite1* supplier = nullptr;
    const DRAWBOT_SurfaceSuite1* surface = nullptr;
    const PFAppSuite4* app = nullptr;

private:
    template <typename Suite>
    void acquire(const char* name, long version, Suite*& suite) {
        if (error_) return;
        error_ = basic_->AcquireSuite(name, version, reinterpret_cast<const void**>(&suite));
    }

    template <typename Suite>
    void release(const char* name, long version, Suite*& suite) {
        if (basic_ && suite) (void)basic_->ReleaseSuite(name, version);
    }

    const SPBasicSuite* basic_ = nullptr;
    PF_Err error_ = PF_Err_NONE;
};

inline PF_ContextPtr effect_context(PF_EventExtra* event) {
    if (!event || !event->contextH || !*event->contextH) return nullptr;
    PF_ContextPtr context = *event->contextH;
    return (context->magic == PF_CONTEXT_MAGIC || context->magic == 0) &&
        context->w_type == PF_Window_EFFECT ? context : nullptr;
}

inline const scene::Analysis* get_analysis(PF_EventExtra* event) {
    PF_ContextPtr context = effect_context(event);
    return context ? reinterpret_cast<const scene::Analysis*>(context->plugin_state[2]) : nullptr;
}

inline PF_Err store_analysis(PF_EventExtra* event, const scene::Analysis& value) {
    PF_ContextPtr context = effect_context(event);
    if (!context) return PF_Err_NONE;
    try {
        auto* analysis = new scene::Analysis(value);
        delete reinterpret_cast<scene::Analysis*>(context->plugin_state[2]);
        context->plugin_state[2] = reinterpret_cast<A_intptr_t>(analysis);
        return PF_Err_NONE;
    } catch (const std::bad_alloc&) {
        return PF_Err_OUT_OF_MEMORY;
    }
}

inline double interaction_now() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

inline studio::InteractionState* interaction_state(PF_EventExtra* event) {
    PF_ContextPtr context = effect_context(event);
    return context ? reinterpret_cast<studio::InteractionState*>(context->plugin_state[3]) : nullptr;
}

inline studio::InteractionState* ensure_interaction_state(PF_EventExtra* event) {
    PF_ContextPtr context = effect_context(event);
    if (!context) return nullptr;
    auto* state = interaction_state(event);
    if (!state) {
        state = new (std::nothrow) studio::InteractionState;
        if (state) context->plugin_state[3] = reinterpret_cast<A_intptr_t>(state);
    }
    return state;
}

inline void remember_frame(PF_EventExtra* event) {
    auto* state = ensure_interaction_state(event);
    if (!state) return;
    const auto& frame = event->effect_win.current_frame;
    state->frame = PF_Rect{frame.left, frame.top, frame.right, frame.bottom};
}

inline bool reduced_motion() {
#if defined(__APPLE__) && defined(__MACH__)
    const CFPropertyListRef value = CFPreferencesCopyAppValue(CFSTR("reduceMotion"), CFSTR("com.apple.universalaccess"));
    bool reduced = false;
    if (value) {
        if (CFGetTypeID(value) == CFBooleanGetTypeID()) {
            reduced = CFBooleanGetValue(static_cast<CFBooleanRef>(value));
        }
        CFRelease(value);
    }
    return reduced;
#else
    return false;
#endif
}

inline PF_Err invalidate_interaction(PF_InData* in_data, PF_EventExtra* event, studio::InteractionState& state) {
    if (state.frame.right <= state.frame.left || state.frame.bottom <= state.frame.top) return PF_Err_NONE;
    Suites suites(in_data);
    const PF_Err error = suites.error();
    if (error) return error;
    return suites.app->PF_InvalidateRect(event->contextH, &state.frame);
}

inline PF_Err update_interaction(PF_InData* in_data, PF_EventExtra* event, studio::Hit hit, bool pressed) {
    auto* state = ensure_interaction_state(event);
    if (!state) {
        TOONPIXEL_LOG("studio_feedback_unavailable hit=%ld", static_cast<long>(hit));
        return PF_Err_NONE;
    }
    if (!studio::retarget_feedback(*state, hit, pressed, interaction_now(), reduced_motion())) return PF_Err_NONE;
    TOONPIXEL_LOG("studio_feedback hit=%ld pressed=%d from=%.6f target=%.6f reduced=%d",
        static_cast<long>(state->display_hit), state->pressed ? 1 : 0, state->from, state->target,
        state->reduced_motion ? 1 : 0);
    return invalidate_interaction(in_data, event, *state);
}

inline studio::VisualState interaction_visual(PF_EventExtra* event) {
    const auto* state = interaction_state(event);
    return state ? studio::VisualState{state->display_hit, studio::feedback_value(*state, interaction_now())} :
        studio::VisualState{};
}

inline PF_Err idle_interaction(PF_InData* in_data, PF_EventExtra* event) {
    auto* state = interaction_state(event);
    if (!state || !studio::needs_feedback_frame(*state, interaction_now())) return PF_Err_NONE;
    if (state->diagnostic_idle_events < 64) {
        ++state->diagnostic_idle_events;
        TOONPIXEL_LOG("studio_idle hit=%ld value=%.6f target=%.6f",
            static_cast<long>(state->display_hit), studio::feedback_value(*state, interaction_now()), state->target);
    }
    const PF_Err error = invalidate_interaction(in_data, event, *state);
    if (!error) event->evt_out_flags |= PF_EO_HANDLED_EVENT | PF_EO_UPDATE_NOW;
    return error;
}

inline PF_Err draw_widget(PF_InData* in_data, PF_EventExtra* event, const RenderSettings& settings,
    const studio::VisualState& feedback = {}) {
    if (!event || event->effect_win.index < 0) return PF_Err_BAD_CALLBACK_PARAM;
    const auto& frame = event->effect_win.current_frame;
    const int width = std::max<A_long>(1, frame.right - frame.left);
    const int height = std::max<A_long>(1, frame.bottom - frame.top);
    const bool ramp_enabled = settings.mode != static_cast<int>(OutputMode::pixel_only);
    std::vector<unsigned char> pixels;
    try {
        if (event->effect_win.index == kRampBar) pixels = studio::ramp_pixels(width, height, settings, ramp_enabled, get_analysis(event), feedback);
        else if (event->effect_win.index == kColorWheel) pixels = studio::wheel_pixels(width, height, settings, feedback);
        else if (event->effect_win.index == kAsciiEditor) pixels = studio::ascii_pixels(width, height, settings);
        else return PF_Err_NONE;
    } catch (const std::bad_alloc&) {
        return PF_Err_OUT_OF_MEMORY;
    }
    Suites suites(in_data);
    PF_Err error = suites.error();
    DRAWBOT_DrawRef draw_ref = nullptr;
    DRAWBOT_SupplierRef supplier_ref = nullptr;
    DRAWBOT_SurfaceRef surface_ref = nullptr;
    DRAWBOT_ImageRef image_ref = nullptr;
    DRAWBOT_BrushRef brush_ref = nullptr;
    DRAWBOT_FontRef font_ref = nullptr;
    DRAWBOT_Boolean supports_argb = 0;
    DRAWBOT_Boolean supports_bgra = 0;
    if (!error) error = suites.custom->PF_GetDrawingReference(event->contextH, &draw_ref);
    if (!error && draw_ref) error = suites.drawbot->GetSupplier(draw_ref, &supplier_ref);
    else if (!error) error = PF_Err_INVALID_CALLBACK;
    if (!error) error = suites.drawbot->GetSurface(draw_ref, &surface_ref);
    if (!error) error = suites.supplier->SupportsPixelLayoutARGB(supplier_ref, &supports_argb);
    if (!error) error = suites.supplier->SupportsPixelLayoutBGRA(supplier_ref, &supports_bgra);
    std::vector<unsigned char> packed;
    try {
        packed = studio::pack_drawbot_pixels(pixels, supports_argb != 0, supports_bgra != 0);
    } catch (const std::bad_alloc&) {
        return PF_Err_OUT_OF_MEMORY;
    }
    if (!error && packed.empty()) error = PF_Err_BAD_CALLBACK_PARAM;
    const DRAWBOT_PixelLayout layout = supports_argb ?
        kDRAWBOT_PixelLayout_32ARGB_Straight : kDRAWBOT_PixelLayout_32BGRA_Straight;
    if (!error) error = suites.supplier->NewImageFromBuffer(supplier_ref, width, height, width * 4,
        layout, packed.data(), &image_ref);
    bool pushed = false;
    if (!error) {
        error = suites.surface->PushStateStack(surface_ref);
        pushed = !error;
    }
    if (!error) {
        const DRAWBOT_Rect32 clip{frame.left, frame.top, width, height};
        error = suites.surface->Clip(surface_ref, supplier_ref, &clip);
    }
    if (!error) {
        const DRAWBOT_PointF32 origin{static_cast<float>(frame.left), static_cast<float>(frame.top)};
        error = suites.surface->DrawImage(surface_ref, image_ref, &origin, 1.0f);
    }
    if (!error) {
        const DRAWBOT_ColorRGBA text_color{0.78f, 0.80f, 0.82f, 1.0f};
        error = suites.supplier->NewBrush(supplier_ref, &text_color, &brush_ref);
        if (!error) error = suites.supplier->NewDefaultFont(supplier_ref, 9.0f, &font_ref);
    }
    auto caption = [&](const std::string& text, float x, float y, float available) {
        if (error) return;
        const auto decoded = localization::utf16_from_utf8(text);
        DRAWBOT_UTF16Char utf16[96]{};
        const std::size_t count = std::min(decoded.size(), std::size_t{95});
        for (std::size_t index = 0; index < count; ++index) utf16[index] = static_cast<DRAWBOT_UTF16Char>(decoded[index]);
        const DRAWBOT_PointF32 origin{x, y};
        error = suites.surface->DrawString(surface_ref, brush_ref, font_ref, utf16, &origin,
            kDRAWBOT_TextAlignment_Left, kDRAWBOT_TextTruncation_End, std::max(1.0f, available));
    };
    const auto language = settings.language;
    if (event->effect_win.index == kRampBar) {
        caption(localization::text(localization::Text::preserve, language), frame.left + 3.0f, frame.top + 11.0f, width * 0.45f);
        caption(localization::text(localization::Text::ramp, language), frame.left + width * 0.53f, frame.top + 11.0f, width * 0.20f);
        const auto* analysis = get_analysis(event);
        caption(localization::text(analysis && analysis->uniform ? localization::Text::flat : localization::Text::automatic, language),
            frame.left + width * 0.79f, frame.top + 11.0f, width * 0.20f);
        char text[64];
        const localization::Text names[]{
            localization::Text::darkest, localization::Text::shadow, localization::Text::middle,
            localization::Text::light, localization::Text::highlight};
        for (int index = 0; index < 5; ++index) {
            if (analysis && analysis->valid && !analysis->uniform) {
                std::snprintf(text, sizeof(text), "%s %.2f", localization::text(names[index], language), analysis->levels[index]);
                caption(text, frame.left + width * (index * 0.20f + 0.01f), frame.top + height * 0.455f, width * 0.19f);
            } else {
                caption(localization::text(names[index], language), frame.left + width * (index * 0.20f + 0.07f),
                    frame.top + height * 0.455f, width * 0.12f);
            }
        }
        std::snprintf(text, sizeof(text), "%s %.3f", localization::text(localization::Text::softness, language), settings.ramp.softness);
        caption(text, frame.left + 1.0f, frame.top + height * 0.495f, width * 0.50f);
        std::snprintf(text, sizeof(text), "-  %d  +", settings.ramp.repeat);
        caption(text, frame.left + width * 0.75f, frame.top + height * 0.525f, width * 0.25f);
        const localization::Text palette_names[]{
            localization::Text::original8, localization::Text::style10, localization::Text::custom, localization::Text::ramp_linked};
        std::snprintf(text, sizeof(text), "%s%s", localization::text(localization::Text::palette, language),
            localization::text(palette_names[settings.palette_mode - 1], language));
        caption(text, frame.left + 1.0f, frame.top + height * 0.625f, width * 0.62f);
        if (settings.palette_mode == static_cast<int>(toonpixel::PaletteMode::custom)) {
            std::snprintf(text, sizeof(text), "- %d +", settings.palette_count);
        } else {
            std::snprintf(text, sizeof(text), "%zu", settings.palette().size());
        }
        caption(text, frame.left + width * 0.75f, frame.top + height * 0.625f, width * 0.25f);
        const localization::Text dither_names[]{localization::Text::off, localization::Text::bayer, localization::Text::floyd};
        std::snprintf(text, sizeof(text), "%s%s %.0f%%", localization::text(localization::Text::dither, language),
            localization::text(dither_names[settings.dither_mode - 1], language), settings.dither * 100.0f);
        caption(text, frame.left + 1.0f, frame.top + height * 0.845f, width * 0.60f);
        std::snprintf(text, sizeof(text), "%dx%d", settings.dither_matrix, settings.dither_matrix);
        caption(text, frame.left + width * 0.79f, frame.top + height * 0.845f, width * 0.21f);
        std::snprintf(text, sizeof(text), "%s %.2f", localization::text(localization::Text::threshold, language), settings.dither_threshold);
        caption(text, frame.left + 1.0f, frame.top + height * 0.925f, width * 0.55f);
    } else if (event->effect_win.index == kColorWheel) {
        const auto layout = studio::wheel_layout(width, height);
        char text[64];
        const int swatch_top = static_cast<int>(layout.top + layout.size + 6.0f);
        std::snprintf(text, sizeof(text), "%s %d%%", localization::text(localization::Text::tint, language),
            static_cast<int>(std::lround(settings.grade.tint_amount * 100.0f)));
        caption(text, frame.left + layout.left + 14.0f, frame.top + swatch_top + 13.0f, layout.size - 14.0f);
        const localization::Text names[]{
            localization::Text::exposure, localization::Text::vibrance, localization::Text::hue, localization::Text::chroma};
        const float values[]{settings.grade.exposure, settings.grade.vibrance, settings.grade.hue, settings.grade.chroma};
        for (int row = 0; row < 4; ++row) {
            const float top = frame.top + height * 0.25f * row;
            caption(localization::text(names[row], language), frame.left + width * 0.40f, top + 10.0f, width * 0.30f);
            std::snprintf(text, sizeof(text), "%.2f", values[row]);
            caption(text, frame.left + width * 0.76f, top + 10.0f, width * 0.22f);
        }
    } else if (event->effect_win.index == kAsciiEditor) {
        caption(localization::text(localization::Text::ascii_hint, language), frame.left + 40.0f, frame.top + 44.0f, width - 42.0f);
    }
    if (!error) error = suites.surface->Flush(surface_ref);
    if (pushed) (void)suites.surface->PopStateStack(surface_ref);
    if (font_ref) (void)suites.supplier->ReleaseObject(reinterpret_cast<DRAWBOT_ObjectRef>(font_ref));
    if (brush_ref) (void)suites.supplier->ReleaseObject(reinterpret_cast<DRAWBOT_ObjectRef>(brush_ref));
    if (image_ref) (void)suites.supplier->ReleaseObject(reinterpret_cast<DRAWBOT_ObjectRef>(image_ref));
    if (!error) event->evt_out_flags |= PF_EO_HANDLED_EVENT;
    TOONPIXEL_LOG("widget_draw index=%ld frame=%ld,%ld,%ld,%ld argb=%d bgra=%d error=%ld",
        static_cast<long>(event->effect_win.index), static_cast<long>(frame.left), static_cast<long>(frame.top),
        static_cast<long>(frame.right), static_cast<long>(frame.bottom), static_cast<int>(supports_argb),
        static_cast<int>(supports_bgra), static_cast<long>(error));
    return error;
}

inline PF_Err pick_color(PF_InData* in_data, const char* title, toonpixel::Rgb& color, bool& accepted) {
    accepted = false;
    Suites suites(in_data);
    PF_Err error = suites.error();
    TOONPIXEL_LOG("pick_color_entry title=%s effect_ref=%p suite_error=%ld", title,
        in_data ? static_cast<void*>(in_data->effect_ref) : nullptr, static_cast<long>(error));
    if (error) return error;
#if TOONPIXEL_HOST
    if (in_data && in_data->effect_ref) {
        const double started = interaction_now();
        const int result = ToonPixelPickColor(title, &color.red, &color.green, &color.blue);
        TOONPIXEL_LOG("native_color_picker title=%s result=%d seconds=%.6f rgb=%.9f,%.9f,%.9f",
            title, result, interaction_now() - started, color.red, color.green, color.blue);
        if (result == 1) {
            TOONPIXEL_LOG("color_picker title=%s cancelled", title);
            return PF_Err_NONE;
        }
        if (result != 0) return PF_Err_INVALID_CALLBACK;
        color = {
            std::clamp(color.red, 0.0f, 1.0f),
            std::clamp(color.green, 0.0f, 1.0f),
            std::clamp(color.blue, 0.0f, 1.0f),
        };
        accepted = true;
        TOONPIXEL_LOG("color_picker title=%s accepted=1", title);
        return PF_Err_NONE;
    } else {
#endif
    PF_PixelFloat initial{};
    initial.alpha = 1.0f;
    initial.red = color.red;
    initial.green = color.green;
    initial.blue = color.blue;
    PF_PixelFloat picked = initial;
    const double started = interaction_now();
    error = suites.app->PF_AppColorPickerDialog(title, &initial, false, &picked);
    TOONPIXEL_LOG("color_picker_return title=%s error=%ld seconds=%.6f initial=%.9f,%.9f,%.9f picked=%.9f,%.9f,%.9f",
        title, static_cast<long>(error), interaction_now() - started,
        initial.red, initial.green, initial.blue, picked.red, picked.green, picked.blue);
    if (error == PF_Interrupt_CANCEL) {
        TOONPIXEL_LOG("color_picker title=%s cancelled", title);
        return PF_Err_NONE;
    }
    if (error) return error;
    color = {
        std::clamp(picked.red, 0.0f, 1.0f),
        std::clamp(picked.green, 0.0f, 1.0f),
        std::clamp(picked.blue, 0.0f, 1.0f),
    };
    accepted = true;
    TOONPIXEL_LOG("color_picker title=%s accepted=1", title);
    return PF_Err_NONE;
#if TOONPIXEL_HOST
    }
#endif
}

inline PF_Err local_point(PF_InData* in_data, const PF_EventExtra* event, float& x, float& y) {
    const auto& frame = event->effect_win.current_frame;
    const float width = std::max<A_long>(1, frame.right - frame.left);
    const float height = std::max<A_long>(1, frame.bottom - frame.top);
    const PF_Point point = event->u.do_click.screen_point;
    if (event->e_type == PF_Event_DRAG) {
        Suites suites(in_data);
        if (suites.error()) return suites.error();
#if TOONPIXEL_DEBUG_LOG
        PF_Point mouse{};
        const PF_Err mouse_error = suites.app->PF_GetMouse(&mouse);
        TOONPIXEL_LOG("widget_drag_mouse point=%ld,%ld mouse=%ld,%ld error=%ld",
            static_cast<long>(point.h), static_cast<long>(point.v),
            static_cast<long>(mouse.h), static_cast<long>(mouse.v), static_cast<long>(mouse_error));
#endif
        if (event->u.do_click.continue_refcon[0] != 0) {
            const auto hit = static_cast<studio::Hit>(event->u.do_click.continue_refcon[1]);
            const bool track_mouse = (event->effect_win.index == kRampBar &&
                ((hit >= studio::Hit::boundary1 && hit <= studio::Hit::boundary4) ||
                    hit == studio::Hit::softness || hit == studio::Hit::dither_threshold)) ||
                event->effect_win.index == kColorWheel;
            if (track_mouse) {
                PF_Point mouse{};
                const PF_Err error = suites.app->PF_GetMouse(&mouse);
                if (error) return error;
                x = (mouse.h - event->u.do_click.continue_refcon[2]) / width;
                y = (mouse.v - event->u.do_click.continue_refcon[3]) / height;
#if TOONPIXEL_DEBUG_LOG
                TOONPIXEL_LOG("widget_mouse_delta point=%ld,%ld mouse=%ld,%ld previous=%ld,%ld delta=%.9f,%.9f",
                    static_cast<long>(point.h), static_cast<long>(point.v),
                    static_cast<long>(mouse.h), static_cast<long>(mouse.v),
                    static_cast<long>(event->u.do_click.continue_refcon[2]),
                    static_cast<long>(event->u.do_click.continue_refcon[3]), x, y);
#endif
                return PF_Err_NONE;
            }
            const PF_Point click{
                static_cast<A_long>(event->u.do_click.continue_refcon[2]),
                static_cast<A_long>(event->u.do_click.continue_refcon[3])};
            const float delta_x = (point.h - click.h) / width;
            const float delta_y = (point.v - click.v) / height;
            x = (click.h - frame.left) / width + delta_x;
            y = (click.v - frame.top) / height + delta_y;
#if TOONPIXEL_DEBUG_LOG
            TOONPIXEL_LOG("widget_drag_anchor point=%ld,%ld previous=%ld,%ld frame=%ld,%ld delta=%.9f,%.9f",
                static_cast<long>(point.h), static_cast<long>(point.v),
                static_cast<long>(click.h), static_cast<long>(click.v),
                static_cast<long>(frame.left), static_cast<long>(frame.top), delta_x, delta_y);
#endif
            return PF_Err_NONE;
        }
        PF_Point local_origin{0, 0};
        PF_Point global_origin{};
        const PF_Err error = suites.app->PF_ConvertLocalToGlobal(&local_origin, &global_origin);
        if (error) return error;
        x = (point.h - global_origin.h) / width;
        y = (point.v - global_origin.v) / height;
        return PF_Err_NONE;
    }
#if TOONPIXEL_DEBUG_LOG
    if (event->e_type == PF_Event_DO_CLICK || event->e_type == PF_Event_DRAG) {
        Suites suites(in_data);
        PF_Point zero{0, 0};
        PF_Point frame_origin{frame.left, frame.top};
        PF_Point zero_global{};
        PF_Point frame_global{};
        const PF_Err zero_error = suites.error() ? suites.error() : suites.app->PF_ConvertLocalToGlobal(&zero, &zero_global);
        const PF_Err frame_error = suites.error() ? suites.error() : suites.app->PF_ConvertLocalToGlobal(&frame_origin, &frame_global);
        TOONPIXEL_LOG("widget_origin zero=%ld,%ld frame=%ld,%ld errors=%ld,%ld",
            static_cast<long>(zero_global.h), static_cast<long>(zero_global.v),
            static_cast<long>(frame_global.h), static_cast<long>(frame_global.v),
            static_cast<long>(zero_error), static_cast<long>(frame_error));
        PF_Point mouse{};
        const PF_Err mouse_error = suites.error() ? suites.error() : suites.app->PF_GetMouse(&mouse);
        PF_Point relative{
            point.h - (event->e_type == PF_Event_DRAG ? zero_global.h : frame.left),
            point.v - (event->e_type == PF_Event_DRAG ? zero_global.v : frame.top)};
        PF_Point relative_global{};
        const PF_Err relative_error = suites.error() ? suites.error() :
            suites.app->PF_ConvertLocalToGlobal(&relative, &relative_global);
        TOONPIXEL_LOG("widget_click_mouse error=%ld point=%ld,%ld relative_global=%ld,%ld conversion_error=%ld",
            static_cast<long>(mouse_error), static_cast<long>(mouse.h), static_cast<long>(mouse.v),
            static_cast<long>(relative_global.h), static_cast<long>(relative_global.v),
            static_cast<long>(relative_error));
    }
#endif
    TOONPIXEL_LOG("widget_input type=%ld screen=%ld,%ld frame=%ld,%ld,%ld,%ld last=%d",
        static_cast<long>(event->e_type), static_cast<long>(point.h), static_cast<long>(point.v),
        static_cast<long>(frame.left), static_cast<long>(frame.top), static_cast<long>(frame.right),
        static_cast<long>(frame.bottom), event->u.do_click.last_time ? 1 : 0);
    x = (point.h - frame.left) / width;
    y = (point.v - frame.top) / height;
    return PF_Err_NONE;
}

inline PF_Err update_parameter(PF_InData* in_data, PF_ParamIndex index, PF_ParamDef* params[]) {
    ParamUtilsHandle suites(in_data);
    const PF_Err error = suites.error();
    if (error) return error;
    params[index]->flags |= PF_ChangeFlag_CHANGED_VALUE;
    params[index]->uu.change_flags |= PF_ChangeFlag_CHANGED_VALUE;
    return suites.suite()->PF_UpdateParamUI(in_data->effect_ref, index, params[index]);
}

inline PF_Err analyze_current_frame(PF_InData* in_data, PF_OutData* out_data,
    PF_ParamDef* params[], PF_EventExtra* event) {
    PF_ParamDef input_parameter{};
    PF_Err error = PF_CHECKOUT_PARAM(in_data, kInput, in_data->current_time,
        in_data->time_step, in_data->time_scale, &input_parameter);
    if (error) return error;
    scene::Analysis analysis;
    WorldSuiteHandle suites(in_data);
    error = suites.error();
    PF_PixelFormat format = PF_PixelFormat_INVALID;
    if (!error) error = suites.format(&input_parameter.u.ld, format);
    if (!error) {
        switch (format) {
            case PF_PixelFormat_ARGB32: analysis = scene::analyze_world<PF_Pixel8>(&input_parameter.u.ld); break;
            case PF_PixelFormat_ARGB64: analysis = scene::analyze_world<PF_Pixel16>(&input_parameter.u.ld); break;
            case PF_PixelFormat_ARGB128: analysis = scene::analyze_world<PF_PixelFloat>(&input_parameter.u.ld); break;
            default: error = PF_Err_BAD_CALLBACK_PARAM; break;
        }
    }
    const PF_Err checkin_error = PF_CHECKIN_PARAM(in_data, &input_parameter);
    if (!error) error = checkin_error;
    if (error) return error;
    error = store_analysis(event, analysis);
    if (error) return error;
    TOONPIXEL_LOG("scene_analysis valid=%d uniform=%d levels=%.5f,%.5f,%.5f,%.5f,%.5f boundaries=%.5f,%.5f,%.5f,%.5f",
        analysis.valid ? 1 : 0, analysis.uniform ? 1 : 0, analysis.levels[0], analysis.levels[1],
        analysis.levels[2], analysis.levels[3], analysis.levels[4], analysis.boundaries[0],
        analysis.boundaries[1], analysis.boundaries[2], analysis.boundaries[3]);
    event->evt_out_flags |= PF_EO_HANDLED_EVENT | PF_EO_UPDATE_NOW;
    out_data->out_flags |= PF_OutFlag_REFRESH_UI;
    if (!analysis.valid || analysis.uniform) return PF_Err_NONE;
    if (params[kMode]->u.pd.value == static_cast<A_long>(OutputMode::pixel_only)) {
        params[kMode]->u.pd.value = static_cast<A_long>(OutputMode::toon_pixel);
        error = update_parameter(in_data, kMode, params);
        if (error) return error;
    }
    if (params[kToonModel]->u.pd.value != static_cast<A_long>(toonpixel::ToonModel::luma_ramp)) {
        params[kToonModel]->u.pd.value = static_cast<A_long>(toonpixel::ToonModel::luma_ramp);
        error = update_parameter(in_data, kToonModel, params);
        if (error) return error;
    }
    for (std::size_t index = 0; index < analysis.boundaries.size(); ++index) {
        const auto parameter = kRampBoundaryParameters[index];
        params[parameter]->u.fs_d.value = analysis.boundaries[index];
        error = update_parameter(in_data, parameter, params);
        if (error) return error;
    }
    out_data->out_flags |= PF_OutFlag_FORCE_RERENDER;
    return PF_Err_NONE;
}

#include "ToonPixelStudioEvents.h"

inline PF_Err widget_event(PF_InData* in_data, PF_OutData* out_data,
    PF_ParamDef* params[], PF_EventExtra* event) {
    if (!event) return PF_Err_BAD_CALLBACK_PARAM;
    TOONPIXEL_LOG("widget_event_dispatch type=%ld index=%ld num_params=%ld",
        static_cast<long>(event->e_type), static_cast<long>(event->effect_win.index),
        static_cast<long>(in_data ? in_data->num_params : -1));
    PF_ContextPtr context = effect_context(event);
#if TOONPIXEL_DEBUG_LOG
    if (event->e_type == PF_Event_NEW_CONTEXT || event->e_type == PF_Event_DO_CLICK ||
        event->e_type == PF_Event_ADJUST_CURSOR) {
        const auto* raw = event->contextH ? *event->contextH : nullptr;
        TOONPIXEL_LOG("widget_context type=%ld area=%ld raw=%p magic=%lx window=%ld state=%p",
            static_cast<long>(event->e_type), static_cast<long>(event->effect_win.area),
            static_cast<const void*>(raw), static_cast<unsigned long>(raw ? raw->magic : 0),
            static_cast<long>(raw ? raw->w_type : -1),
            context ? reinterpret_cast<void*>(context->plugin_state[3]) : nullptr);
    }
#endif
    if ((event->e_type == PF_Event_DO_CLICK || event->e_type == PF_Event_DRAG ||
        event->e_type == PF_Event_ADJUST_CURSOR) && event->effect_win.area != PF_EA_CONTROL) {
        return PF_Err_NONE;
    }
    if (event->e_type == PF_Event_NEW_CONTEXT) {
        if (context) {
            auto* interaction = new (std::nothrow) studio::InteractionState;
            if (!interaction) return PF_Err_OUT_OF_MEMORY;
            context->plugin_state[0] = 1;
            context->plugin_state[1] = 0;
            context->plugin_state[2] = 0;
            context->plugin_state[3] = reinterpret_cast<A_intptr_t>(interaction);
            event->evt_out_flags |= PF_EO_HANDLED_EVENT;
        }
        return PF_Err_NONE;
    }
    if (event->e_type == PF_Event_ACTIVATE || event->e_type == PF_Event_DEACTIVATE) {
        if (context && context->plugin_state[0] != 0) {
            context->plugin_state[0] = event->e_type == PF_Event_ACTIVATE ? 2 : 1;
            PF_Err error = PF_Err_NONE;
            if (event->e_type == PF_Event_DEACTIVATE) {
                auto* interaction = interaction_state(event);
                if (interaction && studio::retarget_feedback(*interaction, studio::Hit::none, false,
                    interaction_now(), true)) error = invalidate_interaction(in_data, event, *interaction);
            }
            if (!error) event->evt_out_flags |= PF_EO_HANDLED_EVENT;
            return error;
        }
        return PF_Err_NONE;
    }
    if (event->e_type == PF_Event_IDLE) return idle_interaction(in_data, event);
    if (event->e_type == PF_Event_CLOSE_CONTEXT) {
        if (context) {
            delete reinterpret_cast<scene::Analysis*>(context->plugin_state[2]);
            delete interaction_state(event);
            context->plugin_state[0] = 0;
            context->plugin_state[1] = 0;
            context->plugin_state[2] = 0;
            context->plugin_state[3] = 0;
            event->evt_out_flags |= PF_EO_HANDLED_EVENT;
        }
        return PF_Err_NONE;
    }
    if (event->e_type == PF_Event_DO_CLICK && context) {
        context->plugin_state[0] = 2;
        context->plugin_state[1] = event->effect_win.index;
    }
    if (event->e_type == PF_Event_KEYDOWN && context && context->plugin_state[1] != 0) {
        event->effect_win.index = static_cast<PF_ParamIndex>(context->plugin_state[1]);
    }
    if (event->e_type == PF_Event_ADJUST_CURSOR) {
        if (event->effect_win.index == kAsciiEditor) {
            event->u.adjust_cursor.set_cursor = PF_Cursor_I_BEAM_0;
            event->evt_out_flags |= PF_EO_HANDLED_EVENT;
            return PF_Err_NONE;
        }
        if (event->effect_win.index != kRampBar && event->effect_win.index != kColorWheel) return PF_Err_NONE;
        if (!params) return PF_Err_BAD_CALLBACK_PARAM;
        remember_frame(event);
        RenderSettings settings;
        PF_Err error = read_settings(in_data, settings, params);
        if (error) return error;
        const auto& frame = event->effect_win.current_frame;
        const float width = std::max<A_long>(1, frame.right - frame.left);
        const float height = std::max<A_long>(1, frame.bottom - frame.top);
        const float x = std::clamp((event->u.adjust_cursor.screen_point.h - frame.left) / width, 0.0f, 1.0f);
        const float y = std::clamp((event->u.adjust_cursor.screen_point.v - frame.top) / height, 0.0f, 1.0f);
        const auto hit = studio::interactive_hit(settings, event->effect_win.index, x, y,
            static_cast<int>(width), static_cast<int>(height));
        error = update_interaction(in_data, event, hit, false);
        if (!error) event->evt_out_flags |= PF_EO_HANDLED_EVENT;
        return error;
    }
    if (event->e_type != PF_Event_DRAW && event->e_type != PF_Event_DO_CLICK &&
        event->e_type != PF_Event_DRAG && event->e_type != PF_Event_KEYDOWN) return PF_Err_NONE;
    if (!params) return PF_Err_BAD_CALLBACK_PARAM;
    if (event->e_type == PF_Event_DRAW) {
        remember_frame(event);
        RenderSettings settings;
        const PF_Err settings_error = read_settings(in_data, settings, params);
        return settings_error ? settings_error : draw_widget(in_data, event, settings, interaction_visual(event));
    }
    if (event->e_type == PF_Event_KEYDOWN && event->effect_win.index == kAsciiEditor) {
        auto cps = arbitrary::codepoints(params[kAsciiRamp]->u.arb_d.value);
        const auto* data = arbitrary::get(params[kAsciiRamp]->u.arb_d.value);
        std::size_t selected = data && arbitrary::validate(data) == PF_Err_NONE ? data->selected : 0;
        selected = std::min(selected, cps.size() - 1);
        const A_u_long key = event->u.key_down.keycode;
        if (PF_KEYCODE_IS_PRINTABLE(key)) {
            const A_u_short cp = static_cast<A_u_short>(PF_KEYCODE_GET_SHORTCUT_CHARACTER(key));
            if (cp < 32 || (cp >= 0xD800 && cp <= 0xDFFF)) return PF_Err_NONE;
            cps[selected] = cp;
        } else {
            switch (PF_KEYCODE_GET_CONTROL_CODE(key)) {
                case PF_ControlCode_Space: cps[selected] = 32; break;
                case PF_ControlCode_Left: selected = selected > 0 ? selected - 1 : cps.size() - 1; break;
                case PF_ControlCode_Right: selected = (selected + 1) % cps.size(); break;
                case PF_ControlCode_Backspace:
                case PF_ControlCode_Delete:
                    if (cps.size() > 1) cps.erase(cps.begin() + selected);
                    else cps[0] = 32;
                    selected = std::min(selected, cps.size() - 1);
                    break;
                case PF_ControlCode_Return:
                case PF_ControlCode_Enter:
                    if (cps.size() < arbitrary::kMaxGlyphs) {
                        ++selected;
                        cps.insert(cps.begin() + selected, 35);
                    }
                    break;
                default: return PF_Err_NONE;
            }
        }
        PF_Err error = arbitrary::replace(params[kAsciiRamp], cps, selected, in_data);
        if (!error) {
            params[kAsciiStyle]->u.pd.value = 1;
            params[kAsciiEnabled]->u.bd.value = 1;
            error = update_parameter(in_data, kAsciiRamp, params);
        }
        if (!error) error = update_parameter(in_data, kAsciiStyle, params);
        if (!error) error = update_parameter(in_data, kAsciiEnabled, params);
        event->evt_out_flags |= PF_EO_HANDLED_EVENT | PF_EO_UPDATE_NOW;
        out_data->out_flags |= PF_OutFlag_REFRESH_UI | PF_OutFlag_FORCE_RERENDER;
        return error;
    }
    if (event->e_type != PF_Event_DO_CLICK && event->e_type != PF_Event_DRAG) return PF_Err_NONE;
    const PF_ParamIndex index = event->effect_win.index;
    if (index != kRampBar && index != kColorWheel && index != kAsciiEditor) return PF_Err_NONE;
    RenderSettings settings;
    PF_Err error = read_settings(in_data, settings, params);
    if (error) return error;
    if (index == kAsciiEditor && !settings.ascii_enabled) {
        event->evt_out_flags |= PF_EO_HANDLED_EVENT;
        return PF_Err_NONE;
    }
    float x = 0.0f;
    float y = 0.0f;
    error = local_point(in_data, event, x, y);
    if (error) return error;
    const auto drag_hit = static_cast<studio::Hit>(event->u.do_click.continue_refcon[1]);
    const bool boundary_drag = event->e_type == PF_Event_DRAG && index == kRampBar &&
        drag_hit >= studio::Hit::boundary1 && drag_hit <= studio::Hit::boundary4;
    x = std::clamp(x, boundary_drag ? -1.0f : 0.0f, 1.0f);
    y = std::clamp(y, 0.0f, 1.0f);
    studio::Hit interaction_hit = drag_hit;
    if (index != kAsciiEditor) {
        const auto& frame = event->effect_win.current_frame;
        if (event->e_type == PF_Event_DO_CLICK) {
            interaction_hit = studio::interactive_hit(settings, index, x, y,
                frame.right - frame.left, frame.bottom - frame.top);
        }
        remember_frame(event);
        error = update_interaction(in_data, event, interaction_hit,
            event->e_type == PF_Event_DO_CLICK ? interaction_hit != studio::Hit::none : !event->u.do_click.last_time);
        if (error) return error;
    }
    if (index == kAsciiEditor) {
        auto cps = arbitrary::codepoints(params[kAsciiRamp]->u.arb_d.value);
        const std::size_t selected = std::min(static_cast<std::size_t>(std::max(0, static_cast<int>(x * 220.0f / 12.0f))), cps.size() - 1);
        PF_Err select_error = arbitrary::replace(params[kAsciiRamp], cps, selected, in_data);
        if (!select_error) select_error = update_parameter(in_data, kAsciiRamp, params);
        event->evt_out_flags |= PF_EO_HANDLED_EVENT | PF_EO_UPDATE_NOW;
        out_data->out_flags |= PF_OutFlag_REFRESH_UI;
        return select_error;
    }
    error = handle_studio_event(in_data, out_data, params, event, index, settings, x, y);
    if (error) return error;
    if (event->e_type == PF_Event_DO_CLICK && !event->u.do_click.send_drag) {
        return update_interaction(in_data, event, interaction_hit, false);
    }
    return PF_Err_NONE;
}

}  // namespace widgets
