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
        regular = juce::Typeface::createSystemTypefaceFor(IBMPlexSansRegular_ttf, (size_t)IBMPlexSansRegular_ttfSize);
        medium = juce::Typeface::createSystemTypefaceFor(IBMPlexSansMedium_ttf, (size_t)IBMPlexSansMedium_ttfSize);
        semibold = juce::Typeface::createSystemTypefaceFor(IBMPlexSansSemiBold_ttf, (size_t)IBMPlexSansSemiBold_ttfSize);
        mono = juce::Typeface::createSystemTypefaceFor(IBMPlexMonoRegular_ttf, (size_t)IBMPlexMonoRegular_ttfSize);
    }
};

// Every LookAndFeel keeps one of these alive, so while an editor is open the
// typefaces are loaded once and shared. Fonts hold their own typeface reference.
using SharedFaces = juce::SharedResourcePointer<Faces>;

juce::Font make(const juce::Typeface::Ptr &face, float size)
{
    return juce::Font(juce::FontOptions(face).withPointHeight(size));
}

juce::String activePalette{"Warm"}, activeAccent{"Amber"};
} // namespace

const std::vector<Palette> &palettes()
{
    static const std::vector<Palette> list{
        // name     light  window      side        raised      hover       pressed     lifted      line        track       toggleOn    toggleOff   scroll      scrollHover menuHi      text        text2       text3       hot
        {"Warm", false, juce::Colour(0xff141312), juce::Colour(0xff191817), juce::Colour(0xff25231f), juce::Colour(0xff1e1c1a), juce::Colour(0xff2e2b26), juce::Colour(0xff2a2723), juce::Colour(0xff2d2a26), juce::Colour(0xff34302b), juce::Colour(0xff4c4741), juce::Colour(0xff3f3b35), juce::Colour(0xff36322d), juce::Colour(0xff4d4842), juce::Colour(0xff322e29), juce::Colour(0xfff1eee8), juce::Colour(0xffada79e), juce::Colour(0xff777168), juce::Colour(0xffe25f4c)},
        {"Cool", false, juce::Colour(0xff121417), juce::Colour(0xff171a1e), juce::Colour(0xff222630), juce::Colour(0xff1c2026), juce::Colour(0xff2c313b), juce::Colour(0xff282d36), juce::Colour(0xff2a2f38), juce::Colour(0xff323845), juce::Colour(0xff46505e), juce::Colour(0xff3a4250), juce::Colour(0xff333a46), juce::Colour(0xff4a5262), juce::Colour(0xff2d323d), juce::Colour(0xffecf0f5), juce::Colour(0xffa3acb8), juce::Colour(0xff6e7785), juce::Colour(0xffe25f4c)},
        {"Black", false, juce::Colour(0xff000000), juce::Colour(0xff0a0a0a), juce::Colour(0xff1a1a1a), juce::Colour(0xff111111), juce::Colour(0xff262626), juce::Colour(0xff202020), juce::Colour(0xff222222), juce::Colour(0xff2a2a2a), juce::Colour(0xff444444), juce::Colour(0xff333333), juce::Colour(0xff303030), juce::Colour(0xff484848), juce::Colour(0xff2a2a2a), juce::Colour(0xffffffff), juce::Colour(0xffa0a0a0), juce::Colour(0xff666666), juce::Colour(0xffe25f4c)},
        {"Light", true, juce::Colour(0xfff4f1eb), juce::Colour(0xffece8e1), juce::Colour(0xffe1dcd3), juce::Colour(0xffe8e4dd), juce::Colour(0xffd6d0c6), juce::Colour(0xffe6e1d9), juce::Colour(0xffd5cfc5), juce::Colour(0xffcdc6bb), juce::Colour(0xff6f675c), juce::Colour(0xffbdb6ab), juce::Colour(0xffc3bcb1), juce::Colour(0xffa8a196), juce::Colour(0xffdbd5cb), juce::Colour(0xff1f1c18), juce::Colour(0xff5a554e), juce::Colour(0xff8a847a), juce::Colour(0xffc9412f)},
    };
    return list;
}

const std::vector<Accent> &accents()
{
    static const std::vector<Accent> list{
        {"Amber", juce::Colour(0xffe4a63f), juce::Colour(0xff1c1405), juce::Colour(0xffb9760f), juce::Colour(0xfffff7e8)},
        {"Coral", juce::Colour(0xffe2725b), juce::Colour(0xff1d0c08), juce::Colour(0xffc94b34), juce::Colour(0xfffff1ee)},
        {"Mint", juce::Colour(0xff6fd1a2), juce::Colour(0xff06231a), juce::Colour(0xff1f8a5c), juce::Colour(0xffeefaf3)},
        {"Sky", juce::Colour(0xff6cb6ff), juce::Colour(0xff061b2e), juce::Colour(0xff1f6fc2), juce::Colour(0xffeef5ff)},
        {"Lilac", juce::Colour(0xffb79cff), juce::Colour(0xff190f33), juce::Colour(0xff6a4fc2), juce::Colour(0xfff3efff)},
        {"Plain", juce::Colour(0xfff1eee8), juce::Colour(0xff141312), juce::Colour(0xff1f1c18), juce::Colour(0xfff4f1eb)},
    };
    return list;
}

juce::String currentPalette() { return activePalette; }
juce::String currentAccent() { return activeAccent; }

void apply(const juce::String &paletteName, const juce::String &accentName)
{
    const Palette *p = &palettes().front();
    for (const auto &candidate : palettes())
        if (candidate.name.equalsIgnoreCase(paletteName))
            p = &candidate;
    const Accent *a = &accents().front();
    for (const auto &candidate : accents())
        if (candidate.name.equalsIgnoreCase(accentName))
            a = &candidate;
    activePalette = p->name;
    activeAccent = a->name;

    colour::light = p->light;
    colour::window = p->window;
    colour::side = p->side;
    colour::raised = p->raised;
    colour::hover = p->hover;
    colour::pressed = p->pressed;
    colour::lifted = p->lifted;
    colour::line = p->line;
    colour::track = p->track;
    colour::toggleOn = p->toggleOn;
    colour::toggleOff = p->toggleOff;
    colour::scroll = p->scroll;
    colour::scrollHover = p->scrollHover;
    colour::menuHighlight = p->menuHighlight;
    colour::text = p->text;
    colour::text2 = p->text2;
    colour::text3 = p->text3;
    colour::hot = p->hot;
    colour::accent = p->light ? a->onLight : a->onDark;
    colour::onAccent = p->light ? a->textOnLight : a->textOnDark;
}

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
LookAndFeel::LookAndFeel() : keepFacesLoaded(std::make_shared<SharedFaces>()) { refreshColours(); }

void LookAndFeel::refreshColours()
{
    using namespace colour;
    setColour(juce::ResizableWindow::backgroundColourId, window);
    setColour(juce::PopupMenu::backgroundColourId, raised);
    setColour(juce::PopupMenu::textColourId, text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, menuHighlight);
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
    setColour(juce::ScrollBar::thumbColourId, scroll);
    setColour(juce::ScrollBar::trackColourId, juce::Colours::transparentBlack);
    setColour(juce::TooltipWindow::backgroundColourId, raised);
    setColour(juce::TooltipWindow::textColourId, text);
    setColour(juce::TooltipWindow::outlineColourId, line);
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
    g.setColour(isMouseOver || isMouseDown ? colour::scrollHover : colour::scroll);
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
        g.setColour(down ? colour::pressed : colour::raised);
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
        g.setColour(down ? colour::pressed : colour::raised);
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

StarButton::StarButton() : juce::Button("Favourite") { setMouseCursor(juce::MouseCursor::PointingHandCursor); }

void StarButton::setFilled(bool f)
{
    if (filled != f)
    {
        filled = f;
        repaint();
    }
}

void StarButton::paintButton(juce::Graphics &g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat();
    if (highlighted || down)
    {
        g.setColour(down ? colour::pressed : colour::raised);
        g.fillRoundedRectangle(r, 6.f);
    }
    juce::Path star;
    star.addStar(r.getCentre().translated(0.f, -0.5f), 5, 3.4f, 7.5f, -juce::MathConstants<float>::pi / 2.f);
    if (filled)
    {
        g.setColour(colour::accent);
        g.fillPath(star);
    }
    else
    {
        g.setColour(highlighted ? colour::text : colour::text2);
        g.strokePath(star, juce::PathStrokeType(1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}
} // namespace awchain::theme
