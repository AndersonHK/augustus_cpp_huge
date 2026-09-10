#pragma once
#include <string>
#include <functional>

// Called on the game thread while the settings UI owns the event loop.
void mod_settings_apply(const std::string &key, int value);
void mod_settings_show(const std::function<void(const char *, int)> &apply_hardcoded);
void mod_settings_validate_live_changes();
