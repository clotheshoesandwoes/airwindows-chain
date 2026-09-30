#include "ChainProcessor.h"
#include "ChainEditor.h"

#include <cmath>
#include <limits>

namespace awchain
{
namespace
{
constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
constexpr double kFadeSeconds = 0.02;

juce::String slotPrefix(int slot) { return "s" + juce::String(slot); }

juce::String tidyUnit(const juce::String &unit)
{
    const auto lower = unit.toLowerCase();
    if (lower == "hz")
        return "Hz";
    if (lower == "khz")
        return "kHz";
    if (lower == "db")
        return "dB";
    return unit;
}

// Airwindows prints values as "%8.4f". Trim that down to something readable.
juce::String tidyValue(const char *display, const char *label)
{
    auto v = juce::String::fromUTF8(display).trim();
    const auto l = tidyUnit(juce::String::fromUTF8(label).trim());

    const bool numeric = v.isNotEmpty() && v.containsOnly("0123456789.-+eE") &&
                         v.containsAnyOf("0123456789");
    if (numeric)
    {
        const auto d = v.getDoubleValue();
        const auto a = std::abs(d);
        v = a >= 100.0 ? juce::String(juce::roundToInt(d)) : juce::String(d, a >= 10.0 ? 1 : 2);
        if (v.startsWithChar('-') && v.containsOnly("-0."))
            v = v.substring(1);
    }
    return l.isEmpty() ? v : v + " " + l;
}

template <typename Mutex> struct Lock
{
    explicit Lock(Mutex &m) : guard(m) {}
    std::lock_guard<Mutex> guard;
};
} // namespace

//==============================================================================
FxParam::FxParam(ChainProcessor &o, int s, int i)
    : Labelled<juce::AudioParameterFloat>(
          juce::ParameterID(slotPrefix(s) + "p" + juce::String(i), 1),
          "Slot " + juce::String(s + 1) + " control " + juce::String(i + 1),
          juce::NormalisableRange<float>(0.f, 1.f), 0.f),
      slot(s), index(i), owner(o)
{
}

juce::String FxParam::getText(float v, int maximumStringLength) const
{
    return owner.formatValue(slot, index, v).substring(0, maximumStringLength);
}

float FxParam::getValueForText(const juce::String &text) const
{
    if (auto v = owner.parseValue(slot, index, text))
        return juce::jlimit(0.f, 1.f, *v);
    return get();
}

MixParam::MixParam(int s)
    : Labelled<juce::AudioParameterFloat>(juce::ParameterID(slotPrefix(s) + "mix", 1),
                                          "Slot " + juce::String(s + 1) + " mix",
                                          juce::NormalisableRange<float>(0.f, 1.f), 1.f)
{
}

juce::String MixParam::getText(float v, int maximumStringLength) const
{
    return (juce::String(juce::roundToInt(v * 100.f)) + "%").substring(0, maximumStringLength);
}

float MixParam::getValueForText(const juce::String &text) const
{
    return juce::jlimit(0.f, 1.f, text.retainCharacters("0123456789.-").getFloatValue() / 100.f);
}

BypassParam::BypassParam(int s)
    : Labelled<juce::AudioParameterBool>(juce::ParameterID(slotPrefix(s) + "bypass", 1),
                                         "Slot " + juce::String(s + 1) + " bypass", false)
{
}

//==============================================================================
ChainProcessor::SharedSettings::SharedSettings()
{
    if (!persistSettings)
        return;
    juce::PropertiesFile::Options o;
    o.applicationName = "Airwindows Chain";
    o.folderName = "Airwindows Chain";
    o.filenameSuffix = "settings";
    o.osxLibrarySubFolder = "Application Support";
    o.millisecondsBeforeSaving = 0;
    props = std::make_unique<juce::PropertiesFile>(o);
}

ChainProcessor::ChainProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    Catalog::get(); // registry tables must exist before any index is used

    for (int s = 0; s < kMaxSlots; ++s)
    {
        auto &p = params[(size_t)s];
        for (int i = 0; i < kParamsPerSlot; ++i)
        {
            p.fx[(size_t)i] = new FxParam(*this, s, i);
            addParameter(p.fx[(size_t)i]);
        }
        p.mix = new MixParam(s);
        addParameter(p.mix);
        p.bypass = new BypassParam(s);
        addParameter(p.bypass);
    }

    for (auto &d : dsp)
        d.sent.fill(kNaN);

    relabelParameters();
    startTimer(250);
}

ChainProcessor::~ChainProcessor()
{
    stopTimer();

    Command c;
    while (toAudio.pop(c))
        delete c.effect;
    for (auto &o : overflow)
        delete o.effect;
    overflow.clear();

    for (auto &d : dsp)
    {
        delete d.next;
        d.next = nullptr;
    }
    collectGarbage();
}

//==============================================================================
bool ChainProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    auto ok = [](const juce::AudioChannelSet &s) {
        return s == juce::AudioChannelSet::mono() || s == juce::AudioChannelSet::stereo();
    };
    return ok(in) && ok(out) && in.size() <= out.size();
}

void ChainProcessor::prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock)
{
    hostSampleRate = sampleRate;
    audioSampleRate = sampleRate;
    AirwinConsolidatedBase::defaultSampleRate = (float)sampleRate;

    maxChunk = juce::jlimit(256, 8192, maximumExpectedSamplesPerBlock > 0 ? maximumExpectedSamplesPerBlock : 512);
    dryBuffer.setSize(2, maxChunk, false, true, false);
    monoBuffer.setSize(1, maxChunk, false, true, false);

    drainCommands(true);

    for (int s = 0; s < kMaxSlots; ++s)
    {
        auto &d = dsp[(size_t)s];
        if (d.effect)
            d.effect->setSampleRate((float)sampleRate);
        const auto &p = params[(size_t)s];
        const float target = d.effect && !p.bypass->get() ? p.mix->get() : 0.f;
        d.wet.reset(sampleRate, kFadeSeconds);
        d.wet.setCurrentAndTargetValue(target);
    }

    Lock<std::recursive_mutex> l(modelLock);
    for (auto &m : model)
        if (m.display)
            m.display->setSampleRate((float)sampleRate);
}

void ChainProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int n = buffer.getNumSamples();
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear(ch, 0, n);

    drainCommands(false);

    if (n <= 0 || maxChunk <= 0 || numOut < 1 || buffer.getNumChannels() < 1)
        return;

    const bool stereoIn = numIn >= 2 && buffer.getNumChannels() >= 2;
    const bool stereoOut = numOut >= 2 && buffer.getNumChannels() >= 2;

    for (int start = 0; start < n; start += maxChunk)
    {
        const int len = juce::jmin(maxChunk, n - start);
        float *left = buffer.getWritePointer(0, start);
        float *right = stereoOut ? buffer.getWritePointer(1, start) : monoBuffer.getWritePointer(0);
        if (!stereoIn)
            juce::FloatVectorOperations::copy(right, left, len);

        runChain(left, right, len);

        if (!stereoOut)
        {
            juce::FloatVectorOperations::add(left, right, len);
            juce::FloatVectorOperations::multiply(left, 0.5f, len);
        }
    }
}

void ChainProcessor::runChain(float *left, float *right, int n)
{
    float *io[2] = {left, right};
    notePeak(inputMeter, left, right, n);

    for (int k = 0; k < dspOrderLength; ++k)
    {
        const int s = dspOrder[(size_t)k];
        auto &d = dsp[(size_t)s];

        if (d.pending)
        {
            if (!d.effect || (!d.wet.isSmoothing() && d.wet.getCurrentValue() <= 0.f))
                installPending(s);
            else
                d.wet.setTargetValue(0.f); // fade the outgoing effect first
        }

        if (d.effect)
        {
            if (!d.pending)
            {
                const auto &p = params[(size_t)s];
                d.wet.setTargetValue(p.bypass->get() ? 0.f : p.mix->get());
                if (slotGeneration[(size_t)s].load(std::memory_order_acquire) == d.generation)
                    sendParams(s);
            }

            const bool moving = d.wet.isSmoothing();
            const float w = d.wet.getCurrentValue();
            if (!moving && w >= 1.f)
            {
                d.effect->processReplacing(io, io, n);
            }
            else if (moving || w > 0.f)
            {
                auto *dryL = dryBuffer.getWritePointer(0);
                auto *dryR = dryBuffer.getWritePointer(1);
                juce::FloatVectorOperations::copy(dryL, left, n);
                juce::FloatVectorOperations::copy(dryR, right, n);
                d.effect->processReplacing(io, io, n);
                for (int i = 0; i < n; ++i)
                {
                    const float g = d.wet.getNextValue();
                    left[i] = dryL[i] + g * (left[i] - dryL[i]);
                    right[i] = dryR[i] + g * (right[i] - dryR[i]);
                }
            }
        }

        notePeak(s, left, right, n);
    }

    notePeak(outputMeter, left, right, n);
}

void ChainProcessor::notePeak(int meter, const float *left, const float *right, int n)
{
    const auto l = juce::FloatVectorOperations::findMinAndMax(left, n);
    const auto r = juce::FloatVectorOperations::findMinAndMax(right, n);
    const float peak = juce::jmax(std::abs(l.getStart()), std::abs(l.getEnd()), std::abs(r.getStart()),
                                  std::abs(r.getEnd()));
    auto &slot = peaks[(size_t)meter];
    float current = slot.load(std::memory_order_relaxed);
    while (peak > current && !slot.compare_exchange_weak(current, peak, std::memory_order_relaxed))
    {
    }
}

void ChainProcessor::sendParams(int s)
{
    auto &d = dsp[(size_t)s];
    const auto &p = params[(size_t)s];
    for (int i = 0; i < d.nParams; ++i)
    {
        const float v = p.fx[(size_t)i]->get();
        if (v != d.sent[(size_t)i]) // NaN after a swap forces every value through
        {
            d.effect->setParameter(i, v);
            d.sent[(size_t)i] = v;
        }
    }
}

void ChainProcessor::drainCommands(bool audioStopped)
{
    Command c;
    while (toAudio.pop(c))
    {
        if (c.kind == Command::Kind::SetOrder)
        {
            applyOrder(c);
            continue;
        }

        const int s = c.slot;
        if (s < 0 || s >= kMaxSlots)
        {
            trash(c.effect);
            continue;
        }

        auto &d = dsp[(size_t)s];
        if (d.pending)
            trash(d.next); // replaced again before anyone heard it
        d.next = c.effect;
        d.nextParams = c.nParams;
        d.nextGeneration = c.generation;
        d.pending = true;

        const bool silent = !d.effect || !inDspOrder(s) ||
                            (!d.wet.isSmoothing() && d.wet.getCurrentValue() <= 0.f);
        if (audioStopped || silent)
            installPending(s);
    }

    if (audioStopped)
        for (int s = 0; s < kMaxSlots; ++s)
            if (dsp[(size_t)s].pending)
                installPending(s);
}

void ChainProcessor::installPending(int s)
{
    auto &d = dsp[(size_t)s];
    trash(d.effect.release());
    d.effect.reset(d.next);
    d.next = nullptr;
    d.pending = false;
    d.nParams = d.nextParams;
    d.generation = d.nextGeneration;
    d.sent.fill(kNaN);
    if (d.effect)
        d.effect->setSampleRate((float)audioSampleRate);
    d.wet.setCurrentAndTargetValue(0.f); // fades in from dry
}

void ChainProcessor::applyOrder(const Command &c)
{
    std::array<int8_t, kMaxSlots> next{};
    int len = 0;
    for (int k = 0; k < c.orderLength && k < kMaxSlots; ++k)
        next[(size_t)len++] = c.order[(size_t)k];

    // An effect that just left the chain keeps playing in its old place until it
    // has faded out, so removing it doesn't click.
    for (int k = 0; k < dspOrderLength; ++k)
    {
        const int s = dspOrder[(size_t)k];
        bool present = false;
        for (int j = 0; j < len; ++j)
            present = present || next[(size_t)j] == s;
        if (present)
            continue;

        auto &d = dsp[(size_t)s];
        const bool audible = d.effect && (d.wet.isSmoothing() || d.wet.getCurrentValue() > 0.f);
        if (!audible || len >= kMaxSlots)
            continue;

        if (!d.pending)
        {
            d.pending = true;
            d.next = nullptr;
            d.nextParams = 0;
            d.nextGeneration = d.generation;
        }
        const int at = juce::jmin(k, len);
        for (int j = len; j > at; --j)
            next[(size_t)j] = next[(size_t)j - 1];
        next[(size_t)at] = (int8_t)s;
        ++len;
    }

    dspOrder = next;
    dspOrderLength = len;
}

bool ChainProcessor::inDspOrder(int slot) const
{
    for (int k = 0; k < dspOrderLength; ++k)
        if (dspOrder[(size_t)k] == slot)
            return true;
    return false;
}

void ChainProcessor::trash(AirwinConsolidatedBase *effect)
{
    // If the queue is ever full the instance leaks rather than being freed here.
    if (effect != nullptr)
        toTrash.push(effect);
}

void ChainProcessor::collectGarbage()
{
    AirwinConsolidatedBase *effect = nullptr;
    while (toTrash.pop(effect))
        delete effect;
}

void ChainProcessor::timerCallback()
{
    collectGarbage();

    Lock<std::recursive_mutex> l(modelLock);
    while (!overflow.empty() && toAudio.push(overflow.front()))
        overflow.erase(overflow.begin());
}

void ChainProcessor::pushCommandLocked(const Command &c)
{
    if (!overflow.empty() || !toAudio.push(c))
        overflow.push_back(c);
}

void ChainProcessor::pushOrderLocked()
{
    Command c;
    c.kind = Command::Kind::SetOrder;
    c.orderLength = (uint8_t)juce::jmin((int)order.size(), kMaxSlots);
    for (size_t k = 0; k < c.orderLength; ++k)
        c.order[k] = (int8_t)order[k];
    pushCommandLocked(c);
}

//==============================================================================
ChainProcessor::NewEffect ChainProcessor::makeEffect(int registryIndex) const
{
    NewEffect n;
    if (registryIndex < 0 || registryIndex >= (int)AirwinRegistry::registry.size())
        return n;

    const auto &r = AirwinRegistry::registry[(size_t)registryIndex];
    n.audio = r.generator();
    n.display = r.generator();
    if (!n.audio || !n.display)
        return {};

    const auto sr = (float)hostSampleRate.load();
    n.audio->setSampleRate(sr);
    n.display->setSampleRate(sr);

    n.nParams = juce::jmin(r.nParams, kParamsPerSlot);
    for (int i = 0; i < n.nParams; ++i)
    {
        char txt[256] = {};
        n.display->getParameterName(i, txt);
        n.names[(size_t)i] = juce::String::fromUTF8(txt).trim();
        n.defaults[(size_t)i] = n.display->getParameter(i);
    }
    return n;
}

void ChainProcessor::assignSlot(int slot, int registryIndex, const SlotValues *values)
{
    auto fresh = makeEffect(registryIndex); // allocate before taking the lock
    if (!fresh.audio)
        registryIndex = -1;
    const int nParams = registryIndex >= 0 ? fresh.nParams : 0;

    SlotValues v;
    v.fx = fresh.defaults;
    if (values != nullptr)
    {
        for (size_t i = 0; i < (size_t)kParamsPerSlot; ++i)
            if (!std::isnan(values->fx[i]))
                v.fx[i] = values->fx[i];
        v.mix = values->mix;
        v.bypass = values->bypass;
    }

    std::unique_ptr<AirwinConsolidatedBase> oldDisplay;
    uint32_t generation = 0;
    {
        Lock<std::recursive_mutex> l(modelLock);
        auto &m = model[(size_t)slot];
        oldDisplay = std::move(m.display);
        m.display = std::move(fresh.display);
        m.registryIndex = registryIndex;
        m.nParams = nParams;
        m.names = fresh.names;
        // Until the new effect arrives, the audio thread stops forwarding this
        // slot's parameters, so the outgoing effect never hears the new values.
        generation = ++slotGeneration[(size_t)slot];
    }

    // Host notifications happen outside the lock: some hosts ask for the
    // parameter text from inside setValueNotifyingHost.
    auto &p = params[(size_t)slot];
    for (int i = 0; i < kParamsPerSlot; ++i)
    {
        const bool used = i < nParams;
        p.fx[(size_t)i]->setDefault(used ? fresh.defaults[(size_t)i] : 0.f);
        p.fx[(size_t)i]->setValueNotifyingHost(used ? juce::jlimit(0.f, 1.f, v.fx[(size_t)i]) : 0.f);
    }
    p.mix->setValueNotifyingHost(juce::jlimit(0.f, 1.f, v.mix));
    p.bypass->setValueNotifyingHost(v.bypass ? 1.f : 0.f);

    Lock<std::recursive_mutex> l(modelLock);
    Command c;
    c.kind = Command::Kind::SetEffect;
    c.slot = (int8_t)slot;
    c.effect = fresh.audio.release();
    c.nParams = nParams;
    c.generation = generation;
    pushCommandLocked(c);
}

int ChainProcessor::firstFreeSlotLocked() const
{
    for (int s = 0; s < kMaxSlots; ++s)
        if (model[(size_t)s].registryIndex < 0 &&
            std::find(order.begin(), order.end(), s) == order.end())
            return s;
    return -1;
}

ChainProcessor::SlotValues ChainProcessor::currentValues(int slot) const
{
    SlotValues v;
    const auto &p = params[(size_t)slot];
    for (size_t i = 0; i < (size_t)kParamsPerSlot; ++i)
        v.fx[i] = p.fx[i]->get();
    v.mix = p.mix->get();
    v.bypass = p.bypass->get();
    return v;
}

std::vector<int> ChainProcessor::getOrder() const
{
    Lock<std::recursive_mutex> l(modelLock);
    return order;
}

ChainProcessor::SlotInfo ChainProcessor::getSlotInfo(int slot) const
{
    SlotInfo info;
    if (slot < 0 || slot >= kMaxSlots)
        return info;
    Lock<std::recursive_mutex> l(modelLock);
    const auto &m = model[(size_t)slot];
    info.slot = slot;
    info.registryIndex = m.registryIndex;
    info.nParams = m.nParams;
    info.paramNames = m.names;
    return info;
}

bool ChainProcessor::isFull() const
{
    Lock<std::recursive_mutex> l(modelLock);
    return (int)order.size() >= kMaxSlots;
}

int ChainProcessor::addEffect(int registryIndex, int position)
{
    if (Catalog::get().find(registryIndex) == nullptr)
        return -1;

    int slot = -1;
    {
        Lock<std::recursive_mutex> l(modelLock);
        slot = firstFreeSlotLocked();
        if (slot < 0)
            return -1;
        model[(size_t)slot].registryIndex = registryIndex; // reserve it
    }

    assignSlot(slot, registryIndex, nullptr);

    {
        Lock<std::recursive_mutex> l(modelLock);
        if (model[(size_t)slot].registryIndex < 0)
            return -1; // the effect could not be created
        if (position < 0 || position > (int)order.size())
            position = (int)order.size();
        order.insert(order.begin() + position, slot);
        pushOrderLocked();
    }

    noteRecent(registryIndex);
    chainChanged();
    return slot;
}

void ChainProcessor::replaceEffect(int slot, int registryIndex)
{
    if (slot < 0 || slot >= kMaxSlots || Catalog::get().find(registryIndex) == nullptr)
        return;
    {
        Lock<std::recursive_mutex> l(modelLock);
        if (std::find(order.begin(), order.end(), slot) == order.end())
            return;
    }
    assignSlot(slot, registryIndex, nullptr);
    noteRecent(registryIndex);
    chainChanged();
}

void ChainProcessor::removeSlot(int slot)
{
    {
        Lock<std::recursive_mutex> l(modelLock);
        auto it = std::find(order.begin(), order.end(), slot);
        if (it == order.end())
            return;
        order.erase(it);
    }
    assignSlot(slot, -1, nullptr); // tells the audio thread first, so it can fade out
    {
        Lock<std::recursive_mutex> l(modelLock);
        pushOrderLocked();
    }
    chainChanged();
}

void ChainProcessor::moveSlot(int fromPosition, int toPosition)
{
    {
        Lock<std::recursive_mutex> l(modelLock);
        const int n = (int)order.size();
        if (fromPosition < 0 || fromPosition >= n)
            return;
        toPosition = juce::jlimit(0, n - 1, toPosition);
        if (toPosition == fromPosition)
            return;
        const int s = order[(size_t)fromPosition];
        order.erase(order.begin() + fromPosition);
        order.insert(order.begin() + toPosition, s);
        pushOrderLocked();
    }
    chainChanged();
}

int ChainProcessor::duplicateSlot(int slot)
{
    int registryIndex = -1, position = -1, target = -1;
    {
        Lock<std::recursive_mutex> l(modelLock);
        auto it = std::find(order.begin(), order.end(), slot);
        if (it == order.end())
            return -1;
        position = (int)(it - order.begin());
        registryIndex = model[(size_t)slot].registryIndex;
        target = firstFreeSlotLocked();
        if (target < 0 || registryIndex < 0)
            return -1;
        model[(size_t)target].registryIndex = registryIndex;
    }

    const auto values = currentValues(slot);
    assignSlot(target, registryIndex, &values);
    {
        Lock<std::recursive_mutex> l(modelLock);
        order.insert(order.begin() + position + 1, target);
        pushOrderLocked();
    }
    chainChanged();
    return target;
}

void ChainProcessor::clearChain()
{
    std::vector<int> old;
    {
        Lock<std::recursive_mutex> l(modelLock);
        old.swap(order);
    }
    for (auto s : old)
        assignSlot(s, -1, nullptr);
    {
        Lock<std::recursive_mutex> l(modelLock);
        pushOrderLocked();
    }
    chainChanged();
}

void ChainProcessor::chainChanged()
{
    relabelParameters();
    ++modelVersion;
    updateHostDisplay(juce::AudioProcessor::ChangeDetails().withParameterInfoChanged(true));
}

void ChainProcessor::relabelParameters()
{
    std::array<int, kMaxSlots> position;
    position.fill(-1);
    std::array<int, kMaxSlots> registry{};
    std::array<int, kMaxSlots> counts{};
    std::array<std::array<juce::String, kParamsPerSlot>, kMaxSlots> names;
    {
        Lock<std::recursive_mutex> l(modelLock);
        for (size_t k = 0; k < order.size(); ++k)
            position[(size_t)order[k]] = (int)k;
        for (size_t s = 0; s < (size_t)kMaxSlots; ++s)
        {
            registry[s] = model[s].registryIndex;
            counts[s] = model[s].nParams;
            names[s] = model[s].names;
        }
    }

    for (int s = 0; s < kMaxSlots; ++s)
    {
        auto &p = params[(size_t)s];
        const auto *e = Catalog::get().find(registry[(size_t)s]);
        if (e == nullptr || position[(size_t)s] < 0)
        {
            const auto base = "Unused " + juce::String(s + 1);
            for (int i = 0; i < kParamsPerSlot; ++i)
                p.fx[(size_t)i]->setLabel(base + "-" + juce::String(i + 1));
            p.mix->setLabel(base + " mix");
            p.bypass->setLabel(base + " bypass");
            continue;
        }

        // Numbered by chain position, like the list in the editor.
        const auto base = juce::String(position[(size_t)s] + 1) + ". " + e->name + ": ";
        for (int i = 0; i < kParamsPerSlot; ++i)
            p.fx[(size_t)i]->setLabel(base + (i < counts[(size_t)s] ? names[(size_t)s][(size_t)i]
                                                                    : juce::String("unused")));
        p.mix->setLabel(base + "Mix");
        p.bypass->setLabel(base + "Bypass");
    }
}

//==============================================================================
juce::String ChainProcessor::formatValue(int slot, int index, float value) const
{
    if (slot < 0 || slot >= kMaxSlots || index < 0 || index >= kParamsPerSlot)
        return "-";

    Lock<std::recursive_mutex> l(modelLock);
    const auto &m = model[(size_t)slot];
    if (!m.display || index >= m.nParams)
        return "-";

    // Some effects format one control using the others, so load them all.
    const auto &p = params[(size_t)slot];
    for (int i = 0; i < m.nParams; ++i)
        m.display->setParameter(i, i == index ? value : p.fx[(size_t)i]->get());

    char display[256] = {}, label[256] = {};
    m.display->getParameterDisplay(index, display);
    m.display->getParameterLabel(index, label);
    return tidyValue(display, label);
}

std::optional<float> ChainProcessor::parseValue(int slot, int index, const juce::String &text) const
{
    if (slot < 0 || slot >= kMaxSlots || index < 0 || index >= kParamsPerSlot)
        return std::nullopt;

    Lock<std::recursive_mutex> l(modelLock);
    const auto &m = model[(size_t)slot];
    if (!m.display || index >= m.nParams)
        return std::nullopt;

    const auto trimmed = text.trim();
    for (const auto &candidate : {trimmed, trimmed.upToFirstOccurrenceOf(" ", false, false)})
    {
        float v = 0.f;
        if (m.display->canConvertParameterTextToValue(index) &&
            m.display->parameterTextToValue(index, candidate.toRawUTF8(), v) && std::isfinite(v))
            return v;
    }
    if (trimmed.containsOnly("0123456789.-") && trimmed.containsAnyOf("0123456789"))
        return trimmed.getFloatValue();
    return std::nullopt;
}

//==============================================================================
juce::String ChainProcessor::getChainName() const
{
    Lock<std::recursive_mutex> l(modelLock);
    return chainName;
}

void ChainProcessor::setChainName(const juce::String &name)
{
    {
        Lock<std::recursive_mutex> l(modelLock);
        chainName = name;
    }
    ++modelVersion;
}

std::unique_ptr<juce::XmlElement> ChainProcessor::chainToXml() const
{
    auto xml = std::make_unique<juce::XmlElement>("AirwindowsChain");
    xml->setAttribute("version", 1);

    std::vector<int> ord;
    std::array<int, kMaxSlots> registry{};
    std::array<int, kMaxSlots> counts{};
    {
        Lock<std::recursive_mutex> l(modelLock);
        ord = order;
        for (size_t s = 0; s < (size_t)kMaxSlots; ++s)
        {
            registry[s] = model[s].registryIndex;
            counts[s] = model[s].nParams;
        }
        xml->setAttribute("name", chainName);
    }

    for (auto s : ord)
    {
        const auto *e = Catalog::get().find(registry[(size_t)s]);
        if (e == nullptr)
            continue;
        const auto &p = params[(size_t)s];
        auto *x = xml->createNewChildElement("Slot");
        x->setAttribute("id", s);
        x->setAttribute("effect", e->name);
        x->setAttribute("mix", (double)p.mix->get());
        x->setAttribute("bypass", p.bypass->get() ? 1 : 0);
        for (int i = 0; i < counts[(size_t)s]; ++i)
            x->setAttribute("p" + juce::String(i), (double)p.fx[(size_t)i]->get());
    }
    return xml;
}

void ChainProcessor::chainFromXml(const juce::XmlElement &xml)
{
    if (!xml.hasTagName("AirwindowsChain"))
        return;

    struct Wanted
    {
        int slot{-1}, registryIndex{-1};
        SlotValues values;
    };
    std::vector<Wanted> wanted;
    std::array<bool, kMaxSlots> taken{};

    for (auto *x : xml.getChildWithTagNameIterator("Slot"))
    {
        if ((int)wanted.size() >= kMaxSlots)
            break;
        const int reg = Catalog::get().indexOf(x->getStringAttribute("effect"));
        if (reg < 0)
            continue; // not in this build of the registry

        Wanted w;
        w.registryIndex = reg;
        w.slot = x->getIntAttribute("id", -1);
        if (w.slot < 0 || w.slot >= kMaxSlots || taken[(size_t)w.slot])
            w.slot = -1;
        else
            taken[(size_t)w.slot] = true;

        for (int i = 0; i < kParamsPerSlot; ++i)
        {
            const auto key = "p" + juce::String(i);
            w.values.fx[(size_t)i] = x->hasAttribute(key) ? (float)x->getDoubleAttribute(key) : kNaN;
        }
        w.values.mix = (float)x->getDoubleAttribute("mix", 1.0);
        w.values.bypass = x->getIntAttribute("bypass", 0) != 0;
        wanted.push_back(w);
    }

    for (auto &w : wanted)
        if (w.slot < 0)
            for (int s = 0; s < kMaxSlots; ++s)
                if (!taken[(size_t)s])
                {
                    w.slot = s;
                    taken[(size_t)s] = true;
                    break;
                }

    std::vector<int> old;
    {
        Lock<std::recursive_mutex> l(modelLock);
        old.swap(order);
    }
    for (auto s : old)
        if (!taken[(size_t)s])
            assignSlot(s, -1, nullptr);
    for (const auto &w : wanted)
        assignSlot(w.slot, w.registryIndex, &w.values);

    {
        Lock<std::recursive_mutex> l(modelLock);
        order.clear();
        for (const auto &w : wanted)
            order.push_back(w.slot);
        chainName = xml.getStringAttribute("name");
        pushOrderLocked();
    }
    chainChanged();
}

void ChainProcessor::getStateInformation(juce::MemoryBlock &dest)
{
    auto xml = chainToXml();
    xml->setAttribute("editorWidth", editorSize.x);
    xml->setAttribute("editorHeight", editorSize.y);
    copyXmlToBinary(*xml, dest);
}

void ChainProcessor::setStateInformation(const void *data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
    {
        editorSize = {juce::jlimit(760, 2400, xml->getIntAttribute("editorWidth", editorSize.x)),
                      juce::jlimit(500, 1600, xml->getIntAttribute("editorHeight", editorSize.y))};
        chainFromXml(*xml);
    }
}

juce::File ChainProcessor::chainsFolder()
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("Airwindows Chain");
}

bool ChainProcessor::saveChain(const juce::File &file)
{
    setChainName(file.getFileNameWithoutExtension());
    auto xml = chainToXml();
    file.getParentDirectory().createDirectory();
    return xml->writeTo(file);
}

bool ChainProcessor::loadChain(const juce::File &file)
{
    auto xml = juce::parseXML(file);
    if (xml == nullptr || !xml->hasTagName("AirwindowsChain"))
        return false;
    chainFromXml(*xml);
    setChainName(file.getFileNameWithoutExtension());
    return true;
}

//==============================================================================
void ChainProcessor::noteRecent(int registryIndex)
{
    const auto *e = Catalog::get().find(registryIndex);
    if (e == nullptr || settings->props == nullptr)
        return;
    const juce::ScopedLock sl(settings->lock);
    auto list = juce::StringArray::fromLines(settings->props->getValue("recent"));
    list.removeEmptyStrings();
    list.removeString(e->name);
    list.insert(0, e->name);
    while (list.size() > 12)
        list.remove(list.size() - 1);
    settings->props->setValue("recent", list.joinIntoString("\n"));
}

juce::StringArray ChainProcessor::getRecentEffects() const
{
    if (settings->props == nullptr)
        return {};
    const juce::ScopedLock sl(settings->lock);
    auto list = juce::StringArray::fromLines(settings->props->getValue("recent"));
    list.removeEmptyStrings();
    return list;
}

juce::AudioProcessorEditor *ChainProcessor::createEditor() { return new ChainEditor(*this); }
} // namespace awchain
