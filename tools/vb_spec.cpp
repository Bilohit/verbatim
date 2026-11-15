// vb_spec: text -> ChainSpec dump and chain hash; golden corpus hashing.
//   vb_spec "<text>"            print the ChainSpec and chainHash
//   vb_spec --corpus in out     write one chainHash per corpus record
// Text and corpus records use \xNN escapes for any byte (a backslash is \x5C).
#include "Catalog.h"
#include "Generate.h"
#include "Serialize.h"

#include <cinttypes>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

namespace
{
int hexVal (char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::string unescape (const std::string& s)
{
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i)
    {
        if (s[i] == '\\' && i + 3 < s.size() && s[i + 1] == 'x' && hexVal (s[i + 2]) >= 0 && hexVal (s[i + 3]) >= 0)
        {
            out += static_cast<char> (hexVal (s[i + 2]) * 16 + hexVal (s[i + 3]));
            i += 3;
        }
        else
        {
            out += s[i];
        }
    }
    return out;
}

void dump (const vb::ChainSpec& spec)
{
    std::printf ("grammarVersion %u lanes %zu\n", spec.grammarVersion, spec.lanes.size());
    for (std::size_t i = 0; i < spec.lanes.size(); ++i)
    {
        const auto& l = spec.lanes[i];
        std::printf ("lane %zu E \"%s\" seed %016" PRIx64 " n %u\n", i, l.entropy.c_str(), l.seed, l.nModules);
        std::printf ("  drive %u duck %u feedback %u split %u chain %u band %u bandCenter %u mixSteps %d mixAbs %u timeDiv %u level %d space %d emphasis %u\n",
                     l.drive, l.duck, l.feedback, l.split, l.chain, l.band, l.bandCenter, l.mixSteps, l.mixAbs, l.timeDiv,
                     l.level, l.space, l.emphasis);
        std::printf ("  lfoA shape %u rate %u depth %u target %u  lfoB rate %u depth %u target %u  env depth %u target %u\n",
                     l.lfoA.shape, l.lfoA.rate, l.lfoA.depth, l.lfoA.target, l.lfoB.rate, l.lfoB.depth, l.lfoB.target,
                     l.env.depth, l.env.target);
        for (std::size_t k = 0; k < l.slots.size(); ++k)
        {
            const auto& m = l.slots[k];
            std::printf ("  %s slot %zu %-6s p %5u %5u %5u %5u %5u %5u mix %5u\n", k < l.nModules ? " " : "~", k,
                         vb::catalog[m.type].name, m.p[0], m.p[1], m.p[2], m.p[3], m.p[4], m.p[5], m.mix);
        }
    }
    std::printf ("chainHash %016" PRIx64 "\n", vb::chainHash (spec));
}

int corpus (const char* inPath, const char* outPath)
{
    std::ifstream in (inPath, std::ios::binary);
    std::ofstream out (outPath, std::ios::binary);
    if (! in || ! out)
    {
        std::cerr << "cannot open corpus files\n";
        return 2;
    }
    std::string line;
    std::size_t n = 0;
    while (std::getline (in, line))
    {
        if (! line.empty() && line.back() == '\r')
            line.pop_back();
        char buf[32];
        std::snprintf (buf, sizeof (buf), "%016" PRIx64 "\n", vb::chainHash (vb::generate (unescape (line))));
        out << buf;
        ++n;
    }
    std::cout << n << " records\n";
    return 0;
}
} // namespace

int main (int argc, char** argv)
{
    if (argc == 4 && std::string (argv[1]) == "--corpus")
        return corpus (argv[2], argv[3]);
    if (argc == 2)
    {
        dump (vb::generate (unescape (argv[1])));
        return 0;
    }
    std::cerr << "usage: vb_spec \"<text>\" | vb_spec --corpus in out\n";
    return 2;
}
