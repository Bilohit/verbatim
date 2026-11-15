#pragma once
#include <string_view>

#include "ChainSpec.h"
#include "Parse.h"

namespace vb
{
LaneSpec generateLane (const LineParse& line);
ChainSpec generate (std::string_view text);
} // namespace vb
