#pragma once

#include <SDL.h>

#include <optional>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

enum class UiAction {
    Left,
    Right,
    Select,
    Back,
    Menu
};

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

    void handleEvent(const SDL_Event& event);

    bool poll(UiInput& out);

    void movePlayer(int from, int to);

    std::vector<std::string> describePlayers() const;

private:
    struct AxisState {
        int last_dir = 0;
        Uint32 last_ms = 0;
    };

    struct Slot {
        int id = -1;
        std::string guid;
        std::string name;
        SDL_JoystickID instance = -1;
        bool attached = false;

        SDL_GameController* game_controller = nullptr;
        SDL_Joystick* joystick = nullptr;
    };

    int createOrFindSlotForGuid(const std::string& guid);
    int findOrderIndexBySlotId(int slot_id) const;
    int playerForInstance(SDL_JoystickID instance) const;

    Slot* slotById(int slot_id);
    const Slot* slotById(int slot_id) const;

    void addDeviceByIndex(int device_index);
    void removeDevice(SDL_JoystickID instance);

    void push(int player, UiAction action);

    std::vector<Slot> slots_;
    std::vector<int> order_;

    std::unordered_map<SDL_JoystickID, int> instance_to_slot_;
    std::unordered_map<int, AxisState> axis_by_slot_;

    std::queue<UiInput> queue_;

    int next_slot_id_ = 0;

    static constexpr Sint16 DEADZONE = 16000;
    static constexpr Uint32 NAV_COOLDOWN_MS = 200;
};
