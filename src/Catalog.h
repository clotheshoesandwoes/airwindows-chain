#pragma once

#include <juce_core/juce_core.h>
#include <map>
#include <vector>

namespace awchain
{
// Consolidated shows at most ten controls per effect; the handful of effects with
// more (the ConsoleX strips, CStrip, Pafnuty) are left out, same as Consolidated.
constexpr int kParamsPerSlot = 10;

struct CatalogEntry
{
    int registryIndex{-1};
    juce::String name, category;
    juce::String summary; // Chris's one-liner, without the leading "Name is"
    int nParams{0};
    bool recommended{false}, basic{false}, latest{false};

    juce::String nameLower, categoryLower, summaryLower;
};

// Read-only view of every effect in the Airwindows registry. Built once, safe to
// read from any thread afterwards.
class Catalog
{
  public:
    static const Catalog &get();

    const std::vector<CatalogEntry> &all() const { return entries; }
    const CatalogEntry *find(int registryIndex) const;
    int indexOf(const juce::String &name) const; // registry index, or -1

    const juce::StringArray &categories() const { return categoryNames; }
    std::vector<int> inCategory(const juce::String &category) const; // Chris's order
    std::vector<int> recommended() const;                            // by category, Chris's order
    std::vector<int> alphabetical() const;
    std::vector<int> search(const juce::String &query) const;

    // Next or previous effect in the same category, Chris's order, wrapping.
    int neighbour(int registryIndex, int direction) const;

    // The Airwindopedia entry as paragraphs, heading line removed.
    juce::StringArray docParagraphs(int registryIndex) const;

  private:
    Catalog();

    std::vector<CatalogEntry> entries; // indexed by registry index
    juce::StringArray categoryNames;
    std::map<juce::String, std::vector<int>> byCategory;
    std::vector<int> alpha;
};
} // namespace awchain
