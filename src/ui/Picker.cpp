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
        const auto labelArea = r.withTrimmedLeft(10.f).withTrimmedRight(40.f);
        g.setFont(sans(13.5f, isSelected ? Weight::medium : Weight::regular));
        g.setColour(isSelected || i == hover ? colour::text : colour::text2);
        g.drawText(s.label, labelArea, juce::Justification::centredLeft, true);
        if (s.count >= 0)
        {
            g.setFont(mono(11.5f));
            g.setColour(colour::text3);
            g.drawText(juce::String(s.count), r.withTrimmedRight(10.f), juce::Justification::centredRight, false);
        }
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
int Picker::Switch::idealWidth() const
{
    return (int)std::ceil(textWidth(sans(14.f), getButtonText())) + 46;
}

void Picker::Switch::paintButton(juce::Graphics &g, bool highlighted, bool down)
{
    const auto r = getLocalBounds().toFloat();
    if (highlighted || down)
    {
        g.setColour(down ? colour::pressed : colour::hover);
        g.fillRoundedRectangle(r, 6.f);
    }
    const bool on = getToggleState();
    const auto dot = juce::Rectangle<float>(8.f, 8.f).withCentre({r.getX() + 18.f, r.getCentreY()});
    g.setColour(on ? colour::accent : colour::text3);
    if (on)
        g.fillEllipse(dot);
    else
        g.drawEllipse(dot, 1.2f);
    g.setFont(sans(14.f, on ? Weight::medium : Weight::regular));
    g.setColour(on || highlighted ? colour::text : colour::text2);
    g.drawText(getButtonText(), r.withTrimmedLeft(32.f), juce::Justification::centredLeft, false);
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

    manualsY = y;
    if (!manuals.isEmpty())
    {
        y += 18 + 24; // the line above and the heading
        for (auto *m : manuals)
        {
            m->setBounds(-8, y, (int)w + 16, 32);
            y += 32;
        }
        y += 30;
    }

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

    if (!manuals.isEmpty())
    {
        g.setColour(colour::line);
        g.fillRect(0.f, (float)manualsY, controls.getWidth(), 1.f);
        g.setFont(sans(13.f, Weight::medium));
        g.setColour(colour::text3);
        g.drawText(manuals.size() > 1 ? "Manuals" : "Manual",
                   juce::Rectangle<float>(0.f, (float)manualsY + 18.f, (float)getWidth(), 18.f),
                   juce::Justification::centredLeft, false);
    }
}

Picker::Preview::ManualLink::ManualLink(const juce::File &f, const juce::String &text) : file(f), label(text)
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setTooltip(f.getFullPathName());
}

void Picker::Preview::ManualLink::paint(juce::Graphics &g)
{
    const auto r = getLocalBounds().toFloat();
    if (isMouseOver())
    {
        g.setColour(colour::hover);
        g.fillRoundedRectangle(r, 6.f);
    }
    const auto kind = file.hasFileExtension("pdf") ? juce::String("PDF") : juce::String("Help");
    const auto detail = kind + ", " + juce::File::descriptionOfSizeInBytes(file.getSize());
    g.setFont(sans(13.f));
    const float dw = textWidth(g.getCurrentFont(), detail) + 2.f;
    g.setColour(colour::text3);
    g.drawText(detail, r.withTrimmedRight(8.f), juce::Justification::centredRight, false);
    g.setFont(sans(14.f));
    g.setColour(colour::text);
    g.drawText(label,
               r.withTrimmedLeft(8.f).withTrimmedRight(dw + 20.f), juce::Justification::centredLeft, true);
}

void Picker::Preview::ManualLink::mouseUp(const juce::MouseEvent &e)
{
    if (contains(e.getPosition()))
        file.startAsProcess();
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

void Picker::Preview::show(int index, const std::optional<ChainProcessor::EffectLabel> &e,
                           const juce::String &controls, const juce::Array<juce::File> &manuals)
{
    if (index != registryIndex)
        viewport.setViewPosition(0, 0);
    registryIndex = index;
    use.setEnabled(e.has_value());
    star.setVisible(!e || !e->hosted);
    viewport.setVisible(e.has_value());
    body.name = e ? e->name : juce::String();
    body.category = e ? e->category : juce::String();
    summaryText = e ? e->summary : juce::String();
    controlText = controls;
    body.docs.setDocument(e && !e->hosted ? "From the Airwindopedia" : juce::String(),
                          e && !e->hosted ? Catalog::get().docParagraphs(index) : juce::StringArray());

    juce::Array<juce::File> shown;
    for (auto *m : body.manuals)
        shown.add(m->file);
    if (shown != manuals)
    {
        body.manuals.clear();
        // One manual is simply the plugin's; several keep their file names to tell them apart.
        for (const auto &f : manuals)
            body.addAndMakeVisible(body.manuals.add(std::make_unique<ManualLink>(
                f, manuals.size() == 1 && e ? e->name + " manual"
                                            : f.getFileNameWithoutExtension().replaceCharacter('_', ' '))));
    }
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

    vocalsOnly = proc.getSetting("vocalsOnly", "0") == "1";
    vocals.setToggleState(vocalsOnly, juce::dontSendNotification);
    vocals.setTooltip("Only effects that suit a voice");
    vocals.onClick = [this] {
        setVocalsOnly(vocals.getToggleState());
        search.grabKeyboardFocus();
    };
    addAndMakeVisible(vocals);

    scanButton.onClick = [this] {
        auto &plugins = proc.hostedPlugins();
        if (plugins.isScanning())
            plugins.stopScan();
        else
            plugins.startScan(true); // gives skipped plugins another go
    };
    addChildComponent(scanButton);
    folders.onClick = [this] { showFolders(); };
    addChildComponent(folders);
    proc.hostedPlugins().addChangeListener(this);

    sourceList.onPick = [this](const juce::String &key) {
        search.setText({}, false);
        showSource(key);
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

Picker::~Picker()
{
    proc.hostedPlugins().removeChangeListener(this);
    search.removeKeyListener(this);
}

void Picker::showSource(const juce::String &key)
{
    auto &plugins = proc.hostedPlugins();
    if (key == "computer" && !plugins.hasScanned())
        plugins.startScan();
    setSource(key);
}

void Picker::showFolders()
{
    auto &plugins = proc.hostedPlugins();
    juce::PopupMenu m;
    m.addSectionHeader("Scanned for VST3 effects");
    m.addItem(HostedPlugins::standardFolder(), false, true, nullptr);
    for (const auto &f : plugins.extraFolders())
    {
        juce::PopupMenu folderMenu;
        folderMenu.addItem("Stop scanning this folder", [this, f] { proc.hostedPlugins().removeFolder(f); });
        m.addSubMenu(f, folderMenu, true, nullptr, true);
    }
    const auto skipped = plugins.skipped();
    if (!skipped.isEmpty())
    {
        // Named so a missing plugin can be explained; Scan again retries them.
        juce::PopupMenu list;
        for (const auto &f : skipped)
            list.addItem(juce::File(f).getFileNameWithoutExtension(), false, false, nullptr);
        m.addSeparator();
        m.addSubMenu("Skipped: crashed or stuck while read (" + juce::String(skipped.size()) + ")", list);
    }
    m.addSeparator();
    m.addItem("Add a folder...", [this] {
        chooser = std::make_unique<juce::FileChooser>("Add a folder of VST3 plugins");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                             [this](const juce::FileChooser &fc) {
                                 const auto f = fc.getResult();
                                 if (!f.isDirectory())
                                     return;
                                 auto &hosted = proc.hostedPlugins();
                                 hosted.addFolder(f);
                                 hosted.stopScan();
                                 hosted.startScan();
                             });
    });
    m.setLookAndFeel(&getLookAndFeel());
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&folders).withMinimumWidth(260));
}

void Picker::changeListenerCallback(juce::ChangeBroadcaster *)
{
    buildSources();
    if (source == "computer" || searching())
    {
        const int row = results.getSelectedRow();
        const int keep = row >= 0 && row < (int)items.size() ? items[(size_t)row] : -1;
        refreshItems(true);
        auto it = std::find(items.begin(), items.end(), keep);
        if (it != items.end())
            highlightRow((int)(it - items.begin()));
    }
    updatePreview(); // a plugin's controls or manual may have come in
    resized();
    repaint();
}

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
    const int currentId = proc.getSlotInfo(replacingSlot).registryIndex;
    const auto current = replacingSlot >= 0 ? proc.slotLabel(replacingSlot) : std::nullopt;
    search.setTextToShowWhenEmpty(current ? "Replace " + current->name + " with..."
                                  : vocalsOnly     ? "Search " + juce::String(Catalog::get().vocalCount()) + " effects for vocals"
                                                   : "Search " + juce::String(total) + " effects",
                                  colour::text3);
    preview.setUseLabel(current ? "Replace" : "Add to chain",
                        current ? juce::String() : juce::String("Shift keeps it open"));
    search.setText({}, false);

    // Replacing starts where the current effect lives: its own category.
    if (current)
        showSource(current->hosted ? juce::String("computer") : "cat:" + current->category);
    else
        showSource(source);

    if (current)
    {
        auto it = std::find(items.begin(), items.end(), currentId);
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
    list.push_back({"recommended", "Chris recommends", countShown(cat.recommended()), false});
    list.push_back({"all", "All effects", countShown(cat.alphabetical()), false});
    const auto &plugins = proc.hostedPlugins();
    list.push_back({"computer", "Your VST3 plugins", plugins.hasScanned() ? (int)plugins.effects().size() : -1, true});
    bool first = true;
    for (const auto &c : cat.categories())
    {
        const int n = countShown(cat.inCategory(c));
        if (n == 0)
            continue; // the vocals switch can empty a category
        list.push_back({"cat:" + c, c, n, first});
        first = false;
    }
    if ((source == "recent" && recentCount == 0) || (source == "favourites" && favouriteCount == 0))
        source = "recommended";
    sourceList.repaint();
    resized();
}

bool Picker::searching() const { return search.getText().trim().isNotEmpty(); }

void Picker::setVocalsOnly(bool on)
{
    vocalsOnly = on;
    vocals.setToggleState(on, juce::dontSendNotification);
    proc.setSetting("vocalsOnly", on ? "1" : "0");
    if (replacingSlot < 0)
        search.setTextToShowWhenEmpty(on ? "Search " + juce::String(Catalog::get().vocalCount()) + " effects for vocals"
                                         : "Search " + juce::String((int)Catalog::get().all().size()) + " effects",
                                      colour::text3);
    buildSources();
    refreshItems();
}

int Picker::countShown(std::vector<int> list) const
{
    if (vocalsOnly)
        Catalog::get().keepVocals(list);
    return (int)list.size();
}

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

void Picker::refreshItems(bool keepPlace)
{
    const auto previous = items;
    const auto &cat = Catalog::get();
    const auto q = search.getText().trim();

    if (q.isNotEmpty())
    {
        items = cat.search(q);
        if (vocalsOnly)
            cat.keepVocals(items);
        for (int id : proc.hostedPlugins().search(q))
            items.push_back(id);
    }
    else if (source == "computer")
        items = proc.hostedPlugins().effects();
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
    if (vocalsOnly && q.isEmpty() && source != "computer")
        cat.keepVocals(items);

    const bool onComputer = q.isEmpty() && source == "computer";
    vocals.setVisible(!onComputer);
    scanButton.setButtonText(proc.hostedPlugins().isScanning() ? "Stop" : "Scan again");
    scanButton.setVisible(onComputer);
    folders.setVisible(onComputer);
    resized();

    sourceList.selectedKey = q.isNotEmpty() ? juce::String() : source;
    sourceList.repaint();

    if (keepPlace && items == previous)
        return; // a scan tick or a plugin read, with the list unchanged
    results.updateContent();
    results.getViewport()->setViewPosition(0, 0);
    results.selectRow(items.empty() ? -1 : 0, true, true);
    updatePreview();
    results.repaint();
}

void Picker::highlightEffect(int id)
{
    if (const auto it = std::find(items.begin(), items.end(), id); it != items.end())
        highlightRow((int)(it - items.begin()));
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
    juce::String controls;
    juce::Array<juce::File> manuals;
    if (isHosted(index))
    {
        auto &plugins = proc.hostedPlugins();
        manuals = plugins.manualsFor(index);
        if (const auto names = plugins.controlsOf(index); !names)
            controls = "Reading...";
        else if (names->isEmpty())
            controls = "In the plugin's own window.";
        else
        {
            constexpr int kListed = 40; // Pro-Q 3 has 345
            controls = names->joinIntoString(", ", 0, kListed);
            if (names->size() > kListed)
                controls << ", and " << (names->size() - kListed) << " more";
            if (names->size() > kParamsPerSlot)
                controls << "\n\nThe first " << kParamsPerSlot
                         << " are sliders in the chain. All of them are in the plugin's own window.";
        }
    }
    else if (index >= 0)
        controls = controlNames(index).joinIntoString(", ");
    preview.show(index, proc.effectLabel(index), controls, manuals);
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
    const auto e = proc.effectLabel(items[(size_t)row]);
    if (!e)
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
    int right = cancel.getX() - 8;
    if (vocals.isVisible())
    {
        vocals.setBounds(right - vocals.idealWidth(), 18, vocals.idealWidth(), 36);
        right = vocals.getX() - 8;
    }
    for (auto *b : {&folders, &scanButton})
        if (b->isVisible())
        {
            b->setBounds(right - b->idealWidth(), 18, b->idealWidth(), 36);
            right = b->getX() - 4;
        }
    if (scanButton.isVisible() && proc.hostedPlugins().isScanning())
        right -= 250; // room for the scan's progress
    search.setBounds(20, 16, right - 4 - 20, 40);

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

    const auto &plugins = proc.hostedPlugins();
    const auto line = results.getBounds().toFloat().withTrimmedTop(24.f).withHeight(24.f).withTrimmedLeft(22.f);
    g.setFont(sans(14.f));
    g.setColour(colour::text3);
    if (!searching() && source == "computer")
    {
        // While scanning: the file being read and how far along, beside Stop, and
        // the same fraction as a line along the bottom of the top bar.
        if (plugins.isScanning())
        {
            const float done = plugins.progress();
            const auto file = juce::File(plugins.nowScanning()).getFileNameWithoutExtension();
            const auto area = juce::Rectangle<float>((float)search.getRight() + 14.f, 18.f,
                                                     (float)(scanButton.getX() - search.getRight() - 20), 36.f);
            g.setFont(mono(11.5f));
            g.drawText(juce::String(juce::roundToInt(done * 100.f)) + "%", area, juce::Justification::centredRight,
                       false);
            g.setFont(sans(13.5f));
            g.drawText(file, area.withTrimmedRight(44.f), juce::Justification::centredLeft, true);
            g.setColour(colour::accent);
            g.fillRect(0.f, (float)kTopBar - 2.f, (float)getWidth() * done, 2.f);
            g.setColour(colour::text3);
            g.setFont(sans(14.f));
            if (items.empty())
                g.drawText("Looking through your VST3 folders.", line, juce::Justification::centredLeft, true);
        }
        else if (items.empty())
            g.drawText("No VST3 effects found. Add a folder under Folders.", line,
                       juce::Justification::centredLeft, true);
    }
    else if (items.empty())
        g.drawText("Nothing matches \"" + search.getText().trim() + "\".", line, juce::Justification::centredLeft,
                   true);
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
