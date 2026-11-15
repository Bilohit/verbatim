#include "Tokenizer.h"

namespace vb
{
namespace
{
bool isEllipsisAt (std::string_view s, std::size_t i)
{
    return i + 2 < s.size() && static_cast<unsigned char> (s[i]) == 0xE2
        && static_cast<unsigned char> (s[i + 1]) == 0x80 && static_cast<unsigned char> (s[i + 2]) == 0xA6;
}

bool isDigit (unsigned char b) { return b >= '0' && b <= '9'; }

bool isWordByte (std::string_view s, std::size_t i)
{
    const auto b = static_cast<unsigned char> (s[i]);
    if ((b >= 'A' && b <= 'Z') || (b >= 'a' && b <= 'z') || b == 0x27)
        return true;
    return b >= 0x80 && ! isEllipsisAt (s, i);
}

bool isOp (unsigned char b)
{
    switch (b)
    {
        case '!': case '?': case '~': case ',': case '-': case '+':
        case '>': case '/': case ':': case '&': case '"':
            return true;
        default:
            return false;
    }
}
} // namespace

std::vector<Token> tokenize (std::string_view s)
{
    std::vector<Token> out;
    std::size_t i = 0;
    while (i < s.size())
    {
        const auto start = i;
        const auto b = static_cast<unsigned char> (s[i]);

        if (isEllipsisAt (s, i)) // rule 1
        {
            out.push_back ({ static_cast<std::uint32_t> (i), 3, TokKind::Ellipsis, 0 });
            i += 3;
        }
        else if (b == '.') // rule 2
        {
            while (i < s.size() && s[i] == '.')
                ++i;
            const auto r = i - start;
            if (r >= 3)
                out.push_back ({ static_cast<std::uint32_t> (start), static_cast<std::uint32_t> (r), TokKind::Ellipsis, 0 });
            else
                for (std::size_t k = 0; k < r; ++k)
                    out.push_back ({ static_cast<std::uint32_t> (start + k), 1, TokKind::Stop, 0 });
        }
        else if (isDigit (b)) // rule 3
        {
            std::uint32_t value = 0;
            while (i < s.size() && isDigit (static_cast<unsigned char> (s[i])))
            {
                value = value * 10u + static_cast<std::uint32_t> (s[i] - '0');
                if (value > 9999u)
                    value = 9999u; // saturate; also keeps the accumulator small
                ++i;
            }
            if (i - start > 6)
                value = 9999u;
            auto kind = TokKind::Num;
            if (i < s.size() && s[i] == '%')
            {
                kind = TokKind::Pct;
                ++i;
            }
            out.push_back ({ static_cast<std::uint32_t> (start), static_cast<std::uint32_t> (i - start), kind, value });
        }
        else if (b <= 0x20 || b == 0x7F) // rule 4
        {
            ++i;
        }
        else if (isWordByte (s, i)) // rule 5
        {
            while (i < s.size() && isWordByte (s, i))
                ++i;
            out.push_back ({ static_cast<std::uint32_t> (start), static_cast<std::uint32_t> (i - start), TokKind::Word, 0 });
        }
        else // rules 6 and 7
        {
            out.push_back ({ static_cast<std::uint32_t> (i), 1, isOp (b) ? TokKind::Op : TokKind::Sym, b });
            ++i;
        }
    }
    return out;
}
} // namespace vb
