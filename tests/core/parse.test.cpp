// G2 (parse columns) and F1 (parse only): docs/04-seed-grammar.md sections 2, 4, 5, 8, 9.
#include <catch2/catch_test_macros.hpp>

#include "Parse.h"
#include "Rng.h"

#include <array>
#include <string>
#include <vector>

using vb::Axis;

namespace
{
using Axes = std::array<int, 6>; // TONE, HEAT, RATE, SPACE, TAIL, DEPTH as listed in section 8

Axes axesOf (const vb::LineParse& l)
{
    return { l.axes[vb::TONE], l.axes[vb::HEAT], l.axes[vb::RATE], l.axes[vb::SPACE], l.axes[vb::TAIL], l.axes[vb::DEPTH] };
}

// Fields shown in section 8 "Modifiers"; everything not set must be at its default.
struct Fields
{
    Axes axes {};
    int drive = 0, duck = 0, feedback = 0, split = 0, chain = 0, band = 0, emphasis = 0;
    int wander = 0, wobble = 0, follow = 0, mixSteps = 0, mixAbs = 255, timeDiv = 0, level = 0;
};

void checkFields (const vb::LineParse& l, const Fields& f)
{
    CHECK (axesOf (l) == f.axes);
    CHECK (l.axes[vb::LEVEL] == f.level);
    CHECK (l.drive == f.drive);
    CHECK (l.duck == f.duck);
    CHECK (l.feedback == f.feedback);
    CHECK (l.split == f.split);
    CHECK (l.chain == f.chain);
    CHECK (l.band == f.band);
    CHECK (l.emphasis == f.emphasis);
    CHECK (l.wander == f.wander);
    CHECK (l.wobble == f.wobble);
    CHECK (l.follow == f.follow);
    CHECK (l.mixSteps == f.mixSteps);
    CHECK (l.mixAbs == f.mixAbs);
    CHECK (l.timeDiv == f.timeDiv);
}
} // namespace

TEST_CASE ("G2 parse example 1: hello", "[G2]")
{
    const auto r = vb::parse ("hello");
    REQUIRE (r.lanes.size() == 1);
    CHECK (r.lanes[0].entropy == "hello");
    checkFields (r.lanes[0], {});
}

TEST_CASE ("G2 parse example 2: dark tape, slow echo!", "[G2]")
{
    const auto r = vb::parse ("dark tape, slow echo!");
    REQUIRE (r.lanes.size() == 1);
    CHECK (r.lanes[0].entropy == "tape echo");
    Fields f; f.axes = { -1, 0, -1, 0, 0, 0 }; f.drive = 1; f.duck = 1;
    checkFields (r.lanes[0], f);
}

TEST_CASE ("G2 parse example 3: tape echo", "[G2]")
{
    const auto r = vb::parse ("tape echo");
    REQUIRE (r.lanes.size() == 1);
    CHECK (r.lanes[0].entropy == "tape echo");
    checkFields (r.lanes[0], {});
}

TEST_CASE ("G2 parse example 4: TAPE echo!!", "[G2]")
{
    const auto r = vb::parse ("TAPE echo!!");
    REQUIRE (r.lanes.size() == 1);
    CHECK (r.lanes[0].entropy == "tape echo");
    Fields f; f.emphasis = 1; f.drive = 2;
    checkFields (r.lanes[0], f);
}

TEST_CASE ("G2 parse example 5: dark!!", "[G2]")
{
    const auto r = vb::parse ("dark!!");
    REQUIRE (r.lanes.size() == 1);
    CHECK (r.lanes[0].entropy.empty());
    Fields f; f.axes = { -1, 0, 0, 0, 0, 0 }; f.drive = 2;
    checkFields (r.lanes[0], f);
}

TEST_CASE ("G2 parse example 6: echo 8 50%...", "[G2]")
{
    const auto r = vb::parse ("echo 8 50%...");
    REQUIRE (r.lanes.size() == 1);
    CHECK (r.lanes[0].entropy == "echo");
    Fields f; f.axes = { 0, 0, 0, 0, 1, 0 }; f.mixAbs = 50; f.timeDiv = 8;
    checkFields (r.lanes[0], f);
}

TEST_CASE ("G2 parse example 7: lo-fi \"radio\" > 7", "[G2]")
{
    const auto r = vb::parse ("lo-fi \"radio\" > 7");
    REQUIRE (r.lanes.size() == 1);
    CHECK (r.lanes[0].entropy == "lo fi radio");
    Fields f; f.feedback = 1; f.band = 1; f.mixSteps = -1; f.timeDiv = 6;
    checkFields (r.lanes[0], f);
}

TEST_CASE ("G2 parse example 8: Cyrillic and emoji", "[G2]")
{
    const std::string text = "\xD0\xBF\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82 \xD0\xBC\xD0\xB8\xD1\x80 \xE2\x9C\x89";
    const auto r = vb::parse (text);
    REQUIRE (r.lanes.size() == 1);
    CHECK (r.lanes[0].entropy == text);
    checkFields (r.lanes[0], {});
}

TEST_CASE ("G2 parse example 9: two lanes with chain", "[G2]")
{
    const auto r = vb::parse ("warm tape\n\n& long echo\xE2\x80\xA6 ~?");
    REQUIRE (r.lanes.size() == 2);
    CHECK (r.lanes[0].entropy == "tape");
    Fields f0; f0.axes = { 0, 1, 0, 0, 0, 0 };
    checkFields (r.lanes[0], f0);
    CHECK (r.lanes[1].entropy == "echo");
    Fields f1; f1.axes = { 0, 0, 0, 0, 2, 0 }; f1.chain = 1; f1.wobble = 1; f1.wander = 1;
    checkFields (r.lanes[1], f1);
}

TEST_CASE ("G2 parse example 10: three spaces", "[G2]")
{
    CHECK (vb::parse ("   ").lanes.empty());
}

TEST_CASE ("G2 parse example 11: four-lane limit", "[G2]")
{
    const auto r = vb::parse ("a\nb\nc\nd\ne");
    REQUIRE (r.lanes.size() == 4);
    CHECK (r.lanes[0].entropy == "a");
    CHECK (r.lanes[3].entropy == "d");
    REQUIRE (r.lines.size() == 5);
    CHECK (r.lines[4].lane == -1);
    CHECK (r.lines[4].ignored);
    REQUIRE (r.lines[4].tokens.size() == 1);
    CHECK (r.lines[4].tokens[0].cls == vb::TokenClass::Ignored);
}

TEST_CASE ("G2 parse example 12: DARK Dark dark bright", "[G2]")
{
    const auto r = vb::parse ("DARK Dark dark bright");
    REQUIRE (r.lanes.size() == 1);
    CHECK (r.lanes[0].entropy.empty());
    Fields f; f.axes = { -2, 0, 0, 0, 0, 0 }; f.emphasis = 1;
    checkFields (r.lanes[0], f);
}

TEST_CASE ("G2 parse example 13: wait.. what...", "[G2]")
{
    const auto r = vb::parse ("wait.. what...");
    REQUIRE (r.lanes.size() == 1);
    CHECK (r.lanes[0].entropy == "wait what");
    Fields f; f.axes = { 0, 0, 0, 0, -1, 0 };
    checkFields (r.lanes[0], f);
}

TEST_CASE ("G2 parse example 14: don't stop", "[G2]")
{
    const auto r = vb::parse ("don't stop");
    REQUIRE (r.lanes.size() == 1);
    CHECK (r.lanes[0].entropy == "don't stop");
    checkFields (r.lanes[0], {});
}

// ---- Rule-level tests (sections 2, 4, 5) ----

TEST_CASE ("P2 CR LF and lone CR become LF", "[parse]")
{
    CHECK (vb::preprocess ("a\r\nb\rc") == "a\nb\nc");
}

TEST_CASE ("P3 truncation steps back off a continuation byte", "[parse]")
{
    std::string t (4095, 'a');
    t += "\xD0\xBF"; // two-byte char straddling the 4096 cut
    const auto p = vb::preprocess (t);
    CHECK (p.size() == 4095);

    CHECK (vb::preprocess (std::string (5000, 'a')).size() == 4096);
}

TEST_CASE ("NUM: longer than 6 digits saturates to 9999 and is ignored as time base", "[parse]")
{
    CHECK (vb::parse ("x 0000005").lanes[0].timeDiv == 0);
    CHECK (vb::parse ("x 5").lanes[0].timeDiv == 4);   // 5 is between 4 and 6: tie goes to smaller
    CHECK (vb::parse ("x 64").lanes[0].timeDiv == 32);
    CHECK (vb::parse ("x 65").lanes[0].timeDiv == 0);
    CHECK (vb::parse ("x 0 12").lanes[0].timeDiv == 12); // out-of-range NUM skipped, first in-range wins
    CHECK (vb::parse ("x 3 16").lanes[0].timeDiv == 3);  // later NUMs ignored
}

TEST_CASE ("PCT: first only, capped at 100; bare % is SYM entropy", "[parse]")
{
    CHECK (vb::parse ("x 150% 20%").lanes[0].mixAbs == 100);
    const auto r = vb::parse ("x %");
    CHECK (r.lanes[0].mixAbs == 255);
    CHECK (r.lanes[0].entropy == "x %");
}

TEST_CASE ("Counted ops saturate at 3, axes clamp at 3", "[parse]")
{
    const auto r = vb::parse ("x !!!!! ,,,,, >>>> :::: ++++++ dark dark dark dark");
    const auto& l = r.lanes[0];
    CHECK (l.drive == 3);
    CHECK (l.duck == 3);
    CHECK (l.feedback == 3);
    CHECK (l.follow == 3);
    CHECK (l.mixSteps == 3);
    CHECK (l.axes[vb::TONE] == -3);
}

TEST_CASE ("Split and LEVEL axis", "[parse]")
{
    const auto r = vb::parse ("x / loud loud");
    CHECK (r.lanes[0].split == 1);
    CHECK (r.lanes[0].axes[vb::LEVEL] == 2);
}

TEST_CASE ("UI token classes (section 9)", "[parse]")
{
    const auto r = vb::parse ("Dark tape, 8 x");
    REQUIRE (r.lines.size() == 1);
    const auto& t = r.lines[0].tokens;
    REQUIRE (t.size() == 5);
    CHECK ((t[0].start == 0 && t[0].length == 4 && t[0].cls == vb::TokenClass::Direction));
    CHECK ((t[1].start == 5 && t[1].length == 4 && t[1].cls == vb::TokenClass::Word));
    CHECK ((t[2].start == 9 && t[2].length == 1 && t[2].cls == vb::TokenClass::Op));
    CHECK ((t[3].start == 11 && t[3].length == 1 && t[3].cls == vb::TokenClass::Number));
    CHECK ((t[4].start == 13 && t[4].length == 1 && t[4].cls == vb::TokenClass::Word));
}

TEST_CASE ("F1 parse fuzz: 100000 random byte strings stay in range", "[F1]")
{
    vb::SplitMix64 rng { 0xF1F1F1F1ull };
    static const char* pieces[] = { "\xE2\x80\xA6", "\xD0\xBF", "\xF0\x9F\x8E\xB5", "\xD9\x85", "\xD7\xA9", "\xE4\xB8\xAD",
                                    "...", "!!!!", "\r\n", "\n", "\x80", "\xFF", "\x01", "DARK", "wide", "99%", "1234567" };
    for (int iter = 0; iter < 100000; ++iter)
    {
        const auto len = static_cast<std::size_t> ((rng.next() >> 32) % 12001u);
        std::string s;
        s.reserve (len);
        while (s.size() < len)
        {
            const auto pick = rng.next();
            if ((pick & 3u) == 0)
                s += pieces[(pick >> 8) % std::size (pieces)];
            else
                s += static_cast<char> ((pick >> 16) & 0xFF);
        }
        const auto r = vb::parse (s);
        REQUIRE (r.lanes.size() <= 4);
        for (const auto& l : r.lanes)
        {
            for (auto a : l.axes)
                REQUIRE ((a >= -3 && a <= 3));
            REQUIRE (l.drive <= 3); REQUIRE (l.duck <= 3); REQUIRE (l.feedback <= 3);
            REQUIRE (l.wander <= 3); REQUIRE (l.wobble <= 3); REQUIRE (l.follow <= 3); REQUIRE (l.emphasis <= 3);
            REQUIRE (l.split <= 1); REQUIRE (l.chain <= 1); REQUIRE (l.band <= 1);
            REQUIRE ((l.mixSteps >= -3 && l.mixSteps <= 3));
            REQUIRE ((l.mixAbs <= 100 || l.mixAbs == 255));
            const bool okDiv = l.timeDiv == 0 || l.timeDiv == 1 || l.timeDiv == 2 || l.timeDiv == 3 || l.timeDiv == 4 || l.timeDiv == 6
                            || l.timeDiv == 8 || l.timeDiv == 12 || l.timeDiv == 16 || l.timeDiv == 24 || l.timeDiv == 32;
            REQUIRE (okDiv);
        }
    }
}
