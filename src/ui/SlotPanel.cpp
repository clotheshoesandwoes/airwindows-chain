#include "SlotPanel.h"

namespace awchain
{
using namespace theme;

namespace
{
constexpr int kPad = 32;
constexpr int kTitleTop = 26;
} // namespace

//==============================================================================
ParamRow::ParamRow(juce::RangedAudioParameter &p, const juce::String &l, Format f, Parse pa, bool k)
    : param(p),
      attachment(p, [this](float v) {
          value = param.convertTo0to1(v);
          repaint();
      }),
      label(l), format(std::move(f)), parse(std::move(pa)), knob(k)
{
    attachment.sendInitialUpdate();
}

ParamRow::~ParamRow()
{
    if (dragging)
        attachment.endGesture();
}

juce::Rectangle<float> ParamRow::labelArea() const
{
    return {0.f, 0.f, juce::jmin(128.f, (float)getWidth() * 0.28f), (float)getHeight()};
}

juce::Rectangle<float> ParamRow::valueArea() const
{
    if (knob)
        return {0.f, (float)getHeight() - 22.f, (float)getWidth(), 20.f};
    return {(float)getWidth() - 96.f, 0.f, 96.f, (float)getHeight()};
}

juce::Rectangle<float> ParamRow::trackArea() const
{
    const auto l = labelArea();
    const auto v = valueArea();
    return {l.getRight() + 14.f, 0.f, juce::jmax(20.f, v.getX() - 20.f - (l.getRight() + 14.f)),
            (float)getHeight()};
}

void ParamRow::resized()
{
    if (entry != nullptr)
        entry->setBounds(knob ? valueArea().reduced(4.f, 0.f).toNearestInt()
                              : valueArea().withTrimmedLeft(10.f).reduced(0.f, 6.f).toNearestInt());
}

void ParamRow::paintKnob(juce::Graphics &g)
{
    const bool active = hovering || dragging;
    const auto centre = juce::Point<float>((float)getWidth() * 0.5f, 38.f);
    const float radius = 28.f;
    const float start = -juce::MathConstants<float>::pi * 0.75f;
    const float sweep = juce::MathConstants<float>::pi * 1.5f;

    juce::Path track;
    track.addCentredArc(centre.x, centre.y, radius, radius, 0.f, start, start + sweep, true);
    g.setColour(active ? colour::track.brighter(0.08f) : colour::track);
    g.strokePath(track, juce::PathStrokeType(3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (value > 0.001f)
    {
        juce::Path arc;
        arc.addCentredArc(centre.x, centre.y, radius, radius, 0.f, start, start + sweep * value, true);
        g.setColour(colour::accent);
        g.strokePath(arc, juce::PathStrokeType(3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const auto body = juce::Rectangle<float>(40.f, 40.f).withCentre(centre);
    g.setColour(active ? colour::pressed : colour::raised);
    g.fillEllipse(body);
    g.setColour(colour::line);
    g.drawEllipse(body.reduced(0.5f), 1.f);

    const float angle = start + sweep * value;
    const auto inner = centre.getPointOnCircumference(7.f, angle);
    const auto outer = centre.getPointOnCircumference(17.f, angle);
    g.setColour(dragging ? colour::accent : colour::text);
    g.drawLine(inner.x, inner.y, outer.x, outer.y, 2.2f);

    g.setFont(sans(12.5f));
    g.setColour(active ? colour::text : colour::text2);
    g.drawText(label, juce::Rectangle<float>(0.f, 72.f, (float)getWidth(), 18.f), juce::Justification::centred, true);

    if (entry == nullptr)
    {
        g.setFont(mono(12.f));
        g.setColour(dragging ? colour::accent : colour::text);
        g.drawText(format ? format(value) : juce::String(value, 2), valueArea(), juce::Justification::centred, true);
    }
}

void ParamRow::paint(juce::Graphics &g)
{
    if (knob)
    {
        paintKnob(g);
        return;
    }

    const bool active = hovering || dragging;

    g.setFont(sans(14.f));
    g.setColour(active ? colour::text : colour::text2);
    g.drawText(label, labelArea(), juce::Justification::centredLeft, true);

    const auto t = trackArea();
    const float cy = t.getCentreY();
    const auto bar = juce::Rectangle<float>(t.getX(), cy - 2.f, t.getWidth(), 4.f);
    g.setColour(active ? colour::track.brighter(0.08f) : colour::track);
    g.fillRoundedRectangle(bar, 2.f);

    const float x = t.getX() + t.getWidth() * value;
    g.setColour(colour::accent);
    g.fillRoundedRectangle(bar.withRight(juce::jmax(bar.getX() + 4.f, x)), 2.f);

    const auto thumb = juce::Rectangle<float>(14.f, 14.f).withCentre({x, cy});
    g.setColour(colour::window);
    g.fillEllipse(thumb.expanded(2.f));
    g.setColour(dragging ? colour::accent.brighter(0.35f) : colour::text);
    g.fillEllipse(thumb);

    if (entry == nullptr)
    {
        g.setFont(mono(13.f));
        g.setColour(dragging ? colour::accent : colour::text);
        g.drawText(format ? format(value) : juce::String(value, 2), valueArea(),
                   juce::Justification::centredRight, true);
    }
}

void ParamRow::mouseEnter(const juce::MouseEvent &)
{
    hovering = true;
    repaint();
}

void ParamRow::mouseExit(const juce::MouseEvent &)
{
    hovering = false;
    repaint();
}

void ParamRow::set(float v, bool asGesture)
{
    value = juce::jlimit(0.f, 1.f, v);
    const auto denormalised = param.convertFrom0to1(value);
    if (asGesture)
        attachment.setValueAsCompleteGesture(denormalised);
    else
        attachment.setValueAsPartOfGesture(denormalised);
    repaint();
}

void ParamRow::mouseDown(const juce::MouseEvent &e)
{
    if (entry != nullptr || e.mods.isPopupMenu())
        return;

    dragging = true;
    lastX = e.position.x;
    lastY = e.position.y;
    attachment.beginGesture();

    // Clicking a fader's track away from the thumb jumps there; grabbing the
    // thumb, or anywhere else, only drags. Fine moves (Ctrl or Shift) never jump.
    const bool fine = e.mods.isShiftDown() || e.mods.isCtrlDown();
    const auto t = trackArea();
    const float thumbX = t.getX() + t.getWidth() * value;
    if (!knob && !fine && t.reduced(0.f, 8.f).contains(e.position) && std::abs(e.position.x - thumbX) > 10.f)
        set((e.position.x - t.getX()) / t.getWidth(), false);
    repaint();
}

void ParamRow::mouseDrag(const juce::MouseEvent &e)
{
    if (!dragging)
        return;
    const float dx = e.position.x - lastX;
    const float dy = e.position.y - lastY;
    lastX = e.position.x;
    lastY = e.position.y;
    // Ctrl or Shift: ten times finer.
    const float scale = e.mods.isShiftDown() || e.mods.isCtrlDown() ? 0.1f : 1.f;
    if (knob)
        set(value + (dx - dy) / 220.f * scale, false); // up or right raises it
    else
        set(value + dx / juce::jmax(40.f, trackArea().getWidth()) * scale, false);
}

void ParamRow::mouseUp(const juce::MouseEvent &)
{
    if (dragging)
    {
        attachment.endGesture();
        dragging = false;
        repaint();
    }
}

void ParamRow::mouseDoubleClick(const juce::MouseEvent &e)
{
    if (valueArea().contains(e.position))
    {
        openEntry();
        return;
    }
    set(param.getDefaultValue(), true);
}

void ParamRow::openEntry()
{
    if (entry != nullptr || !parse)
        return;

    entry = std::make_unique<juce::TextEditor>();
    entry->setFont(mono(knob ? 12.f : 13.f));
    entry->setJustification(knob ? juce::Justification::centred : juce::Justification::centredRight);
    entry->setIndents(8, 0);
    entry->setText(format ? format(value) : juce::String(value, 2), false);
    entry->selectAll();
    entry->onReturnKey = [this] { closeEntry(true); };
    entry->onEscapeKey = [this] { closeEntry(false); };
    entry->onFocusLost = [this] { closeEntry(true); };
    addAndMakeVisible(*entry);
    resized();
    entry->grabKeyboardFocus();
    repaint();
}

void ParamRow::closeEntry(bool commit)
{
    if (entry == nullptr || !entry->isVisible())
        return;

    const auto text = entry->getText();
    entry->setVisible(false);
    if (commit && parse)
        if (auto v = parse(text))
            set(*v, true);

    // The editor is still inside its own callback; delete it afterwards.
    juce::MessageManager::callAsync([safe = juce::Component::SafePointer<ParamRow>(this)] {
        if (safe != nullptr)
        {
            safe->entry.reset();
            safe->repaint();
        }
    });
}

//==============================================================================
void DocView::setDocument(const juce::String &h, const juce::StringArray &p)
{
    heading = h;
    paragraphs = p;
    laidOutWidth = -1;
    repaint();
}

void DocView::layout(int width)
{
    if (width == laidOutWidth)
        return;
    laidOutWidth = width;
    layouts.clear();
    const float w = (float)juce::jmin(width, 640);
    for (const auto &p : paragraphs)
    {
        juce::AttributedString s;
        s.setWordWrap(juce::AttributedString::byWord);
        s.setLineSpacing(7.f);
        s.append(p, sans(14.f), colour::text2);
        juce::TextLayout l;
        l.createLayout(s, w);
        layouts.push_back(std::move(l));
    }
}

int DocView::heightForWidth(int width)
{
    layout(width);
    if (paragraphs.isEmpty())
        return 0;
    float h = 30.f;
    for (const auto &l : layouts)
        h += l.getHeight() + 14.f;
    return (int)std::ceil(h);
}

void DocView::resized() { layout(getWidth()); }

void DocView::paint(juce::Graphics &g)
{
    layout(getWidth());
    if (paragraphs.isEmpty())
        return;

    g.setFont(sans(13.f, Weight::medium));
    g.setColour(colour::text3);
    g.drawText(heading, juce::Rectangle<float>(0.f, 0.f, (float)getWidth(), 18.f),
               juce::Justification::centredLeft, true);

    float y = 30.f;
    for (const auto &l : layouts)
    {
        l.draw(g, juce::Rectangle<float>(0.f, y, l.getWidth(), l.getHeight()));
        y += l.getHeight() + 14.f;
    }
}

//==============================================================================
int SlotPanel::Body::layoutFor(int width)
{
    // Faders stop growing past a comfortable length on wide windows.
    const int w = juce::jmin(width - kPad * 2, 700);
    int y = 6;
    if (noControls)
        y += 40;

    if (knobs)
    {
        const int columns = juce::jmax(1, (width - kPad * 2 + 8) / (ParamRow::knobWidth + 8));
        int i = 0;
        for (auto &r : rows)
        {
            r->setBounds(kPad + (i % columns) * (ParamRow::knobWidth + 8), y + (i / columns) * (ParamRow::knobHeight + 8),
                         ParamRow::knobWidth, ParamRow::knobHeight);
            ++i;
        }
        if (!rows.empty())
            y += ((int)(rows.size() - 1) / columns + 1) * (ParamRow::knobHeight + 8) - 8;
        y += 16;
        dividerY = y;
        y += 16;
        if (mix != nullptr)
        {
            mix->setBounds(kPad, y, ParamRow::knobWidth, ParamRow::knobHeight);
            y += ParamRow::knobHeight;
        }
        dividerWidth = w;
        y += 34;
        const int docWidth = width - kPad * 2;
        const int dh = docs.heightForWidth(docWidth);
        docs.setBounds(kPad, y, docWidth, dh);
        return y + dh + 36;
    }

    for (auto &r : rows)
    {
        r->setBounds(kPad, y, w, ParamRow::height);
        y += ParamRow::height;
    }
    y += 12;
    dividerY = y;
    y += 12;
    if (mix != nullptr)
    {
        mix->setBounds(kPad, y, w, ParamRow::height);
        y += ParamRow::height;
    }
    y += 34;
    const int docWidth = width - kPad * 2;
    const int dh = docs.heightForWidth(docWidth);
    docs.setBounds(kPad, y, docWidth, dh);
    y += dh + 36;
    dividerWidth = w;
    return y;
}

void SlotPanel::Body::paint(juce::Graphics &g)
{
    if (noControls)
    {
        g.setFont(sans(14.f));
        g.setColour(colour::text3);
        g.drawText("No controls. It does one thing.",
                   juce::Rectangle<float>((float)kPad, 6.f, (float)getWidth() - kPad * 2, 40.f),
                   juce::Justification::centredLeft, true);
    }
    g.setColour(colour::line);
    g.fillRect(juce::Rectangle<float>((float)kPad, (float)dividerY, (float)dividerWidth, 1.f));
}

SlotPanel::SlotPanel(ChainProcessor &p) : proc(p)
{
    for (auto *b : headerButtons())
        addChildComponent(b);
    addChildComponent(add);

    previous.setTooltip("Previous effect in this category");
    next.setTooltip("Next effect in this category");
    previous.onClick = [this] {
        if (onStep)
            onStep(-1);
    };
    next.onClick = [this] {
        if (onStep)
            onStep(1);
    };
    star.setTooltip("Favourite");
    star.onClick = [this] {
        const auto *e = Catalog::get().find(registryIndex);
        if (e == nullptr)
            return;
        proc.setFavourite(e->name, !star.isFilled());
        star.setFilled(proc.isFavourite(e->name));
    };
    bypass.onClick = [this] {
        if (slot < 0)
            return;
        auto &b = proc.bypassParam(slot);
        b.beginChangeGesture();
        b.setValueNotifyingHost(b.get() ? 0.f : 1.f);
        b.endChangeGesture();
        pollBypass();
    };
    replace.onClick = [this] {
        if (onReplace)
            onReplace();
    };
    remove.onClick = [this] {
        if (onRemove)
            onRemove();
    };
    add.onClick = [this] {
        if (onAdd)
            onAdd();
    };

    viewport.setViewedComponent(&body, false);
    viewport.setScrollBarsShown(true, false);
    viewport.setScrollBarThickness(10);
    addChildComponent(viewport);
}

std::vector<juce::Component *> SlotPanel::headerButtons()
{
    return {&previous, &next, &star, &bypass, &replace, &remove};
}

void SlotPanel::showSlot(int s)
{
    body.rows.clear();
    body.mix.reset();

    const auto info = proc.getSlotInfo(s);
    const auto *e = Catalog::get().find(info.registryIndex);
    chainEmpty = proc.getOrder().empty();

    if (s < 0 || e == nullptr)
    {
        slot = -1;
        registryIndex = -1;
        for (auto *b : headerButtons())
            b->setVisible(false);
        viewport.setVisible(false);
        add.setVisible(true);
        resized();
        repaint();
        return;
    }

    slot = s;
    registryIndex = info.registryIndex;
    star.setFilled(proc.isFavourite(e->name));
    title = e->name;
    category = e->category;
    summary = e->summary;
    body.knobs = proc.getSetting("knobs", "0") == "1";

    for (int i = 0; i < info.nParams; ++i)
    {
        auto row = std::make_unique<ParamRow>(
            proc.fxParam(s, i), info.paramNames[(size_t)i],
            [this, s, i](float v) { return proc.formatValue(s, i, v); },
            [this, s, i](const juce::String &t) { return proc.parseValue(s, i, t); }, body.knobs);
        body.addAndMakeVisible(*row);
        body.rows.push_back(std::move(row));
    }
    body.noControls = info.nParams == 0;

    body.mix = std::make_unique<ParamRow>(
        proc.mixParam(s), "Mix", [](float v) { return juce::String(juce::roundToInt(v * 100.f)) + "%"; },
        [](const juce::String &t) -> std::optional<float> {
            const auto digits = t.retainCharacters("0123456789.");
            if (digits.isEmpty())
                return std::nullopt;
            return digits.getFloatValue() / 100.f;
        },
        body.knobs);
    body.addAndMakeVisible(*body.mix);
    body.docs.setDocument("From the Airwindopedia", Catalog::get().docParagraphs(registryIndex));
    body.addAndMakeVisible(body.docs);

    for (auto *b : headerButtons())
        b->setVisible(true);
    viewport.setVisible(true);
    add.setVisible(false);

    bypassed = !proc.bypassParam(s).get(); // makes pollBypass apply the current state
    pollBypass();

    viewport.setViewPosition(0, 0);
    resized();
    repaint();
}

void SlotPanel::pollBypass()
{
    if (slot < 0)
        return;
    const bool now = proc.bypassParam(slot).get();
    if (now == bypassed)
        return;
    bypassed = now;
    bypass.setButtonText(bypassed ? "Turn on" : "Bypass");
    // Dim the controls, not Chris's notes; those are still worth reading.
    for (auto &r : body.rows)
        r->setAlpha(bypassed ? 0.4f : 1.f);
    if (body.mix != nullptr)
        body.mix->setAlpha(bypassed ? 0.4f : 1.f);
    resized();
    repaint();
}

void SlotPanel::layoutHeader(int width)
{
    juce::AttributedString s;
    s.setWordWrap(juce::AttributedString::byWord);
    s.setLineSpacing(4.f);
    s.append(summary, sans(15.f), colour::text2);
    summaryLayout.createLayout(s, (float)juce::jmin(width, 640));
}

void SlotPanel::resized()
{
    const int w = getWidth();

    if (slot < 0)
    {
        const int bw = add.idealWidth();
        add.setBounds((w - bw) / 2, getHeight() / 2 + 52, bw, 38);
        return;
    }

    const int available = w - kPad * 2;
    layoutHeader(available);

    // The buttons sit on the title line when there is room, otherwise on
    // their own line under the summary.
    const int buttonsWidth = 30 + 2 + 30 + 6 + 30 + 10 + bypass.idealWidth() + 2 + replace.idealWidth() + 2 +
                             remove.idealWidth();
    const auto titleWidth = (int)std::ceil(textWidth(sans(26.f, Weight::semibold), title));
    buttonsInline = titleWidth + 24 + buttonsWidth <= available;

    const int summaryTop = kTitleTop + 42;
    const int summaryBottom = summaryTop + (int)std::ceil(summaryLayout.getHeight());
    const int buttonsY = buttonsInline ? kTitleTop + 2 : summaryBottom + 12;
    headerHeight = buttonsInline ? summaryBottom + 22 : buttonsY + 32 + 18;

    int x = buttonsInline ? w - kPad - buttonsWidth : kPad - 8;
    previous.setBounds(x, buttonsY, 30, 32);
    next.setBounds(previous.getRight() + 2, buttonsY, 30, 32);
    star.setBounds(next.getRight() + 6, buttonsY, 30, 32);
    x = star.getRight() + 10;
    for (auto *b : {&bypass, &replace, &remove})
    {
        b->setBounds(x, buttonsY, b->idealWidth(), 32);
        x = b->getRight() + 2;
    }

    viewport.setBounds(0, headerHeight, w, juce::jmax(0, getHeight() - headerHeight));
    const int bodyHeight = body.layoutFor(w);
    body.setSize(w, bodyHeight);
    if (bodyHeight > viewport.getHeight())
        body.setSize(viewport.getMaximumVisibleWidth(), body.layoutFor(viewport.getMaximumVisibleWidth()));
}

void SlotPanel::paint(juce::Graphics &g)
{
    g.fillAll(colour::window);

    if (slot < 0)
    {
        const auto w = (float)getWidth();
        const float top = (float)getHeight() / 2.f - 48.f;
        g.setFont(sans(20.f, Weight::medium));
        g.setColour(colour::text);
        g.drawText(chainEmpty ? "Start a chain" : "Nothing selected",
                   juce::Rectangle<float>(0.f, top, w, 28.f), juce::Justification::centred, false);
        g.setFont(sans(14.5f));
        g.setColour(colour::text2);
        g.drawFittedText(chainEmpty ? "Audio runs down the list, from Input to Output.\nAdd the first effect."
                                    : "Pick an effect in the chain to see its controls.",
                         juce::Rectangle<int>(0, (int)top + 38, (int)w, 48), juce::Justification::centredTop, 2);
        return;
    }

    const float left = (float)kPad;
    const float titleRight = buttonsInline ? (float)previous.getX() - 16.f : (float)(getWidth() - kPad);

    const auto titleFont = sans(26.f, Weight::semibold);
    const float baseline = (float)kTitleTop + 4.f + titleFont.getAscent();
    g.setFont(titleFont);
    g.setColour(bypassed ? colour::text2 : colour::text);
    const float titleWidth = juce::jmin(textWidth(titleFont, title), titleRight - left);
    g.drawText(title, juce::Rectangle<float>(left, (float)kTitleTop + 4.f, titleWidth + 2.f, titleFont.getHeight()),
               juce::Justification::centredLeft, true);

    const auto categoryFont = sans(14.f);
    const auto note = bypassed ? juce::String("Bypassed") : category;
    const float categoryX = left + titleWidth + 12.f;
    if (categoryX + textWidth(categoryFont, note) < titleRight)
    {
        g.setFont(categoryFont);
        g.setColour(colour::text3);
        g.drawSingleLineText(note, (int)categoryX, (int)baseline);
    }

    summaryLayout.draw(g, juce::Rectangle<float>(left, (float)kTitleTop + 42.f, summaryLayout.getWidth(),
                                                 summaryLayout.getHeight()));
}
} // namespace awchain
