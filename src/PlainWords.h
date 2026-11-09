#pragma once

#include "AwChainAssets.h"
#include <juce_core/juce_core.h>

namespace awchain
{
// Beginner names for Chris's control names, one effect at a time, written from
// the Airwindopedia (resources/plain-words.json). The same name means different
// things in different effects: Regen is "Repeats" in a delay, "Tail length" in a
// reverb. Shown when the "Control names" setting is "Plain words".
class PlainWords
{
  public:
    static const PlainWords &get()
    {
        static const PlainWords instance;
        return instance;
    }

    // The plain label, or the original when there's none.
    juce::String label(const juce::String &effect, const juce::String &control) const
    {
        const auto e = entry(effect, control);
        return e.isArray() && e.size() > 0 ? e[0].toString() : control;
    }

    // One sentence on what turning it up does, or empty.
    juce::String hint(const juce::String &effect, const juce::String &control) const
    {
        const auto e = entry(effect, control);
        return e.isArray() && e.size() > 1 ? e[1].toString() : juce::String();
    }

  private:
    PlainWords()
    {
        words = juce::JSON::parse(juce::String::fromUTF8(AwChainAssets::plainwords_json,
                                                         AwChainAssets::plainwords_jsonSize));
    }

    juce::var entry(const juce::String &effect, const juce::String &control) const
    {
        return words[juce::Identifier(effect)][juce::Identifier(control)];
    }

    juce::var words;
};
} // namespace awchain
