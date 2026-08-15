#include "input_manager.h"

#include <algorithm>
#include <fstream>

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

  while (!queue_.empty()) {
    queue_.pop();
  }

  SDL_QuitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);
}

void InputManager::closeControllers() {
  for (auto &slot : slots_) {
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
}

void InputManager::rescanControllers() {
  closeControllers();

  int count = SDL_NumJoysticks();

  for (int i = 0; i < count; ++i) {
    addDeviceByIndex(i);
  }
  recomputePlayers();
}

void InputManager::addDeviceByIndex(int device_index) {
  SDL_GameController *gc = nullptr;
  SDL_Joystick *js = nullptr;

  if (SDL_IsGameController(device_index)) {
    gc = SDL_GameControllerOpen(device_index);
    if (!gc) {
      SDL_Log("No se pudo abrir controller index=%d: %s", device_index,
              SDL_GetError());
      return;
    }
    js = SDL_GameControllerGetJoystick(gc);
  } else {
    js = SDL_JoystickOpen(device_index);
    if (!js) {
      SDL_Log("No se pudo abrir joystick index=%d: %s", device_index,
              SDL_GetError());
      return;
    }
  }

  SDL_JoystickID instance = SDL_JoystickInstanceID(js);
  SDL_JoystickGUID guid = SDL_JoystickGetGUID(js);
  char guid_buffer[64] = {};
  SDL_JoystickGetGUIDString(guid, guid_buffer, sizeof(guid_buffer));

  // Verificar si ya hay un slot ATTACHED con este GUID (dispositivo duplicado)
  for (const auto &slot : slots_) {
    if (slot.attached && slot.guid == guid_buffer) {
      SDL_Log("Dispositivo duplicado ignorado: GUID %s ya está attached",
              guid_buffer);
      if (gc)
        SDL_GameControllerClose(gc);
      else if (js)
        SDL_JoystickClose(js);
      return;
    }
  }

  const char *name_c = SDL_JoystickName(js);
  int slot_id = createOrFindSlotForGuid(guid_buffer);

  Slot *slot = slotById(slot_id);
  if (!slot) {
    if (gc)
      SDL_GameControllerClose(gc);
    else if (js)
      SDL_JoystickClose(js);
    return;
  }

  slot->game_controller = gc;
  slot->joystick = js;
  slot->instance = instance;
  slot->attached = true;

  if (name_c)
    slot->name = name_c;

  instance_to_slot_[instance] = slot_id;
  recomputePlayers();

  SDL_Log("Gamepad detected: player=%d name='%s' guid=%s",
          slot->assigned_player, slot->name.c_str(), slot->guid.c_str());
}

void InputManager::removeDevice(SDL_JoystickID instance) {
  auto it = instance_to_slot_.find(instance);
  if (it == instance_to_slot_.end()) {
    return;
  }

  int slot_id = it->second;
  Slot *slot = slotById(slot_id);

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
            findOrderIndexBySlotId(slot_id), slot->name.c_str());
  }

  instance_to_slot_.erase(it);
  recomputePlayers();
}

void InputManager::handleEvent(const SDL_Event &event) {
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
    if (player < 0)
      break;

    switch (event.cbutton.button) {
    case SDL_CONTROLLER_BUTTON_A:
      push(player, UiAction::Select);
      break;
    case SDL_CONTROLLER_BUTTON_B:
      push(player, UiAction::Back);
      break;
    case SDL_CONTROLLER_BUTTON_X: // X en Xbox / Cuadrado en PS
      push(player, UiAction::Alt);
      break;
    case SDL_CONTROLLER_BUTTON_START:
      push(player, UiAction::Menu);
      break;
    case SDL_CONTROLLER_BUTTON_GUIDE:
      push(player, UiAction::Guide);
      break;
    default:
      break;
    }
    break;
  }

  default:
    break;
  }
}

bool InputManager::poll(UiInput &out) {
  if (queue_.empty()) {
    return false;
  }

  out = queue_.front();
  queue_.pop();
  return true;
}

int InputManager::createOrFindSlotForGuid(const std::string &guid) {
  for (const auto &slot : slots_) {
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
  if (it == instance_to_slot_.end())
    return -1;
  const Slot *s = slotById(it->second);
  return s ? s->assigned_player : -1;
}

InputManager::Slot *InputManager::slotById(int slot_id) {
  for (auto &slot : slots_) {
    if (slot.id == slot_id) {
      return &slot;
    }
  }

  return nullptr;
}

const InputManager::Slot *InputManager::slotById(int slot_id) const {
  for (const auto &slot : slots_) {
    if (slot.id == slot_id) {
      return &slot;
    }
  }

  return nullptr;
}

void InputManager::push(int player, UiAction action) {
  if (player < 0 || player >= (int)order_.size())
    return;

  int slot_id = order_[player];
  Slot *slot = slotById(slot_id);
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
    const Slot *slot = slotById(slot_id);
    if (slot && slot->attached) { // solo mandos conectados
      bool active = (now - slot->last_activity_ms) < 2000; // 2 segundos
      out.push_back({player, slot->name, true, active});
    }
  }
  return out;
}

void InputManager::update() {
  Uint32 now = SDL_GetTicks();

  for (auto &slot : slots_) {
    if (!slot.attached || !slot.game_controller)
      continue;

    int player = slot.assigned_player;
    if (player < 0)
      continue;

    SDL_GameController *gc = slot.game_controller;

    bool up =
        SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_UP) ==
            SDL_PRESSED ||
        SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTY) < -DEADZONE;
    bool down =
        SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_DOWN) ==
            SDL_PRESSED ||
        SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTY) > DEADZONE;
    bool left =
        SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_LEFT) ==
            SDL_PRESSED ||
        SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTX) < -DEADZONE;
    bool right =
        SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) ==
            SDL_PRESSED ||
        SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTX) > DEADZONE;

    stepHold(slot.hold_up, up, now, player, UiAction::Up);
    stepHold(slot.hold_down, down, now, player, UiAction::Down);
    stepHold(slot.hold_left, left, now, player, UiAction::Left);
    stepHold(slot.hold_right, right, now, player, UiAction::Right);
  }
}

void InputManager::stepHold(NavHold &h, bool held, Uint32 now, int player,
                            UiAction a) {
  if (!held) {
    h.active = false;
    return;
  }

  if (!h.active) {
    h.active = true;
    h.press_ms = now;
    h.last_repeat_ms = now;
    push(player, a); // primera pulsación inmediata
    return;
  }

  Uint32 held_for = now - h.press_ms;
  if (held_for < HOLD_DELAY_MS)
    return;

  // acelera: 140 ms los primeros ~0.9 s, luego 80 ms
  Uint32 interval = (held_for - HOLD_DELAY_MS > 900) ? HOLD_REPEAT_FAST_MS
                                                     : HOLD_REPEAT_SLOW_MS;

  if (now - h.last_repeat_ms >= interval) {
    h.last_repeat_ms = now;
    push(player, a);
  }
}

std::filesystem::path
InputManager::writeControllersConfig(const std::filesystem::path &dir) const {
  std::error_code ec;
  if (!dir.empty())
    std::filesystem::create_directories(dir, ec);

  std::filesystem::path file = dir / "ludex-controllers.cfg";
  std::ofstream f(file);
  if (!f)
    return {};

  for (int p = 0; p < 8; ++p) {
    const Slot *slot = slotByPlayer(p);
    if (!slot)
      continue;

    f << "[player" << p << "]\n";
    f << "name=" << slot->name << "\n";
    f << "guid=" << slot->guid << "\n";

    if (slot->game_controller) {
      char *m = SDL_GameControllerMapping(slot->game_controller);
      if (m) {
        f << "sdl_mapping=" << m << "\n";
        SDL_free(m);
      }
    }
    f << "\n";
  }

  return file;
}
std::string InputManager::controllersConfigString() const {
  std::string out;

  for (int p = 0; p < 8; ++p) {
    const Slot *slot = slotByPlayer(p);
    if (!slot || !slot->joystick)
      continue;

    SDL_JoystickID inst = SDL_JoystickInstanceID(slot->joystick);
    int dev_index = -1;
    int n = SDL_NumJoysticks();
    for (int i = 0; i < n; ++i) {
      if (SDL_JoystickGetDeviceInstanceID(i) == inst) {
        dev_index = i;
        break;
      }
    }

    std::string pre = "-p" + std::to_string(p + 1);
    out += pre + "index " + std::to_string(dev_index) + " ";
    out += pre + "guid " + slot->guid + " ";
    out += pre + "name \"" + slot->name + "\" ";
    out += pre + "nbbuttons " +
           std::to_string(SDL_JoystickNumButtons(slot->joystick)) + " ";
    out += pre + "nbhats " +
           std::to_string(SDL_JoystickNumHats(slot->joystick)) + " ";
    out += pre + "nbaxes " +
           std::to_string(SDL_JoystickNumAxes(slot->joystick)) + " ";
  }

  return out;
}

void InputManager::applyAssignment(const std::vector<std::string> &guids) {
  assign_guids_ = guids;
  recomputePlayers();
}

void InputManager::recomputePlayers() {
  for (auto &s : slots_)
    s.assigned_player = -1;
  std::vector<bool> taken(8, false);

  // 1) asignados por GUID
  for (size_t p = 0; p < 8 && p < assign_guids_.size(); ++p) {
    if (assign_guids_[p].empty())
      continue;
    for (int slot_id : order_) {
      Slot *s = slotById(slot_id);
      if (s && s->attached && s->assigned_player < 0 &&
          s->guid == assign_guids_[p]) {
        s->assigned_player = (int)p;
        taken[p] = true;
        break;
      }
    }
  }

  // 2) el resto, en orden de conexión, huecos libres
  int next = 0;
  for (int slot_id : order_) {
    Slot *s = slotById(slot_id);
    if (!s || !s->attached || s->assigned_player >= 0)
      continue;
    while (next < 8 && taken[next])
      ++next;
    if (next >= 8)
      break;
    s->assigned_player = next;
    taken[next] = true;
    ++next;
  }
}

const InputManager::Slot *InputManager::slotByPlayer(int p) const {
  for (const auto &s : slots_)
    if (s.attached && s.assigned_player == p)
      return &s;
  return nullptr;
}

std::vector<InputManager::DeviceInfo> InputManager::devices() const {
  std::vector<DeviceInfo> out;
  int n = SDL_NumJoysticks();
  for (int slot_id : order_) {
    const Slot *s = slotById(slot_id);
    if (!s || !s->attached || !s->joystick)
      continue;
    SDL_JoystickID inst = SDL_JoystickInstanceID(s->joystick);
    int idx = -1;
    for (int i = 0; i < n; ++i)
      if (SDL_JoystickGetDeviceInstanceID(i) == inst) {
        idx = i;
        break;
      }
    out.push_back({s->guid, s->name, idx});
  }
  return out;
}
