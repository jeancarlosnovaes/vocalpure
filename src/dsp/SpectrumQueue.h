#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <memory>

// One producer (audio), one consumer (editor). Full queue drops visual frames;
// it never blocks audio. Do not reset the FIFO while the consumer is running.
class SpectrumQueue
{
public:
    static constexpr int size=2048, capacity=8;
    struct Frame { std::array<std::array<float,size>,4> samples {}; double rate=48000; int channels=1; };
    SpectrumQueue(): storage(std::make_unique<Frame[]>(capacity)) {}
    std::atomic<bool> enabled {false};
    void prepare(double rate,int channels) noexcept { pending.rate=rate; pending.channels=channels; position=0; }
    void discardPartial() noexcept { position=0; }
    void push(float inL,float inR,float outL,float outR) noexcept
    {
        const size_t i=size_t(position++);
        pending.samples[0][i]=inL; pending.samples[1][i]=inR;
        pending.samples[2][i]=outL; pending.samples[3][i]=outR;
        if(position!=size) return;
        position=0;
        int start,count,start2,count2; fifo.prepareToWrite(1,start,count,start2,count2);
        if(count>0) { storage[size_t(start)]=pending; fifo.finishedWrite(1); }
    }
    bool pop(Frame& destination) noexcept
    {
        int start,count,start2,count2; fifo.prepareToRead(1,start,count,start2,count2);
        if(count==0) return false;
        destination=storage[size_t(start)]; fifo.finishedRead(1); return true;
    }
private:
    juce::AbstractFifo fifo {capacity};
    std::unique_ptr<Frame[]> storage;
    Frame pending;
    int position=0;
};
