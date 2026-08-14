
#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct Mix_Music;
struct Mix_Chunk;

class AudioManager {
public:
    bool init();
    void shutdown();

    void playScrollSound();
    void playSelectSound();

    void startMusic();
    void stopMusic();
    void pauseMusic();
    void resumeMusic();

    void loadMusicFromDirectory(const std::filesystem::path& dir);
    void loadSoundEffects(const std::filesystem::path& dir);

private:
    std::vector<std::filesystem::path> music_files_;
    int current_track_ = -1;

    Mix_Music* current_music_ = nullptr;
    Mix_Chunk* scroll_sound_ = nullptr;
    Mix_Chunk* select_sound_ = nullptr;

    void playRandomTrack();
    static void musicFinishedCallback();
    static AudioManager* instance_;
};