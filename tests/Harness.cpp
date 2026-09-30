// Headless checks for Airwindows Chain.
//
//   awchain_harness test              processing, state, threading, every effect
//   awchain_harness snap <folder>     renders the editor to PNGs
//   awchain_harness vst3 <file.vst3>  loads the built plugin the way a host does
//
// Never plays audio. Everything is measured.

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "ChainEditor.h"
#include "ChainProcessor.h"
#include "VocalList.h"
#include "AirwinRegistry.h"

#include <iostream>
#include <thread>

using namespace awchain;

namespace
{
int failures = 0;

void check(bool ok, const juce::String &what)
{
    std::cout << (ok ? "ok    " : "FAIL  ") << what << std::endl;
    if (!ok)
        ++failures;
}

int reg(const char *name)
{
    const int i = Catalog::get().indexOf(name);
    if (i < 0)
        std::cout << "missing effect " << name << std::endl;
    return i;
}

juce::AudioBuffer<float> testSignal(double sr, int n)
{
    juce::AudioBuffer<float> b(2, n);
    juce::Random rng(1234);
    for (int i = 0; i < n; ++i)
    {
        const double t = (double)i / sr;
        const float s = (float)(0.3 * std::sin(juce::MathConstants<double>::twoPi * 110.0 * t) +
                                0.2 * std::sin(juce::MathConstants<double>::twoPi * 1760.0 * t)) +
                        0.1f * (rng.nextFloat() * 2.f - 1.f);
        b.setSample(0, i, s);
        b.setSample(1, i, 0.8f * s + 0.05f * (rng.nextFloat() * 2.f - 1.f));
    }
    return b;
}

// Irregular block sizes, the same sequence every time for a given seed.
std::vector<int> blockSizes(int total, int maxBlock, int seed)
{
    std::vector<int> sizes;
    juce::Random rng(seed);
    for (int pos = 0; pos < total;)
    {
        const int len = juce::jmin(total - pos, 1 + rng.nextInt(maxBlock));
        sizes.push_back(len);
        pos += len;
    }
    return sizes;
}

void render(juce::AudioProcessor &p, juce::AudioBuffer<float> &b, const std::vector<int> &sizes)
{
    juce::MidiBuffer midi;
    int pos = 0;
    for (auto len : sizes)
    {
        juce::AudioBuffer<float> view(b.getArrayOfWritePointers(), b.getNumChannels(), pos, len);
        p.processBlock(view, midi);
        pos += len;
    }
}

struct Stage
{
    int registryIndex;
    std::vector<float> params; // empty: defaults
    float mix{1.f};
    bool bypass{false};
    unsigned seed{1};          // some effects seed modulation from rand() when created
};

// What the chain should sound like: the same effects run directly, in order.
void renderReference(const std::vector<Stage> &stages, juce::AudioBuffer<float> &b, double sr,
                     const std::vector<int> &sizes)
{
    AirwinConsolidatedBase::defaultSampleRate = (float)sr;
    std::vector<std::unique_ptr<AirwinConsolidatedBase>> fx;
    for (const auto &s : stages)
    {
        std::srand(s.seed);
        auto e = AirwinRegistry::registry[(size_t)s.registryIndex].generator();
        e->setSampleRate((float)sr);
        for (size_t i = 0; i < s.params.size(); ++i)
            e->setParameter((int)i, s.params[i]);
        fx.push_back(std::move(e));
    }

    std::vector<float> dryL, dryR;
    int pos = 0;
    for (auto len : sizes)
    {
        float *io[2] = {b.getWritePointer(0, pos), b.getWritePointer(1, pos)};
        for (size_t k = 0; k < stages.size(); ++k)
        {
            if (stages[k].bypass)
                continue;
            if (stages[k].mix >= 1.f)
            {
                fx[k]->processReplacing(io, io, len);
                continue;
            }
            dryL.assign(io[0], io[0] + len);
            dryR.assign(io[1], io[1] + len);
            fx[k]->processReplacing(io, io, len);
            const float m = stages[k].mix;
            for (int i = 0; i < len; ++i)
            {
                io[0][i] = dryL[(size_t)i] + m * (io[0][i] - dryL[(size_t)i]);
                io[1][i] = dryR[(size_t)i] + m * (io[1][i] - dryR[(size_t)i]);
            }
        }
        pos += len;
    }
}

float maxDifference(const juce::AudioBuffer<float> &a, const juce::AudioBuffer<float> &b)
{
    float d = 0.f;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < a.getNumSamples(); ++i)
            d = juce::jmax(d, std::abs(a.getSample(ch, i) - b.getSample(ch, i)));
    return d;
}

bool allFinite(const juce::AudioBuffer<float> &b, float &peak)
{
    peak = 0.f;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float v = b.getSample(ch, i);
            if (!std::isfinite(v))
                return false;
            peak = juce::jmax(peak, std::abs(v));
        }
    return true;
}

// Builds the chain through the processor's own API, then sets values.
std::unique_ptr<ChainProcessor> buildChain(const std::vector<Stage> &stages)
{
    auto p = std::make_unique<ChainProcessor>();
    for (const auto &s : stages)
    {
        std::srand(s.seed); // the processor creates the audio instance first
        const int slot = p->addEffect(s.registryIndex);
        for (size_t i = 0; i < s.params.size(); ++i)
            p->fxParam(slot, (int)i).setValueNotifyingHost(s.params[i]);
        p->mixParam(slot).setValueNotifyingHost(s.mix);
        p->bypassParam(slot).setValueNotifyingHost(s.bypass ? 1.f : 0.f);
    }
    return p;
}

// Largest difference from the reference, relative to how loud the output is.
// Fails outright if the output is silent, since silence matches silence.
float compareWithReference(const std::vector<Stage> &stages, double sr = 48000.0, int maxBlock = 512)
{
    const int n = (int)(sr * 1.5);
    const auto sizes = blockSizes(n, maxBlock, 7);

    auto p = buildChain(stages);
    p->prepareToPlay(sr, maxBlock);
    auto out = testSignal(sr, n);
    render(*p, out, sizes);

    auto ref = testSignal(sr, n);
    renderReference(stages, ref, sr, sizes);

    float peak = 0.f;
    if (!allFinite(out, peak) || peak < 0.05f)
    {
        std::cout << "      output peak " << peak << ", treating as a failure" << std::endl;
        return 1.f;
    }
    auto dry = testSignal(sr, n);
    std::cout << "      output peak " << juce::String(peak, 3) << ", differs from the dry signal by up to "
              << juce::String(maxDifference(out, dry), 3) << std::endl;
    return maxDifference(out, ref) / peak;
}

std::vector<Stage> referenceChain()
{
    std::vector<Stage> chain{{reg("Pressure4"), {0.6f, 0.4f, 0.7f, 1.f}},
                             {reg("Density2"), {0.45f, 0.1f, 0.9f, 1.f}},
                             {reg("Air3"), {}},
                             {reg("Galactic"), {0.5f, 0.3f, 0.6f, 0.5f, 0.3f}},
                             {reg("PurestGain"), {0.45f, 1.f}}};
    for (size_t i = 0; i < chain.size(); ++i)
        chain[i].seed = 1000u + (unsigned)i;
    return chain;
}

//==============================================================================
void testCatalog()
{
    std::cout << "\n-- catalog" << std::endl;
    const auto &c = Catalog::get();
    check(c.all().size() >= 500, "catalog has " + juce::String((int)c.all().size()) + " effects");
    check(c.categories().size() >= 20, juce::String(c.categories().size()) + " categories");
    check(c.recommended().size() > 100, juce::String((int)c.recommended().size()) + " recommended");
    {
        int listed = 0;
        for (const char *n : kForVocals)
            ++listed;
        check(c.vocalCount() == listed, juce::String(c.vocalCount()) + " of " + juce::String(listed) + " vocal picks found in the registry");
        auto rec = c.recommended();
        c.keepVocals(rec);
        check(!rec.empty() && (int)rec.size() < c.vocalCount(), juce::String((int)rec.size()) + " recommended effects for vocals");
    }

    for (const char *q : {"tape", "totape", "to tape", "reverb", "de-ess", "sat"})
    {
        const auto r = c.search(q);
        juce::StringArray top;
        for (size_t i = 0; i < juce::jmin<size_t>(4, r.size()); ++i)
            top.add(c.find(r[i])->name);
        std::cout << "      search \"" << q << "\": " << r.size() << " hits, top " << top.joinIntoString(", ") << std::endl;
    }
    const auto tape = c.search("totape");
    check(!tape.empty() && c.find(tape[0])->name.startsWith("ToTape"), "\"totape\" finds ToTape first");

    const auto docs = c.docParagraphs(reg("Density2"));
    check(docs.size() >= 2 && !docs[0].startsWith("#"), "Density2 docs load without the heading line");

    const int d = reg("Density2");
    check(c.neighbour(c.neighbour(d, 1), -1) == d, "stepping forward then back returns to the same effect");
}

void testProcessing()
{
    std::cout << "\n-- processing matches the effects run directly" << std::endl;
    const float tol = 1e-4f;

    auto chain = referenceChain();
    auto d = compareWithReference(chain);
    check(d < tol, "five-effect chain, max difference " + juce::String(d, 8));

    auto mixed = chain;
    mixed[1].mix = 0.5f;
    mixed[3].mix = 0.25f;
    d = compareWithReference(mixed);
    check(d < tol, "mix at 50% and 25%, max difference " + juce::String(d, 8));

    auto bypassed = chain;
    bypassed[2].bypass = true;
    bypassed[3].bypass = true;
    d = compareWithReference(bypassed);
    check(d < tol, "two effects bypassed, max difference " + juce::String(d, 8));

    for (double sr : {44100.0, 96000.0})
    {
        d = compareWithReference(chain, sr, 256);
        check(d < tol, "at " + juce::String((int)sr) + " Hz, max difference " + juce::String(d, 8));
    }

    // Blocks bigger than the host promised get processed in pieces.
    {
        auto p = buildChain(chain);
        p->prepareToPlay(48000.0, 256);
        const int n = 48000;
        auto out = testSignal(48000.0, n);
        std::vector<int> sizes(n / 2000, 2000);
        render(*p, out, sizes);
        auto ref = testSignal(48000.0, n);
        renderReference(chain, ref, 48000.0, sizes);
        float peak = 0.f;
        d = allFinite(out, peak) && peak > 0.05f ? maxDifference(out, ref) / peak : 1.f;
        check(d < tol, "oversized blocks, max difference " + juce::String(d, 8));
    }

    // Reordering
    {
        auto p = buildChain(chain);
        p->moveSlot(4, 0);
        p->moveSlot(1, 3);
        std::vector<Stage> expected;
        for (auto s : p->getOrder())
            expected.push_back(chain[(size_t)s]);
        p->prepareToPlay(48000.0, 512);
        const int n = 48000;
        const auto sizes = blockSizes(n, 512, 3);
        auto out = testSignal(48000.0, n);
        render(*p, out, sizes);
        auto ref = testSignal(48000.0, n);
        renderReference(expected, ref, 48000.0, sizes);
        float peak = 0.f;
        d = allFinite(out, peak) && peak > 0.05f ? maxDifference(out, ref) / peak : 1.f;
        check(d < tol, "reordered chain, max difference " + juce::String(d, 8));
    }

    // Mono in, mono out
    {
        auto p = buildChain(chain);
        juce::AudioProcessor::BusesLayout mono;
        mono.inputBuses.add(juce::AudioChannelSet::mono());
        mono.outputBuses.add(juce::AudioChannelSet::mono());
        check(p->setBusesLayout(mono), "mono layout accepted");
        p->prepareToPlay(48000.0, 512);
        juce::AudioBuffer<float> b(1, 24000);
        auto sig = testSignal(48000.0, 24000);
        b.copyFrom(0, 0, sig, 0, 0, 24000);
        juce::MidiBuffer midi;
        for (int pos = 0; pos < 24000; pos += 480)
        {
            juce::AudioBuffer<float> view(b.getArrayOfWritePointers(), 1, pos, 480);
            p->processBlock(view, midi);
        }
        float peak = 0.f;
        const bool finite = allFinite(b, peak);
        check(finite && peak > 0.01f, "mono processing is finite and not silent (finite " +
                                          juce::String(finite ? "yes" : "no") + ", peak " + juce::String(peak, 4) +
                                          ", in " + juce::String(p->getTotalNumInputChannels()) + ", out " +
                                          juce::String(p->getTotalNumOutputChannels()) + ")");
    }
}

void testChainEditing()
{
    std::cout << "\n-- editing the chain" << std::endl;
    ChainProcessor p;
    const int a = p.addEffect(reg("Density2"));
    const int b = p.addEffect(reg("ToTape8"));
    const int c = p.addEffect(reg("Galactic3"));
    check(p.getOrder() == std::vector<int>({a, b, c}), "three effects added in order");

    auto &first = *p.getParameters()[0];
    check(first.getName(100) == "1. Density2: " + p.getSlotInfo(a).paramNames[0],
          "host sees parameter name \"" + first.getName(100) + "\"");
    check(first.getText(first.getValue(), 100).isNotEmpty(), "host sees value text \"" + first.getText(first.getValue(), 100) + "\"");

    p.moveSlot(2, 0);
    check(p.getOrder() == std::vector<int>({c, a, b}), "move last to first");
    check(p.fxParam(a, 0).getName(100).startsWith("2. Density2"), "names follow the new positions");

    const int d = p.duplicateSlot(a);
    check(d >= 0 && p.getOrder()[2] == d, "duplicate lands right after the original");
    check(std::abs(p.fxParam(d, 0).get() - p.fxParam(a, 0).get()) < 1e-6f, "duplicate copies the values");

    p.removeSlot(b);
    check(p.getOrder() == std::vector<int>({c, a, d}), "remove");
    check(p.canUndo() && p.undoLabel() == "remove ToTape8", "undo is offered: \"" + p.undoLabel() + "\"");
    p.undo();
    check(p.getOrder() == std::vector<int>({c, a, d, b}) && p.getSlotInfo(b).registryIndex == reg("ToTape8"),
          "undo brings the effect back where it was");
    p.removeSlot(b);

    p.setFavourite("Density2", true);
    check(p.isFavourite("Density2") && p.getFavourites().contains("Density2"), "favourite an effect");
    p.setFavourite("Density2", false);
    check(!p.isFavourite("Density2"), "unfavourite it");
    p.setSetting("theme", "Cool");
    check(p.getSetting("theme", "") == "Cool", "settings round trip");

    p.replaceEffect(a, reg("Pressure4"));
    check(p.getSlotInfo(a).registryIndex == reg("Pressure4"), "replace keeps the slot");

    // Fill it up
    int added = 0;
    while (p.addEffect(reg("PurestGain")) >= 0)
        ++added;
    check((int)p.getOrder().size() == kMaxSlots && p.isFull(), "chain stops at " + juce::String(kMaxSlots));

    const int before = reg("Density2");
    check(Catalog::get().find(before) != nullptr, "registry lookups still valid");

    // Round trip through saved state
    juce::MemoryBlock state;
    p.fxParam(c, 1).setValueNotifyingHost(0.123f);
    p.mixParam(c).setValueNotifyingHost(0.4f);
    p.bypassParam(d).setValueNotifyingHost(1.f);
    p.setChainName("Round trip");
    p.getStateInformation(state);

    ChainProcessor q;
    q.setStateInformation(state.getData(), (int)state.getSize());
    check(q.getOrder() == p.getOrder(), "state restores the same slots in the same order");
    check(q.getChainName() == "Round trip", "state restores the chain name");
    check(std::abs(q.fxParam(c, 1).get() - 0.123f) < 1e-6f, "state restores control values");
    check(std::abs(q.mixParam(c).get() - 0.4f) < 1e-6f && q.bypassParam(d).get(), "state restores mix and bypass");
    bool sameEffects = true;
    for (auto s : p.getOrder())
        sameEffects = sameEffects && p.getSlotInfo(s).registryIndex == q.getSlotInfo(s).registryIndex;
    check(sameEffects, "state restores the same effects");

    // Chain files
    auto file = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("awchain-test.awchain");
    check(p.saveChain(file) && file.existsAsFile(), "save chain file");
    ChainProcessor r;
    check(r.loadChain(file) && r.getOrder().size() == p.getOrder().size(), "load chain file");
    file.deleteFile();

    q.clearChain();
    check(q.getOrder().empty(), "clear");
}

void testEditsWhilePlaying()
{
    std::cout << "\n-- edits while audio runs" << std::endl;
    auto p = buildChain(referenceChain());
    p->prepareToPlay(48000.0, 256);

    std::atomic<bool> running{true};
    std::atomic<int> blocks{0};
    std::atomic<bool> bad{false};
    std::thread audio([&] {
        juce::AudioBuffer<float> b(2, 256);
        juce::MidiBuffer midi;
        juce::Random rng(5);
        while (running)
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 256; ++i)
                    b.setSample(ch, i, 0.3f * (rng.nextFloat() * 2.f - 1.f));
            p->processBlock(b, midi);
            float peak = 0.f;
            if (!allFinite(b, peak) || peak > 64.f)
                bad = true;
            ++blocks;
        }
    });

    juce::Random rng(11);
    const auto &all = Catalog::get().all();
    int operations = 0;
    const auto until = juce::Time::getMillisecondCounter() + 4000;
    while (juce::Time::getMillisecondCounter() < until)
    {
        const auto order = p->getOrder();
        const int pick = order.empty() ? -1 : order[(size_t)rng.nextInt((int)order.size())];
        const int effect = all[(size_t)rng.nextInt((int)all.size())].registryIndex;
        switch (rng.nextInt(8))
        {
        case 0:
            p->addEffect(effect, rng.nextInt((int)order.size() + 1));
            break;
        case 1:
            if (pick >= 0)
                p->replaceEffect(pick, effect);
            break;
        case 2:
            if (pick >= 0 && order.size() > 3)
                p->removeSlot(pick);
            break;
        case 3:
            if (order.size() > 1)
                p->moveSlot(rng.nextInt((int)order.size()), rng.nextInt((int)order.size()));
            break;
        case 4:
            if (pick >= 0)
                p->duplicateSlot(pick);
            break;
        case 5:
            if (pick >= 0)
                p->bypassParam(pick).setValueNotifyingHost(rng.nextBool() ? 1.f : 0.f);
            break;
        case 6:
            if (pick >= 0)
            {
                p->fxParam(pick, 0).setValueNotifyingHost(rng.nextFloat());
                p->formatValue(pick, 0, rng.nextFloat());
            }
            break;
        default:
        {
            juce::MemoryBlock m;
            p->getStateInformation(m);
            p->setStateInformation(m.getData(), (int)m.getSize());
        }
        }
        ++operations;
        p->collectGarbage();
        std::this_thread::sleep_for(std::chrono::milliseconds(rng.nextInt(3)));
    }
    running = false;
    audio.join();
    p->collectGarbage();

    check(!bad, juce::String(operations) + " edits against " + juce::String(blocks.load()) +
                    " audio blocks, output stayed finite and bounded");
}

void testEveryEffect()
{
    std::cout << "\n-- every effect in a slot" << std::endl;
    const int n = 24000;
    juce::StringArray broken, loudDefaults, extremeBroken;
    int tested = 0;
    for (const auto &e : Catalog::get().all())
    {
        ChainProcessor p;
        const int slot = p.addEffect(e.registryIndex);
        p.prepareToPlay(48000.0, 512);
        auto b = testSignal(48000.0, n);
        render(p, b, std::vector<int>(n / 480, 480));
        float peak = 0.f;
        if (!allFinite(b, peak))
            broken.add(e.name);
        else if (peak > 16.f)
            loudDefaults.add(e.name + " (" + juce::String(peak, 1) + ")");

        // Every control at the top of its range
        for (int i = 0; i < e.nParams; ++i)
            p.fxParam(slot, i).setValueNotifyingHost(1.f);
        auto b2 = testSignal(48000.0, n);
        render(p, b2, std::vector<int>(n / 480, 480));
        if (!allFinite(b2, peak))
            extremeBroken.add(e.name);
        ++tested;
    }
    check(broken.isEmpty(), juce::String(tested) + " effects at default settings produce finite output" +
                                (broken.isEmpty() ? juce::String() : ": " + broken.joinIntoString(", ")));
    if (!loudDefaults.isEmpty())
        std::cout << "      loud at defaults (not an error): " << loudDefaults.joinIntoString(", ") << std::endl;
    if (!extremeBroken.isEmpty())
        std::cout << "      non-finite with every control at maximum (effect's own behaviour): "
                  << extremeBroken.joinIntoString(", ") << std::endl;
}

//==============================================================================
std::unique_ptr<ChainProcessor> demoChain()
{
    auto p = std::make_unique<ChainProcessor>();
    const int a = p->addEffect(reg("Pressure5"));
    const int b = p->addEffect(reg("Density2"));
    const int c = p->addEffect(reg("ToTape8"));
    const int d = p->addEffect(reg("Air4"));
    const int e = p->addEffect(reg("Galactic3"));
    const int f = p->addEffect(reg("ADClip8"));
    p->fxParam(b, 0).setValueNotifyingHost(0.38f);
    p->mixParam(b).setValueNotifyingHost(0.72f);
    p->fxParam(c, 0).setValueNotifyingHost(0.62f);
    p->fxParam(c, 3).setValueNotifyingHost(0.35f);
    p->bypassParam(e).setValueNotifyingHost(1.f);
    p->mixParam(d).setValueNotifyingHost(0.5f);
    juce::ignoreUnused(a, f);
    p->setChainName("Lead vocal");
    return p;
}

void writePng(juce::Component &c, const juce::File &file, float scale)
{
    const auto image = c.createComponentSnapshot(c.getLocalBounds(), true, scale);
    file.deleteFile();
    juce::FileOutputStream out(file);
    juce::PNGImageFormat().writeImageToStream(image, out);
    std::cout << "wrote " << file.getFullPathName() << " (" << image.getWidth() << "x" << image.getHeight() << ")" << std::endl;
}

void snapshots(const juce::File &folder)
{
    folder.createDirectory();

    {
        auto p = demoChain();
        p->prepareToPlay(48000.0, 512);
        auto audio = testSignal(48000.0, 4800);
        render(*p, audio, std::vector<int>(10, 480));

        std::unique_ptr<juce::AudioProcessorEditor> ed(p->createEditor());
        auto &editor = dynamic_cast<ChainEditor &>(*ed);
        editor.setSize(940, 620);
        editor.syncNow();
        editor.pollNow();
        editor.select(p->getOrder()[2]);
        writePng(editor, folder.getChildFile("main.png"), 1.f);
        writePng(editor, folder.getChildFile("main@2x.png"), 2.f);

        editor.select(p->getOrder()[1]);
        editor.setSize(760, 500);
        writePng(editor, folder.getChildFile("small.png"), 1.f);

        editor.setSize(1280, 820);
        editor.select(p->getOrder()[4]);
        writePng(editor, folder.getChildFile("large.png"), 1.f);

        editor.setSize(940, 620);
        editor.openPickerToAdd();
        writePng(editor, folder.getChildFile("picker.png"), 1.f);
        editor.getPicker()->setQuery("tape");
        writePng(editor, folder.getChildFile("picker-search.png"), 1.f);
        editor.getPicker()->setQuery("zzzz");
        writePng(editor, folder.getChildFile("picker-nothing.png"), 1.f);
        editor.closePicker();
        editor.openPickerToReplace(p->getOrder()[2]);
        writePng(editor, folder.getChildFile("picker-replace.png"), 1.f);
        editor.closePicker();

        editor.getPicker()->setQuery({});
        editor.openPickerToAdd();
        editor.getPicker()->setQuery("tape");
        writePng(editor, folder.getChildFile("browser@2x.png"), 2.f);
        editor.getPicker()->setQuery({});
        // the For vocals switch on: Chris's list, narrowed
        editor.getPicker()->setVocalsOnly(true);
        writePng(editor, folder.getChildFile("browser-vocals@2x.png"), 2.f);
        editor.getPicker()->setVocalsOnly(false);
        writePng(editor, folder.getChildFile("picker@2x.png"), 2.f);
        editor.closePicker();

        editor.showAbout();
        writePng(editor, folder.getChildFile("about.png"), 1.f);
        editor.closeAbout();

        editor.setKnobs(true);
        editor.select(p->getOrder()[2]);
        writePng(editor, folder.getChildFile("knobs.png"), 1.f);
        writePng(editor, folder.getChildFile("knobs@2x.png"), 2.f);
        editor.setSize(760, 500);
        writePng(editor, folder.getChildFile("knobs-small.png"), 1.f);
        editor.setKnobs(false);
    }

    // Themes: one main view per palette, and a 2x2 sheet of them.
    {
        const std::vector<std::pair<juce::String, juce::String>> looks{
            {"Warm", "Amber"}, {"Cool", "Sky"}, {"Black", "Mint"}, {"Light", "Amber"}};
        juce::Image sheet(juce::Image::RGB, 1880, 1240, true);
        int k = 0;
        for (const auto &[palette, accent] : looks)
        {
            auto p = demoChain();
            p->setSetting("theme", palette);
            p->setSetting("accent", accent);
            p->prepareToPlay(48000.0, 512);
            auto audio = testSignal(48000.0, 4800);
            render(*p, audio, std::vector<int>(10, 480));

            std::unique_ptr<juce::AudioProcessorEditor> ed(p->createEditor());
            auto &editor = dynamic_cast<ChainEditor &>(*ed);
            editor.setSize(940, 620);
            editor.syncNow();
            editor.pollNow();
            editor.select(p->getOrder()[2]);
            writePng(editor, folder.getChildFile("theme-" + palette.toLowerCase() + ".png"), 2.f);

            const auto img = editor.createComponentSnapshot(editor.getLocalBounds(), true, 1.f);
            juce::Graphics g(sheet);
            g.drawImageAt(img, (k % 2) * 940, (k / 2) * 620);
            ++k;
        }
        const auto file = folder.getChildFile("themes.png");
        file.deleteFile();
        juce::FileOutputStream out(file);
        juce::PNGImageFormat().writeImageToStream(sheet, out);
        std::cout << "wrote " << file.getFullPathName() << std::endl;

        // Back to the default for anything rendered after this.
        demoChain()->setSetting("theme", "Warm");
        demoChain()->setSetting("accent", "Amber");
    }
    {
        ChainProcessor p;
        std::unique_ptr<juce::AudioProcessorEditor> ed(p.createEditor());
        ed->setSize(940, 620);
        dynamic_cast<ChainEditor &>(*ed).syncNow();
        writePng(*ed, folder.getChildFile("empty.png"), 1.f);
        writePng(*ed, folder.getChildFile("empty@2x.png"), 2.f);
    }
    {
        ChainProcessor p;
        for (int i = 0; i < kMaxSlots; ++i)
            p.addEffect(Catalog::get().all()[(size_t)(i * 29 % (int)Catalog::get().all().size())].registryIndex);
        std::unique_ptr<juce::AudioProcessorEditor> ed(p.createEditor());
        auto &editor = dynamic_cast<ChainEditor &>(*ed);
        editor.setSize(940, 620);
        editor.syncNow();
        writePng(editor, folder.getChildFile("full.png"), 1.f);

        // Narrowest window with a long name: the header buttons have to wrap.
        editor.setSize(760, 500);
        editor.select(p.getOrder()[4]);
        writePng(editor, folder.getChildFile("small-long-name.png"), 1.f);
    }
}

//==============================================================================
// JUCE's host keeps the plugin's own state, base64 encoded, under "IComponent".
juce::String componentState(juce::AudioPluginInstance &instance, juce::MemoryBlock *pluginState)
{
    juce::MemoryBlock blob;
    instance.getStateInformation(blob);
    auto xml = juce::AudioProcessor::getXmlFromBinary(blob.getData(), (int)blob.getSize());
    if (xml == nullptr || xml->getChildByName("IComponent") == nullptr)
        return {};
    if (pluginState != nullptr)
        pluginState->fromBase64Encoding(xml->getChildByName("IComponent")->getAllSubText());
    return xml->getTagName();
}

void setComponentState(juce::AudioPluginInstance &instance, const juce::MemoryBlock &pluginState)
{
    juce::MemoryBlock blob;
    instance.getStateInformation(blob);
    auto xml = juce::AudioProcessor::getXmlFromBinary(blob.getData(), (int)blob.getSize());
    auto *component = xml != nullptr ? xml->getChildByName("IComponent") : nullptr;
    if (component == nullptr)
        return;
    component->deleteAllTextElements();
    component->addTextElement(pluginState.toBase64Encoding());
    juce::MemoryBlock updated;
    juce::AudioProcessor::copyXmlToBinary(*xml, updated);
    instance.setStateInformation(updated.getData(), (int)updated.getSize());
}

float runThrough(juce::AudioPluginInstance &instance, juce::AudioBuffer<float> &b)
{
    juce::MidiBuffer midi;
    for (int pos = 0; pos < b.getNumSamples(); pos += 512)
    {
        juce::AudioBuffer<float> view(b.getArrayOfWritePointers(), 2, pos, juce::jmin(512, b.getNumSamples() - pos));
        instance.processBlock(view, midi);
    }
    float peak = 0.f;
    return allFinite(b, peak) ? peak : -1.f;
}

void loadVst3(const juce::File &file)
{
    std::cout << "\n-- loading " << file.getFullPathName() << std::endl;
    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> found;
    format.findAllTypesForFile(found, file.getFullPathName());
    check(found.size() == 1, "one plugin in the bundle");
    if (found.isEmpty())
        return;
    const auto &desc = *found[0];
    check(desc.name == "Airwindows Chain" && desc.manufacturerName == "Kani",
          "named \"" + desc.name + "\" by \"" + desc.manufacturerName + "\"");

    juce::String error;
    auto instance = format.createInstanceFromDescription(desc, 48000.0, 512, error);
    check(instance != nullptr, "instantiates" + (error.isEmpty() ? juce::String() : ": " + error));
    if (instance == nullptr)
        return;

    // 16 slots of 12, plus the bypass JUCE adds for the host.
    const int expected = kMaxSlots * (kParamsPerSlot + 2);
    const int count = instance->getParameters().size();
    check(count == expected || count == expected + 1, juce::String(count) + " parameters exposed");

    std::cout << "      buses: in " << instance->getTotalNumInputChannels() << ", out "
              << instance->getTotalNumOutputChannels() << std::endl;
    instance->enableAllBuses();
    instance->prepareToPlay(48000.0, 512);

    // Empty chain passes audio straight through.
    {
        auto b = testSignal(48000.0, 24000);
        const auto dry = testSignal(48000.0, 24000);
        runThrough(*instance, b);
        check(maxDifference(b, dry) < 1e-6f, "empty chain passes audio through untouched, difference " +
                                                 juce::String(maxDifference(b, dry), 8));
    }

    // Give it a chain the way a DAW reloads a project.
    auto source = demoChain();
    juce::MemoryBlock state;
    source->getStateInformation(state);
    check(componentState(*instance, nullptr).isNotEmpty(), "host state has the plugin's own state inside");
    setComponentState(*instance, state);

    const auto name = instance->getParameters()[0]->getName(100);
    check(name.startsWith("1. Pressure5"), "restored chain shows in the host's parameter names: \"" + name + "\"");

    auto b = testSignal(48000.0, 48000);
    const auto dry = testSignal(48000.0, 48000);
    const float peak = runThrough(*instance, b);
    check(peak > 0.01f, "processes audio through the restored chain, peak " + juce::String(peak, 3));
    check(maxDifference(b, dry) > 0.01f, "and the chain changes the sound, by up to " + juce::String(maxDifference(b, dry), 3));

    juce::MemoryBlock back;
    componentState(*instance, &back);
    auto xml = juce::AudioProcessor::getXmlFromBinary(back.getData(), (int)back.getSize());
    check(xml != nullptr && xml->getNumChildElements() == 6, "state comes back out with six effects");

    instance->releaseResources();
}
// Renders the editor frame by frame while real audio runs through the chain,
// with a fader being dragged and the selection changing: material for a clip.
//   awchain_harness frames <folder> [seconds] [fps] [scale]
void frames(const juce::File &folder, double seconds, int fps, float scale)
{
    folder.createDirectory();
    auto p = demoChain();
    const double sr = 48000.0;
    p->prepareToPlay(sr, 512);

    std::unique_ptr<juce::AudioProcessorEditor> ed(p->createEditor());
    auto &editor = dynamic_cast<ChainEditor &>(*ed);
    editor.setSize(940, 620);
    editor.syncNow();
    const auto order = p->getOrder();
    editor.select(order[2]); // ToTape8

    const int total = (int)(seconds * fps);
    const int perFrame = (int)(sr / fps);
    juce::AudioBuffer<float> block(2, perFrame);
    juce::MidiBuffer midi;
    juce::Random rng(3);

    // A beat: kick on the beat, hat between, a swell underneath (same as the site's card).
    auto envelope = [](double t) {
        const double beat = t * 1.75;
        const double kick = std::pow(std::max(0.0, 1.0 - std::fmod(beat, 1.0) * 2.6), 2.0);
        const double hat = 0.18 * std::pow(std::max(0.0, 1.0 - std::fmod(beat + 0.5, 1.0) * 4.0), 2.0);
        const double swell = 0.28 + 0.12 * std::sin(t * 0.7);
        return std::min(1.0, swell + kick * 0.72 + hat);
    };

    const int density = order[1], tape = order[2], galactic = order[4];
    for (int f = 0; f < total; ++f)
    {
        const double t = (double)f / fps;

        // audio for this frame
        const float amp = (float)envelope(t) * 0.8f;
        for (int i = 0; i < perFrame; ++i)
        {
            const double tt = (double)(f * perFrame + i) / sr;
            const float v = amp * (float)(0.5 * std::sin(juce::MathConstants<double>::twoPi * 110.0 * tt) +
                                          0.25 * std::sin(juce::MathConstants<double>::twoPi * 1760.0 * tt)) +
                            amp * 0.15f * (rng.nextFloat() * 2.f - 1.f);
            block.setSample(0, i, v);
            block.setSample(1, i, v * 0.9f);
        }
        p->processBlock(block, midi);

        // the story: drag Input on ToTape8, switch to Density2, bypass the reverb, come back
        if (t >= 2.5 && t < 5.5)
        {
            const double k = (t - 2.5) / 3.0;                       // 0..1
            const float v = 0.62f + 0.26f * (float)std::sin(k * juce::MathConstants<double>::pi); // out and back
            p->fxParam(tape, 0).setValueNotifyingHost(v);
        }
        if (f == (int)(6.0 * fps))
            editor.select(density);
        if (f == (int)(7.5 * fps))
            p->bypassParam(galactic).setValueNotifyingHost(0.f); // turn the reverb on
        if (f == (int)(9.0 * fps))
            editor.select(tape);
        if (f == (int)(10.5 * fps))
            p->bypassParam(galactic).setValueNotifyingHost(1.f);

        editor.pollNow();
        const auto image = editor.createComponentSnapshot(editor.getLocalBounds(), true, scale);
        const auto file = folder.getChildFile(juce::String::formatted("frame%04d.png", f));
        file.deleteFile();
        juce::FileOutputStream out(file);
        juce::PNGImageFormat().writeImageToStream(image, out);
        if (f % 30 == 0)
            std::cout << "frame " << f << " / " << total << std::endl;
    }
    std::cout << "wrote " << total << " frames to " << folder.getFullPathName() << std::endl;
}
// Every effect with its category, Chris's one line, its control names and his
// write-up, as JSON: the website's browser is built from this.
//   awchain_harness dump <file.json>
void dump(const juce::File &file)
{
    const auto &cat = Catalog::get();
    juce::Array<juce::var> list;
    for (const auto &e : cat.all())
    {
        auto *o = new juce::DynamicObject();
        o->setProperty("name", e.name);
        o->setProperty("category", e.category);
        o->setProperty("summary", e.summary);
        o->setProperty("recommended", e.recommended);
        o->setProperty("vocals", e.forVocals);
        o->setProperty("latest", e.latest);
        juce::Array<juce::var> params;
        const auto &r = AirwinRegistry::registry[(size_t)e.registryIndex];
        if (auto fx = r.generator())
            for (int i = 0; i < r.nParams && i < kParamsPerSlot; ++i)
            {
                char txt[256] = {};
                fx->getParameterName(i, txt);
                params.add(juce::String::fromUTF8(txt).trim());
            }
        o->setProperty("params", params);
        juce::Array<juce::var> docs;
        for (const auto &para : cat.docParagraphs(e.registryIndex))
            docs.add(para);
        o->setProperty("docs", docs);
        list.add(juce::var(o));
    }
    file.replaceWithText(juce::JSON::toString(juce::var(list)));
    std::cout << "wrote " << list.size() << " effects to " << file.getFullPathName() << std::endl;
}
} // namespace

int main(int argc, char *argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;
    ChainProcessor::persistSettings = false;

    const juce::String mode = argc > 1 ? argv[1] : "test";

    if (mode == "snap")
        snapshots(juce::File(argc > 2 ? juce::String(argv[2]) : juce::File::getCurrentWorkingDirectory().getFullPathName()));
    else if (mode == "vst3")
        loadVst3(juce::File(juce::String(argv[2])));
    else if (mode == "dump")
        dump(juce::File(juce::String(argv[2])));
    else if (mode == "frames")
        frames(juce::File(juce::String(argv[2])), argc > 3 ? juce::String(argv[3]).getDoubleValue() : 12.0,
               argc > 4 ? juce::String(argv[4]).getIntValue() : 30,
               argc > 5 ? juce::String(argv[5]).getFloatValue() : 1.5f);
    else
    {
        testCatalog();
        testProcessing();
        testChainEditing();
        testEditsWhilePlaying();
        testEveryEffect();
    }

    std::cout << "\n" << (failures == 0 ? "all checks passed" : juce::String(failures) + " failed") << std::endl;
    return failures == 0 ? 0 : 1;
}
