#pragma once
#include "core.hpp"

namespace comic {
struct AnimeStep {
    int pose = 0, animation = 0;
    float localTime = 0, loopTime = 0;
};
// Deliberately hardcoded demonstration: 3 poses x 3 clips x 3 seconds = 27 seconds.
AnimeStep animeStep(double seconds);
Scene animeDemo(Font font, std::array<Image, 3> artwork);
void validateAnimeDemo(const Scene &scene);
AnimeStep applyAnimeDemo(Scene &scene, double seconds);
std::string animeStepLabel(const AnimeStep &step);
} // namespace comic
