#include "PluginEditor.h"

HarrisonFaderStrip::HarrisonFaderStrip (BruceSwedienRiderProcessor& proc)
    : processor (proc)
{
    startTimerHz (30);
}

void HarrisonFaderStrip::timerCallback()
{
    const float targetRide = processor.getRideOffsetDb();
    const float targetLvl  = processor.getOutputLevelDb();

    displayedRideDb  += 0.28f * (targetRide - displayedRideDb);
    displayedLevelDb += 0.35f * (targetLvl - displayedLevelDb);
    repaint();
}

void HarrisonFaderStrip::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour (juce::Colour (0xff151210));
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (juce::Colour (0xff4d3f2e));
    g.drawRoundedRectangle (bounds, 6.0f, 1.2f);

    auto faderArea = bounds.removeFromLeft (bounds.getWidth() * 0.54f).reduced (10.0f, 14.0f);

    g.setColour (juce::Colour (0xffe8c27a));
    g.setFont (juce::Font (juce::FontOptions (10.0f).withStyle ("Bold")));
    g.drawText ("FADER", faderArea.removeFromTop (16.0f), juce::Justification::centred);

    auto scaleArea = faderArea.removeFromLeft (28.0f);
    g.setColour (juce::Colour (0xffb8a78c));
    g.setFont (juce::Font (juce::FontOptions (8.5f).withStyle ("Bold")));

    static constexpr std::array<const char*, 5> dbMarks { "+12", "+6", "0", "-6", "-12" };
    for (int i = 0; i < 5; ++i)
    {
        const float yPos = scaleArea.getY() + (static_cast<float> (i) / 4.0f) * scaleArea.getHeight();
        g.drawText (dbMarks[static_cast<size_t> (i)], static_cast<int> (scaleArea.getX()),
                    static_cast<int> (yPos - 6.0f), 26, 12, juce::Justification::centredRight);
    }

    auto trackArea = faderArea.reduced (14.0f, 6.0f);
    g.setColour (juce::Colour (0xff090706));
    g.fillRoundedRectangle (trackArea.getCentreX() - 4.0f, trackArea.getY(), 8.0f, trackArea.getHeight(), 3.0f);

    const float zeroY = trackArea.getCentreY();
    g.setColour (juce::Colour (0xffe8c27a).withAlpha (0.45f));
    g.drawHorizontalLine (static_cast<int> (zeroY), trackArea.getX() - 4.0f, trackArea.getRight() + 4.0f);

    const float clampedRide = std::clamp (displayedRideDb, -12.0f, 12.0f);
    const float normPos = 0.5f - (clampedRide / 24.0f);
    const float capY = trackArea.getY() + normPos * trackArea.getHeight();

    const float barTop = std::min (zeroY, capY);
    const float barH   = std::abs (capY - zeroY);
    g.setColour ((clampedRide >= 0.0f ? juce::Colour (0xfff2b84b) : juce::Colour (0xffd97848)).withAlpha (0.35f));
    g.fillRect (trackArea.getCentreX() - 3.0f, barTop, 6.0f, barH);

    juce::Rectangle<float> cap (trackArea.getCentreX() - 20.0f, capY - 12.0f, 40.0f, 24.0f);
    g.setColour (juce::Colours::black.withAlpha (0.65f));
    g.fillRoundedRectangle (cap.translated (0.0f, 2.0f), 3.0f);
    g.setColour (juce::Colour (0xff362d26));
    g.fillRoundedRectangle (cap, 3.0f);
    g.setColour (juce::Colour (0xff9c7c50));
    g.drawRoundedRectangle (cap, 3.0f, 1.1f);
    g.setColour (juce::Colour (0xfffaf0dc));
    g.drawHorizontalLine (static_cast<int> (cap.getCentreY()), cap.getX() + 4.0f, cap.getRight() - 4.0f);

    auto meterArea = bounds.reduced (10.0f, 14.0f);
    juce::String rideText = (displayedRideDb >= 0.0f ? "+" : "") + juce::String (displayedRideDb, 1) + " dB";
    g.setColour (juce::Colour (0xfff5c469));
    g.setFont (juce::Font (juce::FontOptions (10.0f).withStyle ("Bold")));
    g.drawText (rideText, meterArea.removeFromTop (16.0f), juce::Justification::centred);

    const int numLeds = 24;
    const float normLvl = std::clamp ((displayedLevelDb + 48.0f) / 48.0f, 0.0f, 1.0f);
    const int litCount = static_cast<int> (std::round (normLvl * static_cast<float> (numLeds)));

    const float gap = 2.5f;
    const float ledH = std::max (2.0f, (meterArea.getHeight() - gap * (numLeds - 1)) / static_cast<float> (numLeds));
    const float ledW = std::min (26.0f, meterArea.getWidth() * 0.55f);
    const float ledX = meterArea.getCentreX() - ledW * 0.5f;

    for (int idx = 0; idx < numLeds; ++idx)
    {
        const int fromBottom = (numLeds - 1) - idx;
        const bool lit = fromBottom < litCount;
        const float y = meterArea.getY() + static_cast<float> (idx) * (ledH + gap);

        juce::Colour c = juce::Colour (0xff2b2218);
        if (lit)
        {
            if (idx < 4) c = juce::Colour (0xffe05a36);
            else if (idx < 10) c = juce::Colour (0xfff2b84b);
            else c = juce::Colour (0xffd49438);
        }

        g.setColour (c);
        g.fillRoundedRectangle (ledX, y, ledW, ledH, 1.5f);
    }
}

BruceSwedienRiderEditor::BruceSwedienRiderEditor (BruceSwedienRiderProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), faderStrip (p)
{
    setSize (460, 560);
    addAndMakeVisible (faderStrip);

    styleCombo (sourceBox, sourceLbl, "SOURCE");
    sourceBox.addItemList ({
        "Vocals", "Vocal Bus", "Bass", "Guitar", "Acoustic / Strings",
        "Drum Bus", "Drum Machine", "Keys", "Synth", "Master Bus"
    }, 1);

    styleCombo (workflowBox, workflowLbl, "MODE");
    workflowBox.addItemList ({ "Tracking", "Mixing" }, 1);

    styleCombo (speedBox, speedLbl, "SPEED");
    speedBox.addItemList ({ "Super Fast", "Fast", "Natural", "Slow", "Super Slow" }, 1);

    styleCombo (preampBox, preampLbl, "PREAMP");
    preampBox.addItemList ({ "Clean", "Harrison 32C", "Neve", "API" }, 1);

    styleKnob (amountSlider, amountLbl, "AMOUNT", juce::Colour (0xffd96b43));
    styleKnob (floorSlider,  floorLbl,  "FLOOR",  juce::Colour (0xff5d9ec9));
    styleKnob (gainSlider,   gainLbl,   "GAIN",   juce::Colour (0xffe5b05c));

    styleToggle (pocketBtn, juce::Colour (0xff78c272));
    styleToggle (rideBtn,   juce::Colour (0xfff2b84b));

    auto& v = audioProcessor.apvts;
    sourceAtt   = std::make_unique<ComboAtt>  (v, "source", sourceBox);
    workflowAtt = std::make_unique<ComboAtt>  (v, "workflow", workflowBox);
    speedAtt    = std::make_unique<ComboAtt>  (v, "speed", speedBox);
    preampAtt   = std::make_unique<ComboAtt>  (v, "preamp", preampBox);
    amountAtt   = std::make_unique<SliderAtt> (v, "amount", amountSlider);
    floorAtt    = std::make_unique<SliderAtt> (v, "floor", floorSlider);
    gainAtt     = std::make_unique<SliderAtt> (v, "gain", gainSlider);
    pocketAtt   = std::make_unique<ButtonAtt> (v, "pocket", pocketBtn);
    rideAtt     = std::make_unique<ButtonAtt> (v, "ride", rideBtn);
}

void BruceSwedienRiderEditor::styleKnob (juce::Slider& s, juce::Label& lbl, const juce::String& text, juce::Colour accent)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 16);
    s.setColour (juce::Slider::rotarySliderFillColourId, accent);
    s.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff382f26));
    s.setColour (juce::Slider::thumbColourId, juce::Colour (0xfffaf0dc));
    s.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xfff2e6d0));
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (s);

    lbl.setText (text, juce::dontSendNotification);
    lbl.setJustificationType (juce::Justification::centred);
    lbl.setColour (juce::Label::textColourId, juce::Colour (0xffe5b05c));
    lbl.setFont (juce::Font (juce::FontOptions (9.5f).withStyle ("Bold")));
    addAndMakeVisible (lbl);
}

void BruceSwedienRiderEditor::styleCombo (juce::ComboBox& c, juce::Label& lbl, const juce::String& text)
{
    c.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff191512));
    c.setColour (juce::ComboBox::textColourId, juce::Colour (0xfff2e6d0));
    c.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff6e583d));
    addAndMakeVisible (c);

    lbl.setText (text, juce::dontSendNotification);
    lbl.setJustificationType (juce::Justification::centredLeft);
    lbl.setColour (juce::Label::textColourId, juce::Colour (0xffe5b05c));
    lbl.setFont (juce::Font (juce::FontOptions (9.5f).withStyle ("Bold")));
    addAndMakeVisible (lbl);
}

void BruceSwedienRiderEditor::styleToggle (juce::ToggleButton& b, juce::Colour ledCol)
{
    b.setColour (juce::ToggleButton::textColourId, juce::Colour (0xfff2e6d0));
    b.setColour (juce::ToggleButton::tickColourId, ledCol);
    addAndMakeVisible (b);
}

void BruceSwedienRiderEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    juce::ColourGradient bg (juce::Colour (0xff241e19), 0.0f, 0.0f,
                             juce::Colour (0xff14100d), 0.0f, static_cast<float> (getHeight()), false);
    g.setGradientFill (bg);
    g.fillRect (bounds);

    auto drawWood = [&] (juce::Rectangle<int> r)
    {
        g.setColour (juce::Colour (0xff4a2b16));
        g.fillRect (r);
        g.setColour (juce::Colour (0xff241308));
        g.drawRect (r, 1);
    };
    drawWood (bounds.removeFromLeft (14));
    drawWood (bounds.removeFromRight (14));

    auto header = bounds.removeFromTop (64);
    g.setColour (juce::Colour (0xff120e0b));
    g.fillRect (header);
    g.setColour (juce::Colour (0xffe5a94c).withAlpha (0.55f));
    g.drawHorizontalLine (header.getBottom() - 1, static_cast<float> (header.getX()), static_cast<float> (header.getRight()));

    g.setColour (juce::Colour (0xfff6ead4));
    g.setFont (juce::Font (juce::FontOptions (19.0f).withStyle ("Bold")));
    g.drawText ("BRUCE SWEDIEN'S HANDS", header.reduced (14, 6).removeFromTop (26), juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xffe5a94c));
    g.setFont (juce::Font (juce::FontOptions (9.5f).withStyle ("Bold")));
    g.drawText ("ANALOG CONSOLE RIDER  |  0-SAMPLE LATENCY", header.reduced (14, 0).withTrimmedTop (34), juce::Justification::centredLeft);
}

void BruceSwedienRiderEditor::resized()
{
    auto area = getLocalBounds().reduced (22, 74);

    auto topRow = area.removeFromTop (44);
    auto modeCol = topRow.removeFromRight (126);
    sourceLbl.setBounds (topRow.removeFromTop (14));
    sourceBox.setBounds (topRow.withTrimmedRight (10));

    workflowLbl.setBounds (modeCol.removeFromTop (14));
    workflowBox.setBounds (modeCol);

    area.removeFromTop (10);

    auto bottomBar = area.removeFromBottom (34);
    rideBtn.setBounds   (bottomBar.removeFromLeft (110));
    pocketBtn.setBounds (bottomBar.removeFromLeft (110));

    auto rightCol = area.removeFromRight (174);
    auto leftCol  = area.withTrimmedRight (12);

    faderStrip.setBounds (leftCol);

    speedLbl.setBounds (rightCol.removeFromTop (14));
    speedBox.setBounds (rightCol.removeFromTop (24));
    rightCol.removeFromTop (6);

    amountLbl.setBounds    (rightCol.removeFromTop (14));
    amountSlider.setBounds (rightCol.removeFromTop (72));

    floorLbl.setBounds  (rightCol.removeFromTop (14));
    floorSlider.setBounds (rightCol.removeFromTop (72));

    gainLbl.setBounds  (rightCol.removeFromTop (14));
    gainSlider.setBounds (rightCol.removeFromTop (72));

    preampLbl.setBounds (rightCol.removeFromTop (14));
    preampBox.setBounds (rightCol.removeFromTop (24));
}
