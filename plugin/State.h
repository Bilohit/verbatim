#pragma once
// Plugin state as UTF-8 JSON (docs/02-spec.md section 4).
#include "Resolve.h"

#include <string>

namespace vbplugin
{
inline constexpr int STATE_VERSION = 1;

struct PluginState
{
    std::string text;
    // Host parameter values, stored so a project reload restores them with the text.
    float mix = 100.0f, macro = 0.5f, outputDb = 0.0f;
    vb::Overrides overrides;
    int uiW = 900, uiH = 620;
    bool advanced = false;
};

std::string toJson (const PluginState& state);

// False for corrupt JSON, wrong types, or a newer/unknown stateVersion or grammarVersion (state E3).
bool fromJson (const std::string& json, PluginState& out);
} // namespace vbplugin
