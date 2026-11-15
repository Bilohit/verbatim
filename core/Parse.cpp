#include "Parse.h"

#include "Tokenizer.h"

#include <algorithm>

namespace vb
{
namespace
{
struct Direction
{
    std::string_view word;
    Axis axis;
    int sign;
};

constexpr Direction directions[] = {
    { "dark", TONE, -1 },    { "bright", TONE, +1 }, { "cold", HEAT, -1 },  { "warm", HEAT, +1 },
    { "slow", RATE, -1 },    { "fast", RATE, +1 },   { "narrow", SPACE, -1 }, { "wide", SPACE, +1 },
    { "short", TAIL, -1 },   { "long", TAIL, +1 },   { "thin", DEPTH, -1 }, { "deep", DEPTH, +1 },
    { "soft", LEVEL, -1 },   { "loud", LEVEL, +1 },
};

constexpr std::uint8_t DIVS[] = { 1, 2, 3, 4, 6, 8, 12, 16, 24, 32 };

const Direction* findDirection (std::string_view low)
{
    for (const auto& d : directions)
        if (d.word == low)
            return &d;
    return nullptr;
}

bool isWhitespaceByte (unsigned char b) { return b <= 0x20 || b == 0x7F; }

bool isBlank (std::string_view line)
{
    return std::all_of (line.begin(), line.end(), [] (char c) { return isWhitespaceByte (static_cast<unsigned char> (c)); });
}

std::uint8_t nearestDiv (std::uint32_t n)
{
    std::uint8_t best = DIVS[0];
    std::uint32_t bestDist = 0xFFFFFFFFu;
    for (auto d : DIVS) // ascending, so a strict < keeps the smaller entry on ties
    {
        const auto dist = n > d ? n - d : d - n;
        if (dist < bestDist)
        {
            best = d;
            bestDist = dist;
        }
    }
    return best;
}

std::uint8_t min3 (int n) { return static_cast<std::uint8_t> (std::min (n, 3)); }
std::int8_t clamp3 (int n) { return static_cast<std::int8_t> (std::clamp (n, -3, 3)); }

// Classifies one non-blank line (section 5). Also fills the UI token list.
LineParse classify (std::string_view line, std::vector<UiToken>& ui)
{
    LineParse lp;
    std::vector<std::string> entropy;
    int axisSum[AXIS_COUNT] {};
    int shout = 0, stops = 0, ellipses = 0;
    int opCount[256] {};
    bool haveNum = false, havePct = false;

    for (const auto& t : tokenize (line))
    {
        const auto text = line.substr (t.start, t.length);
        auto cls = TokenClass::Op;
        switch (t.kind)
        {
            case TokKind::Word:
            {
                std::string low (text);
                int letters = 0;
                bool anyLower = false;
                for (auto& c : low)
                {
                    if (c >= 'A' && c <= 'Z')
                    {
                        ++letters;
                        c = static_cast<char> (c - 'A' + 'a');
                    }
                    else if (c >= 'a' && c <= 'z')
                    {
                        ++letters;
                        anyLower = true;
                    }
                }
                if (letters >= 2 && ! anyLower)
                    ++shout;
                if (const auto* d = findDirection (low))
                {
                    axisSum[d->axis] += d->sign;
                    cls = TokenClass::Direction;
                }
                else
                {
                    entropy.push_back (std::move (low));
                    cls = TokenClass::Word;
                }
                break;
            }
            case TokKind::Sym:
                entropy.emplace_back (text);
                cls = TokenClass::Word;
                break;
            case TokKind::Op:   ++opCount[t.value]; break;
            case TokKind::Stop: ++stops; break;
            case TokKind::Ellipsis: ++ellipses; break;
            case TokKind::Num:
                if (! haveNum && t.value >= 1 && t.value <= 64)
                {
                    lp.timeDiv = nearestDiv (t.value);
                    haveNum = true;
                }
                cls = TokenClass::Number;
                break;
            case TokKind::Pct:
                if (! havePct)
                {
                    lp.mixAbs = static_cast<std::uint8_t> (std::min<std::uint32_t> (t.value, 100));
                    havePct = true;
                }
                cls = TokenClass::Number;
                break;
        }
        ui.push_back ({ t.start, t.length, cls });
    }

    for (std::size_t k = 0; k < entropy.size(); ++k)
    {
        if (k > 0)
            lp.entropy += ' ';
        lp.entropy += entropy[k];
    }

    axisSum[TAIL] += ellipses - stops;
    for (int a = 0; a < AXIS_COUNT; ++a)
        lp.axes[static_cast<std::size_t> (a)] = clamp3 (axisSum[a]);

    lp.drive    = min3 (opCount['!']);
    lp.wander   = min3 (opCount['?']);
    lp.wobble   = min3 (opCount['~']);
    lp.duck     = min3 (opCount[',']);
    lp.feedback = min3 (opCount['>']);
    lp.follow   = min3 (opCount[':']);
    lp.split    = opCount['/'] > 0 ? 1 : 0;
    lp.chain    = opCount['&'] > 0 ? 1 : 0;
    lp.band     = opCount['"'] > 0 ? 1 : 0;
    lp.mixSteps = clamp3 (opCount['+'] - opCount['-']);
    lp.emphasis = min3 (shout);
    return lp;
}
} // namespace

std::string preprocess (std::string_view text)
{
    std::string out;
    out.reserve (text.size());
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] == '\r')
        {
            out += '\n';
            if (i + 1 < text.size() && text[i + 1] == '\n')
                ++i;
        }
        else
        {
            out += text[i];
        }
    }
    if (out.size() > MAX_TEXT_BYTES)
    {
        auto cut = MAX_TEXT_BYTES;
        while (cut > 0 && (static_cast<unsigned char> (out[cut]) & 0xC0) == 0x80)
            --cut;
        out.resize (cut);
    }
    return out;
}

ParseResult parse (std::string_view text)
{
    const auto pre = preprocess (text);
    const std::string_view s (pre);
    ParseResult r;

    std::size_t start = 0;
    while (true)
    {
        const auto lf = s.find ('\n', start);
        const auto end = lf == std::string_view::npos ? s.size() : lf;
        const auto line = s.substr (start, end - start);

        TextLine tl { static_cast<std::uint32_t> (start), static_cast<std::uint32_t> (line.size()) };
        if (! isBlank (line))
        {
            if (r.lanes.size() < MAX_LANES)
            {
                tl.lane = static_cast<int> (r.lanes.size());
                r.lanes.push_back (classify (line, tl.tokens));
            }
            else
            {
                tl.ignored = true;
                tl.tokens.push_back ({ 0, tl.length, TokenClass::Ignored });
            }
        }
        r.lines.push_back (std::move (tl));

        if (lf == std::string_view::npos)
            break;
        start = lf + 1;
    }
    return r;
}
} // namespace vb
