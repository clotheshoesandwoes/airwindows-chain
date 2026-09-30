#include "Catalog.h"
#include "AirwinRegistry.h"

namespace awchain
{
namespace
{
juce::String fromStd(const std::string &s) { return juce::String::fromUTF8(s.c_str()); }

juce::String summaryFor(const juce::String &name, const juce::String &what)
{
    auto t = what.trim();
    const auto prefix = name + " is ";
    if (t.startsWith(prefix) && t.length() > prefix.length())
    {
        t = t.substring(prefix.length());
        t = t.substring(0, 1).toUpperCase() + t.substring(1);
    }
    return t;
}

bool inCollection(const AirwinRegistry::awReg &r, const char *c)
{
    return std::find(r.collections.begin(), r.collections.end(), std::string(c)) !=
           r.collections.end();
}

bool isWordChar(juce::juce_wchar c) { return juce::CharacterFunctions::isLetterOrDigit(c); }

// True when token starts a word somewhere in text.
bool startsAWord(const juce::String &text, const juce::String &token)
{
    int from = 0;
    while (true)
    {
        const int at = text.indexOf(from, token);
        if (at < 0)
            return false;
        if (at == 0 || !isWordChar(text[at - 1]))
            return true;
        from = at + 1;
    }
}
} // namespace

const Catalog &Catalog::get()
{
    static const Catalog catalog;
    return catalog;
}

Catalog::Catalog()
{
    if (AirwinConsolidatedBase::defaultSampleRate < 2000.f)
        AirwinConsolidatedBase::defaultSampleRate = 48000.f;

    // Also builds the lookup tables; the registry is unusable until this runs.
    AirwinRegistry::filterAndRebuildRegistry(
        [](const AirwinRegistry::awReg &r) { return r.nParams > kParamsPerSlot; });

    const auto &reg = AirwinRegistry::registry;
    entries.resize(reg.size());
    for (size_t i = 0; i < reg.size(); ++i)
    {
        const auto &r = reg[i];
        auto &e = entries[i];
        e.registryIndex = (int)i;
        e.name = fromStd(r.name);
        e.category = fromStd(r.category);
        e.summary = summaryFor(e.name, fromStd(r.whatText));
        e.nParams = r.nParams;
        e.recommended = inCollection(r, "Recommended");
        e.basic = inCollection(r, "Basic");
        e.latest = inCollection(r, "Latest");
        e.nameLower = e.name.toLowerCase();
        e.categoryLower = e.category.toLowerCase();
        e.summaryLower = e.summary.toLowerCase();
    }

    for (const auto &c : AirwinRegistry::categories)
        categoryNames.add(fromStd(c));

    for (const auto &[cat, names] : AirwinRegistry::fxByCategoryChrisOrder)
    {
        auto &list = byCategory[fromStd(cat)];
        for (const auto &n : names)
            list.push_back(AirwinRegistry::nameToIndex.at(n));
    }

    alpha.resize(entries.size());
    for (size_t i = 0; i < entries.size(); ++i)
        alpha[i] = (int)i;
    std::sort(alpha.begin(), alpha.end(), [this](int a, int b) {
        return entries[(size_t)a].name.compareNatural(entries[(size_t)b].name) < 0;
    });
}

const CatalogEntry *Catalog::find(int registryIndex) const
{
    if (registryIndex < 0 || registryIndex >= (int)entries.size())
        return nullptr;
    return &entries[(size_t)registryIndex];
}

int Catalog::indexOf(const juce::String &name) const
{
    auto it = AirwinRegistry::nameToIndex.find(name.toStdString());
    return it == AirwinRegistry::nameToIndex.end() ? -1 : it->second;
}

std::vector<int> Catalog::inCategory(const juce::String &category) const
{
    auto it = byCategory.find(category);
    return it == byCategory.end() ? std::vector<int>{} : it->second;
}

std::vector<int> Catalog::recommended() const
{
    std::vector<int> out;
    for (const auto &[cat, list] : byCategory)
        for (auto i : list)
            if (entries[(size_t)i].recommended)
                out.push_back(i);
    return out;
}

std::vector<int> Catalog::alphabetical() const { return alpha; }

std::vector<int> Catalog::search(const juce::String &query) const
{
    auto tokens = juce::StringArray::fromTokens(query.toLowerCase(), " ", "");
    tokens.trim();
    tokens.removeEmptyStrings();
    if (tokens.isEmpty())
        return alphabetical();

    const auto joined = tokens.joinIntoString("");

    struct Hit
    {
        int index, score;
    };
    std::vector<Hit> hits;

    for (const auto &e : entries)
    {
        int score = 0;
        bool every = true;
        for (const auto &t : tokens)
        {
            int s = 0;
            if (e.nameLower.startsWith(t))
                s = 60;
            else if (e.nameLower.contains(t))
                s = 40;
            else if (startsAWord(e.categoryLower, t))
                s = 24;
            else if (startsAWord(e.summaryLower, t))
                s = 12;
            else if (t.length() > 3 && e.summaryLower.contains(t))
                s = 5;

            if (s == 0)
            {
                every = false;
                break;
            }
            score += s;
        }

        // "to tape" and "totape" should both find ToTape8.
        if (!every)
        {
            if (joined.length() > 1 && e.nameLower.contains(joined))
                score = 40;
            else
                continue;
        }

        if (e.nameLower == joined)
            score += 200;
        if (e.recommended)
            score += 6;
        if (e.latest)
            score += 3;
        hits.push_back({e.registryIndex, score});
    }

    std::stable_sort(hits.begin(), hits.end(), [this](const Hit &a, const Hit &b) {
        if (a.score != b.score)
            return a.score > b.score;
        return entries[(size_t)a.index].name.compareNatural(entries[(size_t)b.index].name) < 0;
    });

    std::vector<int> out;
    out.reserve(hits.size());
    for (const auto &h : hits)
        out.push_back(h.index);
    return out;
}

int Catalog::neighbour(int registryIndex, int direction) const
{
    const auto *e = find(registryIndex);
    if (e == nullptr)
        return registryIndex;
    const auto list = inCategory(e->category);
    auto it = std::find(list.begin(), list.end(), registryIndex);
    if (it == list.end() || list.size() < 2)
        return registryIndex;
    const auto n = (int)list.size();
    const auto pos = ((int)(it - list.begin()) + direction % n + n) % n;
    return list[(size_t)pos];
}

juce::StringArray Catalog::docParagraphs(int registryIndex) const
{
    juce::StringArray paragraphs;
    if (find(registryIndex) == nullptr)
        return paragraphs;

    const auto raw = fromStd(AirwinRegistry::documentationStringFor(registryIndex));
    const auto lines = juce::StringArray::fromLines(raw);

    juce::String current;
    bool first = true;
    for (const auto &line : lines)
    {
        auto t = line.trim();
        if (first)
        {
            first = false;
            if (t.startsWith("#"))
                continue; // repeats the one-liner
        }
        if (t.startsWith("#"))
            t = t.trimCharactersAtStart("# ");
        if (t.isEmpty())
        {
            if (current.isNotEmpty())
                paragraphs.add(current);
            current.clear();
            continue;
        }
        // Chris writes one paragraph per line; consecutive lines are list items.
        current = current.isEmpty() ? t : current + "\n" + t;
    }
    if (current.isNotEmpty())
        paragraphs.add(current);
    return paragraphs;
}
} // namespace awchain
