#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace awchain::theme
{
namespace colour
{
// Warm near-blacks, one amber accent for what is selected or set.
inline const juce::Colour window{0xff141312};
inline const juce::Colour side{0xff191817};
inline const juce::Colour raised{0xff25231f};
inline const juce::Colour hover{0xff1e1c1a};
inline const juce::Colour line{0xff2d2a26};
inline const juce::Colour track{0xff34302b};
inline const juce::Colour text{0xfff1eee8};
inline const juce::Colour text2{0xffada79e};
inline const juce::Colour text3{0xff777168};
inline const juce::Colour accent{0xffe4a63f};
inline const juce::Colour onAccent{0xff1c1405};
inline const juce::Colour hot{0xffe25f4c}; // clipping
} // namespace colour

enum class Weight
{
    regular,
    medium,
    semibold
};

// Sizes are em sizes in pixels, like CSS font-size.
juce::Font sans(float size, Weight weight = Weight::regular);
juce::Font mono(float size);
float textWidth(const juce::Font &, const juce::String &);

class LookAndFeel : public juce::LookAndFeel_V4
{
  public:
    LookAndFeel();

    juce::Typeface::Ptr getTypefaceForFont(const juce::Font &) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground(juce::Graphics &, int width, int height) override;
    void drawScrollbar(juce::Graphics &, juce::ScrollBar &, int x, int y, int width, int height,
                       bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                       bool isMouseOver, bool isMouseDown) override;
    int getDefaultScrollbarWidth() override { return 10; }
    void fillTextEditorBackground(juce::Graphics &, int width, int height, juce::TextEditor &) override;
    void drawTextEditorOutline(juce::Graphics &, int width, int height, juce::TextEditor &) override;
    void drawCornerResizer(juce::Graphics &, int width, int height, bool isMouseOver, bool isMouseDragging) override;

  private:
    std::shared_ptr<void> keepFacesLoaded;
};

// A text button with no chrome. Primary buttons get the accent fill.
class TextButton : public juce::Button
{
  public:
    TextButton(const juce::String &text, bool primary = false);
    void paintButton(juce::Graphics &, bool highlighted, bool down) override;
    int idealWidth() const;

  private:
    bool primary;
};

// A small chevron button for stepping through effects.
class StepButton : public juce::Button
{
  public:
    explicit StepButton(int direction);
    void paintButton(juce::Graphics &, bool highlighted, bool down) override;

  private:
    int direction;
};
} // namespace awchain::theme
