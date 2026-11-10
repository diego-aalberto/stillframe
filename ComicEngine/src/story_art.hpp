#pragma once
#include "platform.hpp"
#include <map>

namespace comic {
extern const std::array<const char *,12> storyPoses;
extern const std::array<const char *,10> storyGestures;

// Illustrated pose library, ten skeletal motion clips per pose, independent playback clocks.
class StoryArt {
    std::filesystem::path assets;
    std::map<std::string,Scene> characters;
    std::map<std::string,Image> backgrounds;
    struct Playback {float start=0,nextIdle=8;unsigned cycle=0;};
    std::map<std::string,Playback> playback;
    Scene &character(const std::string &id);
  public:
    explicit StoryArt(std::filesystem::path path):assets(std::move(path)){}
    Scene make(const std::string &background,const std::vector<std::string> &cast,Font font);
    void direct(Scene &scene,const std::string &who,const std::string &pose,const std::string &gesture,float now);
    void tick(Scene &scene,float now);
    void audit(const Scene &scene,const std::filesystem::path &report) const;
};
}
