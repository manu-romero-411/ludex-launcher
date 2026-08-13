#include "input_manager.h"

#include <algorithm>
#include <sstream>

bool InputManager::init() {
    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0) {
        SDL_Log("SDL_InitSubSystem(JOYSTICK/GAMECONTROLLER) error: %s",
                SDL_GetError());
        return false;
    }

    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

    rescanControllers();

    return true;
}

void InputManager::shutdown() {
    closeControllers();

    slots_.clear();
    order_.clear();
    instance_to_slot_.clear();
    axis_by_slot_.clear();

    while (!queue_.empty()) {
        queue_.pop();
    }

    SDL_QuitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);
}

void InputManager::closeControllers() {
    for (auto& slot : slots_) {
        if (!slot.attached) {
            continue;
        }

        if (slot.game_controller) {
            SDL_GameControllerClose(slot.game_controller);
            slot.game_controller = nullptr;
            slot.joystick = nullptr;
        } else if (slot.joystick) {
            SDL_JoystickClose(slot.joystick);
            slot.joystick = nullptr;
        }

        slot.attached = false;
        slot.instance = -1;
    }

    instance_to_slot_.clear();
    axis_by_slot_.clear();
}

void InputManager::rescanControllers() {
    closeControllers();

    int count = SDL_NumJoysticks();

    for (int i = 0; i < count; ++i) {
        addDeviceByIndex(i);
    }
}

void InputManager::addDeviceByIndex(int device_index) {
    SDL_GameController* gc = nullptr;
    SDL_Joystick* js = nullptr;

    if (SDL_IsGameController(device_index)) {
        gc = SDL_GameControllerOpen(device_index);
        if (!gc) {
            SDL_Log("No se pudo abrir controller index=%d: %s",
                    device_index,
                    SDL_GetError());
            return;
        }

        js = SDL_GameControllerGetJoystick(gc);
    } else {
        js = SDL_JoystickOpen(device_index);
        if (!js) {
            SDL_Log("No se pudo abrir joystick index=%d: %s",
                    device_index,
                    SDL_GetError());
            return;
        }
    }

    SDL_JoystickID instance = SDL_JoystickInstanceID(js);

    SDL_JoystickGUID guid = SDL_JoystickGetGUID(js);

    char guid_buffer[64] = {};
    SDL_JoystickGetGUIDString(guid, guid_buffer, sizeof(guid_buffer));

    const char* name_c = SDL_JoystickName(js);

    int slot_id = createOrFindSlotForGuid(guid_buffer);

    Slot* slot = slotById(slot_id);
    if (!slot) {
        if (gc) {
            SDL_GameControllerClose(gc);
        } else if (js) {
            SDL_JoystickClose(js);
        }
        return;
    }

    slot->game_controller = gc;
    slot->joystick = js;
    slot->instance = instance;
    slot->attached = true;

    if (name_c) {
        slot->name = name_c;
    }

    instance_to_slot_[instance] = slot_id;

    SDL_Log("Mando detectado: player=%d name='%s' guid=%s",
            findOrderIndexBySlotId(slot_id),
            slot->name.c_str(),
            slot->guid.c_str());
}

void InputManager::removeDevice(SDL_JoystickID instance) {
    auto it = instance_to_slot_.find(instance);
    if (it == instance_to_slot_.end()) {
        return;
    }

    int slot_id = it->second;
    Slot* slot = slotById(slot_id);

    if (slot) {
        if (slot->game_controller) {
            SDL_GameControllerClose(slot->game_controller);
            slot->game_controller = nullptr;
            slot->joystick = nullptr;
        } else if (slot->joystick) {
            SDL_JoystickClose(slot->joystick);
            slot->joystick = nullptr;
        }

        slot->attached = false;
        slot->instance = -1;

        SDL_Log("Mando desconectado: player=%d name='%s'",
                findOrderIndexBySlotId(slot_id),
                slot->name.c_str());
    }

    instance_to_slot_.erase(it);
    axis_by_slot_.erase(slot_id);
}

void InputManager::handleEvent(const SDL_Event& event) {
    switch (event.type) {
        case SDL_CONTROLLERDEVICEADDED: {
            addDeviceByIndex(event.cdevice.which);
            break;
        }

        case SDL_JOYDEVICEADDED: {
            if (!SDL_IsGameController(event.jdevice.which)) {
                addDeviceByIndex(event.jdevice.which);
            }
            break;
        }

        case SDL_CONTROLLERDEVICEREMOVED: {
            removeDevice(event.cdevice.which);
            break;
        }

        case SDL_JOYDEVICEREMOVED: {
            removeDevice(event.jdevice.which);
            break;
        }

        case SDL_CONTROLLERBUTTONDOWN: {
            int player = playerForInstance(event.cbutton.which);
            if (player < 0) {
                break;
            }

            switch (event.cbutton.button) {
                case SDL_CONTROLLER_BUTTON_A:
                    push(player, UiAction::Select);
                    break;

                case SDL_CONTROLLER_BUTTON_B:
                    push(player, UiAction::Back);
                    break;

                case SDL_CONTROLLER_BUTTON_START:
                    push(player, UiAction::Menu);
                    break;

                case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                    push(player, UiAction::Left);
                    break;

                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                    push(player, UiAction::Right);
                    break;

                default:
                    break;
            }

            break;
        }

        case SDL_CONTROLLERAXISMOTION: {
            if (event.caxis.axis != SDL_CONTROLLER_AXIS_LEFTX) {
                break;
            }

            int player = playerForInstance(event.caxis.which);
            if (player < 0) {
                break;
            }

            auto it = instance_to_slot_.find(event.caxis.which);
            if (it == instance_to_slot_.end()) {
                break;
            }

            int slot_id = it->second;
            AxisState& state = axis_by_slot_[slot_id];

            Uint32 now = SDL_GetTicks();

            if (event.caxis.value < -DEADZONE) {
                if (state.last_dir != -1 ||
                    now - state.last_ms >= NAV_COOLDOWN_MS) {
                    push(player, UiAction::Left);
                    state.last_dir = -1;
                    state.last_ms = now;
                }
            } else if (event.caxis.value > DEADZONE) {
                if (state.last_dir != 1 ||
                    now - state.last_ms >= NAV_COOLDOWN_MS) {
                    push(player, UiAction::Right);
                    state.last_dir = 1;
                    state.last_ms = now;
                }
            } else {
                state.last_dir = 0;
            }

            break;
        }

        default:
            break;
    }
}

bool InputManager::poll(UiInput& out) {
    if (queue_.empty()) {
        return false;
    }

    out = queue_.front();
    queue_.pop();
    return true;
}

void InputManager::movePlayer(int from, int to) {
    if (from < 0 || from >= static_cast<int>(order_.size())) {
        return;
    }

    if (to < 0 || to >= static_cast<int>(order_.size())) {
        return;
    }

    if (from == to) {
        return;
    }

    int slot_id = order_[from];

    order_.erase(order_.begin() + from);
    order_.insert(order_.begin() + to, slot_id);

    SDL_Log("Reordenación de mandos: %d -> %d", from, to);
}

std::vector<std::string> InputManager::describePlayers() const {
    std::vector<std::string> result;

    for (int player = 0; player < static_cast<int>(order_.size()); ++player) {
        int slot_id = order_[player];
        const Slot* slot = slotById(slot_id);

        std::ostringstream oss;

        oss << "J" << (player + 1) << ": ";

        if (!slot) {
            oss << "slot inválido";
        } else {
            oss << slot->name;
            oss << " guid=" << slot->guid;
            oss << (slot->attached ? " attached" : " detached");
        }

        result.push_back(oss.str());
    }

    return result;
}

int InputManager::createOrFindSlotForGuid(const std::string& guid) {
    for (const auto& slot : slots_) {
        if (!slot.attached && slot.guid == guid) {
            return slot.id;
        }
    }

    Slot slot;
    slot.id = next_slot_id_++;
    slot.guid = guid;

    slots_.push_back(slot);
    order_.push_back(slot.id);

    return slot.id;
}

int InputManager::findOrderIndexBySlotId(int slot_id) const {
    auto it = std::find(order_.begin(), order_.end(), slot_id);

    if (it == order_.end()) {
        return -1;
    }

    return static_cast<int>(std::distance(order_.begin(), it));
}

int InputManager::playerForInstance(SDL_JoystickID instance) const {
    auto it = instance_to_slot_.find(instance);
    if (it == instance_to_slot_.end()) {
        return -1;
    }

    return findOrderIndexBySlotId(it->second);
}

InputManager::Slot* InputManager::slotById(int slot_id) {
    for (auto& slot : slots_) {
        if (slot.id == slot_id) {
            return &slot;
        }
    }

    return nullptr;
}

const InputManager::Slot* InputManager::slotById(int slot_id) const {
    for (const auto& slot : slots_) {
        if (slot.id == slot_id) {
            return &slot;
        }
    }

    return nullptr;
}

void InputManager::push(int player, UiAction action) {
    UiInput input;
    input.player = player;
    input.action = action;

    queue_.push(input);
}
