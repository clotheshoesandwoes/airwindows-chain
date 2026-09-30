#pragma once

#include "../ChainProcessor.h"
#include "Theme.h"

namespace awchain
{
// The chain, top to bottom, the way the audio runs through it. Eight slots are
// always on show; more appear as they fill, up to sixteen. Click to select,
// drag to reorder, flip the switch to bypass, click an empty slot to add.
class ChainList : public juce::Component, private juce::Timer
{
  public:
    explicit ChainList(ChainProcessor &);

    void rebuild();    // the chain changed shape
    void pollStates(); // bypass, mix and levels move on their own
    void setSelectedSlot(int);
    int getSelectedSlot() const { return selected; }
    int preferredHeight() const;

    std::function<void(int slot)> onSelect;
    std::function<void()> onAdd;
    std::function<void(int slot)> onReplace;
    std::function<void(int slot)> onRemove;
    std::function<void(int slot)> onDuplicate;
    std::function<void(int position)> onInsertAt; // open the browser to add at a position

    void paint(juce::Graphics &) override;
    void mouseMove(const juce::MouseEvent &) override;
    void mouseExit(const juce::MouseEvent &) override;
    void mouseDown(const juce::MouseEvent &) override;
    void mouseDrag(const juce::MouseEvent &) override;
    void mouseUp(const juce::MouseEvent &) override;
    void mouseDoubleClick(const juce::MouseEvent &) override;

    static constexpr int rowHeight = 56;
    static constexpr int topPad = 48;
    static constexpr int bottomPad = 52;
    static constexpr int alwaysShown = 8;
    static constexpr float spineX = 30.f;

  private:
    struct Row
    {
        int slot{-1};
        juce::String name, category;
        bool bypassed{false};
        float mix{1.f};
        float level{0.f}; // peak after this effect, decaying
        float y{0.f};     // where it is drawn; eases toward its place while dragging
    };

    ChainProcessor &proc;
    std::vector<Row> rows; // filled slots only
    int selected{-1};
    int hoverIndex{-1}; // any visible slot, filled or empty
    bool hoverToggle{false};
    float inputLevel{0.f}, outputLevel{0.f};

    int pressedRow{-1};
    bool dragging{false};
    float grabOffset{0.f}, dragTop{0.f};
    int dropIndex{-1};
    juce::Point<float> pressPoint;

    int visibleSlots() const;
    float slotTop(int index) const { return (float)(topPad + index * rowHeight); }
    int indexAt(float y) const;
    juce::Rectangle<float> toggleBounds(float rowY) const;
    juce::Rectangle<float> meterBounds(float centreY, float height) const;
    float targetY(int index) const;
    bool settle(); // one animation step; true while anything still moves
    void timerCallback() override;
    void showMenu(int rowIndex);
    void drawRow(juce::Graphics &, const Row &, int number, bool lifted) const;
    void drawEmpty(juce::Graphics &, int index) const;
    void drawMeter(juce::Graphics &, float level, juce::Rectangle<float>, bool dim) const;
};
} // namespace awchain
