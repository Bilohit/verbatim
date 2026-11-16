#include "Module.h"

#include "Catalog.h"
#include "fx/Chorus.h"
#include "fx/Comp.h"
#include "fx/Crush.h"
#include "fx/Delay.h"
#include "fx/Filter.h"
#include "fx/Phaser.h"
#include "fx/Sat.h"
#include "fx/Verb.h"

namespace vbdsp
{
namespace
{
// Stand-in for effects not implemented yet (tasks T13-T19).
class PassThrough final : public Module
{
public:
    void prepare (double, int) override {}
    void reset() override {}
    void process (float*, float*, int, const ModCtx&) override {}
};
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
        default:        return std::make_unique<PassThrough>();
    }
}

int satLatencySamples()
{
    juce::dsp::Oversampling<float> os (2, Sat::oversamplingStages, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    os.initProcessing (64);
    return static_cast<int> (std::lround (os.getLatencyInSamples()));
}
} // namespace vbdsp
