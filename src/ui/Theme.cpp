#include "Theme.h"
#include "AwChainAssets.h"

namespace awchain::theme
{
namespace
{
struct Faces
{
    juce::Typeface::Ptr regular, medium, semibold, mono;

    Faces()
    {
        using namespace AwChainAssets;
        regular = juce::Typeface::createSystemTypefaceFor(IBMPlexSansRegular_ttf, IBMPlexSansRegular_ttfSize);
        medium = juce::Typeface::createSystemTypefaceFor(IBMPlexSansMedium_ttf, IBMPlexSansMedium_ttfSize);
        semibold = juce::Typeface::createSystemTypefaceFor(IBMPlexSansSemiBold_ttf, IBMPlexSansSemiBold_ttfSize);
        mono = juce::Typeface::createSystemTypefaceFor(IBMPlexMonoRegular_ttf, IBMPlexMonoRegular_ttfSize);
    }
};

// Every LookAndFeel keeps one of these alive, so while an editor is open the
// typefaces are loaded once and shared. Fonts hold their own typeface reference.
using SharedFaces = juce::SharedResourcePointer<Faces>;

juce::Font make(const juce::Typeface::Ptr &face, float size)
{
    return juce::Font(juce::FontOptions(face).withPointHeight(size));
}
} // namespace

juce::Font sans(float size, Weight weight)
{
    SharedFaces f;
    switch (weight)
    {
    case Weight::medium:
        return make(f->medium, size);
    case Weight::semibold:
        return make(f->semibold, size);
    case Weight::regular:
        break;
    }
    return make(f->regular, size);
}

juce::Font mono(float size)
{
    SharedFaces f;
    return make(f->mono, size);
}

float textWidth(const juce::Font &font, const juce::String &text)
{
    return juce::GlyphArrangement::getStringWidth(font, text);
}

//==============================================================================
LookAndFeel::LookAndFeel() : keepFacesLoaded(std::make_shared<SharedFaces>())
{
    using namespace colour;
    setColour(juce::ResizableWindow::backgroundColourId, window);
    setColour(juce::PopupMenu::backgroundColourId, raised);
    setColour(juce::PopupMenu::textColourId, text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff322e29));
    setColour(juce::PopupMenu::highlightedTextColourId, text);
    setColour(juce::PopupMenu::headerTextColourId, text3);
    setColour(juce::TextEditor::backgroundColourId, raised);
    setColour(juce::TextEditor::textColourId, text);
    setColour(juce::TextEditor::highlightColourId, accent.withAlpha(0.35f));
    setColour(juce::TextEditor::highlightedTextColourId, text);
    setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::CaretComponent::caretColourId, accent);
    setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    setColour(juce::ScrollBar::thumbColourId, juce::Colour(0xff3c3833));
    setColour(juce::ScrollBar::trackColourId, juce::Colours::transparentBlack);
}

juce::Typeface::Ptr LookAndFeel::getTypefaceForFont(const juce::Font &font)
{
    SharedFaces f;
    const auto style = font.getTypefaceStyle();
    if (font.isBold() || style.containsIgnoreCase("bold"))
        return f->semibold;
    if (style.containsIgnoreCase("medium"))
        return f->medium;
    return f->regular;
}

juce::Font LookAndFeel::getPopupMenuFont() { return sans(14.f); }

void LookAndFeel::drawPopupMenuBackground(juce::Graphics &g, int width, int height)
{
    g.fillAll(colour::raised);
    g.setColour(colour::line);
    g.drawRect(0, 0, width, height, 1);
}

void LookAndFeel::drawScrollbar(juce::Graphics &g, juce::ScrollBar &, int x, int y, int width,
                                int height, bool vertical, int thumbStart, int thumbSize,
                                bool isMouseOver, bool isMouseDown)
{
    if (thumbSize <= 0)
        return;
    juce::Rectangle<float> thumb;
    if (vertical)
        thumb = {(float)x + (float)width * 0.5f - 2.f, (float)thumbStart + 2.f, 4.f, (float)thumbSize - 4.f};
    else
        thumb = {(float)thumbStart + 2.f, (float)y + (float)height * 0.5f - 2.f, (float)thumbSize - 4.f, 4.f};
    g.setColour(isMouseOver || isMouseDown ? juce::Colour(0xff4d4842) : juce::Colour(0xff36322d));
    g.fillRoundedRectangle(thumb, 2.f);
}

void LookAndFeel::fillTextEditorBackground(juce::Graphics &g, int width, int height, juce::TextEditor &ed)
{
    g.setColour(ed.findColour(juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle(juce::Rectangle<float>(0.f, 0.f, (float)width, (float)height), 6.f);
}

void LookAndFeel::drawTextEditorOutline(juce::Graphics &, int, int, juce::TextEditor &) {}

void LookAndFeel::drawCornerResizer(juce::Graphics &g, int w, int h, bool isMouseOver, bool isMouseDragging)
{
    g.setColour(isMouseOver || isMouseDragging ? colour::text2 : colour::text3.withAlpha(0.6f));
    for (int i = 0; i < 2; ++i)
    {
        const float o = 4.f + (float)i * 4.f;
        g.drawLine((float)w - o, (float)h - 2.f, (float)w - 2.f, (float)h - o, 1.f);
    }
}

//==============================================================================
TextButton::TextButton(const juce::String &text, bool isPrimary) : juce::Button(text), primary(isPrimary)
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

int TextButton::idealWidth() const
{
    return (int)std::ceil(textWidth(sans(14.f, Weight::medium), getButtonText())) + (primary ? 36 : 20);
}

void TextButton::paintButton(juce::Graphics &g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat();
    const auto font = sans(14.f, Weight::medium);
    g.setFont(font);

    if (primary)
    {
        auto fill = colour::accent;
        if (!isEnabled())
            fill = colour::raised;
        else if (down)
            fill = fill.darker(0.15f);
        else if (highlighted)
            fill = fill.brighter(0.08f);
        g.setColour(fill);
        g.fillRoundedRectangle(r, 6.f);
        g.setColour(isEnabled() ? colour::onAccent : colour::text3);
        g.drawText(getButtonText(), r, juce::Justification::centred, false);
        return;
    }

    if (highlighted || down)
    {
        g.setColour(down ? juce::Colour(0xff2e2b26) : colour::raised);
        g.fillRoundedRectangle(r, 6.f);
    }
    g.setColour(!isEnabled() ? colour::text3 : (highlighted ? colour::text : colour::text2));
    g.drawText(getButtonText(), r, juce::Justification::centred, false);
}

StepButton::StepButton(int dir) : juce::Button(dir < 0 ? "Previous" : "Next"), direction(dir)
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void StepButton::paintButton(juce::Graphics &g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat();
    if (highlighted || down)
    {
        g.setColour(down ? juce::Colour(0xff2e2b26) : colour::raised);
        g.fillRoundedRectangle(r, 6.f);
    }
    const auto c = r.getCentre();
    juce::Path p;
    const float s = 4.f;
    if (direction < 0)
    {
        p.startNewSubPath(c.x + s * 0.5f, c.y - s);
        p.lineTo(c.x - s * 0.5f, c.y);
        p.lineTo(c.x + s * 0.5f, c.y + s);
    }
    else
    {
        p.startNewSubPath(c.x - s * 0.5f, c.y - s);
        p.lineTo(c.x + s * 0.5f, c.y);
        p.lineTo(c.x - s * 0.5f, c.y + s);
    }
    g.setColour(highlighted ? colour::text : colour::text2);
    g.strokePath(p, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
} // namespace awchain::theme
