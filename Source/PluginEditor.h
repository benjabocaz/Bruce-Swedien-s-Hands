#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include <array>

class ConsoleLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                         float sliderPosProportional, float rotaryStartAngle,
                         float rotaryEndAngle, juce::Slider& slider) override;

    void drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox& box) override;

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;
};

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
    ~BruceSwedienRiderEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void configureKnob (juce::Slider& s, juce::Label& lbl, const juce::String& title, juce::Colour capCol);
    void configureCombo (juce::ComboBox& c, juce::Label& lbl, const juce::String& title);
    void stepSource (int delta);

    BruceSwedienRiderProcessor& audioProcessor;
    ConsoleLookAndFeel consoleLnf;
    HarrisonFaderStrip faderStrip;

    juce::TextButton prevSourceBtn { "<" };
    juce::TextButton nextSourceBtn { ">" };

    juce::Label sourceLbl, workflowLbl, channelLbl, speedLbl, amountLbl, floorLbl, gainLbl, preampLbl;
    juce::ComboBox sourceBox, workflowBox, channelBox, speedBox, preampBox;
    juce::Slider amountSlider, floorSlider, gainSlider;
    juce::ToggleButton pocketBtn { "POCKET" };
    juce::ToggleButton rideBtn { "RIDE" };

    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<ComboAtt> sourceAtt, workflowAtt, channelAtt, speedAtt, preampAtt;
    std::unique_ptr<SliderAtt> amountAtt, floorAtt, gainAtt;
    std::unique_ptr<ButtonAtt> pocketAtt, rideAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BruceSwedienRiderEditor)
};
