#include "PluginEditor.h"

void ConsoleLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPosProportional, float rotaryStartAngle,
                                           float rotaryEndAngle, juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                          static_cast<float> (width), static_cast<float> (height));
    const float diameter = std::min (bounds.getWidth(), bounds.getHeight()) - 8.0f;
    if (diameter <= 10.0f)
        return;

    const float radius = diameter * 0.5f;
    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY() - 2.0f;
    const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    const int numTicks = 11;
    g.setColour (juce::Colour (0xff9b8a70).withAlpha (0.65f));
    for (int i = 0; i < numTicks; ++i)
    {
        const float t = static_cast<float> (i) / static_cast<float> (numTicks - 1);
        const float tickAngle = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle) - juce::MathConstants<float>::halfPi;
        const float rOuter = radius + 4.5f;
        const float rInner = radius + 1.5f;
        g.drawLine (cx + std::cos (tickAngle) * rInner,
                    cy + std::sin (tickAngle) * rInner,
                    cx + std::cos (tickAngle) * rOuter,
                    cy + std::sin (tickAngle) * rOuter, 1.1f);
    }

    g.setColour (juce::Colours::black.withAlpha (0.65f));
    g.fillEllipse (cx - radius, cy - radius + 2.0f, diameter, diameter);

    juce::ColourGradient bodyGrad (juce::Colour (0xff3c352f), cx - radius * 0.4f, cy - radius * 0.4f,
                                   juce::Colour (0xff171310), cx + radius * 0.5f, cy + radius * 0.5f, false);
    g.setGradientFill (bodyGrad);
    g.fillEllipse (cx - radius, cy - radius, diameter, diameter);

    const float capRadius = radius * 0.72f;
    const juce::Colour capCol = slider.findColour (juce::Slider::rotarySliderFillColourId);
    g.setColour (capCol);
    g.fillEllipse (cx - capRadius, cy - capRadius, capRadius * 2.0f, capRadius * 2.0f);

    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawEllipse (cx - capRadius, cy - capRadius, capRadius * 2.0f, capRadius * 2.0f, 1.0f);

    const float ptrAngle = angle - juce::MathConstants<float>::halfPi;
    g.setColour (juce::Colour (0xfffbf4e4));
    g.drawLine (cx + std::cos (ptrAngle) * (capRadius * 0.25f),
                cy + std::sin (ptrAngle) * (capRadius * 0.25f),
                cx + std::cos (ptrAngle) * (capRadius * 0.92f),
                cy + std::sin (ptrAngle) * (capRadius * 0.92f), 2.2f);
}

void ConsoleLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                     int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (width), static_cast<float> (height));

    juce::ColourGradient winGrad (juce::Colour (0xff1c1610), 0.0f, 0.0f,
                                  juce::Colour (0xff0d0a07), 0.0f, r.getHeight(), false);
    g.setGradientFill (winGrad);
    g.fillRoundedRectangle (r, 4.0f);

    g.setColour (juce::Colour (0xff6e573b));
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.1f);

    const float arrowX = r.getRight() - 13.0f;
    const float arrowY = r.getCentreY();
    juce::Path tri;
    tri.addTriangle (arrowX - 3.5f, arrowY - 2.5f,
                     arrowX + 3.5f, arrowY - 2.5f,
                     arrowX,        arrowY + 3.0f);
    g.setColour (juce::Colour (0xffe8b75c));
    g.fillPath (tri);

    g.setColour (juce::Colour (0xfff6c76b));
    g.setFont (juce::Font (juce::FontOptions (11.5f).withStyle ("Bold")));
    g.drawText (box.getText(), r.reduced (9.0f, 2.0f).withTrimmedRight (15.0f),
                juce::Justification::centredLeft, true);
}

void ConsoleLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                       bool, bool)
{
    auto r = button.getLocalBounds().toFloat();
    const bool active = button.getToggleState();
    const juce::Colour ledCol = button.findColour (juce::ToggleButton::tickColourId);

    g.setColour (juce::Colour (0xff171310));
    g.fillRoundedRectangle (r, 4.0f);

    g.setColour (active ? ledCol.withAlpha (0.85f) : juce::Colour (0xff4d3e2d));
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.1f);

    const float ledSize = 9.0f;
    const float ledX = r.getX() + 9.0f;
    const float ledY = r.getCentreY() - ledSize * 0.5f;

    if (active)
    {
        g.setColour (ledCol.withAlpha (0.35f));
        g.fillEllipse (ledX - 3.0f, ledY - 3.0f, ledSize + 6.0f, ledSize + 6.0f);
    }

    g.setColour (active ? ledCol : juce::Colour (0xff2e261f));
    g.fillEllipse (ledX, ledY, ledSize, ledSize);

    g.setColour (active ? juce::Colour (0xfffaf1de) : juce::Colour (0xffb5a388));
    g.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
    g.drawText (button.getButtonText(),
                r.withTrimmedLeft (ledX + ledSize + 6.0f).withTrimmedRight (6.0f),
                juce::Justification::centredLeft, true);
}

// ============================================================================
// HarrisonFaderStrip
// ============================================================================
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

    g.setColour (juce::Colour (0xff14110f));
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (juce::Colour (0xff524230));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.1f);

    auto faderArea = bounds.removeFromLeft (bounds.getWidth() * 0.54f).reduced (8.0f, 12.0f);

    g.setColour (juce::Colour (0xffe8c27a));
    g.setFont (juce::Font (juce::FontOptions (10.0f).withStyle ("Bold")));
    g.drawText ("dB", faderArea.removeFromTop (16.0f).removeFromLeft (28.0f), juce::Justification::centredRight);

    auto scaleArea = faderArea.removeFromLeft (28.0f);
    g.setColour (juce::Colour (0xffd5c4a7));
    g.setFont (juce::Font (juce::FontOptions (9.0f).withStyle ("Bold")));

    static constexpr std::array<const char*, 5> dbMarks { "+12", "+6", "0", "-6", "-12" };
    for (int i = 0; i < 5; ++i)
    {
        const float yPos = scaleArea.getY() + (static_cast<float> (i) / 4.0f) * scaleArea.getHeight();
        g.drawText (dbMarks[static_cast<size_t> (i)], static_cast<int> (scaleArea.getX()),
                    static_cast<int> (yPos - 6.0f), 26, 12, juce::Justification::centredRight);
    }

    auto trackArea = faderArea.reduced (14.0f, 6.0f);
    g.setColour (juce::Colour (0xff080605));
    g.fillRoundedRectangle (trackArea.getCentreX() - 4.0f, trackArea.getY(), 8.0f, trackArea.getHeight(), 3.0f);

    const float zeroY = trackArea.getCentreY();
    g.setColour (juce::Colour (0xffe8c27a).withAlpha (0.45f));
    g.drawHorizontalLine (static_cast<int> (zeroY), trackArea.getX() - 4.0f, trackArea.getRight() + 4.0f);

    const float clampedRide = std::clamp (displayedRideDb, -12.0f, 12.0f);
    const float normPos = 0.5f - (clampedRide / 24.0f);
    const float capY = trackArea.getY() + normPos * trackArea.getHeight();

    const float barTop = std::min (zeroY, capY);
    const float barH   = std::abs (capY - zeroY);
    g.setColour ((clampedRide >= 0.0f ? juce::Colour (0xfff2b84b) : juce::Colour (0xffd97848)).withAlpha (0.40f));
    g.fillRect (trackArea.getCentreX() - 3.0f, barTop, 6.0f, barH);

    juce::Rectangle<float> cap (trackArea.getCentreX() - 21.0f, capY - 12.0f, 42.0f, 24.0f);
    g.setColour (juce::Colours::black.withAlpha (0.70f));
    g.fillRoundedRectangle (cap.translated (0.0f, 2.0f), 3.0f);

    juce::ColourGradient capGrad (juce::Colour (0xff473b31), cap.getX(), cap.getY(),
                                  juce::Colour (0xff231c17), cap.getX(), cap.getBottom(), false);
    g.setGradientFill (capGrad);
    g.fillRoundedRectangle (cap, 3.0f);

    g.setColour (juce::Colour (0xff9e7d52));
    g.drawRoundedRectangle (cap, 3.0f, 1.1f);
    g.setColour (juce::Colour (0xfffaf0dc));
    g.drawHorizontalLine (static_cast<int> (cap.getCentreY()), cap.getX() + 4.0f, cap.getRight() - 4.0f);

    auto meterArea = bounds.reduced (8.0f, 12.0f);
    juce::String rideText = (displayedRideDb >= 0.0f ? "+" : "") + juce::String (displayedRideDb, 1) + " dB";
    g.setColour (juce::Colour (0xfff5c469));
    g.setFont (juce::Font (juce::FontOptions (10.0f).withStyle ("Bold")));
    g.drawText (rideText, meterArea.removeFromTop (16.0f), juce::Justification::centred);

    const int numLeds = 24;
    const float normLvl = std::clamp ((displayedLevelDb + 48.0f) / 48.0f, 0.0f, 1.0f);
    const int litCount = static_cast<int> (std::round (normLvl * static_cast<float> (numLeds)));

    const float gap = 2.5f;
    const float ledH = std::max (2.0f, (meterArea.getHeight() - gap * (numLeds - 1)) / static_cast<float> (numLeds));
    const float colW = std::min (12.0f, (meterArea.getWidth() - 10.0f) * 0.45f);
    const float colGap = 4.0f;
    const float leftColX = meterArea.getCentreX() - colW - colGap * 0.5f;
    const float rightColX = meterArea.getCentreX() + colGap * 0.5f;

    for (int idx = 0; idx < numLeds; ++idx)
    {
        const int fromBottom = (numLeds - 1) - idx;
        const bool lit = fromBottom < litCount;
        const float y = meterArea.getY() + static_cast<float> (idx) * (ledH + gap);

        juce::Colour c = juce::Colour (0xff261e15);
        if (lit)
        {
            if (idx < 4) c = juce::Colour (0xffe05a36);
            else if (idx < 10) c = juce::Colour (0xfff7c354);
            else c = juce::Colour (0xffe09c38);
        }

        g.setColour (c);
        g.fillRoundedRectangle (leftColX,  y, colW, ledH, 1.2f);
        g.fillRoundedRectangle (rightColX, y, colW, ledH, 1.2f);
    }
}

// ============================================================================
// BruceSwedienRiderEditor
// ============================================================================
BruceSwedienRiderEditor::BruceSwedienRiderEditor (BruceSwedienRiderProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), faderStrip (p)
{
    setLookAndFeel (&consoleLnf);
    setSize (480, 600);
    addAndMakeVisible (faderStrip);

    prevSourceBtn.onClick = [this] { stepSource (-1); };
    nextSourceBtn.onClick = [this] { stepSource (1); };
    addAndMakeVisible (prevSourceBtn);
    addAndMakeVisible (nextSourceBtn);

    configureCombo (sourceBox, sourceLbl, "SOURCE");
    sourceBox.addItemList ({
        "Vocals", "Vocal Bus", "Bass", "Guitar", "Acoustic / Strings",
        "Drum Bus", "Drum Machine", "Keys", "Synth", "Master Bus"
    }, 1);

    configureCombo (workflowBox, workflowLbl, "MODE");
    workflowBox.addItemList ({ "Tracking", "Mixing" }, 1);

    configureCombo (channelBox, channelLbl, "CHANNEL");
    channelBox.addItemList ({ "Auto", "Mono", "Stereo" }, 1);

    configureCombo (speedBox, speedLbl, "SPEED");
    speedBox.addItemList ({ "Super Fast", "Fast", "Natural", "Slow", "Super Slow" }, 1);

    configureCombo (preampBox, preampLbl, "PREAMP");
    preampBox.addItemList ({ "Clean", "Harrison 32C", "Neve", "API" }, 1);

    configureKnob (amountSlider, amountLbl, "AMOUNT", juce::Colour (0xffc94c38));
    configureKnob (floorSlider,  floorLbl,  "FLOOR",  juce::Colour (0xff4782b8));
    configureKnob (gainSlider,   gainLbl,   "GAIN",   juce::Colour (0xffe0a846));

    rideBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xfff2b84b));
    pocketBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff67b868));
    addAndMakeVisible (rideBtn);
    addAndMakeVisible (pocketBtn);

    auto& v = audioProcessor.apvts;
    sourceAtt   = std::make_unique<ComboAtt>  (v, "source", sourceBox);
    workflowAtt = std::make_unique<ComboAtt>  (v, "workflow", workflowBox);
    channelAtt  = std::make_unique<ComboAtt>  (v, "channelMode", channelBox);
    speedAtt    = std::make_unique<ComboAtt>  (v, "speed", speedBox);
    preampAtt   = std::make_unique<ComboAtt>  (v, "preamp", preampBox);
    amountAtt   = std::make_unique<SliderAtt> (v, "amount", amountSlider);
    floorAtt    = std::make_unique<SliderAtt> (v, "floor", floorSlider);
    gainAtt     = std::make_unique<SliderAtt> (v, "gain", gainSlider);
    pocketAtt   = std::make_unique<ButtonAtt> (v, "pocket", pocketBtn);
    rideAtt     = std::make_unique<ButtonAtt> (v, "ride", rideBtn);
}

BruceSwedienRiderEditor::~BruceSwedienRiderEditor()
{
    setLookAndFeel (nullptr);
}

void BruceSwedienRiderEditor::stepSource (int delta)
{
    const int numItems = sourceBox.getNumItems();
    if (numItems <= 0)
        return;

    const int curIdx = std::max (0, sourceBox.getSelectedItemIndex());
    const int nextIdx = (curIdx + delta + numItems) % numItems;
    sourceBox.setSelectedItemIndex (nextIdx, juce::sendNotificationSync);
}

void BruceSwedienRiderEditor::configureKnob (juce::Slider& s, juce::Label& lbl, const juce::String& title, juce::Colour capCol)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 68, 16);
    s.setColour (juce::Slider::rotarySliderFillColourId, capCol);
    s.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xfff5ead6));
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (s);

    lbl.setText (title, juce::dontSendNotification);
    lbl.setJustificationType (juce::Justification::centred);
    lbl.setColour (juce::Label::textColourId, juce::Colour (0xffe8c27a));
    lbl.setFont (juce::Font (juce::FontOptions (9.5f).withStyle ("Bold")));
    addAndMakeVisible (lbl);
}

void BruceSwedienRiderEditor::configureCombo (juce::ComboBox& c, juce::Label& lbl, const juce::String& title)
{
    addAndMakeVisible (c);

    lbl.setText (title, juce::dontSendNotification);
    lbl.setJustificationType (juce::Justification::centredLeft);
    lbl.setColour (juce::Label::textColourId, juce::Colour (0xffe8c27a));
    lbl.setFont (juce::Font (juce::FontOptions (9.5f).withStyle ("Bold")));
    addAndMakeVisible (lbl);
}

void BruceSwedienRiderEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    juce::ColourGradient bg (juce::Colour (0xff26201b), 0.0f, 0.0f,
                             juce::Colour (0xff14100d), 0.0f, static_cast<float> (getHeight()), false);
    g.setGradientFill (bg);
    g.fillRect (bounds);

    auto drawWood = [&] (juce::Rectangle<int> r)
    {
        juce::ColourGradient wood (juce::Colour (0xff59341b), static_cast<float> (r.getX()), 0.0f,
                                   juce::Colour (0xff291609), static_cast<float> (r.getRight()), 0.0f, false);
        g.setGradientFill (wood);
        g.fillRect (r);
        g.setColour (juce::Colour (0xff180d05));
        g.drawRect (r, 1);
    };
    drawWood (bounds.removeFromLeft (14));
    drawWood (bounds.removeFromRight (14));

    auto header = bounds.removeFromTop (66);
    g.setColour (juce::Colour (0xff120e0b));
    g.fillRect (header);
    g.setColour (juce::Colour (0xffe5a94c).withAlpha (0.55f));
    g.drawHorizontalLine (header.getBottom() - 1, static_cast<float> (header.getX()), static_cast<float> (header.getRight()));

    g.setColour (juce::Colour (0xfff6ead4));
    g.setFont (juce::Font (juce::FontOptions (20.0f).withStyle ("Bold")));
    g.drawText ("BRUCE SWEDIEN'S HANDS", header.reduced (14, 6).removeFromTop (28), juce::Justification::centred);

    g.setColour (juce::Colour (0xffe5a94c));
    g.setFont (juce::Font (juce::FontOptions (9.5f).withStyle ("Bold")));
    g.drawText ("ANALOG CONSOLE RIDER  |  0-SAMPLE LATENCY", header.reduced (14, 0).withTrimmedTop (36), juce::Justification::centred);

    g.setColour (juce::Colour (0xffc9b79c));
    g.setFont (juce::Font (juce::FontOptions (8.0f).withStyle ("Bold")));
    g.drawText ("VOCALS  ·  BASS  ·  GUITAR  ·  DRUM BUS  ·  KEYS  ·  SYNTH  ·  MASTER BUS",
                bounds.getX() + 10, bounds.getY() + 6, bounds.getWidth() - 20, 12, juce::Justification::centred);
}

void BruceSwedienRiderEditor::resized()
{
    auto area = getLocalBounds().reduced (22, 74);

    // Fila superior: SOURCE (< >) a la izquierda, MODE y CHANNEL a la derecha
    auto topRow = area.removeFromTop (52);
    topRow.removeFromTop (12);

    auto chanCol = topRow.removeFromRight (94);
    channelLbl.setBounds (chanCol.removeFromTop (14));
    channelBox.setBounds (chanCol.removeFromTop (24));

    topRow.removeFromRight (6);

    auto modeCol = topRow.removeFromRight (104);
    workflowLbl.setBounds (modeCol.removeFromTop (14));
    workflowBox.setBounds (modeCol.removeFromTop (24));

    auto srcCol = topRow.withTrimmedRight (10);
    sourceLbl.setBounds (srcCol.removeFromTop (14));

    auto srcControls = srcCol.removeFromTop (24);
    prevSourceBtn.setBounds (srcControls.removeFromLeft (22));
    srcControls.removeFromLeft (4);
    nextSourceBtn.setBounds (srcControls.removeFromRight (22));
    srcControls.removeFromRight (4);
    sourceBox.setBounds (srcControls);

    area.removeFromTop (10);

    auto bottomBar = area.removeFromBottom (34);
    rideBtn.setBounds   (bottomBar.removeFromLeft (112));
    bottomBar.removeFromLeft (8);
    pocketBtn.setBounds (bottomBar.removeFromLeft (112));

    area.removeFromBottom (6);

    auto rightCol = area.removeFromRight (172);
    auto leftCol  = area.withTrimmedRight (12);

    faderStrip.setBounds (leftCol);

    speedLbl.setBounds (rightCol.removeFromTop (14));
    speedBox.setBounds (rightCol.removeFromTop (24));
    rightCol.removeFromTop (6);

    amountLbl.setBounds    (rightCol.removeFromTop (14));
    amountSlider.setBounds (rightCol.removeFromTop (80));

    floorLbl.setBounds  (rightCol.removeFromTop (14));
    floorSlider.setBounds (rightCol.removeFromTop (80));

    gainLbl.setBounds  (rightCol.removeFromTop (14));
    gainSlider.setBounds (rightCol.removeFromTop (80));

    preampLbl.setBounds (rightCol.removeFromTop (14));
    preampBox.setBounds (rightCol.removeFromTop (24));
}
