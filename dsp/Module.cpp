#include "Module.h"

#include "Catalog.h"
#include "fx/Chorus.h"
#include "fx/Comb.h"
#include "fx/Comp.h"
#include "fx/Crush.h"
#include "fx/Delay.h"
#include "fx/Filter.h"
#include "fx/Grain.h"
#include "fx/Phaser.h"
#include "fx/Pitch.h"
#include "fx/Sat.h"
#include "fx/Trem.h"
#include "fx/Verb.h"

namespace vbdsp
{
namespace
{
} // namespace

std::unique_ptr<Module> makeModule (const vb::ModuleSpec& spec, std::uint64_t laneSeed, int slot)
{
    switch (spec.type)
    {
        case vb::SAT:   return std::make_unique<Sat> (spec);
        case vb::DELAY: return std::make_unique<Delay> (spec);
        case vb::FILTER: return std::make_unique<Filter> (spec);
        case vb::CRUSH: return std::make_unique<Crush> (spec, laneSeed, slot);
        case vb::CHORUS:
        case vb::FLANGE: return std::make_unique<Chorus> (spec);
        case vb::PHASER: return std::make_unique<Phaser> (spec);
        case vb::VERB:   return std::make_unique<Verb> (spec);
        case vb::COMP:   return std::make_unique<Comp> (spec);
        case vb::TREM:   return std::make_unique<Trem> (spec);
        case vb::WOW:    return std::make_unique<Wow> (spec, laneSeed, slot);
        case vb::RING:   return std::make_unique<Ring> (spec);
        case vb::COMB:   return std::make_unique<Comb> (spec);
        case vb::FREEZE: return std::make_unique<Freeze> (spec);
        case vb::PITCH:  return std::make_unique<Pitch> (spec);
        case vb::GRAIN:  return std::make_unique<Grain> (spec, laneSeed, slot);
        default:        return std::make_unique<Sat> (spec); // unreachable: all 16 IDs handled above
    }
}

int satLatencySamples()
{
    juce::dsp::Oversampling<float> os (2, Sat::oversamplingStages, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    os.initProcessing (64);
    return static_cast<int> (std::lround (os.getLatencyInSamples()));
}
} // namespace vbdsp
