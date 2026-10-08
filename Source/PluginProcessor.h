#pragma once

#include <JuceHeader.h>

#include <array>
#include <cstdint>
#include <vector>

class AuraAudioProcessor final : public juce::AudioProcessor
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

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    class SpectralChannel
    {
    public:
        SpectralChannel();
        void prepare(double sampleRate) noexcept;
        void reset() noexcept;
        float processSample(float input, float shiftHz, float bloomAmount, float& delayedDry) noexcept;

    private:
        struct Complex
        {
            float real = 0.0f;
            float imag = 0.0f;
        };

        static constexpr int fftSize = 2048;
        static constexpr int hopSize = fftSize / 4;
        static constexpr int halfFftSize = fftSize / 2;
        static constexpr int fftMask = fftSize - 1;
        static constexpr int outputQueueSize = fftSize * 4;
        static constexpr int outputQueueMask = outputQueueSize - 1;

        void makeTables() noexcept;
        void transform(bool inverse) noexcept;
        void processFrame(float shiftHz, float bloomAmount) noexcept;

        std::array<float, fftSize> inputRing{};
        std::array<double, fftSize> shiftPhaseRing{};
        std::array<float, fftSize> dryDelay{};
        std::array<float, outputQueueSize> outputQueue{};
        std::array<float, outputQueueSize> normalizationQueue{};
        std::array<float, fftSize> analysisWindow{};
        std::array<float, fftSize> synthesisWindow{};
        std::array<Complex, fftSize> fftData{};
        std::array<Complex, fftSize> mappedSpectrum{};
        std::array<Complex, halfFftSize> twiddles{};
        std::array<float, halfFftSize + 1> magnitudes{};
        std::array<std::uint16_t, fftSize> bitReversed{};

        int inputWritePosition = 0;
        int dryWritePosition = 0;
        int outputReadPosition = 0;
        std::uint64_t samplesSeen = 0;
        double currentSampleRate = 44100.0;
        double shiftOscillatorPhase = 0.0;
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
    juce::SmoothedValue<float> grainMixSmoother;
    juce::SmoothedValue<float> grainSizeSmoother;
    juce::SmoothedValue<float> densitySmoother;
    juce::SmoothedValue<float> feedbackSmoother;
    std::atomic<float> inputMeter{ 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AuraAudioProcessor)
};
