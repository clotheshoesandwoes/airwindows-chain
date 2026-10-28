#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <mutex>
#include <optional>

#include "AirwinRegistry.h"
#include "Catalog.h"
#include "Fifo.h"
#include "Hosted.h"
#include "Loudness.h"
#include "Params.h"

namespace awchain
{
constexpr int kMaxSlots = 16;

// A chain of Airwindows effects in one plugin.
//
// Threads: the chain model (which effect sits in which slot, and the order) lives
// on the message thread under modelLock. The audio thread owns its own effect
// instances and learns about changes through a lock-free queue; instances it is
// done with come back through a second queue and are freed on the message thread.
// Nothing is allocated or freed on the audio thread.
class ChainProcessor : public juce::AudioProcessor, private juce::Timer
{
  public:
    ChainProcessor();
    ~ChainProcessor() override;

    //==========================================================================
    const juce::String getName() const override { return "Airwindows Chain"; }
    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout &) const override;
    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override { return true; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; } // the reverbs run long
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String &) override {}

    void getStateInformation(juce::MemoryBlock &) override;
    void setStateInformation(const void *, int) override;

    //==========================================================================
    // The chain. Call these from the message thread.
    struct SlotInfo
    {
        int slot{-1};
        int registryIndex{-1};
        int nParams{0};
        std::array<juce::String, kParamsPerSlot> paramNames;
    };

    // What the UI calls an effect, Airwindows or a plugin from this computer.
    struct EffectLabel
    {
        juce::String name, category, summary;
        bool hosted{false};
        bool missing{false}; // saved in the chain, but can't load on this computer
    };
    std::optional<EffectLabel> effectLabel(int id) const;
    std::optional<EffectLabel> slotLabel(int slot) const; // also names a missing effect
    HostedPlugins &hostedPlugins() { return *hosted; }

    // A plugin from this computer edits in its own window.
    void showPluginWindow(int slot);
    void closePluginWindows();

    std::vector<int> getOrder() const; // slot ids in processing order
    SlotInfo getSlotInfo(int slot) const;
    bool isFull() const;

    int addEffect(int registryIndex, int position = -1); // the new slot, or -1 when full
    void replaceEffect(int slot, int registryIndex);
    void removeSlot(int slot);
    void moveSlot(int fromPosition, int toPosition);
    int duplicateSlot(int slot);
    void clearChain();

    FxParam &fxParam(int slot, int index) { return *params[(size_t)slot].fx[(size_t)index]; }
    MixParam &mixParam(int slot) { return *params[(size_t)slot].mix; }
    BypassParam &bypassParam(int slot) { return *params[(size_t)slot].bypass; }

    juce::String formatValue(int slot, int index, float value) const;
    std::optional<float> parseValue(int slot, int index, const juce::String &text) const;

    // Bumped whenever the chain changes shape; the editor polls it.
    uint32_t getModelVersion() const { return modelVersion.load(); }

    juce::String getChainName() const;
    void setChainName(const juce::String &);

    std::unique_ptr<juce::XmlElement> chainToXml() const;
    void chainFromXml(const juce::XmlElement &);

    static juce::File chainsFolder();
    bool saveChain(const juce::File &);
    bool loadChain(const juce::File &);

    juce::StringArray getRecentEffects() const;

    // Undo for chain edits, message thread. Loading a project starts fresh.
    bool canUndo() const;
    juce::String undoLabel() const; // e.g. "remove Density2"
    void undo();

    // Preferences shared by every instance in the session: theme, favourites.
    juce::String getSetting(const juce::String &key, const juce::String &fallback) const;
    void setSetting(const juce::String &key, const juce::String &value);
    juce::StringArray getFavourites() const;
    bool isFavourite(const juce::String &effectName) const;
    void setFavourite(const juce::String &effectName, bool favourite);

    // Tests turn this off so they don't write into the real settings file;
    // settings then live in memory only.
    inline static bool persistSettings = true;

    // Editor size survives closing the window and reloading the project.
    juce::Point<int> editorSize{940, 620};

    // Peak level after each slot since the last read, plus the chain's input and
    // output. Reading resets it. Levels are linear, 1.0 = full scale.
    static constexpr int inputMeter = kMaxSlots, outputMeter = kMaxSlots + 1;
    float takePeak(int meter) { return peaks[(size_t)meter].exchange(0.f); }

    // Compare: hear the original instead of the chain, turned up or down to the
    // chain's loudness, so the two differ in sound and not in volume. Not saved.
    void setComparing(bool on) { comparing = on; }
    bool isComparing() const { return comparing.load(); }

    // How much louder the chain is than what comes in, in dB, measured as LUFS
    // meters measure over the last few seconds. NaN until there's been signal.
    float loudnessDifference() const { return loudnessDiff.load(); }

    // Frees effect instances the audio thread has let go of. Runs on a timer;
    // public so tests can call it directly.
    void collectGarbage();

    // Tells the host when a plugin in the chain changes its latency (lookahead
    // switched on, a preset loaded). Runs on the timer; public for tests.
    void followLatency();

  private:
    //==========================================================================
    struct SlotParams
    {
        std::array<FxParam *, kParamsPerSlot> fx{};
        MixParam *mix{nullptr};
        BypassParam *bypass{nullptr};
    };
    std::array<SlotParams, kMaxSlots> params;

    // Message-thread model
    struct SlotModel
    {
        int registryIndex{-1};
        std::unique_ptr<AirwinConsolidatedBase> display; // formats values for the UI and host
        int nParams{0};
        std::array<juce::String, kParamsPerSlot> names;
        HostedEffect *hosted{nullptr}; // owned by the audio thread; never outlives it here
        std::unique_ptr<juce::DocumentWindow> window;
        // A saved slot whose effect can't load here (a plugin not installed or
        // not licensed, an effect this build doesn't have). Passes audio
        // through, and is saved again exactly as it came, so opening a project
        // on another computer never loses it.
        std::unique_ptr<juce::XmlElement> missing;
    };
    mutable std::recursive_mutex modelLock;
    std::array<SlotModel, kMaxSlots> model;
    std::vector<int> order;
    juce::String chainName;
    std::atomic<uint32_t> modelVersion{1};

    // Values to put in a slot's parameters when an effect goes in.
    struct SlotValues
    {
        std::array<float, kParamsPerSlot> fx{};
        float mix{1.f};
        bool bypass{false};
        juce::MemoryBlock pluginState; // a hosted plugin's own state
    };

    struct NewEffect
    {
        std::unique_ptr<AirwinConsolidatedBase> audio, display;
        HostedEffect *hosted{nullptr}; // the same object as audio, when it is a plugin
        int nParams{0};
        std::array<float, kParamsPerSlot> defaults{};
        std::array<juce::String, kParamsPerSlot> names;
    };
    NewEffect makeEffect(int registryIndex) const;
    juce::SharedResourcePointer<HostedPlugins> hosted;

    // Puts an effect (or nothing, for registryIndex -1) into a slot and tells
    // the audio thread. values == nullptr means the effect's own defaults.
    void assignSlot(int slot, int registryIndex, const SlotValues *values);
    int firstFreeSlotLocked() const;
    void pushOrderLocked();
    void chainChanged(); // relabel parameters, bump the version, tell the host
    void relabelParameters();
    SlotValues currentValues(int slot) const;
    void noteRecent(int registryIndex);

    struct UndoEntry
    {
        juce::String label;
        std::unique_ptr<juce::XmlElement> state;
    };
    mutable std::mutex undoLock;
    std::vector<UndoEntry> undoStack;
    void rememberForUndo(const juce::String &label);
    juce::String nameOf(int slot) const;

    //==========================================================================
    // Audio thread
    struct Command
    {
        enum class Kind : uint8_t
        {
            SetEffect,
            SetOrder
        };
        Kind kind{Kind::SetEffect};
        int8_t slot{-1};
        uint8_t orderLength{0};
        std::array<int8_t, kMaxSlots> order{};
        AirwinConsolidatedBase *effect{nullptr};
        int nParams{0};
        uint32_t generation{0};
    };

    struct SlotDsp
    {
        std::unique_ptr<AirwinConsolidatedBase> effect;
        int nParams{0};
        uint32_t generation{0};
        std::array<float, kParamsPerSlot> sent{};
        juce::SmoothedValue<float> wet;

        // A replacement waiting for the current effect to fade out.
        bool pending{false};
        AirwinConsolidatedBase *next{nullptr};
        int nextParams{0};
        uint32_t nextGeneration{0};
    };

    std::array<SlotDsp, kMaxSlots> dsp;
    std::array<int8_t, kMaxSlots> dspOrder{};
    int dspOrderLength{0};
    std::array<std::atomic<uint32_t>, kMaxSlots> slotGeneration{};
    std::array<std::atomic<float>, kMaxSlots + 2> peaks{};
    void notePeak(int meter, const float *left, const float *right, int n);

    juce::AudioBuffer<float> dryBuffer, monoBuffer;
    int maxChunk{0};

    // Compare. The original is held back by the chain's latency so it lines up.
    static constexpr int kOriginalLength = 1 << 17;
    juce::AudioBuffer<float> originalLine;
    int originalWrite{0};
    Loudness inputLoudness, outputLoudness;
    float matchGain{1.f};
    juce::SmoothedValue<float> compareMix;
    std::atomic<bool> comparing{false};
    std::atomic<float> loudnessDiff{std::numeric_limits<float>::quiet_NaN()};
    std::atomic<int> chainLatency{0};
    void keepOriginal(const float *left, const float *right, int n);
    void compareWithOriginal(float *left, float *right, int n);
    double audioSampleRate{48000.0};
    std::atomic<double> hostSampleRate{48000.0};

    SpscQueue<Command, 4096> toAudio;
    SpscQueue<AirwinConsolidatedBase *, 4096> toTrash;
    std::vector<Command> overflow; // only if toAudio was ever full; retried on the timer

    void pushCommandLocked(const Command &);
    void drainCommands(bool audioStopped);
    void installPending(int slot);
    void applyOrder(const Command &);
    void sendParams(int slot);
    void runChain(float *left, float *right, int numSamples);
    void trash(AirwinConsolidatedBase *);
    bool inDspOrder(int slot) const;

    void timerCallback() override;
    // Following a hosted plugin's own window: the slider value last seen to agree with it.
    std::array<std::array<float, kParamsPerSlot>, kMaxSlots> lastSynced{};
    std::array<uint32_t, kMaxSlots> dspGenerationSeen{};

  public:
    // Recent effects, shared by every instance in the session.
    struct SharedSettings
    {
        SharedSettings();
        std::unique_ptr<juce::PropertiesFile> props;
        juce::StringPairArray memory; // when nothing is persisted
        juce::CriticalSection lock;
        juce::String get(const juce::String &key, const juce::String &fallback);
        void set(const juce::String &key, const juce::String &value);
    };

  private:
    juce::SharedResourcePointer<SharedSettings> settings;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChainProcessor)
};
} // namespace awchain
