#pragma once
#include <span>
struct DenoiserCapabilities { int sampleRate, frameSize, algorithmicDelay; const char* name; };
struct DenoiserResult { bool ok; float voiceProbability; };
class IDenoiser
{
public:
    virtual ~IDenoiser() = default;
    // prepare/reset may allocate: call only while audio processing is stopped.
    virtual bool prepare() = 0;
    virtual bool reset() = 0;
    virtual DenoiserCapabilities capabilities() const noexcept = 0;
    // Normalised float PCM, exactly frameSize samples; no allocations or IO.
    virtual DenoiserResult processFrame(std::span<const float>, std::span<float>) noexcept = 0;
};
