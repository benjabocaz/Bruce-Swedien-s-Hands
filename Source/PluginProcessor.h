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

    // Biquad estéreo de 2º orden (Direct Form II Transposed) sin asignación en hilo de audio
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

            const float invA0 = 1.0f / (1.0f + alpha);
            b0 = ((1.0f - cosW) * 0.5f) * invA0;
            b1 = (1.0f - cosW) * invA0;
            b2 = b0;
            a1 = (-2.0f * cosW) * invA0;
            a2 = (1.0f - alpha) * invA0;
        }

        void setHighPass (float cutoffHz, float q = 0.7071f)
        {
            const float fc = std::clamp (cutoffHz, 20.0f, static_cast<float> (sr * 0.45));
            const float w0 = juce::MathConstants<float>::twoPi * fc / static_cast<float> (sr);
            const float cosW = std::cos (w0);
            const float sinW = std::sin (w0);
            const float alpha = sinW / (2.0f * std::max (0.1f, q));

            const float invA0 = 1.0f / (1.0f + alpha);
            b0 = ((1.0f + cosW) * 0.5f) * invA0;
            b1 = -(1.0f + cosW) * invA0;
            b2 = b0;
            a1 = (-2.0f * cosW) * invA0;
            a2 = (1.0f - alpha) * invA0;
        }

        void setLowShelf (float cutoffHz, float gainDb, float q = 0.7071f)
        {
            const float fc = std::clamp (cutoffHz, 20.0f, static_cast<float> (sr * 0.45));
            const float A = std::pow (10.0f, gainDb / 40.0f);
            const float w0 = juce::MathConstants<float>::twoPi * fc / static_cast<float> (sr);
            const float cosW = std::cos (w0);
            const float sinW = std::sin (w0);
            const float twoSqrtAAlpha = 2.0f * std::sqrt (A) * sinW / (2.0f * std::max (0.1f, q));

            const float invA0 = 1.0f / ((A + 1.0f) + (A - 1.0f) * cosW + twoSqrtAAlpha);
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

            const float invA0 = 1.0f / ((A + 1.0f) - (A - 1.0f) * cosW + twoSqrtAAlpha);
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
        float wBody;               // peso de energía en banda cuerpo
        float wSub;                // peso de energía en banda grave/sub
        float wSib;                // peso base de energía en banda sibilancia/agudos
        float sibThresholdRatio;   // umbral de relación sibilancia / (cuerpo + sibilancia)
        float sibGuardStrength;    // cuánto reduce el peso sibilante al superar el umbral
        float transientTame;       // cuánto frena transientes secos vs energía sostenida
        float holdMs;              // espera antes de soltar hacia 0 dB
        float deadzoneDb;          // zona muerta musical alrededor del objetivo (dB)
        float floorBiasDb;         // ajuste fino de piso por fuente (dB)
        float stiffnessScale;      // multiplicador de frecuencia natural del resorte
        float dampingScale;        // multiplicador de amortiguamiento
        float preampDriveScale;    // escala de drive del preamp para esta fuente
    };

    static SourceProfile getSourceProfile (int sourceIdx);

    // CAPA 1: Detector espectral multibanda psicoacústico
    class SpectralAnalyzer
    {
    public:
        struct AnalysisResult
        {
            float effectiveLevelDb = -80.0f;
            float noiseFloorEstDb  = -52.0f;
            float deltaDb          = 0.0f;
        };

        void prepare (double sampleRate);
        void configure (const SourceProfile& profile);
        AnalysisResult processSample (float inL, float inR, float userFloorDb, const SourceProfile& profile) noexcept;

    private:
        double sr = 44100.0;
        StereoBiquad2 subLp;
        StereoBiquad2 bodyHp;
        StereoBiquad2 bodyLp;
        StereoBiquad2 sibHp;

        float fastEnvDb = -80.0f;
        float slowEnvDb = -80.0f;
        float noiseFloorEstDb = -52.0f;
        float prevInstDb = -80.0f;
        float fastCoeff = 0.0f;
        float slowCoeff = 0.0f;
        float floorAdaptCoeff = 0.0f;
    };

    // CAPA 2: Física de la mano (2º orden resorte/masa + deadzone + POCKET orgánico)
    class HandMotionEngine
    {
    public:
        void prepare (double sampleRate);
        float processSample (float effectiveLevelDb,
                             float noiseFloorEstDb,
                             float deltaDb,
                             int speedIdx,
                             float amountDb,
                             float floorDb,
                             float trackingMix,
                             bool pocketOn,
                             bool rideOn,
                             const SourceProfile& profile) noexcept;

        float getCurrentRideDb() const noexcept { return currentGainDb; }
        float getCurrentVelocityDbPerSec() const noexcept { return velDbPerSec; }

    private:
        double sr = 44100.0;
        float currentGainDb = 0.0f;
        float velDbPerSec = 0.0f;
        int holdRemaining = 0;

        float phaseA = 0.0f;
        float phaseB = 1.73f;
        float phaseC = 3.41f;
    };

    // CAPA 3: Etapa de consola / preamp (Clean limpio + Harrison 32C / Neve / API + crossfade sin clicks)
    class ConsolePreampStage
    {
    public:
        void prepare (double sampleRate);
        void configurePreamp (int preampIdx);
        void processStereoSample (float& ioL,
                                  float& ioR,
                                  int preampIdx,
                                  int prevPreampIdx,
                                  float preampCrossfade,
                                  float totalGainDb,
                                  float rideDb,
                                  float driveAmount,
                                  float trackingMix) noexcept;

    private:
        float applyModel (float x, int modelIdx, float drive, float faderPushNorm, float trackingMix, int ch) noexcept;

        double sr = 44100.0;
        StereoBiquad2 inShelf;
        StereoBiquad2 outShelf;
        std::array<float, 2> dcX { 0.0f, 0.0f };
        std::array<float, 2> dcY { 0.0f, 0.0f };
    };

    double currentSampleRate = 44100.0;

    SpectralAnalyzer spectralAnalyzer;
    HandMotionEngine handMotionEngine;
    ConsolePreampStage preampStage;

    // Suavizado de parámetros muestra a muestra para evitar clicks
    float smAmountDb = 6.0f;
    float smFloorDb  = -36.0f;
    float smGainDb   = 0.0f;
    float smTrackingMix = 0.0f; // 0.0 = Mixing, 1.0 = Tracking
    float smPreampCrossfade = 1.0f;
    int lastPreampIdx = 1;
    int prevPreampIdx = 1;
    int lastSourceIdx = -1;

    std::atomic<float> inputLevelDb { -60.0f };
    std::atomic<float> outputLevelDb { -60.0f };
    std::atomic<float> rideOffsetDb { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BruceSwedienRiderProcessor)
};
