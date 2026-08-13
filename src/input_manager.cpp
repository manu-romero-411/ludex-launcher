#include "input_manager.h"

#include <algorithm>
#include <sstream>

bool InputManager::init() {
    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0) {
        SDL_Log("SDL_InitSubSystem(JOYSTICK/GAMECONTROLLER) error: %s",
                SDL_GetError());
        return false;
    }

    // Fuerza el estado inicial de joysticks
    SDL_JoystickUpdate();
    SDL_GameControllerUpdate();

    rescanControllers();
    SDL_Log("[ludex] InputManager: %d mandos detectados en arranque",
            SDL_NumJoysticks());

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
                    device_index, SDL_GetError());
            return;
        }
        js = SDL_GameControllerGetJoystick(gc);
    } else {
        js = SDL_JoystickOpen(device_index);
        if (!js) {
            SDL_Log("No se pudo abrir joystick index=%d: %s",
                    device_index, SDL_GetError());
            return;
        }
    }

    SDL_JoystickID instance = SDL_JoystickInstanceID(js);
    SDL_JoystickGUID guid = SDL_JoystickGetGUID(js);
    char guid_buffer[64] = {};
    SDL_JoystickGetGUIDString(guid, guid_buffer, sizeof(guid_buffer));

    // Verificar si ya hay un slot ATTACHED con este GUID (dispositivo duplicado)
    for (const auto& slot : slots_) {
        if (slot.attached && slot.guid == guid_buffer) {
            SDL_Log("Dispositivo duplicado ignorado: GUID %s ya está attached",
                    guid_buffer);
            if (gc) SDL_GameControllerClose(gc);
            else if (js) SDL_JoystickClose(js);
            return;
        }
    }

    const char* name_c = SDL_JoystickName(js);
    int slot_id = createOrFindSlotForGuid(guid_buffer);

    Slot* slot = slotById(slot_id);
    if (!slot) {
        if (gc) SDL_GameControllerClose(gc);
        else if (js) SDL_JoystickClose(js);
        return;
    }

    slot->game_controller = gc;
    slot->joystick = js;
    slot->instance = instance;
    slot->attached = true;

    if (name_c) slot->name = name_c;

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
            if (player < 0) break;

            switch (event.cbutton.button) {
                case SDL_CONTROLLER_BUTTON_A:  push(player, UiAction::Select); break;
                case SDL_CONTROLLER_BUTTON_B:  push(player, UiAction::Back);   break;
                case SDL_CONTROLLER_BUTTON_START: push(player, UiAction::Menu); break;
                case SDL_CONTROLLER_BUTTON_GUIDE: push(player, UiAction::Guide); break;
                default: break;
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
    if (player < 0 || player >= (int)order_.size()) return;

    int slot_id = order_[player];
    Slot* slot = slotById(slot_id);
    if (slot) {
        slot->last_activity_ms = SDL_GetTicks();
    }

    UiInput input;
    input.player = player;
    input.action = action;
    queue_.push(input);
}

std::vector<InputManager::PlayerStatus> InputManager::playerStatus() const {
    std::vector<PlayerStatus> out;
    Uint32 now = SDL_GetTicks();

    for (int player = 0; player < (int)order_.size(); ++player) {
        int slot_id = order_[player];
        const Slot* slot = slotById(slot_id);
        if (slot && slot->attached) {  // solo mandos conectados
            bool active = (now - slot->last_activity_ms) < 2000;  // 2 segundos
            out.push_back({player, slot->name, true, active});
        }
    }
    return out;
}

void InputManager::update() {
    Uint32 now = SDL_GetTicks();

    for (auto& slot : slots_) {
        if (!slot.attached || !slot.game_controller) continue;

        int player = findOrderIndexBySlotId(slot.id);
        if (player < 0) continue;

        SDL_GameController* gc = slot.game_controller;

        bool up =
            SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_UP) == SDL_PRESSED ||
            SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTY) < -DEADZONE;
        bool down =
            SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_DOWN) == SDL_PRESSED ||
            SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTY) > DEADZONE;
        bool left =
            SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_LEFT) == SDL_PRESSED ||
            SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTX) < -DEADZONE;
        bool right =
            SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) == SDL_PRESSED ||
            SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTX) > DEADZONE;

        stepHold(slot.hold_up,    up,    now, player, UiAction::Up);
        stepHold(slot.hold_down,  down,  now, player, UiAction::Down);
        stepHold(slot.hold_left,  left,  now, player, UiAction::Left);
        stepHold(slot.hold_right, right, now, player, UiAction::Right);
    }
}

void InputManager::stepHold(NavHold& h, bool held, Uint32 now,
                            int player, UiAction a) {
    if (!held) { h.active = false; return; }

    if (!h.active) {
        h.active = true;
        h.press_ms = now;
        h.last_repeat_ms = now;
        push(player, a);              // primera pulsación inmediata
        return;
    }

    Uint32 held_for = now - h.press_ms;
    if (held_for < HOLD_DELAY_MS) return;

    // acelera: 140 ms los primeros ~0.9 s, luego 80 ms
    Uint32 interval = (held_for - HOLD_DELAY_MS > 900)
        ? HOLD_REPEAT_FAST_MS : HOLD_REPEAT_SLOW_MS;

    if (now - h.last_repeat_ms >= interval) {
        h.last_repeat_ms = now;
        push(player, a);
    }
}