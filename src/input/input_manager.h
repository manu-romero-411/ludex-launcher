#pragma once

#include <SDL.h>

#include <filesystem>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

enum class UiAction { Left, Right, Up, Down, Select, Back, Menu, Guide, Alt };
struct UiInput {
  int player = -1;
  UiAction action = UiAction::Select;
};

class InputManager {
public:
  bool init();
  void shutdown();

  void closeControllers();
  void rescanControllers();

  void handleEvent(const SDL_Event &event);

  bool poll(UiInput &out);

  void update(); // llamar 1 vez por frame: repeticiones continuas

  struct PlayerStatus {
    int player;
    std::string name;
    bool attached;
    bool active;
  };
  std::vector<PlayerStatus> playerStatus() const;
  std::filesystem::path
  writeControllersConfig(const std::filesystem::path &dir) const;
  std::string controllersConfigString() const;
  struct DeviceInfo {
    std::string guid;
    std::string name;
    int sdl_index;
  };

  void applyAssignment(const std::vector<std::string> &guids);
  std::vector<DeviceInfo> devices() const;
  void clearTransientState() {
    while (!queue_.empty())
      queue_.pop();

    for (auto &slot : slots_) {
      slot.hold_up = {};
      slot.hold_down = {};
      slot.hold_left = {};
      slot.hold_right = {};
      slot.last_activity_ms = 0;
    }
  }

private:
  struct NavHold {
    bool active = false;
    Uint32 press_ms = 0;
    Uint32 last_repeat_ms = 0;
  };

  struct Slot {
    int id = -1;
    std::string guid;
    std::string name;
    SDL_JoystickID instance = -1;
    bool attached = false;

    SDL_GameController *game_controller = nullptr;
    SDL_Joystick *joystick = nullptr;

    Uint32 last_activity_ms = 0;
    NavHold hold_up, hold_down, hold_left, hold_right;
    int assigned_player = -1;
  };

  void recomputePlayers();
  Slot *slotByPlayer(int p);
  const Slot *slotByPlayer(int p) const;
  std::vector<std::string> assign_guids_;

  int createOrFindSlotForGuid(const std::string &guid);
  int findOrderIndexBySlotId(int slot_id) const;
  int playerForInstance(SDL_JoystickID instance) const;

  Slot *slotById(int slot_id);
  const Slot *slotById(int slot_id) const;

  void addDeviceByIndex(int device_index);
  void removeDevice(SDL_JoystickID instance);

  void push(int player, UiAction action);

  std::vector<Slot> slots_;
  std::vector<int> order_;

  std::unordered_map<SDL_JoystickID, int> instance_to_slot_;

  std::queue<UiInput> queue_;

  int next_slot_id_ = 0;

  static constexpr Sint16 DEADZONE = 16000;

  void stepHold(NavHold &h, bool held, Uint32 now, int player, UiAction a);

  static constexpr Uint32 HOLD_DELAY_MS = 400;       // espera antes de repetir
  static constexpr Uint32 HOLD_REPEAT_SLOW_MS = 140; // primera fase
  static constexpr Uint32 HOLD_REPEAT_FAST_MS = 80;  // acelerado
};
