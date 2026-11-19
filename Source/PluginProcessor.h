#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_osc/juce_osc.h>
#include "SwarmEngine.h"
#include "Presets.h"
#include <array>
#include <atomic>

class Swarm8Processor : public juce::AudioProcessor,
                        private juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback>,
                        private juce::Timer
{
public:
    Swarm8Processor();
    ~Swarm8Processor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Swarm8"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- shared with the editor ---------------------------------------------
    juce::AudioProcessorValueTreeState apvts;
    juce::RangedAudioParameter* param (int i) const { return params[(size_t) i]; }

    // MIDI learn: ccForParam returns -1 if unmapped
    void startLearn (int paramIndex)  { learnParam.store (paramIndex); }
    void cancelLearn()                { learnParam.store (-1); }
    int  learningParam() const        { return learnParam.load(); }
    int  ccForParam (int paramIndex) const;
    void clearCC (int paramIndex);
    void resetCCMapToDefaults();
    int  ccMapVersion() const         { return ccVersion.load(); }

    // OSC (for iPad/iPhone control via TouchOSC etc.)
    bool setOscPort (int port);
    int  getOscPort() const   { return oscPort; }
    bool isOscConnected() const { return oscConnected; }

    // live readouts for the UI
    std::atomic<float> uiSwarm { 0 }, uiPitch { 48 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void oscMessageReceived (const juce::OSCMessage& m) override;
    void oscBundleReceived (const juce::OSCBundle& b) override;
    void timerCallback() override;
    void setParamReal (int index, float realValue);
    void setParamNorm (int index, float norm);
    void handleMidi (const juce::MidiMessage& m);

    swarm::SwarmEngine engine;
    std::array<juce::RangedAudioParameter*, swarm::NumParams> params {};
    std::array<std::atomic<float>*, swarm::NumParams> raw {};

    // CC -> param index (or -1); written from message thread and audio thread (learn)
    std::array<std::atomic<int>, 128> ccMap;
    std::atomic<int> learnParam { -1 }, ccVersion { 0 };
    // CC values received on the audio thread, pushed to the host on the message thread
    std::array<std::atomic<float>, swarm::NumParams> ccValue;
    std::array<std::atomic<bool>,  swarm::NumParams> ccPending;

    juce::OSCReceiver osc;
    int  oscPort = 9000;
    bool oscConnected = false;
    int  currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Swarm8Processor)
};
