// G3 corpus hashes and G5 stability over the corpus. docs/06-test-plan.md.
#include <catch2/catch_test_macros.hpp>

#include "Generate.h"
#include "Serialize.h"

#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace
{
std::vector<std::string> readLines (const std::string& path)
{
    std::ifstream in (path, std::ios::binary);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline (in, line))
        lines.push_back (line);
    return lines;
}

int hexVal (char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Corpus records are pure ASCII; any byte may be written as \xNN.
std::string unescape (const std::string& s)
{
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i)
    {
        if (s[i] == '\\' &&i + 3 < s.size() && s[i + 1] == 'x' && hexVal (s[i + 2]) >= 0 && hexVal (s[i + 3]) >= 0)
        {
            out += static_cast<char> (hexVal (s[i + 2]) * 16 + hexVal (s[i + 3]));
            i += 3;
        }
        else
            out += s[i];
    }
    return out;
}

std::vector<std::string> corpus()
{
    std::vector<std::string> texts;
    for (const auto& r : readLines (VERBATIM_GOLDEN_DIR "/corpus.txt"))
        texts.push_back (unescape (r));
    return texts;
}

std::uint64_t parseHex (const std::string& s) { return std::stoull (s, nullptr, 16); }
} // namespace

TEST_CASE ("G3 golden corpus: every record matches its frozen chain hash", "[G3]")
{
    const auto texts = corpus();
    const auto hashes = readLines (VERBATIM_GOLDEN_DIR "/corpus.hashes");
    REQUIRE (texts.size() >= 300);
    REQUIRE (hashes.size() == texts.size());
    for (std::size_t i = 0; i < texts.size(); ++i)
    {
        INFO ("record " << i + 1);
        CHECK (vb::chainHash (vb::generate (texts[i])) == parseHex (hashes[i]));
    }
}

TEST_CASE ("G5 corpus stability: twice in one process and from two threads", "[G5]")
{
    const auto texts = corpus();
    REQUIRE (! texts.empty());
    auto run = [&] {
        std::vector<std::uint64_t> h;
        for (const auto& t : texts)
            h.push_back (vb::chainHash (vb::generate (t)));
        return h;
    };
    std::vector<std::uint64_t> fromThread;
    std::thread t ([&] { fromThread = run(); });
    const auto a = run();
    const auto b = run();
    t.join();
    CHECK (a == b);
    CHECK (a == fromThread);
}
