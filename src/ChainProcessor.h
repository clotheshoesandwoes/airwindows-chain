#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <mutex>
#include <optional>

#include "AirwinRegistry.h"
#include "Catalog.h"
#include "Fifo.h"
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

    // Tests turn this off so they don't write into the real settings file.
    inline static bool persistSettings = true;

    // Editor size survives closing the window and reloading the project.
    juce::Point<int> editorSize{940, 620};

    // Peak level after each slot since the last read, plus the chain's input and
    // output. Reading resets it. Levels are linear, 1.0 = full scale.
    static constexpr int inputMeter = kMaxSlots, outputMeter = kMaxSlots + 1;
    float takePeak(int meter) { return peaks[(size_t)meter].exchange(0.f); }

    // Frees effect instances the audio thread has let go of. Runs on a timer;
    // public so tests can call it directly.
    void collectGarbage();

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
    };

    struct NewEffect
    {
        std::unique_ptr<AirwinConsolidatedBase> audio, display;
        int nParams{0};
        std::array<float, kParamsPerSlot> defaults{};
        std::array<juce::String, kParamsPerSlot> names;
    };
    NewEffect makeEffect(int registryIndex) const;

    // Puts an effect (or nothing, for registryIndex -1) into a slot and tells
    // the audio thread. values == nullptr means the effect's own defaults.
    void assignSlot(int slot, int registryIndex, const SlotValues *values);
    int firstFreeSlotLocked() const;
    void pushOrderLocked();
    void chainChanged(); // relabel parameters, bump the version, tell the host
    void relabelParameters();
    SlotValues currentValues(int slot) const;
    void noteRecent(int registryIndex);

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

  public:
    // Recent effects, shared by every instance in the session.
    struct SharedSettings
    {
        SharedSettings();
        std::unique_ptr<juce::PropertiesFile> props;
        juce::CriticalSection lock;
    };

  private:
    juce::SharedResourcePointer<SharedSettings> settings;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChainProcessor)
};
} // namespace awchain
