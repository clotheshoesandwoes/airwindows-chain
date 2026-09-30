#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace awchain
{
class ChainProcessor;

// The host sees a fixed block of parameters per slot. Their names follow
// whatever effect currently sits in the slot.
template <typename Base> class Labelled : public Base
{
  public:
    using Base::Base;

    juce::String getName(int maximumStringLength) const override
    {
        const juce::SpinLock::ScopedLockType l(labelLock);
        if (label.isEmpty())
            return Base::getName(maximumStringLength);
        return label.substring(0, maximumStringLength);
    }

    void setLabel(const juce::String &s)
    {
        const juce::SpinLock::ScopedLockType l(labelLock);
        label = s;
    }

  private:
    mutable juce::SpinLock labelLock;
    juce::String label;
};

class FxParam : public Labelled<juce::AudioParameterFloat>
{
  public:
    FxParam(ChainProcessor &owner, int slot, int index);

    juce::String getText(float value, int maximumStringLength) const override;
    float getValueForText(const juce::String &text) const override;
    float getDefaultValue() const override { return defaultValue.load(); }
    void setDefault(float v) { defaultValue.store(v); }

    const int slot, index;

  private:
    ChainProcessor &owner;
    std::atomic<float> defaultValue{0.5f};
};

class MixParam : public Labelled<juce::AudioParameterFloat>
{
  public:
    explicit MixParam(int slot);
    juce::String getText(float value, int maximumStringLength) const override;
    float getValueForText(const juce::String &text) const override;
};

class BypassParam : public Labelled<juce::AudioParameterBool>
{
  public:
    explicit BypassParam(int slot);
};
} // namespace awchain
