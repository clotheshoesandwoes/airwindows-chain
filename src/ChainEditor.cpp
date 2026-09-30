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
        g.setColour(down ? juce::Colour(0xff2e2b26) : colour::raised);
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
    setLookAndFeel(&lookAndFeel);
    tooltips.setColour(juce::TooltipWindow::backgroundColourId, colour::raised);
    tooltips.setColour(juce::TooltipWindow::textColourId, colour::text);
    tooltips.setColour(juce::TooltipWindow::outlineColourId, colour::line);

    chainMenu.onClick = [this] { showChainMenu(); };
    addAndMakeVisible(chainMenu);

    listViewport.setViewedComponent(&chainList, false);
    listViewport.setScrollBarsShown(true, false);
    listViewport.setScrollBarThickness(8);
    addAndMakeVisible(listViewport);
    addAndMakeVisible(slotPanel);

    chainList.onSelect = [this](int s) { select(s); };
    chainList.onAdd = [this] { openPickerToAdd(); };
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
                const int slot = proc.addEffect(registryIndex);
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

void ChainEditor::openPickerToAdd()
{
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

    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&chainMenu).withMinimumWidth(240));
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

    g.setFont(sans(12.5f));
    g.setColour(colour::text3);
    g.drawText(juce::String((int)Catalog::get().all().size()) + " effects by Airwindows",
               juce::Rectangle<float>((float)getWidth() - 260.f, 0.f, 240.f, (float)headerHeight),
               juce::Justification::centredRight, false);
}
} // namespace awchain
