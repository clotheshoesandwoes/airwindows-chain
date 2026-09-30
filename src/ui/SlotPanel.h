#pragma once

#include "../ChainProcessor.h"
#include "Theme.h"

namespace awchain
{
// One control: name, a horizontal fader, and the value the effect reports.
// Drag anywhere on the row, click the track to jump, shift for fine moves,
// double-click to reset, double-click the value to type one.
class ParamRow : public juce::Component
{
  public:
    using Format = std::function<juce::String(float)>;
    using Parse = std::function<std::optional<float>(const juce::String &)>;

    ParamRow(juce::RangedAudioParameter &, const juce::String &label, Format, Parse);
    ~ParamRow() override;

    void paint(juce::Graphics &) override;
    void resized() override;
    void mouseEnter(const juce::MouseEvent &) override;
    void mouseExit(const juce::MouseEvent &) override;
    void mouseDown(const juce::MouseEvent &) override;
    void mouseDrag(const juce::MouseEvent &) override;
    void mouseUp(const juce::MouseEvent &) override;
    void mouseDoubleClick(const juce::MouseEvent &) override;

    static constexpr int height = 40;

  private:
    juce::RangedAudioParameter &param;
    juce::ParameterAttachment attachment;
    juce::String label;
    Format format;
    Parse parse;
    float value{0.f};
    bool hovering{false}, dragging{false};
    float lastX{0.f};
    std::unique_ptr<juce::TextEditor> entry;

    juce::Rectangle<float> labelArea() const;
    juce::Rectangle<float> trackArea() const;
    juce::Rectangle<float> valueArea() const;
    void set(float v, bool asGesture);
    void openEntry();
    void closeEntry(bool commit);
};

// Chris's notes for the effect, set as readable paragraphs.
class DocView : public juce::Component
{
  public:
    void setDocument(const juce::String &heading, const juce::StringArray &paragraphs);
    int heightForWidth(int width);
    void paint(juce::Graphics &) override;
    void resized() override;

  private:
    juce::String heading;
    juce::StringArray paragraphs;
    std::vector<juce::TextLayout> layouts;
    int laidOutWidth{-1};
    void layout(int width);
};

// The selected effect: what it is, its controls, and what Chris says about it.
class SlotPanel : public juce::Component
{
  public:
    explicit SlotPanel(ChainProcessor &);

    void showSlot(int slot); // -1 shows the empty state
    int getSlot() const { return slot; }
    void pollBypass();       // bypass can be automated

    std::function<void()> onReplace, onRemove, onAdd;
    std::function<void(int direction)> onStep;

    void paint(juce::Graphics &) override;
    void resized() override;

  private:
    ChainProcessor &proc;
    int slot{-1};
    int registryIndex{-1};
    bool chainEmpty{true};
    bool bypassed{false};
    bool buttonsInline{true};

    juce::String title, category, summary;
    juce::TextLayout summaryLayout;
    int headerHeight{0};

    theme::StepButton previous{-1}, next{1};
    theme::StarButton star;
    theme::TextButton bypass{"Bypass"}, replace{"Replace"}, remove{"Remove"}, add{"Add effect", true};
    std::vector<juce::Component *> headerButtons();

    struct Body : public juce::Component
    {
        std::vector<std::unique_ptr<ParamRow>> rows;
        std::unique_ptr<ParamRow> mix;
        DocView docs;
        bool noControls{false};
        int dividerY{0}, dividerWidth{0};
        int layoutFor(int width);
        void paint(juce::Graphics &) override;
    };
    juce::Viewport viewport;
    Body body;

    void layoutHeader(int width);
};
} // namespace awchain
