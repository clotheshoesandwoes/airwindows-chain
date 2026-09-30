#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace awchain::theme
{
// The active palette. Every paint routine reads these; changing theme assigns
// them all at once on the message thread and repaints.
namespace colour
{
inline juce::Colour window{0xff141312};
inline juce::Colour side{0xff191817};
inline juce::Colour raised{0xff25231f};
inline juce::Colour hover{0xff1e1c1a};
inline juce::Colour pressed{0xff2e2b26};
inline juce::Colour lifted{0xff2a2723};
inline juce::Colour line{0xff2d2a26};
inline juce::Colour track{0xff34302b};
inline juce::Colour toggleOn{0xff4c4741};
inline juce::Colour toggleOff{0xff3f3b35};
inline juce::Colour scroll{0xff36322d};
inline juce::Colour scrollHover{0xff4d4842};
inline juce::Colour menuHighlight{0xff322e29};
inline juce::Colour text{0xfff1eee8};
inline juce::Colour text2{0xffada79e};
inline juce::Colour text3{0xff777168};
inline juce::Colour accent{0xffe4a63f};
inline juce::Colour onAccent{0xff1c1405};
inline juce::Colour hot{0xffe25f4c}; // clipping
inline bool light{false};
} // namespace colour

struct Palette
{
    juce::String name;
    bool light{false};
    juce::Colour window, side, raised, hover, pressed, lifted, line, track, toggleOn, toggleOff, scroll,
        scrollHover, menuHighlight, text, text2, text3, hot;
};

struct Accent
{
    juce::String name;
    juce::Colour onDark, textOnDark;   // for dark palettes
    juce::Colour onLight, textOnLight; // for light palettes
};

const std::vector<Palette> &palettes();
const std::vector<Accent> &accents();
juce::String currentPalette();
juce::String currentAccent();
// Unknown names fall back to the first entry.
void apply(const juce::String &paletteName, const juce::String &accentName);

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
    void refreshColours(); // after theme::apply

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

// A star. Filled when the effect is a favourite.
class StarButton : public juce::Button
{
  public:
    StarButton();
    void setFilled(bool);
    bool isFilled() const { return filled; }
    void paintButton(juce::Graphics &, bool highlighted, bool down) override;

  private:
    bool filled{false};
};
} // namespace awchain::theme
