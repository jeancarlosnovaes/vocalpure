#pragma once
#include <array>
struct FactoryPreset { const char* name; float reduction,width; int profile=0; };
inline constexpr std::array<FactoryPreset,8> factoryPresets {{{"Natural Voice",35,100},
    {"Podcast Clean",65,100},{"Studio Voice",50,100},{"Noisy Room",85,100},
    {"Outdoor Dialogue",90,100},{"Strong Noise Reduction",100,100,1},{"Gentle Cleaning",20,100},
    {"Low CPU",65,100,2}}};
