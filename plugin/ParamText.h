#pragma once
// Mapped parameter values with units for the UI (docs/05-ui-design.md section 6: the UI never maps ranges).
#include "Resolve.h"

#include <juce_core/juce_core.h>

#include <string_view>
#include <vector>

namespace vbplugin
{
// One entry per used param plus "mix": { name, index (0..5, or 6 = mix), u, value, text }.
// timeDiv != 0 shows time params that follow the lane time base as "1/N".
std::vector<juce::var> moduleParams (const vb::ModuleSpec& module, int timeDiv);

// [ { lane, modules: [ { type, name, held, params } ] } ]
juce::var chainPayload (const vb::ResolvedChain& chain);

// Per LF-separated line: [ { start, length, class } ], byte offsets within the line (grammar section 9).
juce::var tokensPayload (std::string_view text);
} // namespace vbplugin
