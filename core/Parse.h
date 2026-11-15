#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace vb
{
inline constexpr std::size_t MAX_TEXT_BYTES = 4096;
inline constexpr std::size_t MAX_LANES = 4;

enum Axis : std::uint8_t { TONE, HEAT, RATE, SPACE, TAIL, DEPTH, LEVEL, AXIS_COUNT };

// Per-lane parse result: entropy string and modifiers (docs/04-seed-grammar.md section 5).
struct LineParse
{
    std::string entropy;
    std::array<std::int8_t, AXIS_COUNT> axes {};
    std::uint8_t drive = 0, wander = 0, wobble = 0, duck = 0, feedback = 0, follow = 0, emphasis = 0;
    std::uint8_t split = 0, chain = 0, band = 0;
    std::int8_t mixSteps = 0;
    std::uint8_t mixAbs = 255;
    std::uint8_t timeDiv = 0;
};

// UI token classes (section 9).
enum class TokenClass : std::uint8_t { Word, Direction, Op, Number, Ignored };

struct UiToken
{
    std::uint32_t start; // byte offset in the line
    std::uint32_t length;
    TokenClass cls;
};

// One LF-separated line of the preprocessed text, blank lines included.
struct TextLine
{
    std::uint32_t start;  // byte offset in the preprocessed text
    std::uint32_t length;
    int lane = -1;        // lane index, or -1 (blank or ignored)
    bool ignored = false; // non-blank line beyond the 4th
    std::vector<UiToken> tokens;
};

struct ParseResult
{
    std::vector<LineParse> lanes;
    std::vector<TextLine> lines;
};

// P2 and P3: line endings to LF, truncate to 4096 bytes at a UTF-8 boundary.
std::string preprocess (std::string_view text);

ParseResult parse (std::string_view text);
} // namespace vb
