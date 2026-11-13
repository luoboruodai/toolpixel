#pragma once

namespace audio {

inline PF_Err read_level(PF_InData* in_data, RenderSettings& settings) {
    if (!in_data || !settings.ascii_enabled || (settings.ascii_drive != 2 && settings.ascii_drive != 4)) {
        return PF_Err_NONE;
    }
    if (!in_data->inter.checkout_layer_audio || !in_data->inter.checkin_layer_audio || !in_data->inter.get_audio_data) {
        return PF_Err_INVALID_CALLBACK;
    }
    constexpr auto rate = static_cast<PF_UFixed>(44100.0 * 65536.0);
    PF_LayerAudio audio = nullptr;
    PF_Err error = PF_CHECKOUT_LAYER_AUDIO(in_data, settings.ascii_audio,
        in_data->current_time, std::max<A_long>(in_data->time_step, 1), in_data->time_scale,
        rate, PF_SSS_2, PF_Channels_MONO, PF_SIGNED_PCM, &audio);
    TOONPIXEL_LOG("audio_checkout index=%d error=%ld audio=%p time=%ld step=%ld scale=%lu",
        settings.ascii_audio, static_cast<long>(error), audio, static_cast<long>(in_data->current_time),
        static_cast<long>(in_data->time_step), static_cast<unsigned long>(in_data->time_scale));
    if (error) {
        return error;
    }
    PF_SndSamplePtr data = nullptr;
    A_long count = 0;
    PF_UFixed actual_rate = 0;
    A_long bytes = 0;
    A_long channels = 0;
    A_long format = 0;
    error = PF_GET_AUDIO_DATA(in_data, audio, &data, &count, &actual_rate, &bytes, &channels, &format);
    if (!error && (!data || count < 0 || bytes != PF_SSS_2 || channels != PF_Channels_MONO || format != PF_SIGNED_PCM)) {
        error = PF_Err_BAD_CALLBACK_PARAM;
    }
    double energy = 0.0;
    if (!error) {
        const auto* samples = static_cast<const int16_t*>(data);
        for (A_long index = 0; index < count; ++index) {
            const double sample = samples[index] / 32768.0;
            energy += sample * sample;
        }
        settings.audio_level = count > 0 ? static_cast<float>(std::sqrt(energy / count) * 2.0) : 0.0f;
        settings.audio_samples = count;
    }
    const PF_Err checkin_error = PF_CHECKIN_LAYER_AUDIO(in_data, audio);
    TOONPIXEL_LOG("audio requested_rate=%lu reported_rate_hex=%lx samples=%ld bytes=%ld channels=%ld format=%ld rms=%.6f error=%ld checkin=%ld",
        static_cast<unsigned long>(rate / 65536.0), static_cast<unsigned long>(actual_rate), static_cast<long>(count), static_cast<long>(bytes),
        static_cast<long>(channels), static_cast<long>(format), settings.audio_level,
        static_cast<long>(error), static_cast<long>(checkin_error));
    return error ? error : checkin_error;
}

}  // namespace audio
