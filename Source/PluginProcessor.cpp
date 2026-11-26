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
    // bodyLpHz, sibilanceHpHz, sibilanceReject, subReject, holdMs, defaultSpeedIdx
    switch (sourceIdx)
    {
        case 0: return { 2600.0f, 4500.0f, 0.85f, 0.70f, 160.0f, 2.0f }; // Vocals: evita "S" y subgraves, respira frases
        case 1: return { 2800.0f, 4800.0f, 0.75f, 0.65f, 240.0f, 2.0f }; // Vocal Bus: bloque coral parejo
        case 2: return {  650.0f, 2500.0f, 0.55f, 0.25f, 140.0f, 1.0f }; // Bass: cuerpo grave/medio definido
        case 3: return { 3200.0f, 5000.0f, 0.65f, 0.60f, 180.0f, 2.0f }; // Guitar: medios con carácter, sin chillidos
        case 4: return { 3500.0f, 5500.0f, 0.50f, 0.75f, 220.0f, 3.0f }; // Acoustic / Strings: fraseo natural y amplio
        case 5: return { 1800.0f, 4500.0f, 0.40f, 0.35f, 260.0f, 2.0f }; // Drum Bus: acompaña sección sin bombear golpes
        case 6: return { 2200.0f, 5000.0f, 0.45f, 0.30f, 150.0f, 1.0f }; // Drum Machine: pulso firme y controlado
        case 7: return { 3000.0f, 5200.0f, 0.55f, 0.55f, 200.0f, 2.0f }; // Keys: dinámica de toque equilibrada
        case 8: return { 3400.0f, 5500.0f, 0.45f, 0.40f, 210.0f, 2.0f }; // Synth: suaviza aperturas de filtro y picos
        case 9:
        default:return { 2500.0f, 6000.0f, 0.60f, 0.45f, 320.0f, 3.0f }; // Master Bus: movimientos muy sutiles y musicales
    }
}

void BruceSwedienRiderProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    setLatencySamples (0);

    bodyLpL.prepare (currentSampleRate);
    bodyLpR.prepare (currentSampleRate);
    sibLpL.prepare (currentSampleRate);
    sibLpR.prepare (currentSampleRate);
    subLpL.prepare (currentSampleRate);
    subLpR.prepare (currentSampleRate);

    envDb = -80.0f;
    sibEnvDb = -80.0f;
    currentGainDb = 0.0f;
    faderVelocityDb = 0.0f;
    holdSamplesRemaining = 0;
    pocketPhase = 0.0f;
    dcBlockerX.fill (0.0f);
    dcBlockerY.fill (0.0f);
    preampToneZ.fill (0.0f);
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

inline float BruceSwedienRiderProcessor::applyPreampSample (float x, int preampIdx, float driveAmount, int ch) noexcept
{
    // preampIdx: 0 = Clean, 1 = Harrison 32C, 2 = Neve, 3 = API
    if (preampIdx == 0 && driveAmount <= 0.001f)
        return x;

    float driven = x;
    switch (preampIdx)
    {
        case 1: // Harrison 32C: medios-graves firmes y saturación suave y abierta
        {
            const float g = 1.0f + driveAmount * 1.65f;
            const float s = x * g;
            driven = std::tanh (s * 0.85f) / 0.85f + 0.04f * driveAmount * (s * s * s);
            // Pequeño realce cálido en medios-bajos
            preampToneZ[ch] += 0.035f * (driven - preampToneZ[ch]);
            driven += 0.14f * driveAmount * preampToneZ[ch];
            break;
        }
        case 2: // Neve: transformador denso y redondeo suave en agudos
        {
            const float g = 1.0f + driveAmount * 2.1f;
            driven = std::tanh (x * g) / std::max (0.45f, std::tanh (g));
            preampToneZ[ch] += 0.18f * (driven - preampToneZ[ch]);
            driven = driven * (1.0f - 0.15f * driveAmount) + preampToneZ[ch] * (0.15f * driveAmount);
            break;
        }
        case 3: // API: punch en medios y ataque rápido
        {
            const float g = 1.0f + driveAmount * 1.85f;
            const float u = x * g;
            driven = u / (1.0f + 0.35f * std::abs (u));
            break;
        }
        case 0:
        default:
        {
            const float g = 1.0f + driveAmount * 0.5f;
            driven = std::tanh (x * g) / std::max (0.6f, std::tanh (g));
            break;
        }
    }

    // DC blocker para mantener limpio el cero eléctrico
    const float dcOut = driven - dcBlockerX[ch] + 0.9992f * dcBlockerY[ch];
    dcBlockerX[ch] = driven;
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

    // Si la pista es mono en el host, duplicamos el canal 0 al 1 para operar en estéreo limpio
    if (numChannels == 1)
        buffer.setSize (2, numSamples, true, false, true);

    const int sourceIdx = static_cast<int> (apvts.getRawParameterValue ("source")->load());
    const int speedIdx  = static_cast<int> (apvts.getRawParameterValue ("speed")->load());
    const float amountDb = apvts.getRawParameterValue ("amount")->load();
    const float floorDb  = apvts.getRawParameterValue ("floor")->load();
    const float gainDb   = apvts.getRawParameterValue ("gain")->load();
    const int preampIdx  = static_cast<int> (apvts.getRawParameterValue ("preamp")->load());
    const bool pocketOn  = apvts.getRawParameterValue ("pocket")->load() >= 0.5f;
    const bool rideOn    = apvts.getRawParameterValue ("ride")->load() >= 0.5f;

    const auto profile = getSourceProfile (sourceIdx);

    bodyLpL.setLowPass (profile.bodyLpHz);
    bodyLpR.setLowPass (profile.bodyLpHz);
    sibLpL.setLowPass (profile.sibilanceHpHz);
    sibLpR.setLowPass (profile.sibilanceHpHz);
    subLpL.setLowPass (120.0f);
    subLpR.setLowPass (120.0f);

    // Constantes de velocidad según el modo elegido (0..4)
    static constexpr std::array<float, 5> followMs { 14.0f, 38.0f, 95.0f, 220.0f, 480.0f };
    static constexpr std::array<float, 5> faderMs  { 25.0f, 65.0f, 155.0f, 340.0f, 720.0f };

    const float detCoeff   = 1.0f - std::exp (-1.0f / (0.001f * followMs[static_cast<size_t> (std::clamp (speedIdx, 0, 4))] * static_cast<float> (currentSampleRate)));
    const float faderCoeff = 1.0f - std::exp (-1.0f / (0.001f * faderMs[static_cast<size_t> (std::clamp (speedIdx, 0, 4))]  * static_cast<float> (currentSampleRate)));
    const int holdTotalSamples = static_cast<int> (0.001f * profile.holdMs * static_cast<float> (currentSampleRate));

    // Objetivo de nivel musical (alrededor de -18 dBFS como punto de equilibrio de consola)
    constexpr float targetDb = -18.0f;

    // Color analógico sutil ligado al movimiento y a la fuente
    const float baseDrive = (sourceIdx == 9 ? 0.08f : 0.18f) + (amountDb / 12.0f) * 0.22f;

    auto* left  = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);

    float peakIn = 1.0e-7f;
    float peakOut = 1.0e-7f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float inL = left[i];
        const float inR = right[i];
        peakIn = std::max ({ peakIn, std::abs (inL), std::abs (inR) });

        // Escucha espectral: separamos cuerpo, agudos/sibilancias y subgraves
        const float subL = subLpL.processLP (inL);
        const float subR = subLpR.processLP (inR);
        const float noSubL = inL - subL * profile.subReject;
        const float noSubR = inR - subR * profile.subReject;

        const float bodyL = bodyLpL.processLP (noSubL);
        const float bodyR = bodyLpR.processLP (noSubR);
        const float sibL  = noSubL - sibLpL.processLP (noSubL);
        const float sibR  = noSubR - sibLpR.processLP (noSubR);

        // Si sube mucho la sibilancia ("S") respecto al cuerpo en voces, reducimos su peso en el detector
        const float bodyAmp = std::max (std::abs (bodyL), std::abs (bodyR));
        const float sibAmp  = std::max (std::abs (sibL), std::abs (sibR));
        const float sibPenalty = (sibAmp > bodyAmp * 1.15f) ? (1.0f - profile.sibilanceReject * 0.85f) : (1.0f - profile.sibilanceReject * 0.35f);

        const float detSample = std::max (std::abs (bodyL + sibL * sibPenalty),
                                        std::abs (bodyR + sibR * sibPenalty));
        const float sampleDb = juce::Decibels::gainToDecibels (std::max (detSample, 1.0e-6f), -80.0f);

        envDb += detCoeff * (sampleDb - envDb);

        float desiredGainDb = 0.0f;
        if (! rideOn || amountDb <= 0.01f)
        {
            desiredGainDb = 0.0f;
            holdSamplesRemaining = 0;
        }
        else if (envDb < floorDb)
        {
            // Por debajo del piso (silencio / respiración): mantenemos hold y luego volvemos suave a 0 dB (sin inflar ruido)
            if (holdSamplesRemaining > 0)
            {
                --holdSamplesRemaining;
                desiredGainDb = currentGainDb;
            }
            else
            {
                desiredGainDb = 0.0f;
            }
        }
        else
        {
            holdSamplesRemaining = holdTotalSamples;
            const float rawDiff = targetDb - envDb;
            // Suavizamos con curva tanh para que el movimiento sea musical y nunca brusco
            const float maxRange = std::max (0.5f, amountDb);
            desiredGainDb = std::tanh (rawDiff / (maxRange * 1.15f)) * maxRange;
        }

        // Inercia y "Pocket" humano: ligera variación orgánicamente lenta y asimetría al subir vs bajar
        float effectiveCoeff = faderCoeff;
        if (desiredGainDb < currentGainDb)
            effectiveCoeff *= 1.18f; // frena/contiene picos con decisión
        else
            effectiveCoeff *= 0.82f; // sube con más calma para sonar natural

        float pocketOffsetDb = 0.0f;
        if (pocketOn && rideOn && amountDb > 0.01f)
        {
            pocketPhase += juce::MathConstants<float>::twoPi * 1.35f / static_cast<float> (currentSampleRate);
            if (pocketPhase > juce::MathConstants<float>::twoPi)
                pocketPhase -= juce::MathConstants<float>::twoPi;

            pocketOffsetDb = std::sin (pocketPhase) * 0.22f * std::min (1.0f, amountDb / 6.0f);
        }

        const float targetWithPocket = desiredGainDb + pocketOffsetDb;
        const float prevGain = currentGainDb;
        currentGainDb += effectiveCoeff * (targetWithPocket - currentGainDb);
        faderVelocityDb = currentGainDb - prevGain;

        const float totalDb = currentGainDb + gainDb;
        const float gainLinear = juce::Decibels::decibelsToGain (totalDb, -60.0f);

        // El movimiento del fader empuja sutilmente el carácter del preamp
        const float dynDrive = std::clamp (baseDrive + std::max (0.0f, faderVelocityDb * 12.0f) * 0.08f, 0.0f, 0.85f);

        const float outL = applyPreampSample (inL * gainLinear, preampIdx, dynDrive, 0);
        const float outR = applyPreampSample (inR * gainLinear, preampIdx, dynDrive, 1);

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
