#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include <array>

class HarrisonFaderStrip : public juce::Component, private juce::Timer
{
public:
    explicit HarrisonFaderStrip (BruceSwedienRiderProcessor& proc);
    void paint (juce::Graphics& g) override;

private:
    void timerCallback() override;

    BruceSwedienRiderProcessor& processor;
    float displayedRideDb = 0.0f;
    float displayedLevelDb = -60.0f;
};

class BruceSwedienRiderEditor : public juce::AudioProcessorEditor
{
public:
    explicit BruceSwedienRiderEditor (BruceSwedienRiderProcessor&);
    ~BruceSwedienRiderEditor() override = default;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void styleKnob (juce::Slider& s, juce::Label& lbl, const juce::String& text, juce::Colour accent);
    void styleCombo (juce::ComboBox& c, juce::Label& lbl, const juce::String& text);
    void styleToggle (juce::ToggleButton& b, juce::Colour ledCol);

    BruceSwedienRiderProcessor& audioProcessor;
    HarrisonFaderStrip faderStrip;

    juce::Label sourceLbl, speedLbl, amountLbl, floorLbl, gainLbl, preampLbl;
    juce::ComboBox sourceBox, speedBox, preampBox;
    juce::Slider amountSlider, floorSlider, gainSlider;
    juce::ToggleButton pocketBtn { "POCKET" }, rideBtn { "RIDE" };

    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<ComboAtt> sourceAtt, speedAtt, preampAtt;
    std::unique_ptr<SliderAtt> amountAtt, floorAtt, gainAtt;
    std::unique_ptr<ButtonAtt> pocketAtt, rideAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BruceSwedienRiderEditor)
};
