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
    // bodyLowHz, bodyHighHz, sibCutoffHz, sibGuard, subGuard, transTame, holdMs, floorBiasDb, stiffnessScale, dampingScale, preampDriveScale
    switch (sourceIdx)
    {
        case 0: return { 210.0f, 2800.0f, 4600.0f, 0.90f, 0.80f, 0.45f, 175.0f,  1.5f, 1.05f, 1.05f, 1.00f }; // Vocals
        case 1: return { 200.0f, 3000.0f, 4800.0f, 0.80f, 0.70f, 0.55f, 250.0f,  2.0f, 0.92f, 1.12f, 1.05f }; // Vocal Bus
        case 2: return {  55.0f,  850.0f, 2400.0f, 0.65f, 0.25f, 0.50f, 150.0f, -1.0f, 1.15f, 1.08f, 1.15f }; // Bass
        case 3: return { 170.0f, 3200.0f, 5000.0f, 0.70f, 0.65f, 0.55f, 190.0f,  0.0f, 1.00f, 1.00f, 1.10f }; // Guitar
        case 4: return { 160.0f, 3500.0f, 5500.0f, 0.60f, 0.75f, 0.65f, 240.0f,  2.5f, 0.85f, 1.15f, 0.95f }; // Acoustic / Strings
        case 5: return {  90.0f, 2200.0f, 4600.0f, 0.50f, 0.35f, 0.82f, 270.0f, -1.5f, 0.90f, 1.10f, 1.20f }; // Drum Bus
        case 6: return { 110.0f, 2600.0f, 5000.0f, 0.55f, 0.30f, 0.72f, 155.0f, -1.0f, 1.12f, 1.05f, 1.12f }; // Drum Machine
        case 7: return { 140.0f, 3100.0f, 5200.0f, 0.60f, 0.55f, 0.50f, 210.0f,  1.0f, 0.95f, 1.05f, 1.00f }; // Keys
        case 8: return { 130.0f, 3400.0f, 5500.0f, 0.55f, 0.45f, 0.60f, 220.0f,  0.5f, 0.92f, 1.08f, 1.08f }; // Synth
        case 9:
        default:return { 120.0f, 2800.0f, 6000.0f, 0.65f, 0.45f, 0.75f, 340.0f,  1.0f, 0.75f, 1.25f, 0.70f }; // Master Bus
    }
}

void BruceSwedienRiderProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    setLatencySamples (0);

    subLpFilter.prepare (currentSampleRate);
    bodyHpFilter.prepare (currentSampleRate);
    bodyLpFilter.prepare (currentSampleRate);
    sibHpFilter.prepare (currentSampleRate);

    preampInShelf.prepare (currentSampleRate);
    preampOutShelf.prepare (currentSampleRate);

    fastEnvDb = -80.0f;
    slowEnvDb = -80.0f;
    sibEnvDb  = -80.0f;
    noiseFloorEstDb = -52.0f;
    prevSampleDb = -80.0f;

    currentGainDb = 0.0f;
    faderVelDbPerSec = 0.0f;
    holdSamplesRemaining = 0;

    pocketPhaseA = 0.0f;
    pocketPhaseB = 1.73f;
    pocketPhaseC = 3.41f;

    smAmountDb = apvts.getRawParameterValue ("amount")->load();
    smFloorDb  = apvts.getRawParameterValue ("floor")->load();
    smGainDb   = apvts.getRawParameterValue ("gain")->load();
    smTrackingMix = (apvts.getRawParameterValue ("workflow")->load() < 0.5f) ? 1.0f : 0.0f;

    lastPreampIdx = static_cast<int> (apvts.getRawParameterValue ("preamp")->load());
    prevPreampIdx = lastPreampIdx;
    smPreampMix   = 1.0f;
    smoothReady   = true;

    dcBlockerX.fill (0.0f);
    dcBlockerY.fill (0.0f);
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

inline float BruceSwedienRiderProcessor::applyPreampSample (float x, int preampIdx, float driveAmount, float faderPushNorm, int ch) noexcept
{
    const float effectiveDrive = std::clamp (driveAmount * (0.85f + 0.35f * faderPushNorm), 0.0f, 1.0f);
    if (preampIdx == 0 && effectiveDrive <= 0.0005f)
        return x;

    float y = x;
    switch (preampIdx)
    {
        case 1: // Harrison 32C: medios-graves firmes, presencia limpia y saturación asimétrica musical
        {
            const float inStage = x * (1.0f + effectiveDrive * 1.45f);
            const float asym = std::tanh (inStage + 0.08f * effectiveDrive * inStage * inStage)
                       - 0.08f * effectiveDrive * 0.0064f;
            const float norm = 1.0f / std::max (0.55f, std::tanh (1.0f + effectiveDrive * 1.45f));
            y = asym * norm;
            break;
        }
        case 2: // Neve: transformador denso con redondeo suave en agudos
        {
            const float g = 1.0f + effectiveDrive * 2.0f;
            y = std::tanh (x * g) / std::max (0.48f, std::tanh (g));
            break;
        }
        case 3: // API: punch rápido y medios articulados
        {
            const float g = 1.0f + effectiveDrive * 1.8f;
            const float u = x * g;
            y = u / (1.0f + 0.32f * std::abs (u));
            break;
        }
        case 0:
        default: // Clean: transparencia con micro-colchón final
        {
            const float g = 1.0f + effectiveDrive * 0.45f;
            y = std::tanh (x * g) / std::max (0.65f, std::tanh (g));
            break;
        }
    }

    const float dcOut = y - dcBlockerX[ch] + 0.9994f * dcBlockerY[ch];
    dcBlockerX[ch] = y;
    dcBlockerY[ch] = dcOut;
    return dcOut;
}

void BruceSwedienRiderProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();
    if (numChannels == 0 || numSamples == 0)
        return;

    if (numChannels == 1)
        buffer.setSize (2, numSamples, true, false, true);

    const int sourceIdx   = static_cast<int> (apvts.getRawParameterValue ("source")->load());
    const int speedIdx    = static_cast<int> (apvts.getRawParameterValue ("speed")->load());
    const int workflowIdx = static_cast<int> (apvts.getRawParameterValue ("workflow")->load()); // 0: Tracking, 1: Mixing
    const float targetAmountDb = apvts.getRawParameterValue ("amount")->load();
    const float targetFloorDb  = apvts.getRawParameterValue ("floor")->load();
    const float targetGainDb   = apvts.getRawParameterValue ("gain")->load();
    const int preampIdx        = static_cast<int> (apvts.getRawParameterValue ("preamp")->load());
    const bool pocketOn        = apvts.getRawParameterValue ("pocket")->load() >= 0.5f;
    const bool rideOn          = apvts.getRawParameterValue ("ride")->load() >= 0.5f;

    if (preampIdx != lastPreampIdx)
    {
        prevPreampIdx = lastPreampIdx;
        lastPreampIdx = preampIdx;
        smPreampMix   = 0.0f;
    }

    const auto profile = getSourceProfile (sourceIdx);
    const float srFloat = static_cast<float> (currentSampleRate);

    // Configuramos filtros multibanda y ecualización sutil del preamp
    subLpFilter.setLowPass (120.0f, 0.7071f);
    bodyHpFilter.setHighPass (profile.bodyLowHz, 0.7071f);
    bodyLpFilter.setLowPass (profile.bodyHighHz, 0.7071f);
    sibHpFilter.setHighPass (profile.sibCutoffHz, 0.7071f);

    if (preampIdx == 1) // Harrison 32C
    {
        preampInShelf.setLowShelf (145.0f, 1.3f, 0.7071f);
        preampOutShelf.setHighShelf (7500.0f, 0.8f, 0.7071f);
    }
    else if (preampIdx == 2) // Neve
    {
        preampInShelf.setLowShelf (110.0f, 1.6f, 0.7071f);
        preampOutShelf.setHighShelf (6500.0f, -1.2f, 0.7071f);
    }
    else if (preampIdx == 3) // API
    {
        preampInShelf.setLowShelf (180.0f, 0.6f, 0.7071f);
        preampOutShelf.setHighShelf (3200.0f, 1.4f, 0.7071f);
    }
    else
    {
        preampInShelf.setLowShelf (120.0f, 0.0f, 0.7071f);
        preampOutShelf.setHighShelf (8000.0f, 0.0f, 0.7071f);
    }

    // Constantes de velocidad según el modo elegido (0: Super Fast .. 4: Super Slow)
    static constexpr std::array<float, 5> speedStiffnessHz { 7.5f, 4.4f, 2.4f, 1.25f, 0.65f };
    static constexpr std::array<float, 5> speedZeta        { 0.92f, 0.96f, 1.02f, 1.08f, 1.15f };

    const size_t sIdx = static_cast<size_t> (std::clamp (speedIdx, 0, 4));
    const float baseOmega = juce::MathConstants<float>::twoPi * speedStiffnessHz[sIdx] * profile.springStiffnessScale;
    const float baseZeta  = speedZeta[sIdx] * profile.dampingScale;
    const float dt = 1.0f / srFloat;

    const float fastCoeff = 1.0f - std::exp (-1.0f / (0.012f * srFloat));
    const float slowCoeff = 1.0f - std::exp (-1.0f / (0.140f * srFloat));
    const float sibCoeff  = 1.0f - std::exp (-1.0f / (0.015f * srFloat));
    const float paramSmCoeff = 1.0f - std::exp (-1.0f / (0.025f * srFloat));
    const float preampSmCoeff = 1.0f - std::exp (-1.0f / (0.035f * srFloat));

    const float targetTrackingMix = (workflowIdx == 0) ? 1.0f : 0.0f;
    const int holdTotalSamples = static_cast<int> (0.001f * profile.holdMs * srFloat);

    // En Tracking somos más prudentes con el rango y la saturación; en Mixing más expresivos
    const float trackingRangeScale = juce::jmap (smTrackingMix, 0.0f, 1.0f, 1.0f, 0.72f);
    const float trackingDriveScale = juce::jmap (smTrackingMix, 0.0f, 1.0f, 1.0f, 0.65f);

    constexpr float targetBalanceDb = -18.0f;

    auto* left  = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);

    float peakIn = 1.0e-7f;
    float peakOut = 1.0e-7f;

    for (int i = 0; i < numSamples; ++i)
    {
        if (! smoothReady)
            smoothReady = true;

        smAmountDb    += paramSmCoeff * (targetAmountDb - smAmountDb);
        smFloorDb     += paramSmCoeff * (targetFloorDb - smFloorDb);
        smGainDb      += paramSmCoeff * (targetGainDb - smGainDb);
        smTrackingMix += paramSmCoeff * (targetTrackingMix - smTrackingMix);
        smPreampMix    = std::min (1.0f, smPreampMix + preampSmCoeff);

        const float inL = left[i];
        const float inR = right[i];
        peakIn = std::max ({ peakIn, std::abs (inL), std::abs (inR) });

        // 1. Escucha espectral multibanda (0 muestras de latencia)
        const float subL  = subLpFilter.processSample (0, inL);
        const float subR  = subLpFilter.processSample (1, inR);
        const float noSubL = inL - subL * profile.subGuard;
        const float noSubR = inR - subR * profile.subGuard;

        const float bodyL = bodyLpFilter.processSample (0, bodyHpFilter.processSample (0, noSubL));
        const float bodyR = bodyLpFilter.processSample (1, bodyHpFilter.processSample (1, noSubR));
        const float sibL  = sibHpFilter.processSample (0, inL);
        const float sibR  = sibHpFilter.processSample (1, inR);

        const float bodyEnergy = std::max (std::abs (bodyL), std::abs (bodyR));
        const float sibEnergy  = std::max (std::abs (sibL), std::abs (sibR));
        const float sibDbInst  = juce::Decibels::gainToDecibels (std::max (sibEnergy, 1.0e-6f), -80.0f);
        sibEnvDb += sibCoeff * (sibDbInst - sibEnvDb);

        // Si sube mucho la sibilancia respecto al cuerpo, reducimos su peso en el detector
        const float sibToBodyRatio = sibEnergy / std::max (bodyEnergy, 1.0e-5f);
        const float sibWeight = (sibToBodyRatio > 1.10f)
            ? std::max (0.05f, 1.0f - profile.sibilanceGuard * std::clamp ((sibToBodyRatio - 0.8f) * 1.4f, 0.0f, 1.0f))
            : (1.0f - profile.sibilanceGuard * 0.30f);

        const float detAmp = std::max (bodyEnergy + sibEnergy * sibWeight,
                                       0.35f * std::max (std::abs (inL), std::abs (inR)));
        const float instDb = juce::Decibels::gainToDecibels (std::max (detAmp, 1.0e-6f), -80.0f);

        fastEnvDb += fastCoeff * (instDb - fastEnvDb);
        slowEnvDb += slowCoeff * (instDb - slowEnvDb);

        // Diferencia entre envolvente rápida y lenta para domar transientes secos en buses/voces
        const float transientSpikeDb = std::max (0.0f, fastEnvDb - slowEnvDb);
        const float effectiveLevelDb = fastEnvDb - transientSpikeDb * profile.transientTame * 0.65f;

        // Estimación adaptativa del piso de ruido / variabilidad de señal
        const float deltaDb = std::abs (instDb - prevSampleDb);
        prevSampleDb = instDb;
        if (instDb < smFloorDb + 6.0f && deltaDb < 1.2f)
            noiseFloorEstDb += (1.0f - std::exp (-dt / 0.35f)) * (instDb - noiseFloorEstDb);

        const float effectiveFloorDb = smFloorDb + profile.floorBiasDb * (0.6f + 0.4f * smTrackingMix);
        const float maxRangeDb = std::max (0.0f, smAmountDb * trackingRangeScale);

        float targetRideDb = 0.0f;
        if (! rideOn || maxRangeDb <= 0.01f)
        {
            targetRideDb = 0.0f;
            holdSamplesRemaining = 0;
        }
        else if (effectiveLevelDb < effectiveFloorDb || (effectiveLevelDb < noiseFloorEstDb + 3.5f && deltaDb < 0.35f))
        {
            // Pausa o respiración: mantenemos brevemente con hold y luego volvemos suave a 0 dB
            if (holdSamplesRemaining > 0)
            {
                --holdSamplesRemaining;
                targetRideDb = currentGainDb;
            }
            else
            {
                targetRideDb = 0.0f;
            }
        }
        else
        {
            holdSamplesRemaining = holdTotalSamples;
            const float diffDb = targetBalanceDb - effectiveLevelDb;
            // Curva no lineal suave: transparente en el centro y musical en los extremos
            const float normDiff = diffDb / std::max (2.0f, maxRangeDb * 1.25f);
            targetRideDb = std::tanh (normDiff) * maxRangeDb;
        }

        // 2. Modelo físico de resorte/masa (2º orden) con asimetría subida vs bajada
        const bool movingDown = (targetRideDb < currentGainDb);
        const float omega = baseOmega * (movingDown ? 1.18f : 0.85f);
        const float zeta  = baseZeta  * (movingDown ? 1.05f : 0.95f);

        const float accel = omega * omega * (targetRideDb - currentGainDb) - 2.0f * zeta * omega * faderVelDbPerSec;
        faderVelDbPerSec += accel * dt;
        currentGainDb += faderVelDbPerSec * dt;
        currentGainDb = std::clamp (currentGainDb, -18.0f, 18.0f);

        // 3. POCKET orgánico no repetitivo (3 osciladores lentos inarmónicos)
        float pocketDb = 0.0f;
        if (pocketOn && rideOn && maxRangeDb > 0.05f)
        {
            pocketPhaseA += juce::MathConstants<float>::twoPi * 0.73f * dt;
            pocketPhaseB += juce::MathConstants<float>::twoPi * 1.19f * dt;
            pocketPhaseC += juce::MathConstants<float>::twoPi * 1.91f * dt;
            if (pocketPhaseA > juce::MathConstants<float>::twoPi) pocketPhaseA -= juce::MathConstants<float>::twoPi;
            if (pocketPhaseB > juce::MathConstants<float>::twoPi) pocketPhaseB -= juce::MathConstants<float>::twoPi;
            if (pocketPhaseC > juce::MathConstants<float>::twoPi) pocketPhaseC -= juce::MathConstants<float>::twoPi;

            const float organicWave = 0.55f * std::sin (pocketPhaseA)
                                    + 0.30f * std::sin (pocketPhaseB)
                                    + 0.15f * std::sin (pocketPhaseC);
            const float pocketScale = std::min (1.0f, maxRangeDb / 6.0f) * (0.75f + 0.25f * (1.0f - smTrackingMix));
            pocketDb = organicWave * 0.24f * pocketScale;
        }

        const float totalGainDb = currentGainDb + pocketDb + smGainDb;
        const float gainLinear = juce::Decibels::decibelsToGain (totalGainDb, -60.0f);

        // 4. Etapa de Preamp con interacción dinámica al movimiento del fader y crossfade suave
        const float baseDrive = (0.14f + 0.16f * (1.0f - smTrackingMix) + 0.18f * (smAmountDb / 12.0f))
                              * profile.preampDriveScale * trackingDriveScale;
        const float faderPushNorm = std::clamp ((currentGainDb + pocketDb) / 12.0f, -0.5f, 1.0f);

        const float preL = preampInShelf.processSample (0, inL * gainLinear);
        const float preR = preampInShelf.processSample (1, inR * gainLinear);

        const float curP_L = applyPreampSample (preL, lastPreampIdx, baseDrive, faderPushNorm, 0);
        const float curP_R = applyPreampSample (preR, lastPreampIdx, baseDrive, faderPushNorm, 1);

        float mixL = curP_L;
        float mixR = curP_R;
        if (smPreampMix < 0.999f)
        {
            const float prevP_L = applyPreampSample (preL, prevPreampIdx, baseDrive, faderPushNorm, 0);
            const float prevP_R = applyPreampSample (preR, prevPreampIdx, baseDrive, faderPushNorm, 1);
            mixL = prevP_L + smPreampMix * (curP_L - prevP_L);
            mixR = prevP_R + smPreampMix * (curP_R - prevP_R);
        }

        const float outL = preampOutShelf.processSample (0, mixL);
        const float outR = preampOutShelf.processSample (1, mixR);

        left[i]  = outL;
        right[i] = outR;

        peakOut = std::max ({ peakOut, std::abs (outL), std::abs (outR) });
    }

    inputLevelDb.store (juce::Decibels::gainToDecibels (peakIn, -60.0f), std::memory_order_relaxed);
    outputLevelDb.store (juce::Decibels::gainToDecibels (peakOut, -60.0f), std::memory_order_relaxed);
    rideOffsetDb.store (currentGainDb, std::memory_order_relaxed);
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
