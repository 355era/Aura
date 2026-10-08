#pragma once

#include <JuceHeader.h>

#include <array>
#include <cstdint>
#include <vector>

class AuraAudioProcessor final : public juce::AudioProcessor, private juce::AsyncUpdater
{
public:
    AuraAudioProcessor();
    ~AuraAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Aura"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destinationData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getParameters() noexcept { return parameters; }
    float getInputLevel() const noexcept { return inputMeter.load(std::memory_order_relaxed); }
    float getLfoValue() const noexcept { return lfoMeter.load(std::memory_order_relaxed); }
    float getLfoPhase() const noexcept { return lfoPhaseMeter.load(std::memory_order_relaxed); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void handleAsyncUpdate() override;

    class SpectralChannel
    {
    public:
        SpectralChannel();
        void prepare(double sampleRate, int requestedFftSize) noexcept;
        void setFftSize(int requestedFftSize) noexcept;
        void reset() noexcept;
        float processSample(float input, float shiftHz, float bloomAmount, float blurAmount,
                            float lowCutHz, float highCutHz, float& delayedDry) noexcept;

    private:
        struct Complex
        {
            float real = 0.0f;
            float imag = 0.0f;
        };

        static constexpr int maximumFftSize = 8192;
        static constexpr int maximumHalfFftSize = maximumFftSize / 2;
        static constexpr int inputMask = maximumFftSize - 1;
        static constexpr int outputQueueSize = maximumFftSize * 4;
        static constexpr int outputQueueMask = outputQueueSize - 1;

        void makeTables() noexcept;
        void transform(bool inverse) noexcept;
        void processFrame(float shiftHz, float bloomAmount, float blurAmount,
                          float lowCutHz, float highCutHz) noexcept;

        std::array<float, maximumFftSize> inputRing{};
        std::array<double, maximumFftSize> shiftPhaseRing{};
        std::array<float, maximumFftSize> dryDelay{};
        std::array<float, outputQueueSize> outputQueue{};
        std::array<float, outputQueueSize> normalizationQueue{};
        std::array<float, maximumFftSize> analysisWindow{};
        std::array<float, maximumFftSize> synthesisWindow{};
        std::array<Complex, maximumFftSize> fftData{};
        std::array<Complex, maximumFftSize> mappedSpectrum{};
        std::array<Complex, maximumHalfFftSize> twiddles{};
        std::array<float, maximumHalfFftSize + 1> magnitudes{};
        std::array<float, maximumHalfFftSize + 1> blurredMagnitudes{};
        std::array<std::uint16_t, maximumFftSize> bitReversed{};

        int inputWritePosition = 0;
        int dryWritePosition = 0;
        int outputReadPosition = 0;
        std::uint64_t samplesSeen = 0;
        double currentSampleRate = 44100.0;
        double shiftOscillatorPhase = 0.0;
        int activeFftSize = 2048;
        int activeHopSize = 512;
        int activeHalfFftSize = 1024;
        int activeFftMask = 2047;
    };

    class GrainDelayChannel
    {
    public:
        void prepare(double sampleRate);
        void reset() noexcept;
        float processSample(float input, float pitchRatio, float sizeMilliseconds,
                            float grainsPerSecond, float feedback) noexcept;

    private:
        struct GrainVoice
        {
            double readPosition = 0.0;
            float readStep = 1.0f;
            int age = 0;
            int length = 1;
            bool active = false;
        };

        static constexpr size_t voiceCount = 8;
        std::vector<float> delayBuffer;
        std::array<GrainVoice, voiceCount> voices{};
        double currentSampleRate = 44100.0;
        int writePosition = 0;
        int samplesUntilNextGrain = 0;
        size_t nextVoice = 0;
    };

    juce::AudioProcessorValueTreeState parameters;
    std::vector<SpectralChannel> channelProcessors;
    std::vector<GrainDelayChannel> grainProcessors;
    juce::SmoothedValue<float> shiftSmoother;
    juce::SmoothedValue<float> mixSmoother;
    juce::SmoothedValue<float> bloomSmoother;
    juce::SmoothedValue<float> blurSmoother;
    juce::SmoothedValue<float> lowCutSmoother;
    juce::SmoothedValue<float> highCutSmoother;
    juce::SmoothedValue<float> grainMixSmoother;
    juce::SmoothedValue<float> grainSizeSmoother;
    juce::SmoothedValue<float> densitySmoother;
    juce::SmoothedValue<float> feedbackSmoother;
    juce::SmoothedValue<float> grainPitchSmoother;
    juce::SmoothedValue<float> toneSmoother;
    juce::SmoothedValue<float> widthSmoother;
    juce::SmoothedValue<float> lfoRateSmoother;
    juce::SmoothedValue<float> lfoDepthSmoother;
    juce::SmoothedValue<float> spectralEnabledSmoother;
    juce::SmoothedValue<float> grainEnabledSmoother;
    juce::SmoothedValue<float> toneEnabledSmoother;
    juce::SmoothedValue<float> outputGainSmoother;
    std::vector<float> toneFilterState;
    std::atomic<float> inputMeter{ 0.0f };
    std::atomic<float> lfoMeter{ 0.0f };
    std::atomic<float> lfoPhaseMeter{ 0.0f };
    double lfoPhase = 0.0;
    float lfoHeldRandom = 0.0f;
    std::uint32_t lfoRandomState = 0x355e4a91U;
    int currentFftSize = 2048;
    std::atomic<int> pendingLatencyUpdate{ 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AuraAudioProcessor)
};
