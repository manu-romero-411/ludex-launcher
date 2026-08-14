// src/controllers_config.h
#pragma once
#include <string>

struct ControllersConfig {
    static constexpr int MAX_PLAYERS = 8;
    int active_player = 0;
    std::string controller_guid[MAX_PLAYERS];
    std::string controller_name[MAX_PLAYERS];
};