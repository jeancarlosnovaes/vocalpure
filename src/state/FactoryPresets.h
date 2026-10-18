#pragma once
#include <array>
struct FactoryPreset { const char* name; float reduction,width; };
inline constexpr std::array<FactoryPreset,7> factoryPresets {{{"Natural Voice",35,100},
    {"Podcast Clean",65,100},{"Studio Voice",50,100},{"Noisy Room",85,100},
    {"Outdoor Dialogue",90,100},{"Strong Noise Reduction",100,100},{"Gentle Cleaning",20,100}}};
