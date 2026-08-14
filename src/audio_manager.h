
#pragma once

#include <atomic>
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

  void loadMusicFromDirectory(const std::filesystem::path &dir);
  void loadSoundEffects(const std::filesystem::path &dir);
  void update();

private:
  std::vector<std::filesystem::path> music_files_;
  int current_track_ = -1;
  bool initialized_ = false;
  Mix_Music *current_music_ = nullptr;
  Mix_Chunk *scroll_sound_ = nullptr;
  Mix_Chunk *select_sound_ = nullptr;

  void playRandomTrack();
  static void musicFinishedCallback();
  static AudioManager *instance_;
  std::atomic<bool> music_finished_{false};
};