#pragma once

#include "../ChainProcessor.h"
#include "SlotPanel.h"
#include "Theme.h"

#include <map>

namespace awchain
{
// Every effect, searchable. Hover one to read about it, click to use it.
class Picker : public juce::Component, private juce::ListBoxModel, private juce::KeyListener
{
  public:
    explicit Picker(ChainProcessor &);
    ~Picker() override;

    void openToAdd();
    void openToReplace(int slot);

    // For tests and screenshots
    void setQuery(const juce::String &);
    void setVocalsOnly(bool); // the "For vocals" switch, as a click on it would
    void highlightRow(int row);
    int getNumResults() const { return (int)items.size(); }

    // keepOpen: shift was held, so the user wants to add more.
    std::function<void(int registryIndex, int replacingSlot, bool keepOpen)> onChoose;
    std::function<void()> onClose;

    void paint(juce::Graphics &) override;
    void paintOverChildren(juce::Graphics &) override;
    void resized() override;

  private:
    int getNumRows() override;
    void paintListBoxItem(int row, juce::Graphics &, int width, int height, bool selected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent &) override;
    void selectedRowsChanged(int lastRowSelected) override;
    void returnKeyPressed(int row) override;
    bool keyPressed(const juce::KeyPress &, juce::Component *) override;

    struct Source
    {
        juce::String key, label;
        int count{0};
        bool gapBefore{false};
        bool toggle{false}, on{false}; // a switch drawn among the sources, not a list
    };

    class SourceList : public juce::Component
    {
      public:
        std::vector<Source> sources;
        juce::String selectedKey;
        std::function<void(const juce::String &)> onPick;

        int preferredHeight() const;
        void paint(juce::Graphics &) override;
        void mouseMove(const juce::MouseEvent &) override;
        void mouseExit(const juce::MouseEvent &) override;
        void mouseDown(const juce::MouseEvent &) override;

      private:
        int hover{-1};
        juce::Rectangle<float> rowBounds(int index) const;
        int rowAt(juce::Point<float>) const;
    };

    class Preview : public juce::Component
    {
      public:
        Preview();
        void show(int registryIndex, const juce::StringArray &controls);
        void setUseLabel(const juce::String &, const juce::String &hint);
        void setFavourite(bool);
        std::function<void()> onUse;
        std::function<void()> onToggleFavourite;

        void resized() override;
        void paint(juce::Graphics &) override;

      private:
        struct Body : public juce::Component
        {
            juce::String name, category;
            juce::TextLayout summary, controls;
            bool hasControls{false};
            int controlsY{0};
            DocView docs;
            int layoutFor(int width, const juce::String &summaryText, const juce::String &controlText);
            void paint(juce::Graphics &) override;
        };

        int registryIndex{-1};
        juce::String summaryText, controlText, hint;
        theme::TextButton use{"Add to chain", true};
        theme::StarButton star;
        juce::Viewport viewport;
        Body body;
    };

    ChainProcessor &proc;
    juce::TextEditor search;
    theme::TextButton cancel{"Cancel"};
    juce::Viewport sourceViewport;
    SourceList sourceList;
    juce::ListBox results;
    Preview preview;

    std::vector<int> items;
    juce::String source{"recommended"};
    bool vocalsOnly{false}; // the "For vocals" switch; kept in the shared settings
    juce::String rememberedSource; // where browsing was before a replace took over
    int replacingSlot{-1};
    std::map<int, juce::StringArray> controlCache;

    void open();
    void buildSources();
    void setSource(const juce::String &key);
    void refreshItems();
    void choose(int row, bool keepOpen = false);
    void updatePreview();
    juce::StringArray controlNames(int registryIndex);
    bool searching() const;
    int countShown(std::vector<int> list) const; // a list's size, after the vocals switch
};
} // namespace awchain
