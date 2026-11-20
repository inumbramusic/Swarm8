#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

//==============================================================================
class SwarmLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SwarmLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

//==============================================================================
// Touch / mouse ribbon. valueParam gets 0..1 from the horizontal position;
// gateParam (optional) is held at 1 while touched (or latched).
class Ribbon : public juce::Component
{
public:
    Ribbon (Swarm8Processor& p, int valueParam, int gateParam, juce::String title);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void setLatch (bool shouldLatch);
    bool latch = false;

private:
    void setFromX (float x);
    Swarm8Processor& proc;
    int valueIndex, gateIndex;
    juce::String title;
    bool touching = false;
};

//==============================================================================
class Swarm8Editor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit Swarm8Editor (Swarm8Processor&);
    ~Swarm8Editor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    struct Control
    {
        int index = -1;
        juce::Label label;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::ComboBox> combo;
        std::unique_ptr<juce::TextButton> button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> cAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bAtt;
        juce::Component* main() const;
    };
    struct Group { juce::String name; std::vector<Control*> controls; juce::Rectangle<int> bounds; };

    void timerCallback() override;
    void refreshLabels();
    void showLearnMenu (Control& c);
    Control* findControl (juce::Component* comp);

    Swarm8Processor& proc;
    SwarmLookAndFeel lnf;
    std::vector<std::unique_ptr<Control>> controls;
    std::vector<std::vector<Group>> rows;

    juce::Label titleLabel, oscLabel, oscPortLabel, learnLabel;
    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, latchBtn { "Latch" }, resetCCBtn { "Reset CCs" }, panicBtn { "Panic" };
    Ribbon pitchRibbon, swarmRibbon;
    juce::Rectangle<int> ribbonArea;
    int lastCCVersion = -1, lastLearn = -2, lastProgram = -1;
    juce::String localIP;
    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Swarm8Editor)
};
