#pragma once
#include "IDenoiser.h"
#include <rnnoise.h>
#include <array>
#include <memory>
class RNNoiseEngine final : public IDenoiser
{
public:
    bool prepare() override;
    bool reset() override { return prepare(); }
    DenoiserCapabilities capabilities() const noexcept override
    { return {48000,480,480,"RNNoise 0.1.1 / original embedded model"}; }
    DenoiserResult processFrame(std::span<const float>, std::span<float>) noexcept override;
private:
    struct Deleter { void operator()(DenoiseState* s) const { if(s) rnnoise_destroy(s); } };
    std::unique_ptr<DenoiseState,Deleter> state;
    std::array<float,480> scaled {},cleaned {};
};
