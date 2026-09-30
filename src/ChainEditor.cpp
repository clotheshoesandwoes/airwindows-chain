#include "ChainEditor.h"

namespace awchain
{
using namespace theme;

//==============================================================================
ChainEditor::ChainMenuButton::ChainMenuButton() : juce::Button("Chain")
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void ChainEditor::ChainMenuButton::setChainName(const juce::String &name)
{
    const auto next = name.isEmpty() ? juce::String("Untitled chain") : name;
    if (next != shown)
    {
        shown = next;
        repaint();
    }
}

int ChainEditor::ChainMenuButton::idealWidth() const
{
    return (int)std::ceil(textWidth(sans(16.f, Weight::medium), shown)) + 46;
}

void ChainEditor::ChainMenuButton::paintButton(juce::Graphics &g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat();
    if (highlighted || down)
    {
        g.setColour(down ? colour::pressed : colour::raised);
        g.fillRoundedRectangle(r, 6.f);
    }
    g.setFont(sans(16.f, Weight::medium));
    g.setColour(colour::text);
    g.drawText(shown, r.withTrimmedLeft(12.f).withTrimmedRight(30.f), juce::Justification::centredLeft, true);

    const auto c = juce::Point<float>(r.getRight() - 18.f, r.getCentreY());
    juce::Path chevron;
    chevron.startNewSubPath(c.x - 4.f, c.y - 2.f);
    chevron.lineTo(c.x, c.y + 2.f);
    chevron.lineTo(c.x + 4.f, c.y - 2.f);
    g.setColour(highlighted ? colour::text2 : colour::text3);
    g.strokePath(chevron, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
ChainEditor::ChainEditor(ChainProcessor &p)
    : juce::AudioProcessorEditor(p), proc(p), chainList(p), slotPanel(p)
{
    theme::apply(proc.getSetting("theme", "Warm"), proc.getSetting("accent", "Amber"));
    lookAndFeel.refreshColours();
    setLookAndFeel(&lookAndFeel);

    chainMenu.onClick = [this] { showChainMenu(); };
    addAndMakeVisible(chainMenu);

    undoButton.onClick = [this] {
        proc.undo();
        syncNow();
    };
    addChildComponent(undoButton);

    aboutButton.setButtonText(juce::String((int)Catalog::get().all().size()) + " effects by Airwindows");
    aboutButton.onClick = [this] { showAbout(); };
    addAndMakeVisible(aboutButton);

    listViewport.setViewedComponent(&chainList, false);
    listViewport.setScrollBarsShown(true, false);
    listViewport.setScrollBarThickness(8);
    addAndMakeVisible(listViewport);
    addAndMakeVisible(slotPanel);

    chainList.onSelect = [this](int s) { select(s); };
    chainList.onAdd = [this] { openPickerToAdd(); };
    chainList.onInsertAt = [this](int position) { openPickerToAdd(position); };
    chainList.onReplace = [this](int s) { openPickerToReplace(s); };
    chainList.onRemove = [this](int s) {
        proc.removeSlot(s);
        syncNow();
    };
    chainList.onDuplicate = [this](int s) {
        const int copy = proc.duplicateSlot(s);
        syncNow();
        if (copy >= 0)
            select(copy);
    };

    slotPanel.onAdd = [this] { openPickerToAdd(); };
    slotPanel.onReplace = [this] { openPickerToReplace(selectedSlot); };
    slotPanel.onRemove = [this] {
        proc.removeSlot(selectedSlot);
        syncNow();
    };
    slotPanel.onStep = [this](int direction) {
        const auto info = proc.getSlotInfo(selectedSlot);
        if (info.registryIndex < 0)
            return;
        proc.replaceEffect(selectedSlot, Catalog::get().neighbour(info.registryIndex, direction));
        syncNow();
    };

    setWantsKeyboardFocus(typeToSearch());
    setResizable(true, true);
    setResizeLimits(760, 500, 2400, 1600);
    setSize(juce::jlimit(760, 2400, p.editorSize.x), juce::jlimit(500, 1600, p.editorSize.y));

    syncNow();
    startTimerHz(30);
}

ChainEditor::~ChainEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

//==============================================================================
void ChainEditor::syncNow()
{
    seenVersion = proc.getModelVersion();
    const auto order = proc.getOrder();

    if (std::find(order.begin(), order.end(), selectedSlot) == order.end())
    {
        // The selected effect went away: stay at the same place in the chain.
        selectedSlot = order.empty() ? -1 : order[(size_t)juce::jlimit(0, (int)order.size() - 1, selectedPosition)];
    }
    else
    {
        selectedPosition = (int)(std::find(order.begin(), order.end(), selectedSlot) - order.begin());
    }

    chainMenu.setChainName(proc.getChainName());
    chainMenu.setSize(chainMenu.idealWidth(), chainMenu.getHeight());
    undoButton.setVisible(proc.canUndo());
    undoButton.setTooltip(proc.canUndo() ? "Undo " + proc.undoLabel() : juce::String());
    undoButton.setBounds(chainMenu.getRight() + 6, chainMenu.getY(), undoButton.idealWidth(), chainMenu.getHeight());
    chainList.setSelectedSlot(selectedSlot);
    chainList.rebuild();
    layoutList();
    showSelected();
}

void ChainEditor::select(int slot)
{
    selectedSlot = slot;
    const auto order = proc.getOrder();
    const auto it = std::find(order.begin(), order.end(), slot);
    if (it != order.end())
        selectedPosition = (int)(it - order.begin());
    chainList.setSelectedSlot(slot);
    showSelected();
}

void ChainEditor::showSelected()
{
    const int effect = proc.getSlotInfo(selectedSlot).registryIndex;
    if (selectedSlot != shownSlot || effect != shownEffect)
    {
        shownSlot = selectedSlot;
        shownEffect = effect;
        slotPanel.showSlot(selectedSlot);
    }
}

void ChainEditor::timerCallback() { pollNow(); }

void ChainEditor::pollNow()
{
    if (proc.getModelVersion() != seenVersion)
        syncNow();
    chainList.pollStates();
    slotPanel.pollBypass();

    // Type to search: take the keyboard while the mouse is here, hand it back
    // when it leaves. Keys we don't use go to the host either way.
    const bool browsing = picker != nullptr && picker->isVisible();
    if (typeToSearch() && !browsing && (about == nullptr || !about->isVisible()) && isShowing())
    {
        const bool over = isMouseOver(true);
        if (over && !hasKeyboardFocus(true))
            grabKeyboardFocus();
        else if (!over && hasKeyboardFocus(false))
            giveAwayKeyboardFocus();
    }
}

bool ChainEditor::typeToSearch() const { return proc.getSetting("typeToSearch", "1") == "1"; }

void ChainEditor::setTypeToSearch(bool on)
{
    proc.setSetting("typeToSearch", on ? "1" : "0");
    setWantsKeyboardFocus(on);
    if (!on)
        giveAwayKeyboardFocus();
}

void ChainEditor::setKnobs(bool on)
{
    proc.setSetting("knobs", on ? "1" : "0");
    shownSlot = -2;
    showSelected();
}

bool ChainEditor::keyPressed(const juce::KeyPress &key)
{
    if (!typeToSearch() || (picker != nullptr && picker->isVisible()) || (about != nullptr && about->isVisible()))
        return false;

    const auto mods = key.getModifiers();
    if (mods.isCtrlDown() || mods.isAltDown() || mods.isCommandDown())
        return false;

    const auto c = key.getTextCharacter();
    const bool letter = c < 128 && juce::CharacterFunctions::isLetterOrDigit((char)c);
    if (!letter && c != '/')
        return false; // space, arrows and the rest belong to the host

    openPickerToAdd();
    if (picker == nullptr || !picker->isVisible())
        return false;
    picker->setQuery(letter ? juce::String::charToString(c) : juce::String());
    return true;
}

//==============================================================================
Picker &ChainEditor::ensurePicker()
{
    if (picker == nullptr)
    {
        picker = std::make_unique<Picker>(proc);
        picker->onClose = [this] { closePicker(); };
        picker->onChoose = [this](int registryIndex, int replacing, bool keepOpen) {
            if (!keepOpen)
                closePicker();
            if (replacing >= 0)
            {
                // Picking the effect that's already there would only reset its controls.
                if (proc.getSlotInfo(replacing).registryIndex != registryIndex)
                    proc.replaceEffect(replacing, registryIndex);
                syncNow();
                select(replacing);
            }
            else
            {
                const int slot = proc.addEffect(registryIndex, insertPosition);
                if (insertPosition >= 0)
                    ++insertPosition; // the next one goes after this one
                syncNow();
                if (slot >= 0)
                    select(slot);
            }
        };
        addChildComponent(*picker);
    }
    picker->setBounds(getLocalBounds());
    picker->setVisible(true);
    picker->toFront(false);
    return *picker;
}

void ChainEditor::openPickerToAdd(int position)
{
    insertPosition = position;
    if (!proc.isFull())
        ensurePicker().openToAdd();
}

void ChainEditor::openPickerToReplace(int slot)
{
    if (proc.getSlotInfo(slot).registryIndex >= 0)
        ensurePicker().openToReplace(slot);
}

void ChainEditor::closePicker()
{
    if (picker != nullptr)
        picker->setVisible(false);
}

//==============================================================================
void ChainEditor::showChainMenu()
{
    juce::PopupMenu m;
    m.addItem("Save chain as...", [this] { saveChainAs(); });
    m.addItem("Open chain file...", [this] { openChainFile(); });

    auto files = ChainProcessor::chainsFolder().findChildFiles(juce::File::findFiles, false, "*.awchain");
    files.sort();
    if (!files.isEmpty())
    {
        m.addSeparator();
        m.addSectionHeader("Saved chains");
        for (const auto &f : files)
            m.addItem(f.getFileNameWithoutExtension(), [this, f] {
                proc.loadChain(f);
                syncNow();
            });
    }

    m.addSeparator();
    m.addItem(proc.canUndo() ? "Undo " + proc.undoLabel() : juce::String("Undo"), proc.canUndo(), false, [this] {
        proc.undo();
        syncNow();
    });
    m.addItem("Clear chain", !proc.getOrder().empty(), false, [this] {
        proc.clearChain();
        proc.setChainName({});
        syncNow();
    });
    m.addItem("Show chains folder", [] {
        auto folder = ChainProcessor::chainsFolder();
        folder.createDirectory();
        folder.revealToUser();
    });

    m.addSeparator();
    juce::PopupMenu themes, accents;
    for (const auto &p : theme::palettes())
        themes.addItem(p.name, true, p.name == theme::currentPalette(),
                       [this, name = p.name] { applyTheme(name, theme::currentAccent()); });
    for (const auto &a : theme::accents())
        accents.addItem(a.name, true, a.name == theme::currentAccent(),
                        [this, name = a.name] { applyTheme(theme::currentPalette(), name); });
    m.addSubMenu("Theme", themes);
    m.addSubMenu("Accent", accents);
    const bool knobs = proc.getSetting("knobs", "0") == "1";
    m.addItem("Knobs instead of faders", true, knobs, [this, knobs] { setKnobs(!knobs); });
    const bool typing = typeToSearch();
    m.addItem("Type to search", true, typing, [this, typing] { setTypeToSearch(!typing); });
    m.addItem("About Airwindows Chain", [this] { showAbout(); });

    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&chainMenu).withMinimumWidth(240));
}

void ChainEditor::applyTheme(const juce::String &palette, const juce::String &accent)
{
    theme::apply(palette, accent);
    proc.setSetting("theme", theme::currentPalette());
    proc.setSetting("accent", theme::currentAccent());
    lookAndFeel.refreshColours();
    picker.reset(); // its text fields hold colours
    shownSlot = -2; // re-lay the panel out with the new colours
    showSelected();
    sendLookAndFeelChange();
    repaint();
}

//==============================================================================
namespace
{
class AboutPanel : public juce::Component
{
  public:
    explicit AboutPanel(std::function<void()> close) : onClose(std::move(close))
    {
        link.setFont(sans(14.f), false, juce::Justification::centredLeft);
        link.setColour(juce::HyperlinkButton::textColourId, colour::accent);
        addAndMakeVisible(link);
        closeButton.onClick = [this] {
            if (onClose)
                onClose();
        };
        addAndMakeVisible(closeButton);
    }

    void resized() override
    {
        width = juce::jmin(560, getWidth() - 80);
        x = (getWidth() - width) / 2;

        juce::AttributedString s;
        s.setWordWrap(juce::AttributedString::byWord);
        s.setLineSpacing(5.f);
        const auto body = sans(14.5f);
        s.append("Up to sixteen Airwindows effects in one plugin, in any order, with Chris Johnson's own notes on "
                 "each one.\n\n",
                 body, colour::text2);
        s.append("The effects are by Chris Johnson (Airwindows, MIT). They are packaged for hosts by airwin2rack, "
                 "from BaconPaul and the Surge Synth Team (MIT). The plugin is built with JUCE, so the built "
                 "binary is GPLv3; the code of the plugin itself is MIT. Type is IBM Plex.\n\n",
                 body, colour::text2);
        s.append("Made by Sean Kani.", body, colour::text2);
        text.createLayout(s, (float)width);

        const int total = 74 + (int)std::ceil(text.getHeight()) + 16 + 24 + 28 + 32;
        top = juce::jmax(32, (getHeight() - total) / 2);
        link.setBounds(x, top + 74 + (int)std::ceil(text.getHeight()) + 16, width, 24);
        closeButton.setBounds(x + width - closeButton.idealWidth(), link.getBottom() + 28, closeButton.idealWidth(), 32);
    }

    void paint(juce::Graphics &g) override
    {
        g.fillAll(colour::window);
        g.setFont(sans(26.f, Weight::semibold));
        g.setColour(colour::text);
        g.drawText("Airwindows Chain", (float)x, (float)top, (float)width, 34.f, juce::Justification::centredLeft, false);
        g.setFont(mono(12.5f));
        g.setColour(colour::text3);
        g.drawText(juce::String("version ") + AWCHAIN_VERSION, (float)x, (float)top + 38.f, (float)width, 18.f,
                   juce::Justification::centredLeft, false);
        text.draw(g, juce::Rectangle<float>((float)x, (float)top + 74.f, (float)width, text.getHeight()));
    }

  private:
    std::function<void()> onClose;
    juce::HyperlinkButton link{"github.com/clotheshoesandwoes/airwindows-chain",
                               juce::URL("https://github.com/clotheshoesandwoes/airwindows-chain")};
    theme::TextButton closeButton{"Close"};
    juce::TextLayout text;
    int width{0}, x{0}, top{0};
};
} // namespace

void ChainEditor::showAbout()
{
    about = std::make_unique<AboutPanel>([this] { closeAbout(); });
    addAndMakeVisible(*about);
    about->setBounds(getLocalBounds());
    about->toFront(false);
}

void ChainEditor::closeAbout()
{
    if (about == nullptr)
        return;
    about->setVisible(false); // gone now; freed once its own click handler has returned
    juce::MessageManager::callAsync([safe = juce::Component::SafePointer<ChainEditor>(this)] {
        if (safe != nullptr && safe->about != nullptr && !safe->about->isVisible())
            safe->about.reset();
    });
}

void ChainEditor::saveChainAs()
{
    auto folder = ChainProcessor::chainsFolder();
    folder.createDirectory();
    auto name = proc.getChainName();
    if (name.isEmpty())
        name = "Untitled chain";

    chooser = std::make_unique<juce::FileChooser>("Save chain", folder.getChildFile(name + ".awchain"), "*.awchain");
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles |
                             juce::FileBrowserComponent::warnAboutOverwriting,
                         [this](const juce::FileChooser &fc) {
                             auto f = fc.getResult();
                             if (f == juce::File())
                                 return;
                             if (!f.hasFileExtension("awchain"))
                                 f = f.withFileExtension("awchain");
                             proc.saveChain(f);
                             syncNow();
                         });
}

void ChainEditor::openChainFile()
{
    auto folder = ChainProcessor::chainsFolder();
    chooser = std::make_unique<juce::FileChooser>("Open chain", folder.isDirectory() ? folder : juce::File(),
                                                  "*.awchain");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                         [this](const juce::FileChooser &fc) {
                             const auto f = fc.getResult();
                             if (f.existsAsFile())
                             {
                                 proc.loadChain(f);
                                 syncNow();
                             }
                         });
}

//==============================================================================
int ChainEditor::listWidth() const { return juce::jlimit(270, 340, (int)((float)getWidth() * 0.32f)); }

void ChainEditor::layoutList()
{
    const int h = chainList.preferredHeight();
    const bool scrolls = h > listViewport.getHeight();
    chainList.setSize(listViewport.getWidth() - (scrolls ? listViewport.getScrollBarThickness() : 0), h);
}

void ChainEditor::resized()
{
    proc.editorSize = {getWidth(), getHeight()};
    const int lw = listWidth();

    chainMenu.setBounds(16, (headerHeight - 34) / 2, chainMenu.idealWidth(), 34);
    undoButton.setBounds(chainMenu.getRight() + 6, chainMenu.getY(), undoButton.idealWidth(), 34);
    aboutButton.setBounds(getWidth() - 16 - aboutButton.idealWidth(), chainMenu.getY(), aboutButton.idealWidth(), 34);
    if (about != nullptr)
        about->setBounds(getLocalBounds());
    listViewport.setBounds(0, headerHeight, lw, getHeight() - headerHeight);
    layoutList();
    slotPanel.setBounds(lw + 1, headerHeight, getWidth() - lw - 1, getHeight() - headerHeight);
    if (picker != nullptr)
        picker->setBounds(getLocalBounds());
}

void ChainEditor::paint(juce::Graphics &g)
{
    g.fillAll(colour::window);
    const int lw = listWidth();
    g.setColour(colour::side);
    g.fillRect(0, headerHeight, lw, getHeight() - headerHeight);
    g.setColour(colour::line);
    g.fillRect(0, headerHeight - 1, getWidth(), 1);
    g.fillRect(lw, headerHeight, 1, getHeight() - headerHeight);

}
} // namespace awchain
