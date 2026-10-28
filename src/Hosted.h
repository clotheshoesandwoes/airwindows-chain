#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <map>
#include <optional>

#include "AirwinRegistry.h"
#include "Catalog.h"

namespace awchain
{
// Effect ids from here up are VST3 plugins found on this computer; below it
// they are Airwindows registry indices.
constexpr int kHostedBase = 1 << 20;
inline bool isHosted(int id) { return id >= kHostedBase; }

// The VST3 effects installed on this computer, shared by every instance in the
// session. Call from the message thread. The list is kept between sessions, so
// a scan only has to happen once, and again when plugins are installed. The
// scan itself runs on its own thread so the host stays usable while a slow
// plugin loads.
class HostedPlugins : public juce::ChangeBroadcaster, private juce::Timer, private juce::AsyncUpdater
{
  public:
    HostedPlugins();
    ~HostedPlugins() override;

    std::vector<int> effects() const; // ids, by name
    std::vector<int> search(const juce::String &query) const;
    const juce::PluginDescription *find(int id) const;
    int idFor(const juce::PluginDescription &); // lists it if this computer never has

    bool hasScanned() const { return scanned; }
    bool isScanning() const { return scan != nullptr; }
    juce::String nowScanning() const; // the file being read
    float progress() const;           // 0 to 1
    void startScan(bool retrySkipped = false);
    void stopScan(); // keeps what was found

    // Files that crashed or hung while being read, or held nothing loadable.
    // They are passed over until a scan is started with retrySkipped.
    juce::StringArray skipped() const { return known.getBlacklistedFiles(); }

    // Folders scanned besides the standard VST3 folder.
    juce::StringArray extraFolders() const { return extra; }
    void addFolder(const juce::File &);
    void removeFolder(const juce::String &path);
    static juce::String standardFolder();

    // The names of a plugin's controls, read once by the scanner helper and
    // kept. Nothing while they are being read; a change message says when they
    // arrive. Empty when the plugin couldn't be read.
    std::optional<juce::StringArray> controlsOf(int id);

    // Manuals the plugin's maker installed with it (PDF or Windows help), best
    // first. Empty while the maker's folders are being looked through; a change
    // message says when that's done.
    juce::Array<juce::File> manualsFor(int id);

    std::unique_ptr<juce::AudioPluginInstance> create(int id, double sampleRate, int blockSize,
                                                      juce::String &error);

    // Tests turn this off so they don't read or write the real plugin list.
    inline static bool persist = true;

  private:
    class Scan;

    juce::AudioPluginFormatManager formats;
    juce::KnownPluginList known; // locks itself, so the scan thread can add to it
    std::vector<juce::PluginDescription> listed; // id = kHostedBase + position; only grows
    std::unique_ptr<Scan> scan;
    std::vector<std::unique_ptr<Scan>> stopping; // stopped scans still finishing a file
    juce::StringArray extra;
    bool scanned{false};

    // Read on the pool's threads; guarded by readLock.
    juce::CriticalSection readLock;
    std::map<juce::String, juce::StringArray> controlNames; // by identifier string
    std::optional<juce::PluginDescription> controlsWanted;  // the latest asked for
    juce::String controlsReading; // identifier string of the one being read now
    bool readingControls{false};
    std::map<juce::String, juce::Array<juce::File>> docsByMaker; // every manual under a maker's folders
    juce::StringArray makersBeingIndexed;

    static juce::File folder();
    static juce::File scannerHelper();
    void absorbKnown();
    void save();
    void timerCallback() override;
    void handleAsyncUpdate() override;
    void readControls();
    void indexManuals(const juce::String &makerKey, const juce::Array<juce::File> &bundles);

    juce::ThreadPool pool{juce::ThreadPoolOptions{}.withThreadName("Airwindows Chain reader").withNumberOfThreads(2)};
};

// Runs a hosted plugin where an Airwindows effect would sit in the chain.
// prepare() is called on the message thread while the audio thread is not using
// this instance: before it is handed over, and from the chain's prepareToPlay.
class HostedEffect : public AirwinConsolidatedBase
{
  public:
    explicit HostedEffect(std::unique_ptr<juce::AudioPluginInstance>);
    ~HostedEffect() override;

    juce::AudioPluginInstance &plugin() { return *instance; }
    void prepare(double sampleRate, int blockSize);

    // The plugin's first automatable controls stand in for a slot's controls,
    // so the chain's sliders and the host's automation reach them.
    int numControls() const { return (int)controls.size(); }
    juce::AudioProcessorParameter &control(int i) const { return *controls[(size_t)i]; }

    // The plugin's latency, and the dry signal held back by it so that a mix of
    // dry and wet lines up. Audio thread; follows the plugin from block to block.
    int latency() const { return juce::jmax(0, lag); }
    void followLatency(); // call before each block
    static constexpr int kMaxLag = 1 << 16;
    void delayDry(float *left, float *right, int n);

    void processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) override;
    void processDoubleReplacing(double **, double **, VstInt32) override {}
    float getParameter(VstInt32 i) override { return controls[(size_t)i]->getValue(); }
    void setParameter(VstInt32 i, float v) override { controls[(size_t)i]->setValue(v); }
    void getParameterLabel(VstInt32, char *text) override { text[0] = 0; }
    void getParameterName(VstInt32 i, char *text) override;
    void getParameterDisplay(VstInt32, char *text) override { text[0] = 0; }

  private:
    std::unique_ptr<juce::AudioPluginInstance> instance;
    std::vector<juce::AudioProcessorParameter *> controls;
    juce::AudioBuffer<float> scratch, dryLine;
    int lag{-1}, dryPos{0};
    juce::MidiBuffer midi;
    int blockSize{0};
    bool prepared{false};
};
} // namespace awchain
