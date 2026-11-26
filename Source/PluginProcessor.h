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

    struct OnePoleFilter
    {
        double sr = 44100.0;
        float z1 = 0.0f;
        float alpha = 0.0f;

        void prepare (double sampleRate)
        {
            sr = sampleRate > 0.0 ? sampleRate : 44100.0;
            reset();
        }

        void reset() { z1 = 0.0f; }

        void setLowPass (float cutoffHz)
        {
            const float fc = std::clamp (cutoffHz, 20.0f, static_cast<float> (sr * 0.45));
            alpha = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * fc / static_cast<float> (sr));
        }

        inline float processLP (float x) noexcept
        {
            z1 += alpha * (x - z1);
            return z1;
        }
    };

    struct SourceProfile
    {
        float bodyLpHz;
        float sibilanceHpHz;
        float sibilanceReject;   // 0.0..1.0 cuánto ignora picos agudos/sibilantes vs cuerpo
        float subReject;       // 0.0..1.0 cuánto limpia subgraves crudos en el detector
        float holdMs;          // espera antes de soltar hacia 0 dB
        float defaultSpeedIdx; // 0: Super Fast, 1: Fast, 2: Natural, 3: Slow, 4: Super Slow
    };

    static SourceProfile getSourceProfile (int sourceIdx);
    float applyPreampSample (float x, int preampIdx, float driveAmount, int ch) noexcept;

    double currentSampleRate = 44100.0;

    // Filtros detectores espectrales (cero latencia)
    OnePoleFilter bodyLpL, bodyLpR;
    OnePoleFilter sibLpL, sibLpR;
    OnePoleFilter subLpL, subLpR;

    // Estado del rider y de la "mano"
    float envDb = -80.0f;
    float sibEnvDb = -80.0f;
    float currentGainDb = 0.0f;
    float faderVelocityDb = 0.0f;
    int holdSamplesRemaining = 0;
    float pocketPhase = 0.0f;

    // Estado del preamp analógico
    std::array<float, 2> dcBlockerX { 0.0f, 0.0f };
    std::array<float, 2> dcBlockerY { 0.0f, 0.0f };
    std::array<float, 2> preampToneZ { 0.0f, 0.0f };

    // Medidores para la UI
    std::atomic<float> inputLevelDb { -60.0f };
    std::atomic<float> outputLevelDb { -60.0f };
    std::atomic<float> rideOffsetDb { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BruceSwedienRiderProcessor)
};
