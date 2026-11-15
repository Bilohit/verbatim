#pragma once
#include <cstdint>
#include <string_view>
#include <vector>

namespace vb
{
// Byte-level tokens of one line (docs/04-seed-grammar.md section 4).
enum class TokKind : std::uint8_t { Word, Op, Stop, Ellipsis, Num, Pct, Sym };

struct Token
{
    std::uint32_t start;  // byte offset in the line
    std::uint32_t length; // bytes consumed
    TokKind kind;
    std::uint32_t value;  // NUM/PCT: decimal value (saturated); OP/SYM: the byte
};

std::vector<Token> tokenize (std::string_view line);
} // namespace vb
