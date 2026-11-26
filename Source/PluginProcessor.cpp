#include "PluginProcessor.h"
#include "PluginEditor.h"

BruceSwedienRiderProcessor::BruceSwedienRiderProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    setLatencySamples (0);
}

juce::AudioProcessorValueTreeState::ParameterLayout BruceSwedienRiderProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "source", "Source",
        juce::StringArray {
            "Vocals", "Vocal Bus", "Bass", "Guitar", "Acoustic / Strings",
            "Drum Bus", "Drum Machine", "Keys", "Synth", "Master Bus"
        }, 0));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "speed", "Speed",
        juce::StringArray { "Super Fast", "Fast", "Natural", "Slow", "Super Slow" }, 2));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "workflow", "Mode",
        juce::StringArray { "Tracking", "Mixing" }, 1));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "channelMode", "Channel Mode",
        juce::StringArray { "Auto", "Mono", "Stereo" }, 0));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "amount", "Amount", juce::NormalisableRange<float> (0.0f, 12.0f, 0.1f), 6.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "floor", "Floor", juce::NormalisableRange<float> (-60.0f, -12.0f, 0.5f), -36.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "gain", "Gain", juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "preamp", "Preamp",
        juce::StringArray { "Clean", "Harrison 32C", "Neve", "API" }, 1));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "pocket", "Pocket", true));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "ride", "Ride", true));

    return { params.begin(), params.end() };
}

BruceSwedienRiderProcessor::SourceProfile BruceSwedienRiderProcessor::getSourceProfile (int sourceIdx)
{
    // bodyLow, bodyHigh, sibCut, wBody, wSub, wSib, sibRatioThr, sibGuard, transTame, holdMs, deadzoneDb, floorBias, stiffScale, dampScale, driveScale
    switch (sourceIdx)
    {
        case 0: return { 210.0f, 2800.0f, 4600.0f, 1.15f, 0.20f, 0.35f, 0.44f, 0.90f, 0.45f, 175.0f, 1.1f,  1.5f, 1.05f, 1.05f, 1.00f }; // Vocals
        case 1: return { 200.0f, 3000.0f, 4800.0f, 1.10f, 0.25f, 0.40f, 0.48f, 0.80f, 0.55f, 250.0f, 1.3f,  2.0f, 0.92f, 1.12f, 1.05f }; // Vocal Bus
        case 2: return {  55.0f,  850.0f, 2400.0f, 1.25f, 0.85f, 0.25f, 0.52f, 0.65f, 0.50f, 150.0f, 1.0f, -1.0f, 1.15f, 1.08f, 1.15f }; // Bass
        case 3: return { 170.0f, 3200.0f, 5000.0f, 1.15f, 0.35f, 0.45f, 0.50f, 0.70f, 0.55f, 190.0f, 1.2f,  0.0f, 1.00f, 1.00f, 1.10f }; // Guitar
        case 4: return { 160.0f, 3500.0f, 5500.0f, 1.10f, 0.25f, 0.50f, 0.55f, 0.60f, 0.65f, 240.0f, 1.3f,  2.5f, 0.85f, 1.15f, 0.95f }; // Acoustic / Strings
        case 5: return {  90.0f, 2200.0f, 4600.0f, 1.05f, 0.75f, 0.55f, 0.58f, 0.50f, 0.82f, 270.0f, 1.4f, -1.5f, 0.90f, 1.10f, 1.20f }; // Drum Bus
        case 6: return { 110.0f, 2600.0f, 5000.0f, 1.10f, 0.80f, 0.50f, 0.55f, 0.55f, 0.72f, 155.0f, 1.1f, -1.0f, 1.12f, 1.05f, 1.12f }; // Drum Machine
        case 7: return { 140.0f, 3100.0f, 5200.0f, 1.10f, 0.45f, 0.50f, 0.54f, 0.60f, 0.50f, 210.0f, 1.2f,  1.0f, 0.95f, 1.05f, 1.00f }; // Keys
        case 8: return { 130.0f, 3400.0f, 5500.0f, 1.05f, 0.55f, 0.55f, 0.56f, 0.55f, 0.60f, 220.0f, 1.2f,  0.5f, 0.92f, 1.08f, 1.08f }; // Synth
        case 9:
        default:return { 120.0f, 2800.0f, 6000.0f, 1.00f, 0.60f, 0.45f, 0.55f, 0.65f, 0.75f, 340.0f, 1.6f,  1.0f, 0.75f, 1.25f, 0.70f }; // Master Bus
    }
}

// ============================================================================
// CAPA 1: SpectralAnalyzer (fader consistente en mono y estéreo)
// ============================================================================
void BruceSwedienRiderProcessor::SpectralAnalyzer::prepare (double sampleRate)
{
    sr = sampleRate > 0.0 ? sampleRate : 44100.0;
    subLp.prepare (sr);
    bodyHp.prepare (sr);
    bodyLp.prepare (sr);
    sibHp.prepare (sr);

    fastEnvDb = -80.0f;
    slowEnvDb = -80.0f;
    noiseFloorEstDb = -52.0f;
    prevInstDb = -80.0f;

    const float srF = static_cast<float> (sr);
    fastCoeff       = 1.0f - std::exp (-1.0f / (0.012f * srF));
    slowCoeff       = 1.0f - std::exp (-1.0f / (0.140f * srF));
    floorAdaptCoeff = 1.0f - std::exp (-1.0f / (0.350f * srF));
}

void BruceSwedienRiderProcessor::SpectralAnalyzer::configure (const SourceProfile& profile)
{
    subLp.setLowPass (120.0f, 0.7071f);
    bodyHp.setHighPass (profile.bodyLowHz, 0.7071f);
    bodyLp.setLowPass (profile.bodyHighHz, 0.7071f);
    sibHp.setHighPass (profile.sibCutoffHz, 0.7071f);
}

BruceSwedienRiderProcessor::SpectralAnalyzer::AnalysisResult
BruceSwedienRiderProcessor::SpectralAnalyzer::processSample (float inL, float inR, float userFloorDb, const SourceProfile& profile) noexcept
{
    constexpr float eps = 1.0e-12f;

    // Señal combinada unificada para que el detector del fader responda igual en mono y estéreo
    const float detSignal = 0.5f * (inL + inR);

    const float sub  = subLp.processSample (0, detSignal);
    const float body = bodyLp.processSample (0, bodyHp.processSample (0, detSignal));
    const float sib  = sibHp.processSample (0, detSignal);

    const float eSub  = sub * sub;
    const float eBody = body * body;
    const float eSib  = sib * sib;

    // Relación normalizada sibilancia vs (cuerpo + sibilancia)
    const float sibRatio = eSib / (eBody + eSib + eps);
    const float excessSib = std::clamp ((sibRatio - profile.sibThresholdRatio) / std::max (0.05f, 1.0f - profile.sibThresholdRatio), 0.0f, 1.0f);
    const float sibWeight = profile.wSib * (1.0f - profile.sibGuardStrength * excessSib);

    // Energía ponderada percibida
    const float perceivedEnergy = profile.wBody * eBody + profile.wSub * eSub + sibWeight * eSib;
    const float instDb = std::clamp (10.0f * std::log10 (perceivedEnergy + eps), -80.0f, 6.0f);

    fastEnvDb += fastCoeff * (instDb - fastEnvDb);
    slowEnvDb += slowCoeff * (instDb - slowEnvDb);

    // Separación de transientes vs energía sostenida
    const float transientExcessDb = std::max (0.0f, fastEnvDb - slowEnvDb);
    const float effectiveDb = fastEnvDb - transientExcessDb * profile.transientTame * 0.65f;

    const float deltaDb = std::abs (instDb - prevInstDb);
    prevInstDb = instDb;

    if (instDb < userFloorDb + 6.0f && deltaDb < 1.2f)
        noiseFloorEstDb += floorAdaptCoeff * (instDb - noiseFloorEstDb);

    return { effectiveDb, noiseFloorEstDb, deltaDb };
}

// ============================================================================
// CAPA 2: HandMotionEngine
// ============================================================================
void BruceSwedienRiderProcessor::HandMotionEngine::prepare (double sampleRate)
{
    sr = sampleRate > 0.0 ? sampleRate : 44100.0;
    currentGainDb = 0.0f;
    velDbPerSec = 0.0f;
    holdRemaining = 0;
    phaseA = 0.0f;
    phaseB = 1.73f;
    phaseC = 3.41f;
}

float BruceSwedienRiderProcessor::HandMotionEngine::processSample (float effectiveLevelDb,
                                                                 float noiseFloorEstDb,
                                                                 float deltaDb,
                                                                 int speedIdx,
                                                                 float amountDb,
                                                                 float floorDb,
                                                                 float trackingMix,
                                                                 bool pocketOn,
                                                                 bool rideOn,
                                                                 const SourceProfile& profile) noexcept
{
    const float srF = static_cast<float> (sr);
    const float dt = 1.0f / srF;

    const float trackingRangeScale = juce::jmap (trackingMix, 0.0f, 1.0f, 1.0f, 0.72f);
    const float maxRangeDb = std::max (0.0f, amountDb * trackingRangeScale);
    const float effectiveFloorDb = floorDb + profile.floorBiasDb * (0.6f + 0.4f * trackingMix);
    const int holdSamples = static_cast<int> (0.001f * profile.holdMs * srF);

    constexpr float targetBalanceDb = -18.0f;
    float targetRideDb = 0.0f;

    if (! rideOn || maxRangeDb <= 0.01f)
    {
        targetRideDb = 0.0f;
        holdRemaining = 0;
    }
    else if (effectiveLevelDb < effectiveFloorDb || (effectiveLevelDb < noiseFloorEstDb + 3.5f && deltaDb < 0.35f))
    {
        if (holdRemaining > 0)
        {
            --holdRemaining;
            targetRideDb = currentGainDb;
        }
        else
        {
            targetRideDb = 0.0f;
        }
    }
    else
    {
        holdRemaining = holdSamples;

        const float rawDelta = targetBalanceDb - effectiveLevelDb;
        const float absDelta = std::abs (rawDelta);
        const float signDelta = (rawDelta >= 0.0f) ? 1.0f : -1.0f;
        const float deadzone = profile.deadzoneDb * (0.85f + 0.30f * trackingMix);
        const float effectiveDelta = signDelta * std::max (0.0f, absDelta - deadzone);

        const float kNorm = std::max (2.0f, maxRangeDb * 1.25f);
        targetRideDb = maxRangeDb * std::tanh (effectiveDelta / kNorm);
    }

    static constexpr std::array<float, 5> speedWnHz { 7.5f, 4.4f, 2.4f, 1.25f, 0.65f };
    static constexpr std::array<float, 5> speedZeta { 0.92f, 0.96f, 1.02f, 1.08f, 1.15f };

    const size_t sIdx = static_cast<size_t> (std::clamp (speedIdx, 0, 4));
    const bool movingDown = (targetRideDb < currentGainDb);

    const float wn = juce::MathConstants<float>::twoPi * speedWnHz[sIdx]
                   * profile.stiffnessScale * (movingDown ? 1.18f : 0.85f);
    const float zeta = speedZeta[sIdx] * profile.dampingScale
                     * (1.0f + 0.12f * trackingMix) * (movingDown ? 1.05f : 0.95f);

    const float accel = wn * wn * (targetRideDb - currentGainDb) - 2.0f * zeta * wn * velDbPerSec;
    velDbPerSec += accel * dt;
    currentGainDb += velDbPerSec * dt;
    currentGainDb = std::clamp (currentGainDb, -18.0f, 18.0f);

    float pocketDb = 0.0f;
    if (pocketOn && rideOn && maxRangeDb > 0.05f)
    {
        phaseA += juce::MathConstants<float>::twoPi * 0.73f * dt;
        phaseB += juce::MathConstants<float>::twoPi * 1.19f * dt;
        phaseC += juce::MathConstants<float>::twoPi * 1.91f * dt;
        if (phaseA > juce::MathConstants<float>::twoPi) phaseA -= juce::MathConstants<float>::twoPi;
        if (phaseB > juce::MathConstants<float>::twoPi) phaseB -= juce::MathConstants<float>::twoPi;
        if (phaseC > juce::MathConstants<float>::twoPi) phaseC -= juce::MathConstants<float>::twoPi;

        const float wave = 0.55f * std::sin (phaseA)
                         + 0.30f * std::sin (phaseB)
                         + 0.15f * std::sin (phaseC);
        const float pocketScale = std::min (1.0f, maxRangeDb / 6.0f) * (0.75f + 0.25f * (1.0f - trackingMix));
        pocketDb = wave * 0.24f * pocketScale;
    }

    return currentGainDb + pocketDb;
}

// ============================================================================
// CAPA 3: ConsolePreampStage
// ============================================================================
void BruceSwedienRiderProcessor::ConsolePreampStage::prepare (double sampleRate)
{
    sr = sampleRate > 0.0 ? sampleRate : 44100.0;
    inShelf.prepare (sr);
    outShelf.prepare (sr);
    dcX.fill (0.0f);
    dcY.fill (0.0f);
}

void BruceSwedienRiderProcessor::ConsolePreampStage::configurePreamp (int preampIdx)
{
    if (preampIdx == 1) // Harrison 32C
    {
        inShelf.setLowShelf (145.0f, 1.3f, 0.7071f);
        outShelf.setHighShelf (7500.0f, 0.7f, 0.7071f);
    }
    else if (preampIdx == 2) // Neve
    {
        inShelf.setLowShelf (110.0f, 1.6f, 0.7071f);
        outShelf.setHighShelf (6500.0f, -1.2f, 0.7071f);
    }
    else if (preampIdx == 3) // API
    {
        inShelf.setLowShelf (180.0f, 0.6f, 0.7071f);
        outShelf.setHighShelf (3200.0f, 1.2f, 0.7071f);
    }
    else // Clean
    {
        inShelf.setLowShelf (120.0f, 0.0f, 0.7071f);
        outShelf.setHighShelf (8000.0f, 0.0f, 0.7071f);
    }
}

inline float BruceSwedienRiderProcessor::ConsolePreampStage::applyModel (float x,
                                                                      int modelIdx,
                                                                      float drive,
                                                                      float faderPushNorm,
                                                                      float trackingMix,
                                                                      int ch) noexcept
{
    if (modelIdx == 0)
        return x;

    const float dynScale = 0.85f + 0.35f * faderPushNorm;
    const float trackingScale = juce::jmap (trackingMix, 0.0f, 1.0f, 1.0f, 0.65f);
    const float effDrive = std::clamp (drive * dynScale * trackingScale, 0.0f, 1.0f);

    if (effDrive <= 0.0005f)
        return x;

    float y = x;
    switch (modelIdx)
    {
        case 1: // Harrison 32C: asimetría suave y calibrada para simetría L/R limpia
        {
            const float g = 1.0f + effDrive * 1.45f;
            const float u = x * g;
            const float asym = 0.035f * effDrive * (u * u);
            const float raw = std::tanh (u + asym);
            const float norm = 1.0f / std::max (0.55f, std::tanh (g));
            y = raw * norm;
            break;
        }
        case 2: // Neve
        {
            const float g = 1.0f + effDrive * 2.0f;
            y = std::tanh (x * g) / std::max (0.48f, std::tanh (g));
            break;
        }
        case 3: // API
        {
            const float g = 1.0f + effDrive * 1.8f;
            const float u = x * g;
            y = u / (1.0f + 0.32f * std::abs (u));
            break;
        }
        default:
            y = x;
            break;
    }

    const float dcOut = y - dcX[ch] + 0.9994f * dcY[ch];
    dcX[ch] = y;
    dcY[ch] = dcOut;
    return dcOut;
}

void BruceSwedienRiderProcessor::ConsolePreampStage::processStereoSample (float& ioL,
                                                                        float& ioR,
                                                                        int preampIdx,
                                                                        int prevPreampIdx,
                                                                        float preampCrossfade,
                                                                        float totalGainDb,
                                                                        float rideDb,
                                                                        float driveAmount,
                                                                        float trackingMix) noexcept
{
    const float gainLin = juce::Decibels::decibelsToGain (totalGainDb, -60.0f);
    const float faderPushNorm = std::clamp (rideDb / 12.0f, -0.5f, 1.0f);

    const float preL = inShelf.processSample (0, ioL * gainLin);
    const float preR = inShelf.processSample (1, ioR * gainLin);

    const float curL = applyModel (preL, preampIdx, driveAmount, faderPushNorm, trackingMix, 0);
    const float curR = applyModel (preR, preampIdx, driveAmount, faderPushNorm, trackingMix, 1);

    float mixL = curL;
    float mixR = curR;
    if (preampCrossfade < 0.999f && preampIdx != prevPreampIdx)
    {
        const float oldL = applyModel (preL, prevPreampIdx, driveAmount, faderPushNorm, trackingMix, 0);
        const float oldR = applyModel (preR, prevPreampIdx, driveAmount, faderPushNorm, trackingMix, 1);
        mixL = oldL + preampCrossfade * (curL - oldL);
        mixR = oldR + preampCrossfade * (curR - oldR);
    }

    ioL = outShelf.processSample (0, mixL);
    ioR = outShelf.processSample (1, mixR);
}

// ============================================================================
// PROCESSOR PRINCIPAL
// ============================================================================
void BruceSwedienRiderProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    setLatencySamples (0);

    spectralAnalyzer.prepare (currentSampleRate);
    handMotionEngine.prepare (currentSampleRate);
    preampStage.prepare (currentSampleRate);

    smAmountDb = apvts.getRawParameterValue ("amount")->load();
    smFloorDb  = apvts.getRawParameterValue ("floor")->load();
    smGainDb   = apvts.getRawParameterValue ("gain")->load();
    smTrackingMix = (apvts.getRawParameterValue ("workflow")->load() < 0.5f) ? 1.0f : 0.0f;

    const int initChanMode = static_cast<int> (apvts.getRawParameterValue ("channelMode")->load());
    smChannelModeMix = (initChanMode == 1) ? 1.0f : 0.0f;

    lastPreampIdx = static_cast<int> (apvts.getRawParameterValue ("preamp")->load());
    prevPreampIdx = lastPreampIdx;
    smPreampCrossfade = 1.0f;
    lastSourceIdx = -1;
}

void BruceSwedienRiderProcessor::releaseResources() {}

#ifndef JucePlugin_PreferredChannelConfigurations
bool BruceSwedienRiderProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        || layouts.getMainInputChannelSet() == juce::AudioChannelSet::mono();
}
#endif

void BruceSwedienRiderProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int origNumChannels = buffer.getNumChannels();
    const int numSamples      = buffer.getNumSamples();
    if (origNumChannels == 0 || numSamples == 0)
        return;

    const bool inputWasMono = (origNumChannels == 1);

    if (origNumChannels == 1)
    {
        buffer.setSize (2, numSamples, true, true, true);
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);
    }

    const int sourceIdx      = static_cast<int> (apvts.getRawParameterValue ("source")->load());
    const int speedIdx       = static_cast<int> (apvts.getRawParameterValue ("speed")->load());
    const int workflowIdx    = static_cast<int> (apvts.getRawParameterValue ("workflow")->load()); // 0: Tracking, 1: Mixing
    const int channelModeIdx = static_cast<int> (apvts.getRawParameterValue ("channelMode")->load()); // 0: Auto, 1: Mono, 2: Stereo
    const float targetAmountDb = apvts.getRawParameterValue ("amount")->load();
    const float targetFloorDb  = apvts.getRawParameterValue ("floor")->load();
    const float targetGainDb   = apvts.getRawParameterValue ("gain")->load();
    const int preampIdx        = static_cast<int> (apvts.getRawParameterValue ("preamp")->load());
    const bool pocketOn        = apvts.getRawParameterValue ("pocket")->load() >= 0.5f;
    const bool rideOn          = apvts.getRawParameterValue ("ride")->load() >= 0.5f;

    const auto profile = getSourceProfile (sourceIdx);

    if (sourceIdx != lastSourceIdx)
    {
        spectralAnalyzer.configure (profile);
        lastSourceIdx = sourceIdx;
    }

    if (preampIdx != lastPreampIdx)
    {
        prevPreampIdx = lastPreampIdx;
        lastPreampIdx = preampIdx;
        smPreampCrossfade = 0.0f;
    }

    preampStage.configurePreamp (lastPreampIdx);

    const float srF = static_cast<float> (currentSampleRate);
    const float paramCoeff = 1.0f - std::exp (-1.0f / (0.025f * srF));
    const float xfadeCoeff = 1.0f - std::exp (-1.0f / (0.035f * srF));
    const float targetTrackingMix = (workflowIdx == 0) ? 1.0f : 0.0f;

    // 0: Auto (sigue inputWasMono), 1: Mono forzado, 2: Stereo forzado
    const bool wantMono = (channelModeIdx == 1) || (channelModeIdx == 0 && inputWasMono);
    const float targetMonoMix = wantMono ? 1.0f : 0.0f;

    auto* left  = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);

    float peakIn = 1.0e-7f;
    float peakOut = 1.0e-7f;
    float lastRideWithPocket = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        smAmountDb       += paramCoeff * (targetAmountDb - smAmountDb);
        smFloorDb        += paramCoeff * (targetFloorDb - smFloorDb);
        smGainDb         += paramCoeff * (targetGainDb - smGainDb);
        smTrackingMix    += paramCoeff * (targetTrackingMix - smTrackingMix);
        smChannelModeMix += paramCoeff * (targetMonoMix - smChannelModeMix);
        smPreampCrossfade = std::min (1.0f, smPreampCrossfade + xfadeCoeff);

        const float rawL = left[i];
        const float rawR = right[i];
        peakIn = std::max ({ peakIn, std::abs (rawL), std::abs (rawR) });

        // El detector lee la señal combinada para mover el fader igual en mono y estéreo
        const auto analysis = spectralAnalyzer.processSample (rawL, rawR, smFloorDb, profile);

        const float rideWithPocketDb = handMotionEngine.processSample (
            analysis.effectiveLevelDb,
            analysis.noiseFloorEstDb,
            analysis.deltaDb,
            speedIdx,
            smAmountDb,
            smFloorDb,
            smTrackingMix,
            pocketOn,
            rideOn,
            profile);

        lastRideWithPocket = rideWithPocketDb;

        // El modo de canal decide si la señal de audio/preamp se suma a mono o conserva estéreo
        const float monoSum = 0.5f * (rawL + rawR);
        float outL = rawL + smChannelModeMix * (monoSum - rawL);
        float outR = rawR + smChannelModeMix * (monoSum - rawR);

        const float totalGainDb = rideWithPocketDb + smGainDb;
        const float baseDrive = (0.14f + 0.16f * (1.0f - smTrackingMix) + 0.18f * (smAmountDb / 12.0f))
                              * profile.preampDriveScale;

        preampStage.processStereoSample (outL, outR,
                                         lastPreampIdx, prevPreampIdx, smPreampCrossfade,
                                         totalGainDb, rideWithPocketDb, baseDrive, smTrackingMix);

        if (smChannelModeMix > 0.999f)
        {
            const float avgOut = 0.5f * (outL + outR);
            outL = avgOut;
            outR = avgOut;
        }

        left[i]  = outL;
        right[i] = outR;

        peakOut = std::max ({ peakOut, std::abs (outL), std::abs (outR) });
    }

    inputLevelDb.store (juce::Decibels::gainToDecibels (peakIn, -60.0f), std::memory_order_relaxed);
    outputLevelDb.store (juce::Decibels::gainToDecibels (peakOut, -60.0f), std::memory_order_relaxed);
    rideOffsetDb.store (lastRideWithPocket, std::memory_order_relaxed);
}

juce::AudioProcessorEditor* BruceSwedienRiderProcessor::createEditor()
{
    return new BruceSwedienRiderEditor (*this);
}

void BruceSwedienRiderProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void BruceSwedienRiderProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BruceSwedienRiderProcessor();
}
