#include "Hosted.h"

namespace awchain
{
//==============================================================================
// One pass over the plugin folders, a file at a time, on its own thread. Each
// file is read by the scanner helper in a process of its own: a plugin that
// crashes or never returns while being read is killed and skipped, and the
// host never notices. Without the helper, files are read here, as a fallback.
class HostedPlugins::Scan : public juce::Thread
{
  public:
    Scan(juce::KnownPluginList &k, juce::AudioPluginFormat &f, const juce::FileSearchPath &paths,
         const juce::File &scannerHelper)
        : juce::Thread("Airwindows Chain plugin scan"), known(k), format(f), helper(scannerHelper)
    {
        for (const auto &file : format.searchPathsForPlugins(paths, true))
            if (!file.contains("Airwindows Chain.vst3"))
                files.add(file);
    }

    ~Scan() override { stopThread(10000); }

    void run() override
    {
        for (int i = 0; i < files.size() && !threadShouldExit(); ++i)
        {
            const auto file = files[i];
            {
                const juce::ScopedLock l(lock);
                current = file;
            }
            progress = (float)i / (float)files.size();
            if (known.isListingUpToDate(file, format) || known.getBlacklistedFiles().contains(file))
                continue;
            if (helper.existsAsFile())
                readElsewhere(file);
            else
                readHere(file);
        }
        progress = 1.f;
    }

    juce::String file() const
    {
        const juce::ScopedLock l(lock);
        return current;
    }

    std::atomic<float> progress{0.f};

  private:
    static constexpr juce::uint32 kPatienceMs = 90000; // a licence prompt can take a while

    juce::KnownPluginList &known;
    juce::AudioPluginFormat &format;
    const juce::File helper;
    juce::StringArray files;
    juce::CriticalSection lock;
    juce::String current;

    void add(const juce::String &file, juce::PluginDescription d)
    {
        // So the next scan knows this file hasn't changed and skips it.
        d.lastFileModTime = juce::File(file).getLastModificationTime();
        d.lastInfoUpdateTime = juce::Time::getCurrentTime();
        known.addType(d);
    }

    void readElsewhere(const juce::String &file)
    {
        const auto out = juce::File::createTempFile(".xml");
        juce::ChildProcess child;
        if (!child.start(juce::StringArray{helper.getFullPathName(), file, out.getFullPathName()}, 0))
        {
            readHere(file);
            return;
        }

        // Short waits, so Stop is answered at once.
        const auto deadline = juce::Time::getMillisecondCounter() + kPatienceMs;
        while (child.isRunning())
        {
            if (threadShouldExit() || juce::Time::getMillisecondCounter() > deadline)
            {
                child.kill();
                out.deleteFile();
                if (!threadShouldExit())
                    known.addToBlacklist(file); // hung
                return;
            }
            child.waitForProcessToFinish(100);
        }

        auto xml = child.getExitCode() == 0 ? juce::parseXML(out) : nullptr;
        out.deleteFile();
        int added = 0;
        if (xml != nullptr)
            for (auto *e : xml->getChildIterator())
            {
                juce::PluginDescription d;
                if (d.loadFromXml(*e))
                {
                    add(file, d);
                    ++added;
                }
            }
        if (added == 0)
            known.addToBlacklist(file); // crashed, or held nothing it could load
    }

    void readHere(const juce::String &file)
    {
        juce::OwnedArray<juce::PluginDescription> found;
        format.findAllTypesForFile(found, file);
        for (auto *d : found)
            add(file, *d);
        if (found.isEmpty())
            known.addToBlacklist(file);
    }
};

//==============================================================================
HostedPlugins::HostedPlugins()
{
    formats.addFormat(std::make_unique<juce::VST3PluginFormat>());
    if (persist)
    {
        if (auto xml = juce::parseXML(folder().getChildFile("Plugins.xml")))
        {
            known.recreateFromXml(*xml);
            scanned = true;
        }
        extra = juce::StringArray::fromLines(folder().getChildFile("Folders.txt").loadFileAsString());
        extra.removeEmptyStrings();
        if (auto xml = juce::parseXML(folder().getChildFile("Controls.xml")))
            for (auto *plugin : xml->getChildIterator())
            {
                auto &names = controlNames[plugin->getStringAttribute("id")];
                for (auto *c : plugin->getChildIterator())
                    names.add(c->getStringAttribute("name"));
            }
    }
    absorbKnown();
}

HostedPlugins::~HostedPlugins()
{
    stopTimer();
    if (scan != nullptr)
        scan->signalThreadShouldExit();
    for (auto &s : stopping)
        s->signalThreadShouldExit();
    scan.reset();
    stopping.clear();
}

juce::File HostedPlugins::folder()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Airwindows Chain");
}

juce::String HostedPlugins::standardFolder()
{
    return juce::VST3PluginFormat().getDefaultLocationsToSearch().toString().upToFirstOccurrenceOf(";", false, false);
}

void HostedPlugins::absorbKnown()
{
    for (const auto &d : known.getTypes())
        idFor(d);
}

int HostedPlugins::idFor(const juce::PluginDescription &saved)
{
    // A project from another computer names the plugin where it was installed
    // there. The same plugin installed in another folder here is the same
    // plugin: same format, same ids, same name.
    const auto samePlugin = [&saved](const juce::PluginDescription &here) {
        return here.pluginFormatName == saved.pluginFormatName && here.uniqueId == saved.uniqueId &&
               here.deprecatedUid == saved.deprecatedUid && here.name == saved.name &&
               juce::File(here.fileOrIdentifier).exists();
    };
    auto d = saved;
    if (!juce::File(saved.fileOrIdentifier).exists())
    {
        auto candidates = known.getTypes();
        candidates.addArray(listed.data(), (int)listed.size());
        for (const auto &here : candidates)
            if (samePlugin(here))
            {
                d = here;
                break;
            }
    }

    const auto key = d.createIdentifierString();
    for (size_t i = 0; i < listed.size(); ++i)
        if (listed[i].createIdentifierString() == key)
            return kHostedBase + (int)i;
    listed.push_back(d);
    return kHostedBase + (int)listed.size() - 1;
}

const juce::PluginDescription *HostedPlugins::find(int id) const
{
    const int i = id - kHostedBase;
    return isHosted(id) && i < (int)listed.size() ? &listed[(size_t)i] : nullptr;
}

std::vector<int> HostedPlugins::effects() const
{
    std::vector<int> ids;
    for (const auto &d : known.getTypes())
    {
        // Instruments need notes, and this plugin inside itself would recurse.
        if (d.isInstrument || d.name == "Airwindows Chain")
            continue;
        const auto key = d.createIdentifierString();
        for (size_t i = 0; i < listed.size(); ++i)
            if (listed[i].createIdentifierString() == key)
                ids.push_back(kHostedBase + (int)i);
    }
    std::sort(ids.begin(), ids.end(), [this](int a, int b) {
        return find(a)->name.compareNatural(find(b)->name, false) < 0;
    });
    return ids;
}

std::vector<int> HostedPlugins::search(const juce::String &query) const
{
    const auto words = juce::StringArray::fromTokens(query.toLowerCase(), " ", "");
    std::vector<int> ids;
    for (int id : effects())
    {
        const auto *d = find(id);
        const auto text = (d->name + " " + d->manufacturerName + " " + d->category).toLowerCase();
        bool all = true;
        for (const auto &w : words)
            all = all && (w.isEmpty() || text.contains(w));
        if (all)
            ids.push_back(id);
    }
    return ids;
}

//==============================================================================
std::optional<juce::StringArray> HostedPlugins::controlsOf(int id)
{
    const auto *d = find(id);
    if (d == nullptr)
        return juce::StringArray();
    const auto key = d->createIdentifierString();
    const juce::ScopedLock l(readLock);
    if (auto it = controlNames.find(key); it != controlNames.end())
        return it->second;
    if (key != controlsReading)
        controlsWanted = *d;
    if (!readingControls)
    {
        readingControls = true;
        pool.addJob([this] { readControls(); });
    }
    return std::nullopt;
}

// Pool thread. Reads whichever plugin was asked for last, until none is waiting,
// so moving quickly down the list doesn't line up a read for every row.
void HostedPlugins::readControls()
{
    static constexpr juce::uint32 kPatienceMs = 60000;
    const auto helper = scannerHelper();
    auto *job = juce::ThreadPoolJob::getCurrentThreadPoolJob();
    for (;;)
    {
        juce::PluginDescription d;
        {
            const juce::ScopedLock l(readLock);
            if (!controlsWanted.has_value() || job->shouldExit())
            {
                readingControls = false;
                controlsReading = {};
                return;
            }
            d = *controlsWanted;
            controlsWanted.reset();
            controlsReading = d.createIdentifierString();
        }

        juce::StringArray names;
        const auto in = juce::File::createTempFile(".xml"), out = juce::File::createTempFile(".xml");
        juce::ChildProcess child;
        if (helper.existsAsFile() && d.createXml()->writeTo(in) &&
            child.start(juce::StringArray{helper.getFullPathName(), "--controls", in.getFullPathName(),
                                          out.getFullPathName()},
                        0))
        {
            const auto deadline = juce::Time::getMillisecondCounter() + kPatienceMs;
            while (child.isRunning() && !job->shouldExit() && juce::Time::getMillisecondCounter() < deadline)
                child.waitForProcessToFinish(100);
            if (child.isRunning())
                child.kill();
            else if (const auto xml = child.getExitCode() == 0 ? juce::parseXML(out) : nullptr)
                for (auto *e : xml->getChildIterator())
                    names.add(e->getStringAttribute("name"));
        }
        in.deleteFile();
        out.deleteFile();

        const juce::ScopedLock l(readLock);
        if (job->shouldExit())
            continue; // leaves without keeping a half-read result
        controlNames[controlsReading] = names;
        controlsReading = {};
        triggerAsyncUpdate();
    }
}

//==============================================================================
// Lowercase letters and digits only, so "Comp FET-76" meets "comp-fet76".
static juce::String squash(const juce::String &s)
{
    return s.toLowerCase().retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789");
}

// The word a maker's install folders are named by: "Universal Audio, Inc."
// installs to "Universal Audio".
static juce::String makerKey(const juce::String &maker)
{
    static const juce::StringArray generic{"the", "audio", "music", "sound", "software", "plugins",
                                           "labs", "inc", "ltd", "llc", "gmbh"};
    for (const auto &w : juce::StringArray::fromTokens(maker, " ,.-_", ""))
        if (const auto key = squash(w); key.length() >= 3 && !generic.contains(key))
            return key;
    return {};
}

// Whether a manual is about this plugin: its name holds the plugin's
// ("clavinet-v_Manual_1_0_EN" for Clavinet V), or the plugin's name starts with
// the manual's once words like Manual and version numbers are dropped
// ("API-2500.pdf" for API-2500 Stereo).
static bool describes(const juce::File &doc, const juce::String &pluginName)
{
    static const juce::StringArray filler{"manual", "user", "users", "guide", "help", "readme",
                                          "read",   "me",   "en",    "eng",   "english", "s"};
    const auto plugin = squash(pluginName);
    const auto name = doc.getFileNameWithoutExtension();
    if (plugin.length() >= 3 && squash(name).contains(plugin))
        return true;
    juce::String core;
    for (const auto &w : juce::StringArray::fromTokens(name.toLowerCase(), " -_.()'", ""))
        if (!w.containsOnly("0123456789") && !filler.contains(w))
            core += squash(w);
    return core.length() >= 4 && plugin.startsWith(core);
}

juce::Array<juce::File> HostedPlugins::manualsFor(int id)
{
    const auto *d = find(id);
    const auto key = d != nullptr ? makerKey(d->manufacturerName) : juce::String();
    if (key.isEmpty())
        return {};

    juce::Array<juce::File> all;
    {
        const juce::ScopedLock l(readLock);
        auto it = docsByMaker.find(key);
        if (it == docsByMaker.end())
        {
            if (!makersBeingIndexed.contains(key))
            {
                makersBeingIndexed.add(key);
                // Some makers put the manual inside the plugin's own bundle.
                juce::Array<juce::File> bundles;
                for (const auto &p : listed)
                    if (makerKey(p.manufacturerName) == key && juce::File(p.fileOrIdentifier).isDirectory())
                        bundles.add(juce::File(p.fileOrIdentifier));
                pool.addJob([this, key, bundles] { indexManuals(key, bundles); });
            }
            return {};
        }
        all = it->second;
    }

    juce::Array<juce::File> found;
    juce::StringArray names;
    for (const auto &f : all)
        if (describes(f, d->name) && !names.contains(f.getFileName(), true))
        {
            found.add(f);
            names.add(f.getFileName());
        }
    // Manuals and guides before read-me notes, then the shortest name.
    const auto rank = [](const juce::File &f) {
        const auto n = f.getFileName().toLowerCase();
        int r = n.contains("read me") || n.contains("readme") ? 2 : 0;
        if (!n.contains("manual") && !n.contains("guide") && !n.contains("help"))
            ++r;
        return r * 1000 + n.length();
    };
    std::sort(found.begin(), found.end(), [&](const juce::File &a, const juce::File &b) { return rank(a) < rank(b); });
    found.removeRange(3, found.size());
    return found;
}

// Pool thread. Every PDF and Windows help file in the maker's install folders.
void HostedPlugins::indexManuals(const juce::String &key, const juce::Array<juce::File> &bundles)
{
    auto *job = juce::ThreadPoolJob::getCurrentThreadPoolJob();
    juce::Array<juce::File> folders = bundles;
    const auto programs = juce::File::getSpecialLocation(juce::File::globalApplicationsDirectory);
    const auto programsX86 = juce::File::getSpecialLocation(juce::File::globalApplicationsDirectoryX86);
    for (const auto &root : {programs, programsX86, programs.getChildFile("Common Files"),
                             programsX86.getChildFile("Common Files")})
        for (const auto &entry : juce::RangedDirectoryIterator(root, false, "*", juce::File::findDirectories))
            if (squash(entry.getFile().getFileName()).contains(key))
                folders.addIfNotAlreadyThere(entry.getFile());

    juce::Array<juce::File> docs;
    for (const auto &folder : folders)
        for (const auto &entry : juce::RangedDirectoryIterator(folder, true, "*.pdf;*.chm", juce::File::findFiles))
        {
            if (job->shouldExit())
                return;
            docs.add(entry.getFile());
        }

    const juce::ScopedLock l(readLock);
    docsByMaker[key] = docs;
    makersBeingIndexed.removeString(key);
    triggerAsyncUpdate();
}

void HostedPlugins::handleAsyncUpdate()
{
    if (persist)
    {
        juce::XmlElement xml("CONTROLS");
        {
            const juce::ScopedLock l(readLock);
            for (const auto &[key, names] : controlNames)
            {
                auto *plugin = xml.createNewChildElement("PLUGIN");
                plugin->setAttribute("id", key);
                for (const auto &n : names)
                    plugin->createNewChildElement("CONTROL")->setAttribute("name", n);
            }
        }
        folder().createDirectory();
        xml.writeTo(folder().getChildFile("Controls.xml"));
    }
    sendChangeMessage();
}

juce::String HostedPlugins::nowScanning() const { return scan != nullptr ? scan->file() : juce::String(); }

float HostedPlugins::progress() const { return scan != nullptr ? scan->progress.load() : 0.f; }

void HostedPlugins::startScan(bool retrySkipped)
{
    if (scan != nullptr)
        return;

    // Plugins that have been uninstalled since the last scan
    for (const auto &d : known.getTypes())
        if (!juce::File::isAbsolutePath(d.fileOrIdentifier) || !juce::File(d.fileOrIdentifier).exists())
            known.removeType(d);
    if (retrySkipped)
    {
        known.clearBlacklistedFiles();
        // Plugins whose controls couldn't be read get another go too.
        const juce::ScopedLock l(readLock);
        for (auto it = controlNames.begin(); it != controlNames.end();)
            it = it->second.isEmpty() ? controlNames.erase(it) : std::next(it);
    }

    auto *vst3 = formats.getFormat(0);
    auto paths = vst3->getDefaultLocationsToSearch();
    for (const auto &f : extra)
        paths.addIfNotAlreadyThere(juce::File(f));

    scan = std::make_unique<Scan>(known, *vst3, paths, scannerHelper());
    scan->startThread();
    startTimer(100);
    sendChangeMessage();
}

juce::File HostedPlugins::scannerHelper()
{
    // Next to this module (the standalone app), in the VST3 bundle's Resources
    // (the VST3), or in the installed VST3 bundle (the CLAP, which is one file).
    const juce::String name = "Airwindows Chain Scanner.exe";
    const auto self = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    for (const auto &f : {self.getSiblingFile(name), self.getParentDirectory().getSiblingFile("Resources").getChildFile(name),
                          juce::File(standardFolder()).getChildFile("Airwindows Chain.vst3/Contents/Resources/" + name)})
        if (f.existsAsFile())
            return f;
    return {};
}

void HostedPlugins::stopScan()
{
    if (scan == nullptr)
        return;
    scan->signalThreadShouldExit();
    stopping.push_back(std::move(scan));
    scanned = true;
    absorbKnown();
    save();
    sendChangeMessage();
}

void HostedPlugins::timerCallback()
{
    stopping.erase(std::remove_if(stopping.begin(), stopping.end(), [](auto &s) { return !s->isThreadRunning(); }),
                   stopping.end());

    absorbKnown();
    if (scan != nullptr && !scan->isThreadRunning())
    {
        scan.reset();
        scanned = true;
        known.scanFinished();
        absorbKnown();
        save();
    }
    if (scan == nullptr && stopping.empty())
        stopTimer();
    sendChangeMessage();
}

void HostedPlugins::addFolder(const juce::File &f)
{
    extra.addIfNotAlreadyThere(f.getFullPathName());
    if (persist)
    {
        folder().createDirectory();
        folder().getChildFile("Folders.txt").replaceWithText(extra.joinIntoString("\n"));
    }
    sendChangeMessage();
}

void HostedPlugins::removeFolder(const juce::String &path)
{
    extra.removeString(path);
    if (persist)
        folder().getChildFile("Folders.txt").replaceWithText(extra.joinIntoString("\n"));
    // What came from that folder goes too.
    for (const auto &d : known.getTypes())
        if (juce::File(d.fileOrIdentifier).isAChildOf(juce::File(path)))
            known.removeType(d);
    save();
    sendChangeMessage();
}

void HostedPlugins::save()
{
    if (!persist)
        return;
    if (auto xml = known.createXml())
    {
        folder().createDirectory();
        xml->writeTo(folder().getChildFile("Plugins.xml"));
    }
}

std::unique_ptr<juce::AudioPluginInstance> HostedPlugins::create(int id, double sampleRate, int blockSize,
                                                                 juce::String &error)
{
    const auto *d = find(id);
    if (d == nullptr)
    {
        error = "Not on this computer";
        return {};
    }
    auto instance = formats.createPluginInstance(*d, sampleRate, blockSize, error);
    if (instance == nullptr)
        return {};

    // Stereo in, stereo out, no sidechains: the chain has two channels to give.
    instance->disableNonMainBuses();
    auto layout = instance->getBusesLayout();
    for (auto *buses : {&layout.inputBuses, &layout.outputBuses})
        if (!buses->isEmpty())
            buses->getReference(0) = juce::AudioChannelSet::stereo();
    if (instance->checkBusesLayoutSupported(layout))
        instance->setBusesLayout(layout);
    return instance;
}

//==============================================================================
HostedEffect::HostedEffect(std::unique_ptr<juce::AudioPluginInstance> p)
    : AirwinConsolidatedBase(0, 0, 0), instance(std::move(p))
{
    const auto *bypass = instance->getBypassParameter();
    for (auto *param : instance->getParameters())
        if ((int)controls.size() < kParamsPerSlot && param != bypass && param->isAutomatable() &&
            !param->isMetaParameter())
            controls.push_back(param);
}

void HostedEffect::getParameterName(VstInt32 i, char *text)
{
    juce::String(controls[(size_t)i]->getName(40)).copyToUTF8(text, 64);
}

HostedEffect::~HostedEffect()
{
    if (prepared)
        instance->releaseResources();
}

void HostedEffect::prepare(double sampleRate, int block)
{
    if (prepared)
        instance->releaseResources();
    blockSize = juce::jmax(16, block);
    instance->setRateAndBufferSizeDetails(sampleRate, blockSize);
    instance->prepareToPlay(sampleRate, blockSize);
    const int channels = juce::jmax(2, instance->getTotalNumInputChannels(), instance->getTotalNumOutputChannels());
    scratch.setSize(channels, blockSize, false, true, false);
    // Room for the longest delay up front, since a plugin can change its latency
    // while playing (lookahead, linear phase) and the audio thread can't allocate.
    dryLine.setSize(2, kMaxLag, false, true, false);
    lag = -1; // read on the first block
    midi.ensureSize(512);
    prepared = true;
}

void HostedEffect::followLatency()
{
    if (const int now = juce::jlimit(0, kMaxLag, instance->getLatencySamples()); prepared && now != lag)
    {
        lag = now;
        dryLine.clear(0, juce::jmax(1, lag));
        dryPos = 0;
    }
}

void HostedEffect::delayDry(float *left, float *right, int n)
{
    if (lag <= 0)
        return;
    auto *dl = dryLine.getWritePointer(0);
    auto *dr = dryLine.getWritePointer(1);
    for (int i = 0; i < n; ++i)
    {
        std::swap(left[i], dl[dryPos]);
        std::swap(right[i], dr[dryPos]);
        dryPos = dryPos + 1 == lag ? 0 : dryPos + 1;
    }
}

void HostedEffect::processReplacing(float **inputs, float **outputs, VstInt32 n)
{
    if (!prepared)
        return;
    const int ins = instance->getTotalNumInputChannels();
    const int outs = instance->getTotalNumOutputChannels();

    for (int start = 0; start < n; start += blockSize)
    {
        const int len = juce::jmin(blockSize, n - start);
        scratch.clear();
        if (ins == 1)
        {
            auto *m = scratch.getWritePointer(0);
            juce::FloatVectorOperations::add(m, inputs[0] + start, inputs[1] + start, len);
            juce::FloatVectorOperations::multiply(m, 0.5f, len);
        }
        else
        {
            scratch.copyFrom(0, 0, inputs[0] + start, len);
            scratch.copyFrom(1, 0, inputs[1] + start, len);
        }

        juce::AudioBuffer<float> view(scratch.getArrayOfWritePointers(), scratch.getNumChannels(), len);
        midi.clear();
        instance->processBlock(view, midi);

        juce::FloatVectorOperations::copy(outputs[0] + start, scratch.getReadPointer(0), len);
        juce::FloatVectorOperations::copy(outputs[1] + start, scratch.getReadPointer(outs >= 2 ? 1 : 0), len);
    }
}
} // namespace awchain
