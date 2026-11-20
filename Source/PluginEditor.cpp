#include "PluginEditor.h"

using namespace swarm;

namespace colours
{
    const juce::Colour bg     { 0xff111113 };
    const juce::Colour panel  { 0xff1b1b1f };
    const juce::Colour edge   { 0xff2c2c33 };
    const juce::Colour text   { 0xffe4e2dc };
    const juce::Colour dim    { 0xff8b8a86 };
    const juce::Colour accent { 0xffff9a2e };
    const juce::Colour learn  { 0xff39d0ff };
}

//==============================================================================
SwarmLookAndFeel::SwarmLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, colours::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, colours::text);
    setColour (juce::ComboBox::backgroundColourId, colours::edge);
    setColour (juce::ComboBox::outlineColourId, colours::edge);
    setColour (juce::ComboBox::textColourId, colours::text);
    setColour (juce::ComboBox::arrowColourId, colours::accent);
    setColour (juce::PopupMenu::backgroundColourId, colours::panel);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::accent.withAlpha (0.35f));
    setColour (juce::TextButton::buttonColourId, colours::edge);
    setColour (juce::TextButton::buttonOnColourId, colours::accent);
    setColour (juce::TextButton::textColourOffId, colours::text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::black);
}

void SwarmLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float lineW = juce::jmax (2.5f, radius * 0.16f);
    const float arcR = radius - lineW * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);

    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (colours::edge);
    g.strokePath (track, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // bipolar knobs fill from the centre
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    juce::Path val;
    val.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
    g.setColour (s.isEnabled() ? colours::accent : colours::dim);
    g.strokePath (val, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (colours::panel.brighter (0.15f));
    g.fillEllipse (juce::Rectangle<float> (arcR * 1.3f, arcR * 1.3f).withCentre (c));
    juce::Path tick;
    tick.addRoundedRectangle (-1.5f, -arcR * 0.62f, 3.0f, arcR * 0.4f, 1.5f);
    g.setColour (colours::text);
    g.fillPath (tick, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
}

//==============================================================================
Ribbon::Ribbon (Swarm8Processor& p, int v, int gte, juce::String t)
    : proc (p), valueIndex (v), gateIndex (gte), title (std::move (t)) {}

void Ribbon::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (colours::panel.darker (0.3f));
    g.fillRoundedRectangle (r, 6.0f);

    const bool gated = gateIndex >= 0 && proc.param (gateIndex)->getValue() > 0.5f;
    juce::ColourGradient grad (colours::accent.withAlpha (gated ? 0.28f : 0.10f), r.getX(), 0,
                               colours::accent.withAlpha (gated ? 0.06f : 0.02f), r.getRight(), 0, false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 6.0f);

    // pitch ribbon: draw note grid
    if (valueIndex == pRibbon)
    {
        const auto* pv = proc.apvts.getRawParameterValue ("ribBase");
        const auto* pr = proc.apvts.getRawParameterValue ("ribRange");
        const float base = pv->load(), range = pr->load() * 12.0f;
        for (int st = 0; st <= (int) range; ++st)
        {
            const int note = (int) base + st;
            const float x = r.getX() + r.getWidth() * (float) st / range;
            const bool isC = note % 12 == 0;
            g.setColour (colours::dim.withAlpha (isC ? 0.7f : 0.18f));
            g.drawVerticalLine ((int) x, r.getY() + (isC ? 2.0f : r.getHeight() * 0.6f), r.getBottom() - 2.0f);
            if (isC)
            {
                g.setFont (11.0f);
                g.drawText (juce::MidiMessage::getMidiNoteName (note, true, true, 3), (int) x + 3, (int) r.getY() + 2, 40, 14,
                            juce::Justification::left);
            }
        }
    }
    else // swarm ribbon: show the actual (lagged) swarm as a ghost marker
    {
        const float sx = r.getX() + r.getWidth() * proc.uiSwarm.load();
        g.setColour (colours::learn.withAlpha (0.55f));
        g.fillRect (sx - 1.0f, r.getY() + 4, 2.0f, r.getHeight() - 8);
    }

    const float v = proc.param (valueIndex)->getValue();
    const float x = r.getX() + r.getWidth() * v;
    g.setColour (colours::accent);
    g.fillRoundedRectangle (x - 3.0f, r.getY() + 2, 6.0f, r.getHeight() - 4, 3.0f);

    g.setColour (colours::text.withAlpha (0.8f));
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawText (title, r.reduced (10, 4), juce::Justification::bottomLeft);
}

void Ribbon::setFromX (float x)
{
    const float v = juce::jlimit (0.0f, 1.0f, (x - 1.0f) / juce::jmax (1.0f, (float) getWidth() - 2.0f));
    proc.param (valueIndex)->setValueNotifyingHost (v);
    repaint();
}

void Ribbon::mouseDown (const juce::MouseEvent& e)
{
    touching = true;
    proc.param (valueIndex)->beginChangeGesture();
    setFromX ((float) e.position.x);
    if (gateIndex >= 0) proc.param (gateIndex)->setValueNotifyingHost (1.0f);
}

void Ribbon::mouseDrag (const juce::MouseEvent& e) { setFromX ((float) e.position.x); }

void Ribbon::mouseUp (const juce::MouseEvent&)
{
    touching = false;
    proc.param (valueIndex)->endChangeGesture();
    if (gateIndex >= 0 && ! latch) proc.param (gateIndex)->setValueNotifyingHost (0.0f);
    repaint();
}

void Ribbon::setLatch (bool shouldLatch)
{
    latch = shouldLatch;
    if (! latch && ! touching && gateIndex >= 0) proc.param (gateIndex)->setValueNotifyingHost (0.0f);
}

//==============================================================================
juce::Component* Swarm8Editor::Control::main() const
{
    if (slider) return slider.get();
    if (combo)  return combo.get();
    return button.get();
}

Swarm8Editor::Swarm8Editor (Swarm8Processor& p)
    : AudioProcessorEditor (&p), proc (p),
      pitchRibbon (p, pRibbon, pRibGate, "PITCH RIBBON  -  touch to play"),
      swarmRibbon (p, pSwarm, -1, "SWARM RIBBON")
{
    setLookAndFeel (&lnf);

    // ---- build controls from the parameter table ----------------------------
    const char* rowDefs[4][4] = { { "OSC", "SWARM", nullptr, nullptr },
                                  { "VOICE", "PITCH", nullptr, nullptr },
                                  { "FILTER", "FILTER A", "FILTER B", "LFO" },
                                  { "ENV A", "ENV B", "AMP", "OUTPUT" } };
    for (auto& rd : rowDefs)
    {
        std::vector<Group> row;
        for (auto* gname : rd)
        {
            if (gname == nullptr) continue;
            Group grp; grp.name = gname;
            for (int i = 0; i < NumParams; ++i)
            {
                const auto& d = kParams[i];
                if (juce::String (d.group) != gname) continue;

                auto c = std::make_unique<Control>();
                c->index = i;
                c->label.setText (d.name, juce::dontSendNotification);
                c->label.setJustificationType (juce::Justification::centred);
                c->label.setFont (juce::Font (juce::FontOptions (11.5f)));
                c->label.setColour (juce::Label::textColourId, colours::dim);
                c->label.setTooltip ("Click for MIDI learn");
                c->label.addMouseListener (this, false);
                addAndMakeVisible (c->label);

                if (d.type == PType::Choice)
                {
                    c->combo = std::make_unique<juce::ComboBox>();
                    c->combo->addItemList (juce::StringArray::fromTokens (d.choices, "|", ""), 1);
                    addAndMakeVisible (*c->combo);
                    c->cAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, d.id, *c->combo);
                }
                else if (d.type == PType::Bool)
                {
                    c->button = std::make_unique<juce::TextButton> ("On");
                    c->button->setClickingTogglesState (true);
                    addAndMakeVisible (*c->button);
                    c->bAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, d.id, *c->button);
                    auto* btn = c->button.get();
                    btn->onStateChange = [btn] { btn->setButtonText (btn->getToggleState() ? "On" : "Off"); };
                    btn->setButtonText (btn->getToggleState() ? "On" : "Off");
                }
                else
                {
                    c->slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
                    c->slider->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
                    c->slider->setColour (juce::Slider::textBoxBackgroundColourId, colours::bg.withAlpha (0.5f));
                    c->slider->setColour (juce::Slider::textBoxTextColourId, colours::text);
                    c->slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
                    c->slider->addMouseListener (this, true);
                    addAndMakeVisible (*c->slider);
                    c->sAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, d.id, *c->slider);
                    c->slider->setDoubleClickReturnValue (true, d.def);
                }
                grp.controls.push_back (c.get());
                controls.push_back (std::move (c));
            }
            if (! grp.controls.empty()) row.push_back (grp);
        }
        rows.push_back (row);
    }

    // ---- top bar -------------------------------------------------------------
    #ifdef JucePlugin_VersionString
    titleLabel.setText (juce::String ("SWARM8  v") + JucePlugin_VersionString, juce::dontSendNotification);
   #else
    titleLabel.setText ("SWARM8  dev", juce::dontSendNotification);
   #endif
    titleLabel.setFont (juce::Font (juce::FontOptions (20.0f, juce::Font::bold)));
    titleLabel.setColour (juce::Label::textColourId, colours::accent);
    addAndMakeVisible (titleLabel);

    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.onChange = [this] { const int id = presetBox.getSelectedId(); if (id > 0) proc.setCurrentProgram (id - 1); };
    addAndMakeVisible (presetBox);
    prevBtn.onClick = [this] { const int n = proc.getNumPrograms(); proc.setCurrentProgram ((proc.getCurrentProgram() + n - 1) % n); };
    nextBtn.onClick = [this] { proc.setCurrentProgram ((proc.getCurrentProgram() + 1) % proc.getNumPrograms()); };
    addAndMakeVisible (prevBtn); addAndMakeVisible (nextBtn);

    learnLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    learnLabel.setColour (juce::Label::textColourId, colours::learn);
    learnLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (learnLabel);

    resetCCBtn.setTooltip ("Restore the default MIDI CC map");
    resetCCBtn.onClick = [this] { proc.resetCCMapToDefaults(); };
    addAndMakeVisible (resetCCBtn);

    panicBtn.setTooltip ("Stop all sound: releases held/latched notes, sustain pedal and ribbon");
    panicBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff6b1d1d));
    panicBtn.onClick = [this]
    {
        latchBtn.setToggleState (false, juce::sendNotificationSync);
        proc.requestPanic();
    };
    addAndMakeVisible (panicBtn);

    for (auto& a : juce::IPAddress::getAllAddresses (false))
        if (! a.toString().startsWith ("127.") && ! a.toString().startsWith ("169.254")) { localIP = a.toString(); break; }
    oscLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    oscLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (oscLabel);
    oscPortLabel.setText (juce::String (proc.getOscPort()), juce::dontSendNotification);
    oscPortLabel.setEditable (true);
    oscPortLabel.setTooltip ("OSC port (click to edit)");
    oscPortLabel.setColour (juce::Label::backgroundColourId, colours::edge);
    oscPortLabel.setJustificationType (juce::Justification::centred);
    oscPortLabel.onTextChange = [this] { proc.setOscPort (oscPortLabel.getText().getIntValue()); };
    addAndMakeVisible (oscPortLabel);

    // ---- ribbons ---------------------------------------------------------------
    addAndMakeVisible (pitchRibbon);
    addAndMakeVisible (swarmRibbon);
    latchBtn.setClickingTogglesState (true);
    latchBtn.setTooltip ("Keep the ribbon note sounding after you let go");
    latchBtn.onClick = [this] { pitchRibbon.setLatch (latchBtn.getToggleState()); };
    addAndMakeVisible (latchBtn);

    setResizable (true, true);
    setResizeLimits (1000, 720, 2200, 1500);
    setSize (1240, 860);
    startTimerHz (30);
}

Swarm8Editor::~Swarm8Editor()
{
    stopTimer();
    for (auto& c : controls)
    {
        c->label.removeMouseListener (this);
        if (c->slider) c->slider->removeMouseListener (this);
    }
    setLookAndFeel (nullptr);
}

//==============================================================================
void Swarm8Editor::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg);
    for (auto& row : rows)
        for (auto& grp : row)
        {
            g.setColour (colours::panel);
            g.fillRoundedRectangle (grp.bounds.toFloat(), 8.0f);
            g.setColour (colours::edge);
            g.drawRoundedRectangle (grp.bounds.toFloat().reduced (0.5f), 8.0f, 1.0f);
            g.setColour (colours::accent.withAlpha (0.9f));
            g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
            g.drawText (grp.name, grp.bounds.reduced (10, 4).removeFromTop (16), juce::Justification::left);
        }
    g.setColour (colours::panel);
    g.fillRoundedRectangle (ribbonArea.toFloat(), 8.0f);
}

void Swarm8Editor::resized()
{
    auto area = getLocalBounds().reduced (10);

    auto top = area.removeFromTop (34);
    titleLabel.setBounds (top.removeFromLeft (170));
    prevBtn.setBounds (top.removeFromLeft (30).reduced (0, 3));
    presetBox.setBounds (top.removeFromLeft (220).reduced (4, 3));
    nextBtn.setBounds (top.removeFromLeft (30).reduced (0, 3));
    oscPortLabel.setBounds (top.removeFromRight (60).reduced (4, 5));
    oscLabel.setBounds (top.removeFromRight (230));
    resetCCBtn.setBounds (top.removeFromRight (90).reduced (4, 4));
    panicBtn.setBounds (top.removeFromRight (70).reduced (4, 4));
    learnLabel.setBounds (top.reduced (8, 0));
    area.removeFromTop (8);

    ribbonArea = area.removeFromBottom (juce::jmax (120, area.getHeight() / 7));
    area.removeFromBottom (8);
    {
        auto r = ribbonArea.reduced (10);
        auto pitchRow = r.removeFromTop (r.getHeight() / 2).reduced (0, 3);
        latchBtn.setBounds (pitchRow.removeFromRight (70).reduced (4, juce::jmax (4, (pitchRow.getHeight() - 30) / 2)));
        pitchRibbon.setBounds (pitchRow);
        auto swarmRow = r.reduced (0, 3);
        swarmRow.removeFromRight (70);
        swarmRibbon.setBounds (swarmRow);
    }

    const int rowH = area.getHeight() / (int) rows.size();
    const int gap = 8;
    // one uniform cell width (set by the busiest row) so every knob is the same size;
    // drop-down menus get a wider cell so their text fits
    auto weight = [] (const Control* c) { return c->combo ? 1.7f : 1.0f; };
    auto groupUnits = [&] (const Group& g) { float u = 0; for (auto* c : g.controls) u += weight (c); return u; };
    float cell = 1.0e9f;
    for (auto& row : rows)
    {
        float total = 0;
        for (auto& grp : row) total += groupUnits (grp);
        cell = juce::jmin (cell, (float) (area.getWidth() - gap * ((int) row.size() - 1) - 12 * (int) row.size()) / total);
    }
    for (auto& row : rows)
    {
        auto rowArea = area.removeFromTop (rowH).withTrimmedBottom (gap);
        int used = gap * ((int) row.size() - 1);
        for (auto& grp : row) used += (int) (cell * groupUnits (grp)) + 12;
        rowArea = rowArea.withSizeKeepingCentre (used, rowArea.getHeight());

        for (auto& grp : row)
        {
            const int gw = (int) (cell * groupUnits (grp)) + 12;
            grp.bounds = rowArea.removeFromLeft (gw);
            rowArea.removeFromLeft (gap);

            auto inner = grp.bounds.reduced (6, 4);
            inner.removeFromTop (20);
            for (auto* c : grp.controls)
            {
                auto cr = inner.removeFromLeft ((int) (cell * weight (c)));
                c->label.setBounds (cr.removeFromTop (16));
                cr.reduce (2, 2);
                if (c->slider) c->slider->setBounds (cr);
                else
                {
                    auto b = cr.withSizeKeepingCentre (cr.getWidth() - 4, 26);
                    if (c->combo) c->combo->setBounds (b); else c->button->setBounds (b.reduced (6, 0));
                }
            }
        }
    }
}

//==============================================================================
Swarm8Editor::Control* Swarm8Editor::findControl (juce::Component* comp)
{
    for (auto& c : controls)
        for (auto* p = comp; p != nullptr && p != this; p = p->getParentComponent())
            if (p == &c->label || p == c->main()) return c.get();
    return nullptr;
}

void Swarm8Editor::mouseDown (const juce::MouseEvent& e)
{
    if (e.eventComponent == this) return;
    auto* c = findControl (e.eventComponent);
    if (c == nullptr) return;
    const bool onLabel = e.eventComponent == &c->label;
    if (onLabel || e.mods.isPopupMenu()) showLearnMenu (*c);
}

void Swarm8Editor::showLearnMenu (Control& c)
{
    const int idx = c.index;
    const int cc = proc.ccForParam (idx);
    juce::PopupMenu m;
    m.addSectionHeader (juce::String (kParams[idx].name) + (cc >= 0 ? "  (CC " + juce::String (cc) + ")" : ""));
    if (proc.learningParam() == idx) m.addItem (1, "Cancel MIDI Learn");
    else                             m.addItem (1, "MIDI Learn - move a knob/fader");
    m.addItem (2, "Clear CC", cc >= 0);
    m.addSeparator();
    m.addItem (3, "Reset all CCs to defaults");
    m.addItem (4, "OSC address: /swarm/" + juce::String (kParams[idx].id), false);

    juce::Component::SafePointer<Swarm8Editor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&c.label), [safe, idx] (int r)
    {
        if (safe == nullptr) return;
        auto& pr = safe->proc;
        if (r == 1) { if (pr.learningParam() == idx) pr.cancelLearn(); else pr.startLearn (idx); }
        if (r == 2) pr.clearCC (idx);
        if (r == 3) pr.resetCCMapToDefaults();
        safe->refreshLabels();
    });
}

void Swarm8Editor::refreshLabels()
{
    const int learning = proc.learningParam();
    for (auto& c : controls)
    {
        const int cc = proc.ccForParam (c->index);
        juce::String t (kParams[c->index].name);
        if (learning == c->index) t = "LEARN...";
        else if (cc >= 0)         t << "  " << cc;
        c->label.setText (t, juce::dontSendNotification);
        c->label.setColour (juce::Label::textColourId,
                            learning == c->index ? colours::learn : (cc >= 0 ? colours::text : colours::dim));
    }
    learnLabel.setText (learning >= 0 ? "Move a MIDI control to assign it to " + juce::String (kParams[learning].name)
                                      : "Click a knob's name for MIDI learn",
                        juce::dontSendNotification);
}

void Swarm8Editor::timerCallback()
{
    const int v = proc.ccMapVersion(), l = proc.learningParam();
    if (v != lastCCVersion || l != lastLearn) { lastCCVersion = v; lastLearn = l; refreshLabels(); }

    if (proc.getCurrentProgram() != lastProgram)
    {
        lastProgram = proc.getCurrentProgram();
        presetBox.setSelectedId (lastProgram + 1, juce::dontSendNotification);
    }

    oscLabel.setText (proc.isOscConnected() ? "OSC listening  " + (localIP.isNotEmpty() ? localIP : juce::String ("?")) + "  port"
                                            : "OSC port unavailable - try another",
                      juce::dontSendNotification);
    oscLabel.setColour (juce::Label::textColourId, proc.isOscConnected() ? colours::dim : juce::Colours::orangered);

    const int vm = juce::roundToInt (proc.apvts.getRawParameterValue ("voiceMode")->load());
    for (auto& c : controls)
    {
        bool relevant = true;
        if (c->index == pVoices) relevant = vm == 1 || vm == 3;
        if (c->index == pMorphTime || c->index == pMorphSync || c->index == pMorphCurve || c->index == pStagger)
            relevant = vm == 2 || vm == 3;
        if (c->index == pChordSpread || c->index == pChordLatch) relevant = vm == 2;
        if (c->index == pLegato || c->index == pDrone) relevant = vm == 0 || vm == 2;
        const float alpha = relevant ? 1.0f : 0.35f;
        if (c->main()->getAlpha() != alpha) { c->main()->setAlpha (alpha); c->label.setAlpha (alpha); }
    }

    pitchRibbon.repaint();
    swarmRibbon.repaint();
}
