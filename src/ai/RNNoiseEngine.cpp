#include "RNNoiseEngine.h"
#include <algorithm>
#include <cmath>
#include <mutex>
#include <stdexcept>
bool RNNoiseEngine::prepare()
{
    // Serialise the lazy global FFT/window initialisation outside audio.
    static std::once_flag initialised;
    try
    {
        std::call_once(initialised, [] {
            std::unique_ptr<DenoiseState,Deleter> warm(rnnoise_create(nullptr));
            if(!warm) throw std::bad_alloc();
            std::array<float,480> zero {},result {};
            rnnoise_process_frame(warm.get(),result.data(),zero.data());
        });
        state.reset(rnnoise_create(nullptr));
    }
    catch(...) { state.reset(); }
    return state!=nullptr;
}
DenoiserResult RNNoiseEngine::processFrame(std::span<const float> in,std::span<float> out) noexcept
{
    if(!state || in.size()!=480 || out.size()!=480) return {false,0};
    for(size_t i=0;i<480;++i)
    {
        if(!std::isfinite(in[i])) return {false,0};
        scaled[i]=std::clamp(in[i],-1.0f,1.0f)*32768.0f;
    }
    const float vad=rnnoise_process_frame(state.get(),cleaned.data(),scaled.data());
    if(!std::isfinite(vad)) return {false,0};
    for(size_t i=0;i<480;++i)
    {
        if(!std::isfinite(cleaned[i])) return {false,0};
        out[i]=cleaned[i]/32768.0f;
    }
    return {true,std::clamp(vad,0.0f,1.0f)};
}
