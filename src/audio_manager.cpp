#include "audio_manager.h"

#include <SDL_mixer.h>
#include <algorithm>
#include <iostream>

AudioManager *AudioManager::instance_ = nullptr;

bool AudioManager::init() {
  instance_ = this;

  if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
    std::cerr << "[ludex] Mix_OpenAudio failed: " << Mix_GetError() << "\n";
    return false;
  }

  Mix_AllocateChannels(16);
  Mix_VolumeMusic(MIX_MAX_VOLUME / 2);
  Mix_Volume(-1, MIX_MAX_VOLUME / 2);

  Mix_HookMusicFinished(musicFinishedCallback);
  initialized_ = true;
  return true;
}

void AudioManager::shutdown() {
  if (current_music_) {
    Mix_FreeMusic(current_music_);
    current_music_ = nullptr;
  }
  if (scroll_sound_) {
    Mix_FreeChunk(scroll_sound_);
    scroll_sound_ = nullptr;
  }
  if (select_sound_) {
    Mix_FreeChunk(select_sound_);
    select_sound_ = nullptr;
  }

  Mix_CloseAudio();
  Mix_Quit();
  instance_ = nullptr;
}

void AudioManager::loadMusicFromDirectory(const std::filesystem::path &dir) {
  music_files_.clear();

  std::error_code ec;
  if (!std::filesystem::is_directory(dir, ec)) {
    std::cerr << "[ludex] Music directory not found: " << dir << "\n";
    return;
  }

  for (const auto &entry : std::filesystem::directory_iterator(dir, ec)) {
    if (!entry.is_regular_file())
      continue;

    std::string ext = entry.path().extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".mp3" || ext == ".ogg" || ext == ".wav") {
      music_files_.push_back(entry.path());
    }
  }

  std::sort(music_files_.begin(), music_files_.end());
  std::cout << "[ludex] Loaded " << music_files_.size() << " music tracks from "
            << dir << "\n";
}

void AudioManager::loadSoundEffects(const std::filesystem::path &dir) {
  std::error_code ec;

  auto scroll_path = dir / "scroll.wav";
  if (std::filesystem::exists(scroll_path, ec)) {
    scroll_sound_ = Mix_LoadWAV(scroll_path.c_str());
    if (!scroll_sound_) {
      std::cerr << "[ludex] Failed to load scroll.wav: " << Mix_GetError()
                << "\n";
    }
  }

  auto select_path = dir / "select.wav";
  if (std::filesystem::exists(select_path, ec)) {
    select_sound_ = Mix_LoadWAV(select_path.c_str());
    if (!select_sound_) {
      std::cerr << "[ludex] Failed to load select.wav: " << Mix_GetError()
                << "\n";
    }
  }
}

void AudioManager::playScrollSound() {
  if (scroll_sound_) {
    Mix_PlayChannel(-1, scroll_sound_, 0);
  }
}

void AudioManager::playSelectSound() {
  if (select_sound_) {
    Mix_PlayChannel(-1, select_sound_, 0);
  }
}

void AudioManager::playRandomTrack() {
  if (music_files_.empty())
    return;

  if (current_music_) {
    Mix_FreeMusic(current_music_);
    current_music_ = nullptr;
  }

  int attempts = 0;
  int new_track;
  do {
    new_track = std::rand() % music_files_.size();
    attempts++;
  } while (new_track == current_track_ && music_files_.size() > 1 &&
           attempts < 10);

  current_track_ = new_track;
  current_music_ = Mix_LoadMUS(music_files_[current_track_].c_str());

  if (!current_music_) {
    std::cerr << "[ludex] Failed to load music: "
              << music_files_[current_track_] << " - " << Mix_GetError()
              << "\n";
    return;
  }

  if (Mix_PlayMusic(current_music_, 1) == -1) {
    std::cerr << "[ludex] Failed to play music: " << Mix_GetError() << "\n";
    Mix_FreeMusic(current_music_);
    current_music_ = nullptr;
  }
}

void AudioManager::musicFinishedCallback() {
  if (instance_) {
    instance_->music_finished_.store(true);
  }
}

void AudioManager::startMusic() {
  if (!music_files_.empty() && !Mix_PlayingMusic()) {
    playRandomTrack();
  }
  music_finished_.store(false);
}

void AudioManager::stopMusic() {
  Mix_HookMusicFinished(nullptr); // evita reentrada durante el halt/free
  if (Mix_PlayingMusic()) {
    Mix_HaltMusic();
  }
  if (current_music_) {
    Mix_FreeMusic(current_music_);
    current_music_ = nullptr;
  }
  music_finished_.store(false);
}

void AudioManager::pauseMusic() {
  if (Mix_PlayingMusic() && !Mix_PausedMusic()) {
    Mix_PauseMusic();
  }
}

void AudioManager::resumeMusic() {
  if (Mix_PausedMusic()) {
    Mix_ResumeMusic();
  } else if (!Mix_PlayingMusic() && !music_files_.empty()) {
    playRandomTrack();
  }
}

void AudioManager::update() {
  if (music_finished_.exchange(false)) {
    playRandomTrack();
  }
}