#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float pi = juce::MathConstants<float>::pi;

}

AuraAudioProcessor::AuraAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "AURA_PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout AuraAudioProcessor::createParameterLayout()
{
    using Parameter = juce::AudioParameterFloat;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<Parameter>(juce::ParameterID{ "shift", 1 }, "Shift",
                                           juce::NormalisableRange<float>{ -24.0f, 24.0f, 1.0f },
                                           0.0f, "st"));
    layout.add(std::make_unique<Parameter>(juce::ParameterID{ "mix", 1 }, "Mix",
                                           juce::NormalisableRange<float>{ 0.0f, 100.0f, 0.1f },
                                           100.0f, "%"));
    layout.add(std::make_unique<Parameter>(juce::ParameterID{ "bloom", 1 }, "Bloom",
                                           juce::NormalisableRange<float>{ 0.0f, 100.0f, 0.1f },
                                           18.0f, "%"));
    return layout;
}

void AuraAudioProcessor::prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock)
{
    juce::ignoreUnused(sampleRate, maximumExpectedSamplesPerBlock);
    channelProcessors.clear();
    channelProcessors.resize(static_cast<size_t>(getTotalNumInputChannels()));
    for (auto& channel : channelProcessors)
        channel.reset();

    setLatencySamples(2048);
    inputMeter.store(0.0f, std::memory_order_relaxed);
}

void AuraAudioProcessor::releaseResources()
{
    channelProcessors.clear();
}

bool AuraAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    if (input != output)
        return false;

    return input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo();
}

void AuraAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const auto* shiftParameter = parameters.getRawParameterValue("shift");
    const auto* mixParameter = parameters.getRawParameterValue("mix");
    const auto* bloomParameter = parameters.getRawParameterValue("bloom");
    const auto shift = shiftParameter != nullptr ? shiftParameter->load(std::memory_order_relaxed) : 0.0f;
    const auto mix = mixParameter != nullptr
                         ? juce::jlimit(0.0f, 1.0f, mixParameter->load(std::memory_order_relaxed) * 0.01f)
                         : 1.0f;
    const auto bloom = bloomParameter != nullptr
                           ? juce::jlimit(0.0f, 1.0f, bloomParameter->load(std::memory_order_relaxed) * 0.01f)
                           : 0.0f;
    const auto pitchRatio = std::pow(2.0f, shift / 12.0f);

    const auto channelsToProcess = juce::jmin(buffer.getNumChannels(), static_cast<int>(channelProcessors.size()));
    double inputSumSquares = 0.0;
    for (int channelIndex = 0; channelIndex < channelsToProcess; ++channelIndex)
    {
        auto* samples = buffer.getWritePointer(channelIndex);
        auto& channel = channelProcessors[static_cast<size_t>(channelIndex)];
        for (int sampleIndex = 0; sampleIndex < buffer.getNumSamples(); ++sampleIndex)
        {
            const auto input = samples[sampleIndex];
            inputSumSquares += static_cast<double>(input) * static_cast<double>(input);
            float delayedDry = 0.0f;
            const auto wet = channel.processSample(input, pitchRatio, bloom, delayedDry);
            const auto output = delayedDry + (wet - delayedDry) * mix;
            samples[sampleIndex] = output;
        }
    }

    const auto sampleCount = channelsToProcess * buffer.getNumSamples();
    const auto rms = sampleCount > 0 ? static_cast<float>(std::sqrt(inputSumSquares / sampleCount)) : 0.0f;
    const auto previousMeter = inputMeter.load(std::memory_order_relaxed);
    inputMeter.store(previousMeter * 0.82f + rms * 0.18f, std::memory_order_relaxed);
}

juce::AudioProcessorEditor* AuraAudioProcessor::createEditor()
{
    return new AuraAudioProcessorEditor(*this);
}

double AuraAudioProcessor::getTailLengthSeconds() const
{
    const auto sampleRate = getSampleRate();
    return sampleRate > 0.0 ? 4096.0 / sampleRate : 0.0;
}

void AuraAudioProcessor::getStateInformation(juce::MemoryBlock& destinationData)
{
    if (const auto xml = parameters.copyState().createXml())
        copyXmlToBinary(*xml, destinationData);
}

void AuraAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        const auto restoredState = juce::ValueTree::fromXml(*xml);
        if (restoredState.isValid() && restoredState.getType() == parameters.state.getType())
            parameters.replaceState(restoredState);
    }
}

AuraAudioProcessor::SpectralChannel::SpectralChannel()
{
    makeTables();
}

void AuraAudioProcessor::SpectralChannel::makeTables() noexcept
{
    for (int index = 0; index < fftSize; ++index)
    {
        const auto phase = 2.0f * pi * (static_cast<float>(index) + 0.5f) / static_cast<float>(fftSize);
        analysisWindow[static_cast<size_t>(index)] = std::sqrt(0.5f - 0.5f * std::cos(phase));
    }

    for (int index = 0; index < hopSize; ++index)
    {
        float overlapPower = 0.0f;
        for (int overlap = 0; overlap < fftSize / hopSize; ++overlap)
        {
            const auto windowIndex = index + overlap * hopSize;
            const auto window = analysisWindow[static_cast<size_t>(windowIndex)];
            overlapPower += window * window;
        }

        for (int overlap = 0; overlap < fftSize / hopSize; ++overlap)
        {
            const auto windowIndex = index + overlap * hopSize;
            synthesisWindow[static_cast<size_t>(windowIndex)] =
                analysisWindow[static_cast<size_t>(windowIndex)] / juce::jmax(overlapPower, 1.0e-8f);
        }
    }

    for (int index = 0; index < halfFftSize; ++index)
    {
        const auto phase = -2.0f * pi * static_cast<float>(index) / static_cast<float>(fftSize);
        twiddles[static_cast<size_t>(index)] = { std::cos(phase), std::sin(phase) };
    }

    for (int index = 0; index < fftSize; ++index)
    {
        auto value = static_cast<unsigned int>(index);
        unsigned int reversed = 0;
        for (int bit = 0; bit < 11; ++bit)
        {
            reversed = (reversed << 1U) | (value & 1U);
            value >>= 1U;
        }
        bitReversed[static_cast<size_t>(index)] = static_cast<std::uint16_t>(reversed);
    }
}

void AuraAudioProcessor::SpectralChannel::reset() noexcept
{
    inputRing.fill(0.0f);
    dryDelay.fill(0.0f);
    outputQueue.fill(0.0f);
    normalizationQueue.fill(0.0f);
    previousInputPhase.fill(0.0f);
    synthesisPhase.fill(0.0f);
    inputWritePosition = 0;
    dryWritePosition = 0;
    outputReadPosition = 0;
    samplesSeen = 0;
    phaseInitialized = false;
}

float AuraAudioProcessor::SpectralChannel::processSample(float input, float pitchRatio,
                                                         float bloomAmount, float& delayedDry) noexcept
{
    delayedDry = dryDelay[static_cast<size_t>(dryWritePosition)];
    dryDelay[static_cast<size_t>(dryWritePosition)] = input;
    dryWritePosition = (dryWritePosition + 1) & fftMask;

    inputRing[static_cast<size_t>(inputWritePosition)] = input;
    inputWritePosition = (inputWritePosition + 1) & fftMask;

    const auto queueIndex = static_cast<size_t>(outputReadPosition);
    const auto normalization = normalizationQueue[queueIndex];
    const auto wetOutput = normalization > 1.0e-8f ? outputQueue[queueIndex] / normalization : 0.0f;
    outputQueue[queueIndex] = 0.0f;
    normalizationQueue[queueIndex] = 0.0f;
    ++samplesSeen;

    if (samplesSeen >= static_cast<std::uint64_t>(fftSize)
        && (samplesSeen - static_cast<std::uint64_t>(fftSize)) % static_cast<std::uint64_t>(hopSize) == 0)
    {
        processFrame(pitchRatio, bloomAmount);
    }

    outputReadPosition = (outputReadPosition + 1) & outputQueueMask;
    return wetOutput;
}

void AuraAudioProcessor::SpectralChannel::transform(bool inverse) noexcept
{
    for (int index = 0; index < fftSize; ++index)
    {
        const auto reversed = static_cast<int>(bitReversed[static_cast<size_t>(index)]);
        if (reversed > index)
            std::swap(fftData[static_cast<size_t>(index)], fftData[static_cast<size_t>(reversed)]);
    }

    for (int size = 2; size <= fftSize; size <<= 1)
    {
        const auto halfSize = size / 2;
        const auto twiddleStep = fftSize / size;
        for (int start = 0; start < fftSize; start += size)
        {
            for (int offset = 0; offset < halfSize; ++offset)
            {
                auto twiddle = twiddles[static_cast<size_t>(offset * twiddleStep)];
                if (inverse)
                    twiddle.imag = -twiddle.imag;

                const auto evenIndex = start + offset;
                const auto oddIndex = evenIndex + halfSize;
                const auto even = fftData[static_cast<size_t>(evenIndex)];
                const auto oddValue = fftData[static_cast<size_t>(oddIndex)];
                const Complex odd{
                    oddValue.real * twiddle.real - oddValue.imag * twiddle.imag,
                    oddValue.real * twiddle.imag + oddValue.imag * twiddle.real
                };
                fftData[static_cast<size_t>(evenIndex)] = { even.real + odd.real, even.imag + odd.imag };
                fftData[static_cast<size_t>(oddIndex)] = { even.real - odd.real, even.imag - odd.imag };
            }
        }
    }

    if (inverse)
    {
        const auto scale = 1.0f / static_cast<float>(fftSize);
        for (auto& value : fftData)
        {
            value.real *= scale;
            value.imag *= scale;
        }
    }
}

void AuraAudioProcessor::SpectralChannel::processFrame(float pitchRatio, float bloomAmount) noexcept
{
    for (int index = 0; index < fftSize; ++index)
    {
        const auto inputIndex = (inputWritePosition + index) & fftMask;
        fftData[static_cast<size_t>(index)] = {
            inputRing[static_cast<size_t>(inputIndex)] * analysisWindow[static_cast<size_t>(index)], 0.0f
        };
    }

    transform(false);
    mappedSpectrum.fill(Complex{ 0.0f, 0.0f });

    for (int sourceBin = 0; sourceBin <= halfFftSize; ++sourceBin)
    {
        const auto source = fftData[static_cast<size_t>(sourceBin)];
        const auto sourceMagnitude = std::sqrt(source.real * source.real + source.imag * source.imag);
        const auto inputPhase = std::atan2(source.imag, source.real);
        const auto expectedAdvance = 2.0f * pi * static_cast<float>(sourceBin * hopSize)
                                     / static_cast<float>(fftSize);
        auto outputPhase = inputPhase;
        if (phaseInitialized)
        {
            const auto phaseDifference = inputPhase - previousInputPhase[static_cast<size_t>(sourceBin)]
                                         - expectedAdvance;
            const auto residual = std::remainder(phaseDifference, 2.0f * pi);
            synthesisPhase[static_cast<size_t>(sourceBin)] += (expectedAdvance + residual) * pitchRatio;
            outputPhase = synthesisPhase[static_cast<size_t>(sourceBin)];
        }
        else
        {
            synthesisPhase[static_cast<size_t>(sourceBin)] = inputPhase;
        }
        previousInputPhase[static_cast<size_t>(sourceBin)] = inputPhase;

        const auto targetPosition = static_cast<float>(sourceBin) * pitchRatio;
        if (targetPosition > static_cast<float>(halfFftSize))
            continue;

        const auto lowerBin = static_cast<int>(targetPosition);
        const auto fraction = targetPosition - static_cast<float>(lowerBin);
        const Complex pitchedSource{ sourceMagnitude * std::cos(outputPhase),
                                     sourceMagnitude * std::sin(outputPhase) };
        const auto lowerWeight = 1.0f - fraction;
        auto& lower = mappedSpectrum[static_cast<size_t>(lowerBin)];
        lower.real += pitchedSource.real * lowerWeight;
        lower.imag += pitchedSource.imag * lowerWeight;

        const auto upperBin = lowerBin + 1;
        if (upperBin <= halfFftSize)
        {
            auto& upper = mappedSpectrum[static_cast<size_t>(upperBin)];
            upper.real += pitchedSource.real * fraction;
            upper.imag += pitchedSource.imag * fraction;
        }
    }
    phaseInitialized = true;

    for (int bin = 0; bin <= halfFftSize; ++bin)
    {
        const auto& value = mappedSpectrum[static_cast<size_t>(bin)];
        magnitudes[static_cast<size_t>(bin)] = std::sqrt(value.real * value.real + value.imag * value.imag);
    }

    for (int bin = 0; bin <= halfFftSize; ++bin)
    {
        const auto centerMagnitude = magnitudes[static_cast<size_t>(bin)];
        const auto leftMagnitude = magnitudes[static_cast<size_t>(juce::jmax(0, bin - 1))];
        const auto rightMagnitude = magnitudes[static_cast<size_t>(juce::jmin(halfFftSize, bin + 1))];
        const auto softenedMagnitude = centerMagnitude * 0.5f + (leftMagnitude + rightMagnitude) * 0.25f;
        const auto targetMagnitude = centerMagnitude + (softenedMagnitude - centerMagnitude) * bloomAmount;
        auto& value = mappedSpectrum[static_cast<size_t>(bin)];
        if (centerMagnitude > 1.0e-12f)
        {
            const auto scale = targetMagnitude / centerMagnitude;
            value.real *= scale;
            value.imag *= scale;
        }
        else if (targetMagnitude > 1.0e-12f)
        {
            const auto leftBin = juce::jmax(0, bin - 1);
            const auto rightBin = juce::jmin(halfFftSize, bin + 1);
            const auto referenceBin = magnitudes[static_cast<size_t>(leftBin)]
                                      >= magnitudes[static_cast<size_t>(rightBin)] ? leftBin : rightBin;
            const auto reference = mappedSpectrum[static_cast<size_t>(referenceBin)];
            const auto referenceMagnitude = std::sqrt(reference.real * reference.real + reference.imag * reference.imag);
            if (referenceMagnitude > 1.0e-12f)
            {
                const auto scale = targetMagnitude / referenceMagnitude;
                value = { reference.real * scale, reference.imag * scale };
            }
        }
        fftData[static_cast<size_t>(bin)] = value;
    }

    fftData[0].imag = 0.0f;
    fftData[static_cast<size_t>(halfFftSize)].imag = 0.0f;
    for (int bin = 1; bin < halfFftSize; ++bin)
    {
        const auto value = fftData[static_cast<size_t>(bin)];
        fftData[static_cast<size_t>(fftSize - bin)] = { value.real, -value.imag };
    }

    transform(true);

    for (int index = 0; index < fftSize; ++index)
    {
        const auto queueIndex = (outputReadPosition + 1 + index) & outputQueueMask;
        outputQueue[static_cast<size_t>(queueIndex)] +=
            fftData[static_cast<size_t>(index)].real * synthesisWindow[static_cast<size_t>(index)];
        normalizationQueue[static_cast<size_t>(queueIndex)] +=
            analysisWindow[static_cast<size_t>(index)] * synthesisWindow[static_cast<size_t>(index)];
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AuraAudioProcessor();
}
