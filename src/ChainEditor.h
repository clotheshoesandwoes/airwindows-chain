#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "ChainProcessor.h"
#include "ui/ChainList.h"
#include "ui/Picker.h"
#include "ui/SlotPanel.h"
#include "ui/Theme.h"

namespace awchain
{
class ChainEditor : public juce::AudioProcessorEditor, private juce::Timer
{
  public:
    explicit ChainEditor(ChainProcessor &);
    ~ChainEditor() override;

    void paint(juce::Graphics &) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress &) override;

    // Also used by the test harness
    void syncNow();
    void select(int slot);
    void openPickerToAdd(int position = -1);
    void openPickerToReplace(int slot);
    void closePicker();
    Picker *getPicker() { return picker.get(); }
    void pollNow(); // what the timer does, for tests
    void applyTheme(const juce::String &palette, const juce::String &accent);
    void setKnobs(bool);
    void setTypeToSearch(bool);
    bool typeToSearch() const;
    void showAbout();
    void closeAbout();

    static constexpr int headerHeight = 56;

  private:
    class ChainMenuButton : public juce::Button
    {
      public:
        ChainMenuButton();
        void setChainName(const juce::String &);
        int idealWidth() const;
        void paintButton(juce::Graphics &, bool highlighted, bool down) override;

      private:
        juce::String shown{"Untitled chain"};
    };

    ChainProcessor &proc;
    theme::LookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips{this, 700};
    ChainMenuButton chainMenu;
    theme::TextButton undoButton{"Undo"}, aboutButton{"Airwindows"};
    juce::Viewport listViewport;
    ChainList chainList;
    SlotPanel slotPanel;
    std::unique_ptr<Picker> picker;
    std::unique_ptr<juce::Component> about;
    std::unique_ptr<juce::FileChooser> chooser;
    int insertPosition{-1};

    // The size the window should open at. FL Studio hands a reopened editor a
    // size of its own after attaching it; for the first moments the editor
    // answers with this one, then takes whatever the user makes it.
    juce::Point<int> wantedSize;
    int settleTicks{0};
    bool sizeSettled{false};

    uint32_t seenVersion{0};
    int selectedSlot{-1}, selectedPosition{0};
    int shownSlot{-2}, shownEffect{-2};

    void timerCallback() override;
    void showSelected();
    void layoutList();
    void showChainMenu();
    void saveChainAs();
    void openChainFile();
    Picker &ensurePicker();
    int listWidth() const;
};
} // namespace awchain
