#include "Picker.h"

namespace awchain
{
using namespace theme;

namespace
{
constexpr int kTopBar = 72;
constexpr int kSourceWidth = 208;
constexpr int kRowHeight = 58;
} // namespace

//==============================================================================
int Picker::SourceList::preferredHeight() const
{
    if (sources.empty())
        return 0;
    return (int)rowBounds((int)sources.size() - 1).getBottom() + 16;
}

juce::Rectangle<float> Picker::SourceList::rowBounds(int index) const
{
    float y = 12.f;
    for (int i = 0; i < index; ++i)
        y += 30.f + (sources[(size_t)i + 1].gapBefore ? 14.f : 0.f);
    if (index == 0 && !sources.empty() && sources[0].gapBefore)
        y += 14.f;
    return {10.f, y, (float)getWidth() - 20.f, 30.f};
}

int Picker::SourceList::rowAt(juce::Point<float> p) const
{
    for (int i = 0; i < (int)sources.size(); ++i)
        if (rowBounds(i).contains(p))
            return i;
    return -1;
}

void Picker::SourceList::paint(juce::Graphics &g)
{
    for (int i = 0; i < (int)sources.size(); ++i)
    {
        const auto &s = sources[(size_t)i];
        const auto r = rowBounds(i);
        const bool isSelected = s.key == selectedKey;
        if (isSelected || i == hover)
        {
            g.setColour(isSelected ? colour::raised : colour::hover);
            g.fillRoundedRectangle(r, 6.f);
        }
        g.setFont(sans(13.5f, isSelected ? Weight::medium : Weight::regular));
        g.setColour(isSelected ? colour::text : (i == hover ? colour::text : colour::text2));
        g.drawText(s.label, r.withTrimmedLeft(10.f).withTrimmedRight(40.f), juce::Justification::centredLeft, true);
        g.setFont(mono(11.5f));
        g.setColour(colour::text3);
        g.drawText(juce::String(s.count), r.withTrimmedRight(10.f), juce::Justification::centredRight, false);
    }
}

void Picker::SourceList::mouseMove(const juce::MouseEvent &e)
{
    const int h = rowAt(e.position);
    if (h != hover)
    {
        hover = h;
        repaint();
    }
    setMouseCursor(h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void Picker::SourceList::mouseExit(const juce::MouseEvent &)
{
    hover = -1;
    repaint();
}

void Picker::SourceList::mouseDown(const juce::MouseEvent &e)
{
    const int i = rowAt(e.position);
    if (i >= 0 && onPick)
        onPick(sources[(size_t)i].key);
}

//==============================================================================
int Picker::Preview::Body::layoutFor(int width, const juce::String &summaryText, const juce::String &controlText)
{
    const float w = (float)juce::jmin(width, 560);
    int y = 72; // name and category are painted above this

    juce::AttributedString s;
    s.setWordWrap(juce::AttributedString::byWord);
    s.setLineSpacing(4.f);
    s.append(summaryText, sans(15.f), colour::text2);
    summary.createLayout(s, w);
    y += (int)std::ceil(summary.getHeight()) + 24;

    hasControls = controlText.isNotEmpty();
    controlsY = y;
    juce::AttributedString c;
    c.setWordWrap(juce::AttributedString::byWord);
    c.setLineSpacing(4.f);
    c.append(hasControls ? controlText : juce::String("None"), sans(14.f), colour::text2);
    controls.createLayout(c, w);
    y += 24 + (int)std::ceil(controls.getHeight()) + 30;

    const int dh = docs.heightForWidth(width);
    docs.setBounds(0, y, width, dh);
    return y + dh + 24;
}

void Picker::Preview::Body::paint(juce::Graphics &g)
{
    g.setFont(sans(22.f, Weight::semibold));
    g.setColour(colour::text);
    g.drawText(name, juce::Rectangle<float>(0.f, 4.f, (float)getWidth(), 32.f), juce::Justification::centredLeft, true);
    g.setFont(sans(13.5f));
    g.setColour(colour::text3);
    g.drawText(category, juce::Rectangle<float>(0.f, 38.f, (float)getWidth(), 20.f),
               juce::Justification::centredLeft, true);

    summary.draw(g, juce::Rectangle<float>(0.f, 72.f, summary.getWidth(), summary.getHeight()));

    g.setFont(sans(13.f, Weight::medium));
    g.setColour(colour::text3);
    g.drawText("Controls", juce::Rectangle<float>(0.f, (float)controlsY, (float)getWidth(), 18.f),
               juce::Justification::centredLeft, false);
    controls.draw(g, juce::Rectangle<float>(0.f, (float)controlsY + 24.f, controls.getWidth(), controls.getHeight()));
}

Picker::Preview::Preview()
{
    viewport.setViewedComponent(&body, false);
    viewport.setScrollBarsShown(true, false);
    viewport.setScrollBarThickness(10);
    addAndMakeVisible(viewport);
    body.addAndMakeVisible(body.docs);
    addAndMakeVisible(use);
    use.onClick = [this] {
        if (onUse)
            onUse();
    };
    star.setTooltip("Favourite");
    addAndMakeVisible(star);
    star.onClick = [this] {
        if (onToggleFavourite)
            onToggleFavourite();
    };
}

void Picker::Preview::setFavourite(bool f) { star.setFilled(f); }

void Picker::Preview::setUseLabel(const juce::String &label, const juce::String &hintText)
{
    use.setButtonText(label);
    hint = hintText;
    resized();
    repaint();
}

void Picker::Preview::show(int index, const juce::StringArray &controlList)
{
    registryIndex = index;
    const auto *e = Catalog::get().find(index);
    use.setEnabled(e != nullptr);
    if (e == nullptr)
    {
        body.name = {};
        body.category = {};
        summaryText = {};
        controlText = {};
        body.docs.setDocument({}, {});
    }
    else
    {
        body.name = e->name;
        body.category = e->category;
        summaryText = e->summary;
        controlText = controlList.joinIntoString(", ");
        body.docs.setDocument("From the Airwindopedia", Catalog::get().docParagraphs(index));
    }
    viewport.setViewPosition(0, 0);
    resized();
    body.repaint();
}

void Picker::Preview::resized()
{
    const int pad = 28;
    const int bw = use.idealWidth();
    use.setBounds(getWidth() - pad - bw, getHeight() - 20 - 36, bw, 36);
    star.setBounds(use.getX() - 8 - 36, use.getY(), 36, 36);

    viewport.setBounds(pad, 24, getWidth() - pad, juce::jmax(0, use.getY() - 16 - 24));
    const int w = viewport.getWidth() - pad;
    body.setSize(w, body.layoutFor(w, summaryText, controlText));
}

void Picker::Preview::paint(juce::Graphics &g)
{
    g.setColour(colour::line);
    g.fillRect(0, use.getY() - 16, getWidth(), 1);
    if (hint.isNotEmpty())
    {
        g.setFont(sans(12.5f));
        g.setColour(colour::text3);
        g.drawText(hint, juce::Rectangle<float>(28.f, (float)use.getY(), (float)star.getX() - 40.f, (float)use.getHeight()),
                   juce::Justification::centredLeft, true);
    }
}

//==============================================================================
Picker::Picker(ChainProcessor &p) : proc(p), results("Effects", this)
{
    setWantsKeyboardFocus(false);

    search.setFont(sans(16.f));
    search.setIndents(44, 0);
    search.setJustification(juce::Justification::centredLeft);
    search.setColour(juce::TextEditor::backgroundColourId, colour::raised);
    search.addKeyListener(this);
    search.onTextChange = [this] { refreshItems(); };
    search.onEscapeKey = [this] {
        if (onClose)
            onClose();
    };
    addAndMakeVisible(search);

    cancel.onClick = [this] {
        if (onClose)
            onClose();
    };
    addAndMakeVisible(cancel);

    sourceList.onPick = [this](const juce::String &key) {
        search.setText({}, false);
        setSource(key);
        search.grabKeyboardFocus();
    };
    sourceViewport.setViewedComponent(&sourceList, false);
    sourceViewport.setScrollBarsShown(true, false);
    sourceViewport.setScrollBarThickness(8);
    addAndMakeVisible(sourceViewport);

    results.setRowHeight(kRowHeight);
    results.setMouseMoveSelectsRows(true);
    results.setOutlineThickness(0);
    results.getViewport()->setScrollBarThickness(10);
    addAndMakeVisible(results);

    preview.onUse = [this] { choose(results.getSelectedRow()); };
    preview.onToggleFavourite = [this] {
        const int row = results.getSelectedRow();
        const auto *e = row >= 0 && row < (int)items.size() ? Catalog::get().find(items[(size_t)row]) : nullptr;
        if (e == nullptr)
            return;
        const bool now = !proc.isFavourite(e->name);
        proc.setFavourite(e->name, now);
        preview.setFavourite(now);
        buildSources();
        if (source == "favourites" && !searching())
        {
            refreshItems();
            auto it = std::find(items.begin(), items.end(), e->registryIndex);
            highlightRow(it == items.end() ? 0 : (int)(it - items.begin()));
        }
    };
    addAndMakeVisible(preview);
}

Picker::~Picker() { search.removeKeyListener(this); }

void Picker::openToAdd()
{
    replacingSlot = -1;
    if (rememberedSource.isNotEmpty())
    {
        source = rememberedSource;
        rememberedSource.clear();
    }
    open();
}

void Picker::openToReplace(int slot)
{
    replacingSlot = slot;
    if (rememberedSource.isEmpty())
        rememberedSource = source;
    open();
}

void Picker::open()
{
    buildSources();
    const auto total = (int)Catalog::get().all().size();
    const auto *current = Catalog::get().find(proc.getSlotInfo(replacingSlot).registryIndex);
    search.setTextToShowWhenEmpty(current != nullptr ? "Replace " + current->name + " with..."
                                                     : "Search " + juce::String(total) + " effects",
                                  colour::text3);
    preview.setUseLabel(current != nullptr ? "Replace" : "Add to chain",
                        current != nullptr ? juce::String() : juce::String("Shift keeps it open"));
    search.setText({}, false);

    // Replacing starts where the current effect lives: its own category.
    if (current != nullptr)
        setSource("cat:" + current->category);
    else
        setSource(source);

    if (current != nullptr)
    {
        auto it = std::find(items.begin(), items.end(), current->registryIndex);
        if (it != items.end())
            highlightRow((int)(it - items.begin()));
    }
    search.grabKeyboardFocus();
}

void Picker::buildSources()
{
    const auto &cat = Catalog::get();
    auto &list = sourceList.sources;
    list.clear();

    int favouriteCount = 0;
    for (const auto &f : proc.getFavourites())
        favouriteCount += cat.indexOf(f) >= 0 ? 1 : 0;
    if (favouriteCount > 0)
        list.push_back({"favourites", "Favourites", favouriteCount, false});

    const auto recent = proc.getRecentEffects();
    int recentCount = 0;
    for (const auto &r : recent)
        recentCount += cat.indexOf(r) >= 0 ? 1 : 0;
    if (recentCount > 0)
        list.push_back({"recent", "Recent", recentCount, false});
    list.push_back({"recommended", "Chris recommends", (int)cat.recommended().size(), false});
    list.push_back({"all", "All effects", (int)cat.all().size(), false});
    bool first = true;
    for (const auto &c : cat.categories())
    {
        list.push_back({"cat:" + c, c, (int)cat.inCategory(c).size(), first});
        first = false;
    }
    if ((source == "recent" && recentCount == 0) || (source == "favourites" && favouriteCount == 0))
        source = "recommended";
    sourceList.repaint();
    resized();
}

bool Picker::searching() const { return search.getText().trim().isNotEmpty(); }

void Picker::setSource(const juce::String &key)
{
    source = key;
    refreshItems();
}

void Picker::setQuery(const juce::String &q)
{
    search.setText(q, false);
    search.moveCaretToEnd();
    refreshItems();
}

void Picker::refreshItems()
{
    const auto &cat = Catalog::get();
    const auto q = search.getText().trim();

    if (q.isNotEmpty())
        items = cat.search(q);
    else if (source == "recent" || source == "favourites")
    {
        items.clear();
        for (const auto &r : source == "recent" ? proc.getRecentEffects() : proc.getFavourites())
            if (const int i = cat.indexOf(r); i >= 0)
                items.push_back(i);
    }
    else if (source == "all")
        items = cat.alphabetical();
    else if (source.startsWith("cat:"))
        items = cat.inCategory(source.substring(4));
    else
        items = cat.recommended();

    sourceList.selectedKey = q.isNotEmpty() ? juce::String() : source;
    sourceList.repaint();

    results.updateContent();
    results.getViewport()->setViewPosition(0, 0);
    results.selectRow(items.empty() ? -1 : 0, true, true);
    updatePreview();
    results.repaint();
}

void Picker::highlightRow(int row)
{
    if (row >= 0 && row < (int)items.size())
    {
        results.selectRow(row, false, true);
        results.scrollToEnsureRowIsOnscreen(row);
    }
    updatePreview();
}

juce::StringArray Picker::controlNames(int registryIndex)
{
    if (auto it = controlCache.find(registryIndex); it != controlCache.end())
        return it->second;

    juce::StringArray names;
    if (registryIndex >= 0 && registryIndex < (int)AirwinRegistry::registry.size())
    {
        const auto &r = AirwinRegistry::registry[(size_t)registryIndex];
        if (auto fx = r.generator())
        {
            for (int i = 0; i < r.nParams && i < kParamsPerSlot; ++i)
            {
                char txt[256] = {};
                fx->getParameterName(i, txt);
                names.add(juce::String::fromUTF8(txt).trim());
            }
        }
    }
    controlCache[registryIndex] = names;
    return names;
}

void Picker::updatePreview()
{
    const int row = results.getSelectedRow();
    const int index = row >= 0 && row < (int)items.size() ? items[(size_t)row] : -1;
    preview.show(index, index >= 0 ? controlNames(index) : juce::StringArray());
    const auto *e = Catalog::get().find(index);
    preview.setFavourite(e != nullptr && proc.isFavourite(e->name));
}

void Picker::choose(int row, bool keepOpen)
{
    if (row < 0 || row >= (int)items.size() || !onChoose)
        return;
    onChoose(items[(size_t)row], replacingSlot, keepOpen && replacingSlot < 0);
}

//==============================================================================
int Picker::getNumRows() { return (int)items.size(); }

void Picker::paintListBoxItem(int row, juce::Graphics &g, int width, int height, bool selected)
{
    if (row < 0 || row >= (int)items.size())
        return;
    const auto *e = Catalog::get().find(items[(size_t)row]);
    if (e == nullptr)
        return;

    const auto area = juce::Rectangle<float>(8.f, 2.f, (float)width - 16.f, (float)height - 4.f);
    if (selected)
    {
        g.setColour(colour::raised);
        g.fillRoundedRectangle(area, 6.f);
    }

    const float x = area.getX() + 14.f;
    float right = area.getRight() - 14.f;

    const bool showCategory = searching() || !source.startsWith("cat:");
    if (showCategory)
    {
        const auto f = sans(12.5f);
        const float cw = textWidth(f, e->category) + 2.f;
        g.setFont(f);
        g.setColour(colour::text3);
        g.drawText(e->category, juce::Rectangle<float>(right - cw, area.getY() + 9.f, cw, 20.f),
                   juce::Justification::centredRight, false);
        right -= cw + 16.f;
    }

    g.setFont(sans(14.5f, Weight::medium));
    g.setColour(colour::text);
    g.drawText(e->name, juce::Rectangle<float>(x, area.getY() + 9.f, right - x, 20.f),
               juce::Justification::centredLeft, true);

    g.setFont(sans(13.f));
    g.setColour(selected ? colour::text2 : colour::text3);
    g.drawText(e->summary, juce::Rectangle<float>(x, area.getY() + 30.f, area.getRight() - 14.f - x, 18.f),
               juce::Justification::centredLeft, true);
}

void Picker::listBoxItemClicked(int row, const juce::MouseEvent &e)
{
    if (!e.mods.isPopupMenu())
        choose(row, e.mods.isShiftDown());
}

void Picker::selectedRowsChanged(int) { updatePreview(); }

void Picker::returnKeyPressed(int row) { choose(row); }

bool Picker::keyPressed(const juce::KeyPress &key, juce::Component *)
{
    const int n = (int)items.size();
    const int row = results.getSelectedRow();
    const int page = juce::jmax(1, results.getHeight() / kRowHeight - 1);

    auto moveTo = [this, n](int r) {
        if (n > 0)
            highlightRow(juce::jlimit(0, n - 1, r));
        return true;
    };

    if (key == juce::KeyPress::downKey)
        return moveTo(row + 1);
    if (key == juce::KeyPress::upKey)
        return moveTo(row - 1);
    if (key == juce::KeyPress::pageDownKey)
        return moveTo(row + page);
    if (key == juce::KeyPress::pageUpKey)
        return moveTo(row - page);
    if (key == juce::KeyPress::returnKey)
    {
        choose(row, key.getModifiers().isShiftDown());
        return true;
    }
    if (key == juce::KeyPress::escapeKey)
    {
        if (onClose)
            onClose();
        return true;
    }
    return false;
}

//==============================================================================
void Picker::resized()
{
    const int w = getWidth(), h = getHeight();
    const int cw = cancel.idealWidth();
    cancel.setBounds(w - 20 - cw, 18, cw, 36);
    search.setBounds(20, 16, cancel.getX() - 12 - 20, 40);

    const int bodyTop = kTopBar;
    sourceViewport.setBounds(0, bodyTop, kSourceWidth, h - bodyTop);
    sourceList.setSize(kSourceWidth - 8, juce::jmax(sourceList.preferredHeight(), sourceViewport.getHeight()));

    const int previewWidth = juce::jlimit(300, 460, (int)(w * 0.36));
    preview.setBounds(w - previewWidth, bodyTop, previewWidth, h - bodyTop);
    results.setBounds(kSourceWidth + 1, bodyTop + 6, preview.getX() - kSourceWidth - 2, h - bodyTop - 6);
}

void Picker::paint(juce::Graphics &g)
{
    g.fillAll(colour::window);

    g.setColour(colour::side);
    g.fillRect(0, kTopBar, kSourceWidth, getHeight() - kTopBar);

    g.setColour(colour::line);
    g.fillRect(0, kTopBar - 1, getWidth(), 1);
    g.fillRect(kSourceWidth, kTopBar, 1, getHeight() - kTopBar);
    g.fillRect(preview.getX() - 1, kTopBar, 1, getHeight() - kTopBar);

    if (items.empty())
    {
        g.setFont(sans(14.f));
        g.setColour(colour::text3);
        g.drawText("Nothing matches \"" + search.getText().trim() + "\".",
                   results.getBounds().toFloat().withTrimmedTop(24.f).withHeight(24.f).withTrimmedLeft(22.f),
                   juce::Justification::centredLeft, true);
    }
}

void Picker::paintOverChildren(juce::Graphics &g)
{
    // Magnifier inside the search field
    const auto s = search.getBounds().toFloat();
    const auto c = juce::Point<float>(s.getX() + 22.f, s.getCentreY() - 1.f);
    g.setColour(colour::text3);
    g.drawEllipse(juce::Rectangle<float>(11.f, 11.f).withCentre(c), 1.5f);
    g.drawLine(c.x + 4.f, c.y + 4.f, c.x + 8.f, c.y + 8.f, 1.5f);
}
} // namespace awchain
