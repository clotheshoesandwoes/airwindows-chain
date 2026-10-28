// Airwindows Chain Scanner: reads VST3 plugins for the plugin, out of its process.
//
//   "Airwindows Chain Scanner.exe" <plugin.vst3> <out.xml>
//       what the file holds
//   "Airwindows Chain Scanner.exe" --controls <description.xml> <out.xml>
//       the names of one plugin's controls
//
// A plugin that crashes or hangs while being read (a licence check that never
// returns, say) takes down only this process, never the host.

#include <juce_audio_processors/juce_audio_processors.h>

#if JUCE_WINDOWS
#include <windows.h>
// after windows.h, which it depends on
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")
#endif

static int listFile(juce::VST3PluginFormat &format, const juce::String &file, const juce::File &out)
{
    juce::OwnedArray<juce::PluginDescription> found;
    format.findAllTypesForFile(found, file);

    juce::XmlElement list("PLUGINS");
    for (auto *d : found)
        list.addChildElement(d->createXml().release());
    return list.writeTo(out) ? 0 : 3;
}

static int listControls(juce::VST3PluginFormat &format, const juce::File &description, const juce::File &out)
{
    juce::PluginDescription d;
    const auto xml = juce::parseXML(description);
    if (xml == nullptr || !d.loadFromXml(*xml))
        return 2;
    juce::String error;
    const auto plugin = format.createInstanceFromDescription(d, 48000.0, 512, error);
    if (plugin == nullptr)
        return 4;

    // The same parameters the chain treats as controls, in the plugin's order.
    juce::XmlElement list("CONTROLS");
    const auto *bypass = plugin->getBypassParameter();
    for (auto *p : plugin->getParameters())
        if (p != bypass && p->isAutomatable() && !p->isMetaParameter())
            list.createNewChildElement("CONTROL")->setAttribute("name", p->getName(60));
    return list.writeTo(out) ? 0 : 3;
}

int main(int argc, char *argv[])
{
#if JUCE_WINDOWS
    // A crash ends quietly instead of putting up a Windows error dialog.
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
#endif
    juce::StringArray args;
#if JUCE_WINDOWS
    // argv is in the ANSI code page, which can't hold every path (a user named
    // José has one in every temp file); the wide command line can.
    juce::ignoreUnused(argv);
    int count = 0;
    if (auto **wide = CommandLineToArgvW(GetCommandLineW(), &count))
    {
        for (int i = 0; i < count; ++i)
            args.add(juce::String(wide[i]));
        LocalFree(wide);
    }
    argc = args.size();
#else
    for (int i = 0; i < argc; ++i)
        args.add(juce::String::fromUTF8(argv[i]));
#endif
    if (argc < 3)
        return 2;

    juce::ScopedJuceInitialiser_GUI gui;
    juce::VST3PluginFormat format;
    if (args[1] == "--controls")
        return argc < 4 ? 2 : listControls(format, juce::File(args[2]), juce::File(args[3]));
    return listFile(format, args[1], juce::File(args[2]));
}
