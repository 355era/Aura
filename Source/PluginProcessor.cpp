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
                                           juce::NormalisableRange<float>{ -1500.0f, 1500.0f, 0.1f },
                                           0.0f, "Hz"));
    layout.add(std::make_unique<Parameter>(juce::ParameterID{ "mix", 1 }, "Mix",
                                           juce::NormalisableRange<float>{ 0.0f, 100.0f, 0.1f },
                                           100.0f, "%"));
    layout.add(std::make_unique<Parameter>(juce::ParameterID{ "bloom", 1 }, "Bloom",
                                           juce::NormalisableRange<float>{ 0.0f, 100.0f, 0.1f },
                                           18.0f, "%"));
    layout.add(std::make_unique<Parameter>(juce::ParameterID{ "grain", 1 }, "Grain",
                                           juce::NormalisableRange<float>{ 0.0f, 100.0f, 0.1f },
                                           22.0f, "%"));
    layout.add(std::make_unique<Parameter>(juce::ParameterID{ "grainsize", 1 }, "Grain Size",
                                           juce::NormalisableRange<float>{ 25.0f, 240.0f, 1.0f },
                                           120.0f, "ms"));
    layout.add(std::make_unique<Parameter>(juce::ParameterID{ "density", 1 }, "Density",
                                           juce::NormalisableRange<float>{ 2.0f, 24.0f, 0.1f },
                                           12.0f, "gr/s"));
    layout.add(std::make_unique<Parameter>(juce::ParameterID{ "feedback", 1 }, "Feedback",
                                           juce::NormalisableRange<float>{ 0.0f, 75.0f, 0.1f },
                                           18.0f, "%"));
    return layout;
}

void AuraAudioProcessor::prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock)
{
    juce::ignoreUnused(maximumExpectedSamplesPerBlock);
    const auto safeSampleRate = juce::jmax(8000.0, sampleRate);
    channelProcessors.clear();
    channelProcessors.resize(static_cast<size_t>(getTotalNumInputChannels()));
    for (auto& channel : channelProcessors)
        channel.prepare(safeSampleRate);

    grainProcessors.clear();
    grainProcessors.resize(static_cast<size_t>(getTotalNumInputChannels()));
    for (auto& channel : grainProcessors)
        channel.prepare(safeSampleRate);

    const auto prepareSmoother = [this, safeSampleRate](juce::SmoothedValue<float>& smoother,
                                                   const juce::String& parameterID,
                                                   float fallback, float scale)
    {
        const auto* parameter = parameters.getRawParameterValue(parameterID);
        const auto initial = parameter != nullptr
                                 ? parameter->load(std::memory_order_relaxed) * scale
                                 : fallback;
        smoother.reset(safeSampleRate, 0.04);
        smoother.setCurrentAndTargetValue(initial);
    };
    prepareSmoother(shiftSmoother, "shift", 0.0f, 1.0f);
    prepareSmoother(mixSmoother, "mix", 1.0f, 0.01f);
    prepareSmoother(bloomSmoother, "bloom", 0.0f, 0.01f);
    prepareSmoother(grainMixSmoother, "grain", 0.0f, 0.01f);
    prepareSmoother(grainSizeSmoother, "grainsize", 120.0f, 1.0f);
    prepareSmoother(densitySmoother, "density", 12.0f, 1.0f);
    prepareSmoother(feedbackSmoother, "feedback", 0.0f, 0.01f);

    setLatencySamples(2048);
    inputMeter.store(0.0f, std::memory_order_relaxed);
}

void AuraAudioProcessor::releaseResources()
{
    channelProcessors.clear();
    grainProcessors.clear();
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
    const auto* grainParameter = parameters.getRawParameterValue("grain");
    const auto* grainSizeParameter = parameters.getRawParameterValue("grainsize");
    const auto* densityParameter = parameters.getRawParameterValue("density");
    const auto* feedbackParameter = parameters.getRawParameterValue("feedback");
    const auto shift = shiftParameter != nullptr ? shiftParameter->load(std::memory_order_relaxed) : 0.0f;
    const auto mix = mixParameter != nullptr
                         ? juce::jlimit(0.0f, 1.0f, mixParameter->load(std::memory_order_relaxed) * 0.01f)
                         : 1.0f;
    const auto bloom = bloomParameter != nullptr
                           ? juce::jlimit(0.0f, 1.0f, bloomParameter->load(std::memory_order_relaxed) * 0.01f)
                           : 0.0f;
    const auto grainMix = grainParameter != nullptr
                              ? juce::jlimit(0.0f, 1.0f, grainParameter->load(std::memory_order_relaxed) * 0.01f)
                              : 0.0f;
    const auto grainSize = grainSizeParameter != nullptr
                               ? grainSizeParameter->load(std::memory_order_relaxed) : 120.0f;
    const auto density = densityParameter != nullptr
                             ? densityParameter->load(std::memory_order_relaxed) : 12.0f;
    const auto feedback = feedbackParameter != nullptr
                              ? juce::jlimit(0.0f, 0.75f,
                                             feedbackParameter->load(std::memory_order_relaxed) * 0.01f)
                              : 0.0f;
    shiftSmoother.setTargetValue(shift);
    mixSmoother.setTargetValue(mix);
    bloomSmoother.setTargetValue(bloom);
    grainMixSmoother.setTargetValue(grainMix);
    grainSizeSmoother.setTargetValue(grainSize);
    densitySmoother.setTargetValue(density);
    feedbackSmoother.setTargetValue(feedback);

    const auto channelsToProcess = juce::jmin(buffer.getNumChannels(), static_cast<int>(channelProcessors.size()));
    double inputSumSquares = 0.0;
    for (int sampleIndex = 0; sampleIndex < buffer.getNumSamples(); ++sampleIndex)
    {
        const auto shiftHz = shiftSmoother.getNextValue();
        const auto currentMix = mixSmoother.getNextValue();
        const auto currentBloom = bloomSmoother.getNextValue();
        const auto currentGrainMix = grainMixSmoother.getNextValue();
        const auto currentGrainSize = grainSizeSmoother.getNextValue();
        const auto currentDensity = densitySmoother.getNextValue();
        const auto currentFeedback = feedbackSmoother.getNextValue();
        const auto grainPitchRatio = std::exp2(shiftHz / 1200.0f);

        for (int channelIndex = 0; channelIndex < channelsToProcess; ++channelIndex)
        {
            auto* samples = buffer.getWritePointer(channelIndex);
            auto& channel = channelProcessors[static_cast<size_t>(channelIndex)];
            const auto input = samples[sampleIndex];
            inputSumSquares += static_cast<double>(input) * static_cast<double>(input);
            float delayedDry = 0.0f;
            const auto spectralWet = channel.processSample(input, shiftHz, currentBloom, delayedDry);
            const auto grainWet = grainProcessors[static_cast<size_t>(channelIndex)].processSample(
                spectralWet, grainPitchRatio, currentGrainSize, currentDensity, currentFeedback);
            const auto processedWet = spectralWet + (grainWet - spectralWet) * currentGrainMix;
            const auto output = delayedDry + (processedWet - delayedDry) * currentMix;
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
    return 2.0;
}

void AuraAudioProcessor::getStateInformation(juce::MemoryBlock& destinationData)
{
    auto state = parameters.copyState();
    state.setProperty("auraStateVersion", 2, nullptr);
    if (const auto xml = state.createXml())
        copyXmlToBinary(*xml, destinationData);
}

void AuraAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        const auto restoredState = juce::ValueTree::fromXml(*xml);
        if (restoredState.isValid() && restoredState.getType() == parameters.state.getType())
        {
            // Older Aura projects stored the Shift dial as semitones. Convert that saved
            // control value to the new 440 Hz-referenced spectral-frequency offset.
            const auto stateVersion = static_cast<int>(restoredState.getProperty("auraStateVersion", 1));
            if (stateVersion < 2)
            {
                auto oldShift = restoredState.getChildWithProperty("id", "shift");
                if (oldShift.isValid())
                {
                    const auto semitones = static_cast<float>(oldShift.getProperty("value", 0.0f));
                    const auto offsetHz = (std::exp2(semitones / 12.0f) - 1.0f) * 440.0f;
                    oldShift.setProperty("value", juce::jlimit(-1500.0f, 1500.0f, offsetHz), nullptr);
                }
            }
            parameters.replaceState(restoredState);
        }
    }
}

AuraAudioProcessor::SpectralChannel::SpectralChannel()
{
    makeTables();
}

void AuraAudioProcessor::SpectralChannel::prepare(double sampleRate) noexcept
{
    currentSampleRate = juce::jmax(8000.0, sampleRate);
    reset();
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
    shiftPhaseRing.fill(0.0);
    dryDelay.fill(0.0f);
    outputQueue.fill(0.0f);
    normalizationQueue.fill(0.0f);
    inputWritePosition = 0;
    dryWritePosition = 0;
    outputReadPosition = 0;
    samplesSeen = 0;
    shiftOscillatorPhase = 0.0;
}

float AuraAudioProcessor::SpectralChannel::processSample(float input, float shiftHz,
                                                         float bloomAmount, float& delayedDry) noexcept
{
    delayedDry = dryDelay[static_cast<size_t>(dryWritePosition)];
    dryDelay[static_cast<size_t>(dryWritePosition)] = input;
    dryWritePosition = (dryWritePosition + 1) & fftMask;

    inputRing[static_cast<size_t>(inputWritePosition)] = input;
    shiftPhaseRing[static_cast<size_t>(inputWritePosition)] = shiftOscillatorPhase;
    shiftOscillatorPhase = std::remainder(
        shiftOscillatorPhase + 2.0 * juce::MathConstants<double>::pi
                                * static_cast<double>(shiftHz) / currentSampleRate,
        2.0 * juce::MathConstants<double>::pi);
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
        processFrame(shiftHz, bloomAmount);
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

void AuraAudioProcessor::SpectralChannel::processFrame(float shiftHz, float bloomAmount) noexcept
{
    for (int index = 0; index < fftSize; ++index)
    {
        const auto inputIndex = (inputWritePosition + index) & fftMask;
        fftData[static_cast<size_t>(index)] = {
            inputRing[static_cast<size_t>(inputIndex)] * analysisWindow[static_cast<size_t>(index)], 0.0f
        };
    }

    transform(false);
    const auto sampleRate = static_cast<float>(currentSampleRate);
    const auto nyquist = sampleRate * 0.5f;
    for (int bin = 0; bin <= halfFftSize; ++bin)
    {
        auto value = fftData[static_cast<size_t>(bin)];
        const auto sourceFrequency = static_cast<float>(bin) * sampleRate / static_cast<float>(fftSize);
        if (shiftHz > 0.0f && sourceFrequency + shiftHz > nyquist)
            value = { 0.0f, 0.0f };
        mappedSpectrum[static_cast<size_t>(bin)] = value;
        magnitudes[static_cast<size_t>(bin)] = std::sqrt(value.real * value.real + value.imag * value.imag);
    }

    for (int bin = 0; bin <= halfFftSize; ++bin)
    {
        const auto centerMagnitude = magnitudes[static_cast<size_t>(bin)];
        const auto leftMagnitude = magnitudes[static_cast<size_t>(juce::jmax(0, bin - 1))];
        const auto rightMagnitude = magnitudes[static_cast<size_t>(juce::jmin(halfFftSize, bin + 1))];
        const auto softenedMagnitude = centerMagnitude * 0.5f + (leftMagnitude + rightMagnitude) * 0.25f;
        const auto targetMagnitude = centerMagnitude + (softenedMagnitude - centerMagnitude) * bloomAmount;
        auto value = mappedSpectrum[static_cast<size_t>(bin)];
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
        const auto sourceFrequency = static_cast<float>(bin) * sampleRate / static_cast<float>(fftSize);
        if (shiftHz > 0.0f && sourceFrequency + shiftHz > nyquist)
            value = { 0.0f, 0.0f };
        if (bin > 0 && bin < halfFftSize)
        {
            value.real *= 2.0f;
            value.imag *= 2.0f;
        }
        fftData[static_cast<size_t>(bin)] = value;
        if (bin > 0 && bin < halfFftSize)
            fftData[static_cast<size_t>(fftSize - bin)] = { 0.0f, 0.0f };
    }

    fftData[0].imag = 0.0f;
    fftData[static_cast<size_t>(halfFftSize)].imag = 0.0f;
    transform(true);

    for (int index = 0; index < fftSize; ++index)
    {
        const auto queueIndex = (outputReadPosition + 1 + index) & outputQueueMask;
        const auto inputIndex = (inputWritePosition + index) & fftMask;
        const auto phase = static_cast<float>(shiftPhaseRing[static_cast<size_t>(inputIndex)]);
        const auto analyticSample = fftData[static_cast<size_t>(index)];
        const auto shiftedSample = analyticSample.real * std::cos(phase)
                                   - analyticSample.imag * std::sin(phase);
        outputQueue[static_cast<size_t>(queueIndex)] +=
            shiftedSample * synthesisWindow[static_cast<size_t>(index)];
        normalizationQueue[static_cast<size_t>(queueIndex)] +=
            analysisWindow[static_cast<size_t>(index)] * synthesisWindow[static_cast<size_t>(index)];
    }
}

void AuraAudioProcessor::GrainDelayChannel::prepare(double sampleRate)
{
    currentSampleRate = juce::jmax(8000.0, sampleRate);
    const auto bufferSize = static_cast<size_t>(std::ceil(currentSampleRate * 2.0)) + 8U;
    delayBuffer.assign(bufferSize, 0.0f);
    reset();
}

void AuraAudioProcessor::GrainDelayChannel::reset() noexcept
{
    std::fill(delayBuffer.begin(), delayBuffer.end(), 0.0f);
    voices.fill(GrainVoice{});
    writePosition = 0;
    samplesUntilNextGrain = 0;
    nextVoice = 0;
}

float AuraAudioProcessor::GrainDelayChannel::processSample(float input, float pitchRatio,
                                                           float sizeMilliseconds,
                                                           float grainsPerSecond,
                                                           float feedback) noexcept
{
    if (delayBuffer.empty())
        return input;

    const auto bufferSize = static_cast<int>(delayBuffer.size());
    const auto grainLength = juce::jlimit(
        32, bufferSize / 3,
        juce::roundToInt(static_cast<float>(currentSampleRate) *
                         juce::jlimit(25.0f, 240.0f, sizeMilliseconds) * 0.001f));

    if (samplesUntilNextGrain <= 0)
    {
        auto& voice = voices[nextVoice];
        voice.length = grainLength;
        voice.age = 0;
        voice.readStep = juce::jlimit(0.25f, 4.0f, pitchRatio);
        voice.readPosition = static_cast<double>(writePosition - grainLength);
        if (voice.readPosition < 0.0)
            voice.readPosition += static_cast<double>(bufferSize);
        voice.active = true;
        nextVoice = (nextVoice + 1U) % voiceCount;
        const auto safeDensity = juce::jlimit(2.0f, 24.0f, grainsPerSecond);
        samplesUntilNextGrain = juce::jmax(1, juce::roundToInt(
            static_cast<float>(currentSampleRate) / safeDensity));
    }
    --samplesUntilNextGrain;

    float grainSum = 0.0f;
    float windowSum = 0.0f;
    for (auto& voice : voices)
    {
        if (!voice.active)
            continue;
        if (voice.age >= voice.length)
        {
            voice.active = false;
            continue;
        }

        const auto phase = static_cast<float>(voice.age) /
                           static_cast<float>(juce::jmax(1, voice.length - 1));
        const auto window = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * phase);
        const auto readIndex = static_cast<int>(voice.readPosition);
        const auto nextIndex = readIndex + 1 == bufferSize ? 0 : readIndex + 1;
        const auto fraction = static_cast<float>(voice.readPosition - static_cast<double>(readIndex));
        const auto first = delayBuffer[static_cast<size_t>(readIndex)];
        const auto second = delayBuffer[static_cast<size_t>(nextIndex)];
        const auto sample = first + (second - first) * fraction;
        grainSum += sample * window;
        windowSum += window;

        voice.readPosition += static_cast<double>(voice.readStep);
        if (voice.readPosition >= static_cast<double>(bufferSize))
            voice.readPosition -= static_cast<double>(bufferSize);
        ++voice.age;
    }

    const auto grainOutput = windowSum > 1.0e-5f ? grainSum / windowSum : 0.0f;
    const auto boundedFeedback = juce::jlimit(0.0f, 0.75f, feedback);
    const auto feedbackOnly = std::tanh(grainOutput) * boundedFeedback;
    const auto writeSample = input + feedbackOnly;
    delayBuffer[static_cast<size_t>(writePosition)] = writeSample;
    if (++writePosition >= bufferSize)
        writePosition = 0;

    return grainOutput;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AuraAudioProcessor();
}
