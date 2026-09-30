#include "ChainList.h"

#include <map>

namespace awchain
{
using namespace theme;

ChainList::ChainList(ChainProcessor &p) : proc(p) {}

int ChainList::preferredHeight() const
{
    return topPad + (int)rows.size() * rowHeight + addHeight + bottomPad;
}

void ChainList::rebuild()
{
    std::map<int, float> previous;
    for (const auto &r : rows)
        previous[r.slot] = r.y;
    const bool animate = !rows.empty();

    rows.clear();
    const auto order = proc.getOrder();
    for (size_t i = 0; i < order.size(); ++i)
    {
        Row r;
        r.slot = order[i];
        const auto info = proc.getSlotInfo(r.slot);
        if (const auto *e = Catalog::get().find(info.registryIndex))
        {
            r.name = e->name;
            r.category = e->category;
        }
        r.bypassed = proc.bypassParam(r.slot).get();
        r.mix = proc.mixParam(r.slot).get();
        const auto it = previous.find(r.slot);
        r.y = animate && it != previous.end() ? it->second : slotTop((int)i);
        rows.push_back(r);
    }

    dragging = false;
    pressedRow = -1;
    dropIndex = -1;
    hoverRow = -1;
    setSize(getWidth(), preferredHeight());
    if (settle())
        startTimerHz(60);
    repaint();
}

void ChainList::pollStates()
{
    bool changed = false;
    auto meter = [&changed](float &shown, float peak) {
        const float next = juce::jmax(peak, shown * 0.82f);
        if (std::abs(next - shown) > 0.002f || (next == 0.f && shown != 0.f))
            changed = true;
        shown = next < 0.001f ? 0.f : next;
    };

    for (auto &r : rows)
    {
        const bool b = proc.bypassParam(r.slot).get();
        const float m = proc.mixParam(r.slot).get();
        if (b != r.bypassed || std::abs(m - r.mix) > 0.001f)
        {
            r.bypassed = b;
            r.mix = m;
            changed = true;
        }
        meter(r.level, proc.takePeak(r.slot));
    }
    meter(inputLevel, proc.takePeak(ChainProcessor::inputMeter));
    meter(outputLevel, proc.takePeak(ChainProcessor::outputMeter));

    if (changed)
        repaint();
}

void ChainList::drawMeter(juce::Graphics &g, float level, float x, float y, float width, bool dim) const
{
    if (level <= 0.f || width <= 0.f)
        return;
    // -60 dB .. 0 dB across the bar; anything over full scale shows hot.
    const float db = 20.f * std::log10(juce::jmax(level, 1.0e-6f));
    const float fraction = juce::jlimit(0.f, 1.f, (db + 60.f) / 60.f);
    g.setColour(level > 1.f ? colour::hot : (dim ? colour::text3 : colour::accent.withAlpha(0.55f)));
    g.fillRoundedRectangle(x, y, juce::jmax(2.f, width * fraction), 2.f, 1.f);
}

void ChainList::setSelectedSlot(int slot)
{
    if (selected != slot)
    {
        selected = slot;
        repaint();
    }
}

//==============================================================================
int ChainList::rowAt(float y) const
{
    for (int i = 0; i < (int)rows.size(); ++i)
        if (y >= slotTop(i) && y < slotTop(i) + (float)rowHeight)
            return i;
    return -1;
}

juce::Rectangle<float> ChainList::toggleBounds(float rowY) const
{
    return {(float)getWidth() - 18.f - 30.f, rowY + (float)rowHeight * 0.5f - 8.f, 30.f, 16.f};
}

juce::Rectangle<float> ChainList::addBounds() const
{
    return {0.f, slotTop((int)rows.size()), (float)getWidth(), (float)addHeight};
}

float ChainList::targetY(int index) const
{
    if (!dragging || pressedRow < 0)
        return slotTop(index);
    int visual = index < pressedRow ? index : index - 1;
    if (visual >= dropIndex)
        ++visual;
    return slotTop(visual);
}

bool ChainList::settle()
{
    bool moving = false;
    for (int i = 0; i < (int)rows.size(); ++i)
    {
        if (dragging && i == pressedRow)
            continue;
        auto &y = rows[(size_t)i].y;
        const float d = targetY(i) - y;
        if (std::abs(d) < 0.5f)
            y = targetY(i);
        else
        {
            y += d * 0.3f;
            moving = true;
        }
    }
    return moving;
}

void ChainList::timerCallback()
{
    if (!settle() && !dragging)
        stopTimer();
    repaint();
}

//==============================================================================
void ChainList::paint(juce::Graphics &g)
{
    const float inY = 26.f;
    const auto add = addBounds();
    const float outY = add.getBottom() + 22.f;

    // The signal path: one line from input to output through every effect.
    g.setColour(colour::line);
    g.fillRect(juce::Rectangle<float>(spineX - 0.75f, inY, 1.5f, outY - inY));

    auto terminal = [&](float y, const juce::String &label, float level) {
        g.setColour(colour::side);
        g.fillEllipse(spineX - 5.f, y - 5.f, 10.f, 10.f);
        g.setColour(colour::text3);
        g.drawEllipse(spineX - 4.f, y - 4.f, 8.f, 8.f, 1.25f);
        g.setFont(sans(12.5f));
        g.drawText(label, juce::Rectangle<float>(56.f, y - 10.f, 160.f, 20.f),
                   juce::Justification::centredLeft, false);
        drawMeter(g, level, 56.f, y + 12.f, (float)getWidth() - 56.f - 48.f, false);
    };
    terminal(inY, "Input", inputLevel);

    for (int i = 0; i < (int)rows.size(); ++i)
    {
        if (dragging && i == pressedRow)
            continue;
        int visual = i;
        if (dragging)
        {
            visual = i < pressedRow ? i : i - 1;
            if (visual >= dropIndex)
                ++visual;
        }
        drawRow(g, rows[(size_t)i], visual + 1, false);
    }

    // Add
    const bool full = (int)rows.size() >= kMaxSlots;
    if (hoverAdd && !full && !dragging)
    {
        g.setColour(colour::hover);
        g.fillRect(add);
    }
    const auto disc = juce::Rectangle<float>(22.f, 22.f).withCentre({spineX, add.getCentreY()});
    g.setColour(hoverAdd && !full ? colour::hover : colour::side);
    g.fillEllipse(disc);
    g.setColour(full ? colour::line : (hoverAdd ? colour::text2 : colour::text3));
    g.drawEllipse(disc.reduced(0.5f), 1.f);
    if (!full)
    {
        const auto c = disc.getCentre();
        g.fillRect(juce::Rectangle<float>(c.x - 4.5f, c.y - 0.6f, 9.f, 1.2f));
        g.fillRect(juce::Rectangle<float>(c.x - 0.6f, c.y - 4.5f, 1.2f, 9.f));
    }
    g.setFont(sans(14.f));
    g.setColour(full ? colour::text3 : (hoverAdd ? colour::text : colour::text2));
    g.drawText(full ? juce::String("The chain is full") : juce::String("Add effect"),
               juce::Rectangle<float>(54.f, add.getY(), add.getWidth() - 60.f, add.getHeight()),
               juce::Justification::centredLeft, false);

    terminal(outY, "Output", outputLevel);

    if (dragging && pressedRow >= 0)
    {
        auto lifted = rows[(size_t)pressedRow];
        lifted.y = dragTop;
        drawRow(g, lifted, dropIndex + 1, true);
    }
}

void ChainList::drawRow(juce::Graphics &g, const Row &row, int number, bool lifted) const
{
    const float w = (float)getWidth();
    const auto area = juce::Rectangle<float>(0.f, row.y, w, (float)rowHeight);
    const bool isSelected = row.slot == selected;
    const bool isHover = !dragging && hoverRow >= 0 && hoverRow < (int)rows.size() &&
                         rows[(size_t)hoverRow].slot == row.slot;

    auto background = colour::side;
    if (lifted)
        background = juce::Colour(0xff2a2723);
    else if (isSelected)
        background = colour::raised;
    else if (isHover)
        background = colour::hover;

    if (background != colour::side)
    {
        g.setColour(background);
        g.fillRect(area);
    }
    if (lifted)
    {
        g.setColour(colour::line.brighter(0.2f));
        g.drawRect(area, 1.f);
    }

    // Position on the signal path
    const auto disc = juce::Rectangle<float>(24.f, 24.f).withCentre({spineX, area.getCentreY()});
    if (isSelected)
    {
        g.setColour(colour::accent);
        g.fillEllipse(disc);
    }
    else
    {
        g.setColour(background);
        g.fillEllipse(disc);
        g.setColour(row.bypassed ? colour::line.brighter(0.1f) : colour::text3);
        g.drawEllipse(disc.reduced(0.5f), 1.f);
    }
    g.setFont(mono(11.5f));
    g.setColour(isSelected ? colour::onAccent : (row.bypassed ? colour::text3 : colour::text2));
    g.drawText(juce::String(number), disc.translated(0.f, -0.5f), juce::Justification::centred, false);

    // Name and category
    const auto toggle = toggleBounds(row.y);
    const float textX = 56.f;
    const float textRight = toggle.getX() - 12.f;
    g.setFont(sans(15.f, Weight::medium));
    g.setColour(row.bypassed ? colour::text3 : colour::text);
    g.drawText(row.name, juce::Rectangle<float>(textX, row.y + 8.f, textRight - textX, 21.f),
               juce::Justification::centredLeft, true);

    g.setFont(sans(12.5f));
    g.setColour(colour::text3);
    g.drawText(row.bypassed ? juce::String("Bypassed") : row.category,
               juce::Rectangle<float>(textX, row.y + 29.f, textRight - textX - 44.f, 18.f),
               juce::Justification::centredLeft, true);

    if (!row.bypassed && row.mix < 0.995f)
    {
        g.setFont(mono(11.5f));
        g.setColour(colour::text2);
        g.drawText(juce::String(juce::roundToInt(row.mix * 100.f)) + "%",
                   juce::Rectangle<float>(textRight - 44.f, row.y + 29.f, 44.f, 18.f),
                   juce::Justification::centredRight, false);
    }

    // Level after this effect
    drawMeter(g, row.level, textX, row.y + (float)rowHeight - 6.f, textRight - textX, row.bypassed);

    // On / bypass. Neutral on purpose: the list's one accent is the selection.
    const bool on = !row.bypassed;
    if (on)
    {
        g.setColour(juce::Colour(0xff4c4741));
        g.fillRoundedRectangle(toggle, toggle.getHeight() * 0.5f);
    }
    else
    {
        g.setColour(juce::Colour(0xff3f3b35));
        g.drawRoundedRectangle(toggle.reduced(0.5f), toggle.getHeight() * 0.5f, 1.f);
    }
    const float k = toggle.getHeight() - 5.f;
    const float kx = on ? toggle.getRight() - toggle.getHeight() * 0.5f
                        : toggle.getX() + toggle.getHeight() * 0.5f;
    g.setColour(on ? colour::text : colour::text3);
    g.fillEllipse(juce::Rectangle<float>(k, k).withCentre({kx, toggle.getCentreY()}));
}

//==============================================================================
void ChainList::mouseMove(const juce::MouseEvent &e)
{
    const int r = rowAt(e.position.y);
    const bool overAdd = addBounds().contains(e.position);
    const bool overToggle = r >= 0 && toggleBounds(slotTop(r)).expanded(6.f).contains(e.position);
    if (r != hoverRow || overAdd != hoverAdd || overToggle != hoverToggle)
    {
        hoverRow = r;
        hoverAdd = overAdd;
        hoverToggle = overToggle;
        repaint();
    }
    setMouseCursor(overToggle || (overAdd && (int)rows.size() < kMaxSlots)
                       ? juce::MouseCursor::PointingHandCursor
                       : juce::MouseCursor::NormalCursor);
}

void ChainList::mouseExit(const juce::MouseEvent &)
{
    hoverRow = -1;
    hoverAdd = hoverToggle = false;
    repaint();
}

void ChainList::mouseDown(const juce::MouseEvent &e)
{
    pressedRow = -1;
    dragging = false;

    if (addBounds().contains(e.position))
    {
        if ((int)rows.size() < kMaxSlots && onAdd && !e.mods.isPopupMenu())
            onAdd();
        return;
    }

    const int r = rowAt(e.position.y);
    if (r < 0)
        return;
    const int slot = rows[(size_t)r].slot;

    if (e.mods.isPopupMenu())
    {
        showMenu(r);
        return;
    }

    if (toggleBounds(slotTop(r)).expanded(6.f).contains(e.position))
    {
        auto &b = proc.bypassParam(slot);
        b.beginChangeGesture();
        b.setValueNotifyingHost(b.get() ? 0.f : 1.f);
        b.endChangeGesture();
        pollStates();
        return;
    }

    pressedRow = r;
    pressPoint = e.position;
    grabOffset = e.position.y - rows[(size_t)r].y;
    if (slot != selected)
    {
        setSelectedSlot(slot);
        if (onSelect)
            onSelect(slot);
    }
}

void ChainList::mouseDrag(const juce::MouseEvent &e)
{
    if (pressedRow < 0 || rows.size() < 2)
        return;

    if (!dragging)
    {
        if (e.position.getDistanceFrom(pressPoint) < 5.f)
            return;
        dragging = true;
        dropIndex = pressedRow;
        startTimerHz(60);
    }

    const int n = (int)rows.size();
    dragTop = juce::jlimit(slotTop(0) - 12.f, slotTop(n - 1) + 12.f, e.position.y - grabOffset);
    dropIndex = juce::jlimit(0, n - 1, juce::roundToInt((dragTop - (float)topPad) / (float)rowHeight));

    if (auto *vp = findParentComponentOfClass<juce::Viewport>())
    {
        const auto p = vp->getLocalPoint(this, e.getPosition());
        vp->autoScroll(p.x, p.y, 28, 10);
    }
    repaint();
}

void ChainList::mouseUp(const juce::MouseEvent &)
{
    if (dragging && pressedRow >= 0)
    {
        const int from = pressedRow, to = dropIndex;
        rows[(size_t)from].y = dragTop;
        dragging = false;
        pressedRow = -1;
        if (from != to)
        {
            // Reorder locally so the rows settle smoothly, then tell the processor.
            auto moved = rows[(size_t)from];
            rows.erase(rows.begin() + from);
            rows.insert(rows.begin() + to, moved);
            proc.moveSlot(from, to);
        }
        startTimerHz(60);
    }
    pressedRow = -1;
    dragging = false;
}

void ChainList::mouseDoubleClick(const juce::MouseEvent &e)
{
    const int r = rowAt(e.position.y);
    if (r < 0 || toggleBounds(slotTop(r)).expanded(6.f).contains(e.position))
        return;
    if (onReplace)
        onReplace(rows[(size_t)r].slot);
}

void ChainList::showMenu(int rowIndex)
{
    const int slot = rows[(size_t)rowIndex].slot;
    const int n = (int)rows.size();
    const bool bypassed = rows[(size_t)rowIndex].bypassed;

    juce::PopupMenu m;
    m.addItem("Replace...", [this, slot] {
        if (onReplace)
            onReplace(slot);
    });
    m.addItem("Duplicate", n < kMaxSlots, false, [this, slot] {
        if (onDuplicate)
            onDuplicate(slot);
    });
    m.addItem(bypassed ? "Turn on" : "Bypass", [this, slot] {
        auto &b = proc.bypassParam(slot);
        b.beginChangeGesture();
        b.setValueNotifyingHost(b.get() ? 0.f : 1.f);
        b.endChangeGesture();
        pollStates();
    });
    m.addSeparator();
    m.addItem("Move up", rowIndex > 0, false, [this, rowIndex] { proc.moveSlot(rowIndex, rowIndex - 1); });
    m.addItem("Move down", rowIndex < n - 1, false,
              [this, rowIndex] { proc.moveSlot(rowIndex, rowIndex + 1); });
    m.addSeparator();
    m.addItem("Remove", [this, slot] {
        if (onRemove)
            onRemove(slot);
    });

    const auto area = localAreaToGlobal(juce::Rectangle<int>(0, (int)slotTop(rowIndex), getWidth(), rowHeight));
    m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(area).withMinimumWidth(180));
}
} // namespace awchain
