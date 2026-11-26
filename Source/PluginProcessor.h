#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>
#include <algorithm>

class BruceSwedienRiderProcessor : public juce::AudioProcessor
{
public:
    BruceSwedienRiderProcessor();
    ~BruceSwedienRiderProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    float getInputLevelDb() const noexcept { return inputLevelDb.load (std::memory_order_relaxed); }
    float getOutputLevelDb() const noexcept { return outputLevelDb.load (std::memory_order_relaxed); }
    float getRideOffsetDb() const noexcept { return rideOffsetDb.load (std::memory_order_relaxed); }

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Biquad estéreo de 2º orden (Direct Form II Transposed) sin asignación en audio thread
    struct StereoBiquad2
    {
        double sr = 44100.0;
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
        std::array<float, 2> z1 { 0.0f, 0.0f };
        std::array<float, 2> z2 { 0.0f, 0.0f };

        void prepare (double sampleRate)
        {
            sr = sampleRate > 0.0 ? sampleRate : 44100.0;
            reset();
        }

        void reset()
        {
            z1.fill (0.0f);
            z2.fill (0.0f);
        }

        void setLowPass (float cutoffHz, float q = 0.7071f)
        {
            const float fc = std::clamp (cutoffHz, 20.0f, static_cast<float> (sr * 0.45));
            const float w0 = juce::MathConstants<float>::twoPi * fc / static_cast<float> (sr);
            const float cosW = std::cos (w0);
            const float sinW = std::sin (w0);
            const float alpha = sinW / (2.0f * std::max (0.1f, q));

            const float norm = 1.0f / (1.0f + alpha);
            b0 = ((1.0f - cosW) * 0.5f) * norm;
            b1 = (1.0f - cosW) * norm;
            b2 = b0;
            a1 = (-2.0f * cosW) * norm;
            a2 = (1.0f - alpha) * norm;
        }

        void setHighPass (float cutoffHz, float q = 0.7071f)
        {
            const float fc = std::clamp (cutoffHz, 20.0f, static_cast<float> (sr * 0.45));
            const float w0 = juce::MathConstants<float>::twoPi * fc / static_cast<float> (sr);
            const float cosW = std::cos (w0);
            const float sinW = std::sin (w0);
            const float alpha = sinW / (2.0f * std::max (0.1f, q));

            const float norm = 1.0f / (1.0f + alpha);
            b0 = ((1.0f + cosW) * 0.5f) * norm;
            b1 = -(1.0f + cosW) * norm;
            b2 = b0;
            a1 = (-2.0f * cosW) * norm;
            a2 = (1.0f - alpha) * norm;
        }

        void setLowShelf (float cutoffHz, float gainDb, float q = 0.7071f)
        {
            const float fc = std::clamp (cutoffHz, 20.0f, static_cast<float> (sr * 0.45));
            const float A = std::pow (10.0f, gainDb / 40.0f);
            const float w0 = juce::MathConstants<float>::twoPi * fc / static_cast<float> (sr);
            const float cosW = std::cos (w0);
            const float sinW = std::sin (w0);
            const float twoSqrtAAlpha = 2.0f * std::sqrt (A) * sinW / (2.0f * std::max (0.1f, q));

            const float a0 = (A + 1.0f) + (A - 1.0f) * cosW + twoSqrtAAlpha;
            const float invA0 = 1.0f / a0;

            b0 = (A * ((A + 1.0f) - (A - 1.0f) * cosW + twoSqrtAAlpha)) * invA0;
            b1 = (2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosW)) * invA0;
            b2 = (A * ((A + 1.0f) - (A - 1.0f) * cosW - twoSqrtAAlpha)) * invA0;
            a1 = (-2.0f * ((A - 1.0f) + (A + 1.0f) * cosW)) * invA0;
            a2 = ((A + 1.0f) + (A - 1.0f) * cosW - twoSqrtAAlpha) * invA0;
        }

        void setHighShelf (float cutoffHz, float gainDb, float q = 0.7071f)
        {
            const float fc = std::clamp (cutoffHz, 20.0f, static_cast<float> (sr * 0.45));
            const float A = std::pow (10.0f, gainDb / 40.0f);
            const float w0 = juce::MathConstants<float>::twoPi * fc / static_cast<float> (sr);
            const float cosW = std::cos (w0);
            const float sinW = std::sin (w0);
            const float twoSqrtAAlpha = 2.0f * std::sqrt (A) * sinW / (2.0f * std::max (0.1f, q));

            const float a0 = (A + 1.0f) - (A - 1.0f) * cosW + twoSqrtAAlpha;
            const float invA0 = 1.0f / a0;

            b0 = (A * ((A + 1.0f) + (A - 1.0f) * cosW + twoSqrtAAlpha)) * invA0;
            b1 = (-2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosW)) * invA0;
            b2 = (A * ((A + 1.0f) + (A - 1.0f) * cosW - twoSqrtAAlpha)) * invA0;
            a1 = (2.0f * ((A - 1.0f) - (A + 1.0f) * cosW)) * invA0;
            a2 = ((A + 1.0f) - (A - 1.0f) * cosW - twoSqrtAAlpha) * invA0;
        }

        inline float processSample (int ch, float x) noexcept
        {
            const float y = b0 * x + z1[ch];
            z1[ch] = b1 * x - a1 * y + z2[ch];
            z2[ch] = b2 * x - a2 * y;
            return y;
        }
    };

    struct SourceProfile
    {
        float bodyLowHz;
        float bodyHighHz;
        float sibCutoffHz;
        float sibilanceGuard;     // 0.0..1.0 cuánto ignora picos sibilantes frente al cuerpo
        float subGuard;           // 0.0..1.0 cuánto limpia subgraves en el detector
        float transientTame;      // 0.0..1.0 cuánto frena golpes secos vs energía sostenida
        float holdMs;             // espera antes de soltar hacia 0 dB
        float floorBiasDb;        // ajuste fino de piso por fuente (ej. más prudente en voces/acústica)
        float springStiffnessScale; // rigidez relativa del modelo resorte/masa para esta fuente
        float dampingScale;       // amortiguamiento relativo para esta fuente
        float preampDriveScale;   // cuánto carácter empuja por defecto en esta fuente
    };

    static SourceProfile getSourceProfile (int sourceIdx);
    float applyPreampSample (float x, int preampIdx, float driveAmount, float faderPushNorm, int ch) noexcept;

    double currentSampleRate = 44100.0;

    // Filtros detectores multibanda (0 muestras de latencia)
    StereoBiquad2 subLpFilter;
    StereoBiquad2 bodyHpFilter;
    StereoBiquad2 bodyLpFilter;
    StereoBiquad2 sibHpFilter;

    // Modelado tonal sutil del preamp (Harrison 32C / Neve / API / Clean)
    StereoBiquad2 preampInShelf;
    StereoBiquad2 preampOutShelf;

    // Envolventes del detector espectral
    float fastEnvDb = -80.0f;
    float slowEnvDb = -80.0f;
    float sibEnvDb  = -80.0f;
    float noiseFloorEstDb = -52.0f;
    float prevSampleDb = -80.0f;

    // Modelo físico de 2º orden (resorte + masa + amortiguamiento) para el fader
    float currentGainDb = 0.0f;
    float faderVelDbPerSec = 0.0f;
    int holdSamplesRemaining = 0;

    // Osciladores inarmónicos lentos para POCKET orgánico (sin patrón repetitivo obvio)
    float pocketPhaseA = 0.0f;
    float pocketPhaseB = 1.73f;
    float pocketPhaseC = 3.41f;

    // Parámetros suavizados muestra a muestra (evita clicks al mover controles en vivo)
    float smAmountDb = 6.0f;
    float smFloorDb  = -36.0f;
    float smGainDb   = 0.0f;
    float smTrackingMix = 0.0f; // 0.0 = MIXING, 1.0 = TRACKING
    float smPreampMix   = 1.0f; // suavizado al cambiar entre Clean / 32C / Neve / API
    int lastPreampIdx   = 1;
    int prevPreampIdx   = 1;
    bool smoothReady    = false;

    // DC blocker por canal
    std::array<float, 2> dcBlockerX { 0.0f, 0.0f };
    std::array<float, 2> dcBlockerY { 0.0f, 0.0f };

    // Medidores para la UI
    std::atomic<float> inputLevelDb { -60.0f };
    std::atomic<float> outputLevelDb { -60.0f };
    std::atomic<float> rideOffsetDb { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BruceSwedienRiderProcessor)
};
