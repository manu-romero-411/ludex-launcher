// src/shell_actions.h
#pragma once
#include "../../input/input_manager.h"
#include "core/bluetooth_manager.h"
#include <functional>
#include <vector>

struct App;
struct ShellState;
struct Config;

struct ShellActions {
  std::function<void(const App &)> launch;
  std::function<void()> open_settings;
  std::function<void()> quit;
  std::function<void()> poweroff;
  std::function<void()> reboot;
  std::function<void()> suspend;
  std::function<std::vector<InputManager::PlayerStatus>()> player_status;
  std::function<void()> reload_ui_icons;
  std::function<void()> open_controllers;
  std::function<void()> apply_controllers;
  std::function<std::vector<InputManager::DeviceInfo>()> devices;

  // Bluetooth
  std::function<bool()> bluetooth_available;
  std::function<void()> bluetooth_scan;
  std::function<void(const std::string &)> bluetooth_pair;
  std::function<void(const std::string &)> bluetooth_connect;
  std::function<void(const std::string &)> bluetooth_disconnect;
  std::function<void(const std::string &)> bluetooth_remove;
  std::function<std::vector<BluetoothDevice>()> bluetooth_devices;
  std::function<bool()> bluetooth_scanning;
  std::function<int()> bluetooth_scan_remaining;
  std::function<void()> open_bluetooth;
  std::function<std::vector<BluetoothDevice>()> bluetooth_discovered;
  std::function<void()> bluetooth_cancel_pin;
  std::function<void(const std::string &mac, const std::string &pin)>
      bluetooth_submit_pin;
  std::function<void()> bluetooth_cancel_scan;
};