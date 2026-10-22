#pragma once
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>
#include <numbers>
// Causal 128-tap Blackman-windowed sinc. A call covers 10 ms.
// Fractional phases are prepared off audio. Delay: exactly 1/300 s per stage.
class ReferenceFrameResampler
{
public:
    void prepare(int from,int to)
    {
        inCount=from/100; outCount=to/100;
        history.fill(0); starts.resize(size_t(outCount)); weights.resize(size_t(outCount)*taps);
        const double cutoff=0.49*std::min(1.0,double(to)/from);
        for(int j=0;j<outCount;++j)
        {
            const double centre=double(j)*from/to-from/300;
            const int start=int(std::floor(centre))-taps/2+1;
            starts[size_t(j)]=start;
            double sum=0;
            for(int k=0;k<taps;++k)
            {
                const double x=double(start+k)-centre,u=x/(taps/2);
                const double window=0.42+0.5*std::cos(std::numbers::pi*u)+0.08*std::cos(2*std::numbers::pi*u);
                const double coefficient=(std::abs(x)<1e-12 ? 2*cutoff : std::sin(2*std::numbers::pi*cutoff*x)/(std::numbers::pi*x))*window;
                weights[size_t(j)*taps+size_t(k)]=float(coefficient); sum+=coefficient;
            }
            for(int k=0;k<taps;++k) weights[size_t(j)*taps+size_t(k)]/=float(sum);
        }
    }
    void process(const float* in,float* out) noexcept
    {
        for(int j=0;j<outCount;++j)
        {
            float sum=0; const auto* kernel=weights.data()+size_t(j)*taps;
            for(int k=0;k<taps;++k)
            {
                const int index=starts[size_t(j)]+k;
                sum+=kernel[k]*(index<0 ? history[size_t(inCount+index)] : in[index]);
            }
            out[j]=sum;
        }
        std::copy_n(in,inCount,history.data());
    }
private:
    static constexpr int taps=128;
    int inCount=480,outCount=480;
    std::array<float,960> history {};
    std::vector<int> starts;
    std::vector<float> weights;
};
