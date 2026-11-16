#include "ParamText.h"

#include "Catalog.h"
#include "Parse.h"

#include <cmath>

namespace vbplugin
{
namespace
{
enum class Map { Lin, Exp, Disc };

struct Desc
{
    const char* name;
    Map map;
    double lo, hi;    // Disc: lo = offset added to the index, hi = count
    const char* unit; // "%" multiplies by 100
    const char* labels = nullptr; // Disc: '|' separated
    bool timeBased = false;
};

// shortcut: ranges duplicate dsp/fx/*.cpp (docs/03-dsp-catalog.md); keep both in step when a range is tuned.
const std::vector<Desc>& descs (int type)
{
    static const std::vector<Desc> table[vb::EFFECT_COUNT] = {
        { { "drive", Map::Lin, 0, 24, "dB" }, { "tone", Map::Lin, -6, 6, "dB" }, { "shape", Map::Disc, 0, 4, "", "tanh|tube|hard|fold" }, { "bias", Map::Lin, 0, 0.25, "" } },
        { { "bits", Map::Lin, 4, 12, "bits" }, { "rate", Map::Exp, 2000, 24000, "Hz" }, { "jitter", Map::Lin, 0, 0.3, "%" } },
        { { "cutoff", Map::Exp, 0, 0, "Hz" }, { "reso", Map::Exp, 0.5, 8, "Q" }, { "mode", Map::Disc, 0, 4, "", "LP|HP|BP|notch" }, { "slope", Map::Disc, 0, 2, "", "12 dB|24 dB" } },
        { { "time", Map::Exp, 30, 900, "ms", nullptr, true }, { "feedback", Map::Lin, 0, 0.8, "%" }, { "tone", Map::Exp, 1000, 12000, "Hz" }, { "mode", Map::Disc, 0, 3, "", "stereo|ping-pong|tape" } },
        { { "decay", Map::Lin, 0.3, 0.95, "%" }, { "damp", Map::Lin, 0.8, 0.1, "%" }, { "predelay", Map::Lin, 0, 80, "ms" }, { "width", Map::Lin, 0.5, 1, "%" } },
        { { "rate", Map::Exp, 0.1, 3, "Hz" }, { "depth", Map::Lin, 0.1, 0.6, "%" }, { "delay", Map::Lin, 7, 25, "ms" }, { "feedback", Map::Lin, -0.3, 0.3, "%" } },
        { { "rate", Map::Exp, 0.05, 4, "Hz" }, { "depth", Map::Lin, 0.3, 1, "%" }, { "centre", Map::Exp, 300, 3000, "Hz" }, { "feedback", Map::Lin, 0, 0.7, "%" } },
        { { "rate", Map::Exp, 0.5, 12, "Hz", nullptr, true }, { "depth", Map::Lin, 0.2, 0.9, "%" }, { "shape", Map::Disc, 0, 3, "", "sine|triangle|square" }, { "pan", Map::Lin, 0, 1, "%" } },
        { { "rate", Map::Exp, 0.3, 1.5, "Hz" }, { "depth", Map::Lin, 0.5, 5, "ms" }, { "flutter", Map::Lin, 0, 0.4, "ms" }, { "age", Map::Exp, 3500, 16000, "Hz" } },
        { { "length", Map::Exp, 60, 500, "ms", nullptr, true }, { "period", Map::Disc, 2, 7, "x" }, { "repeats", Map::Disc, 1, 3, "x" } },
        { { "pitch", Map::Exp, 55, 880, "Hz" }, { "feedback", Map::Lin, 0.3, 0.9, "%" }, { "damp", Map::Exp, 1500, 12000, "Hz" }, { "polarity", Map::Disc, 0, 2, "", "+|-" } },
        { { "freq", Map::Exp, 20, 2000, "Hz" }, { "wave", Map::Disc, 0, 2, "", "sine|triangle" } },
        { { "interval", Map::Disc, 0, 8, "", "-12 st|-7 st|-5 st|+3 st|+4 st|+5 st|+7 st|+12 st" }, { "detune", Map::Lin, -15, 15, "ct" }, { "window", Map::Lin, 30, 80, "ms" }, { "feedback", Map::Lin, 0, 0.4, "%" } },
        { { "size", Map::Exp, 20, 200, "ms" }, { "density", Map::Exp, 4, 40, "/s" }, { "spray", Map::Lin, 0, 500, "ms" }, { "pitch", Map::Disc, 0, 4, "", "0|0 +12|0 -12|0 +7 +12" }, { "reverse", Map::Lin, 0, 0.5, "%" } },
        { { "threshold", Map::Lin, -30, -8, "dB" }, { "ratio", Map::Exp, 2, 10, ":1" }, { "attack", Map::Exp, 1, 50, "ms" }, { "release", Map::Exp, 40, 400, "ms" } },
        { { "rate", Map::Exp, 0.05, 2, "Hz" }, { "depth", Map::Lin, 0.2, 0.8, "%" }, { "delay", Map::Lin, 1, 8, "ms" }, { "feedback", Map::Lin, -0.7, 0.7, "%" } },
    };
    return table[type];
}

constexpr double mixRange[vb::EFFECT_COUNT][2] = { { 0.5, 1.0 }, { 0.3, 1.0 }, { 0.7, 1.0 }, { 0.15, 0.5 }, { 0.10, 0.45 }, { 0.3, 0.6 },
                                                    { 0.3, 0.6 }, { 0.7, 1.0 }, { 0.8, 1.0 }, { 0.3, 0.8 }, { 0.2, 0.6 }, { 0.15, 0.6 },
                                                    { 0.2, 0.5 }, { 0.2, 0.6 }, { 0.5, 1.0 }, { 0.3, 0.5 } };

juce::String number (double v)
{
    const double a = std::abs (v);
    return juce::String (v, a >= 100 ? 0 : a >= 10 ? 1 : 2);
}

juce::var entry (const char* name, int index, std::uint16_t u, double value, const juce::String& text)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("name", name);
    o->setProperty ("index", index);
    o->setProperty ("u", static_cast<int> (u));
    o->setProperty ("value", value);
    o->setProperty ("text", text);
    return juce::var (o);
}
} // namespace

std::vector<juce::var> moduleParams (const vb::ModuleSpec& m, int timeDiv)
{
    std::vector<juce::var> out;
    const auto type = static_cast<int> (m.type % vb::EFFECT_COUNT);
    const auto& d = descs (type);
    for (std::size_t q = 0; q < d.size(); ++q)
    {
        const auto u = m.p[q];
        const double x = u / 65535.0;
        double lo = d[q].lo, hi = d[q].hi;
        if (type == vb::FILTER && q == 0) // cutoff range by mode
        {
            const int mode = (m.p[2] * 4) >> 16;
            lo = mode == 0 ? 200 : mode == 1 ? 40 : 150;
            hi = mode == 0 ? 12000 : mode == 1 ? 2000 : 6000;
        }
        double value;
        juce::String text;
        if (d[q].map == Map::Disc)
        {
            const int idx = (u * static_cast<int> (hi)) >> 16;
            value = lo + idx;
            text = d[q].labels != nullptr ? juce::StringArray::fromTokens (d[q].labels, "|", "")[idx] : juce::String (static_cast<int> (value)) + d[q].unit;
        }
        else
        {
            value = d[q].map == Map::Lin ? lo + (hi - lo) * x : lo * std::pow (hi / lo, x);
            if (type == vb::COMB && q == 0)
                value = 440.0 * std::pow (2.0, std::round (12.0 * std::log2 (value / 440.0)) / 12.0);
            if (d[q].timeBased && timeDiv != 0)
                text = "1/" + juce::String (timeDiv);
            else if (juce::String (d[q].unit) == "%")
                text = number (value * 100.0) + " %";
            else
                text = number (value) + (juce::String (d[q].unit).isEmpty() ? "" : " " + juce::String (d[q].unit));
        }
        out.push_back (entry (d[q].name, static_cast<int> (q), u, value, text));
    }
    const double mix = mixRange[type][0] + (mixRange[type][1] - mixRange[type][0]) * (m.mix / 65535.0);
    out.push_back (entry ("mix", 6, m.mix, mix, number (mix * 100.0) + " %"));
    return out;
}

juce::var chainPayload (const vb::ResolvedChain& chain)
{
    juce::Array<juce::var> lanes;
    for (std::size_t i = 0; i < chain.lanes.size(); ++i)
    {
        juce::Array<juce::var> modules;
        for (const auto& rm : chain.lanes[i].modules)
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("type", static_cast<int> (rm.spec.type));
            o->setProperty ("name", vb::catalog[rm.spec.type % vb::EFFECT_COUNT].name);
            o->setProperty ("held", rm.held);
            juce::Array<juce::var> raw;
            for (auto v : rm.spec.p)
                raw.add (static_cast<int> (v));
            o->setProperty ("p", raw); // all six, so an edit can send the whole module back
            o->setProperty ("mix", static_cast<int> (rm.spec.mix));
            juce::Array<juce::var> params;
            for (auto& p : moduleParams (rm.spec, chain.lanes[i].lane.timeDiv))
                params.add (p);
            o->setProperty ("params", params);
            modules.add (juce::var (o));
        }
        auto* lane = new juce::DynamicObject();
        lane->setProperty ("lane", static_cast<int> (i));
        lane->setProperty ("modules", modules);
        lanes.add (juce::var (lane));
    }
    return lanes;
}

juce::var tokensPayload (std::string_view text)
{
    static const char* classes[] = { "word", "direction", "op", "number", "ignored" };
    juce::Array<juce::var> lines;
    for (const auto& line : vb::parse (text).lines)
    {
        juce::Array<juce::var> tokens;
        for (const auto& t : line.tokens)
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("start", static_cast<int> (t.start));
            o->setProperty ("length", static_cast<int> (t.length));
            o->setProperty ("class", classes[static_cast<int> (t.cls)]);
            tokens.add (juce::var (o));
        }
        lines.add (tokens);
    }
    return lines;
}
} // namespace vbplugin
