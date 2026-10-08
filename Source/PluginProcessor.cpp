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
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "fft", 1 }, "FFT Size", juce::StringArray{ "2048", "4096", "8192" }, 0));
    layout.add(std::make_unique<Parameter>(juce::ParameterID{ "lowcut", 1 }, "Low Cut",
                                           juce::NormalisableRange<float>{ 20.0f, 1000.0f, 1.0f, 0.35f },
                                           20.0f, "Hz"));
    layout.add(std::make_unique<Parameter>(juce::ParameterID{ "highcut", 1 }, "High Cut",
                                           juce::NormalisableRange<float>{ 2000.0f, 20000.0f, 1.0f, 0.4f },
                                           20000.0f, "Hz"));
    layout.add(std::make_unique<Parameter>(juce::ParameterID{ "mix", 1 }, "Mix",
                                           juce::NormalisableRange<float>{ 0.0f, 100.0f, 0.1f },
                                           100.0f, "%"));
    layout.add(std::make_unique<Parameter>(juce::ParameterID{ "bloom", 1 }, "Bloom",
                                           juce::NormalisableRange<float>{ 0.0f, 100.0f, 0.1f },
                                           18.0f, "%"));
    layout.add(std::make_unique<Parameter>(juce::ParameterID{ "blur", 1 }, "Blur",
                                           juce::NormalisableRange<float>{ 0.0f, 100.0f, 0.1f },
                                           12.0f, "%"));
    layout.add(std::make_unique<Parameter>(juce::ParameterID{ "grain", 1 }, "Grain",
                                           juce::NormalisableRange<float>{ 0.0f, 100.0f, 0.1f },
                                           22.0f, "%"));
    layout.add(std::make_unique<Parameter>(juce::ParameterID{ "grainsize", 1 }, "Grain Size",
                                           juce::NormalisableRange<float>{ 25.0f, 240.0f, 1.0f },
                                           120.0f, "ms"));
    layout.add(std::make_unique<Parameter>(juce::ParameterID{ "grainpitch", 1 }, "Grain Pitch",
                                           juce::NormalisableRange<float>{ -24.0f, 24.0f, 0.01f },
                                           0.0f, "st"));
    layout.add(std::make_unique<Parameter>(juce::ParameterID{ "density", 1 }, "Density",
                                           juce::NormalisableRange<float>{ 2.0f, 24.0f, 0.1f },
                                           12.0f, "gr/s"));
    layout.add(std::make_unique<Parameter>(juce::ParameterID{ "feedback", 1 }, "Feedback",
                                           juce::NormalisableRange<float>{ 0.0f, 75.0f, 0.1f },
                                           18.0f, "%"));
    layout.add(std::make_unique<Parameter>(juce::ParameterID{ "tone", 1 }, "Tone",
                                           juce::NormalisableRange<float>{ 1000.0f, 20000.0f, 1.0f },
                                           18000.0f, "Hz"));
    layout.add(std::make_unique<Parameter>(juce::ParameterID{ "width", 1 }, "Width",
                                           juce::NormalisableRange<float>{ 0.0f, 200.0f, 0.1f },
                                           110.0f, "%"));
    layout.add(std::make_unique<Parameter>(juce::ParameterID{ "lforate", 1 }, "LFO Rate",
                                           juce::NormalisableRange<float>{ 0.05f, 20.0f, 0.01f, 0.4f },
                                           0.25f, "Hz"));
    layout.add(std::make_unique<Parameter>(juce::ParameterID{ "lfodepth", 1 }, "LFO Depth",
                                           juce::NormalisableRange<float>{ 0.0f, 100.0f, 0.1f },
                                           0.0f, "%"));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "lfoshape", 1 }, "LFO Shape",
        juce::StringArray{ "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Random Hold" }, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "lfotarget", 1 }, "LFO Target",
        juce::StringArray{ "Off", "Shift", "Bloom", "Blur", "Grain", "Pitch", "Size", "Density",
                           "Feedback", "Tone", "Width" }, 1));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "spectral_on", 1 }, "Spectral", true));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "grain_on", 1 }, "Grain", true));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "tone_on", 1 }, "Tone Filter", true));
    layout.add(std::make_unique<Parameter>(juce::ParameterID{ "output", 1 }, "Output",
                                           juce::NormalisableRange<float>{ -18.0f, 6.0f, 0.01f },
                                           0.0f, "dB"));
    return layout;
}

void AuraAudioProcessor::prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock)
{
    juce::ignoreUnused(maximumExpectedSamplesPerBlock);
    const auto safeSampleRate = juce::jmax(8000.0, sampleRate);
    if (const auto* fftParameter = parameters.getRawParameterValue("fft"))
        currentFftSize = 2048 << juce::jlimit(0, 2, juce::roundToInt(fftParameter->load()));
    else
        currentFftSize = 2048;
    channelProcessors.clear();
    channelProcessors.resize(static_cast<size_t>(getTotalNumInputChannels()));
    for (auto& channel : channelProcessors)
        channel.prepare(safeSampleRate, currentFftSize);

    grainProcessors.clear();
    grainProcessors.resize(static_cast<size_t>(getTotalNumInputChannels()));
    for (auto& channel : grainProcessors)
        channel.prepare(safeSampleRate);
    toneFilterState.assign(static_cast<size_t>(getTotalNumInputChannels()), 0.0f);

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
    prepareSmoother(blurSmoother, "blur", 0.0f, 0.01f);
    prepareSmoother(lowCutSmoother, "lowcut", 20.0f, 1.0f);
    prepareSmoother(highCutSmoother, "highcut", 20000.0f, 1.0f);
    prepareSmoother(grainMixSmoother, "grain", 0.0f, 0.01f);
    prepareSmoother(grainSizeSmoother, "grainsize", 120.0f, 1.0f);
    prepareSmoother(densitySmoother, "density", 12.0f, 1.0f);
    prepareSmoother(feedbackSmoother, "feedback", 0.0f, 0.01f);
    prepareSmoother(grainPitchSmoother, "grainpitch", 0.0f, 1.0f);
    prepareSmoother(toneSmoother, "tone", 18000.0f, 1.0f);
    prepareSmoother(widthSmoother, "width", 1.1f, 0.01f);
    prepareSmoother(lfoRateSmoother, "lforate", 0.25f, 1.0f);
    prepareSmoother(lfoDepthSmoother, "lfodepth", 0.0f, 0.01f);
    prepareSmoother(spectralEnabledSmoother, "spectral_on", 1.0f, 1.0f);
    prepareSmoother(grainEnabledSmoother, "grain_on", 1.0f, 1.0f);
    prepareSmoother(toneEnabledSmoother, "tone_on", 1.0f, 1.0f);
    prepareSmoother(outputGainSmoother, "output", 0.0f, 1.0f);

    setLatencySamples(currentFftSize);
    pendingLatencyUpdate.store(0, std::memory_order_relaxed);
    inputMeter.store(0.0f, std::memory_order_relaxed);
    lfoPhase = 0.0;
    lfoHeldRandom = 0.0f;
    lfoRandomState = 0x355e4a91U;
    lfoMeter.store(0.0f, std::memory_order_relaxed);
    lfoPhaseMeter.store(0.0f, std::memory_order_relaxed);
}

void AuraAudioProcessor::releaseResources()
{
    cancelPendingUpdate();
    channelProcessors.clear();
    grainProcessors.clear();
    toneFilterState.clear();
}

void AuraAudioProcessor::handleAsyncUpdate()
{
    const auto requestedLatency = pendingLatencyUpdate.exchange(0, std::memory_order_relaxed);
    if (requestedLatency > 0 && requestedLatency != getLatencySamples())
        setLatencySamples(requestedLatency);
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

    const auto* fftParameter = parameters.getRawParameterValue("fft");
    const auto requestedFftSize = fftParameter != nullptr
                                      ? 2048 << juce::jlimit(0, 2, juce::roundToInt(fftParameter->load()))
                                      : currentFftSize;
    if (requestedFftSize != currentFftSize)
    {
        for (auto& channel : channelProcessors)
            channel.setFftSize(requestedFftSize);
        currentFftSize = requestedFftSize;
        pendingLatencyUpdate.store(currentFftSize, std::memory_order_relaxed);
        triggerAsyncUpdate();
    }

    const auto* shiftParameter = parameters.getRawParameterValue("shift");
    const auto* mixParameter = parameters.getRawParameterValue("mix");
    const auto* bloomParameter = parameters.getRawParameterValue("bloom");
    const auto* blurParameter = parameters.getRawParameterValue("blur");
    const auto* grainParameter = parameters.getRawParameterValue("grain");
    const auto* grainSizeParameter = parameters.getRawParameterValue("grainsize");
    const auto* densityParameter = parameters.getRawParameterValue("density");
    const auto* feedbackParameter = parameters.getRawParameterValue("feedback");
    const auto* toneParameter = parameters.getRawParameterValue("tone");
    const auto* widthParameter = parameters.getRawParameterValue("width");
    const auto* grainPitchParameter = parameters.getRawParameterValue("grainpitch");
    const auto* lfoRateParameter = parameters.getRawParameterValue("lforate");
    const auto* lfoDepthParameter = parameters.getRawParameterValue("lfodepth");
    const auto* lfoShapeParameter = parameters.getRawParameterValue("lfoshape");
    const auto* lfoTargetParameter = parameters.getRawParameterValue("lfotarget");
    const auto* lowCutParameter = parameters.getRawParameterValue("lowcut");
    const auto* highCutParameter = parameters.getRawParameterValue("highcut");
    const auto* spectralEnabledParameter = parameters.getRawParameterValue("spectral_on");
    const auto* grainEnabledParameter = parameters.getRawParameterValue("grain_on");
    const auto* toneEnabledParameter = parameters.getRawParameterValue("tone_on");
    const auto* outputParameter = parameters.getRawParameterValue("output");
    const auto shift = shiftParameter != nullptr ? shiftParameter->load(std::memory_order_relaxed) : 0.0f;
    const auto mix = mixParameter != nullptr
                         ? juce::jlimit(0.0f, 1.0f, mixParameter->load(std::memory_order_relaxed) * 0.01f)
                         : 1.0f;
    const auto bloom = bloomParameter != nullptr
                           ? juce::jlimit(0.0f, 1.0f, bloomParameter->load(std::memory_order_relaxed) * 0.01f)
                           : 0.0f;
    const auto blur = blurParameter != nullptr
                          ? juce::jlimit(0.0f, 1.0f, blurParameter->load(std::memory_order_relaxed) * 0.01f)
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
    const auto tone = toneParameter != nullptr
                          ? juce::jlimit(1000.0f, 20000.0f, toneParameter->load(std::memory_order_relaxed))
                          : 18000.0f;
    const auto lowCut = lowCutParameter != nullptr
                            ? juce::jlimit(20.0f, 1000.0f, lowCutParameter->load(std::memory_order_relaxed))
                            : 20.0f;
    const auto highCut = highCutParameter != nullptr
                             ? juce::jlimit(2000.0f, 20000.0f, highCutParameter->load(std::memory_order_relaxed))
                             : 20000.0f;
    const auto width = widthParameter != nullptr
                           ? juce::jlimit(0.0f, 2.0f, widthParameter->load(std::memory_order_relaxed) * 0.01f)
                           : 1.1f;
    const auto grainPitch = grainPitchParameter != nullptr
                                ? juce::jlimit(-24.0f, 24.0f,
                                              grainPitchParameter->load(std::memory_order_relaxed))
                                : 0.0f;
    const auto lfoRate = lfoRateParameter != nullptr
                             ? juce::jlimit(0.05f, 20.0f, lfoRateParameter->load(std::memory_order_relaxed))
                             : 0.25f;
    const auto lfoDepth = lfoDepthParameter != nullptr
                              ? juce::jlimit(0.0f, 1.0f,
                                             lfoDepthParameter->load(std::memory_order_relaxed) * 0.01f)
                              : 0.0f;
    const auto lfoShape = lfoShapeParameter != nullptr
                              ? juce::roundToInt(lfoShapeParameter->load(std::memory_order_relaxed)) : 0;
    const auto lfoTarget = lfoTargetParameter != nullptr
                               ? juce::roundToInt(lfoTargetParameter->load(std::memory_order_relaxed)) : 0;
    const auto spectralEnabled = spectralEnabledParameter != nullptr
                                     ? spectralEnabledParameter->load(std::memory_order_relaxed) : 1.0f;
    const auto grainEnabled = grainEnabledParameter != nullptr
                                  ? grainEnabledParameter->load(std::memory_order_relaxed) : 1.0f;
    const auto toneEnabled = toneEnabledParameter != nullptr
                                 ? toneEnabledParameter->load(std::memory_order_relaxed) : 1.0f;
    const auto outputDb = outputParameter != nullptr
                              ? juce::jlimit(-18.0f, 6.0f, outputParameter->load(std::memory_order_relaxed))
                              : 0.0f;
    shiftSmoother.setTargetValue(shift);
    mixSmoother.setTargetValue(mix);
    bloomSmoother.setTargetValue(bloom);
    blurSmoother.setTargetValue(blur);
    lowCutSmoother.setTargetValue(lowCut);
    highCutSmoother.setTargetValue(highCut);
    grainMixSmoother.setTargetValue(grainMix);
    grainSizeSmoother.setTargetValue(grainSize);
    densitySmoother.setTargetValue(density);
    feedbackSmoother.setTargetValue(feedback);
    toneSmoother.setTargetValue(tone);
    widthSmoother.setTargetValue(width);
    grainPitchSmoother.setTargetValue(grainPitch);
    lfoRateSmoother.setTargetValue(lfoRate);
    lfoDepthSmoother.setTargetValue(lfoDepth);
    spectralEnabledSmoother.setTargetValue(spectralEnabled);
    grainEnabledSmoother.setTargetValue(grainEnabled);
    toneEnabledSmoother.setTargetValue(toneEnabled);
    outputGainSmoother.setTargetValue(outputDb);

    const auto channelsToProcess = juce::jmin(buffer.getNumChannels(), static_cast<int>(channelProcessors.size()));
    double inputSumSquares = 0.0;
    for (int sampleIndex = 0; sampleIndex < buffer.getNumSamples(); ++sampleIndex)
    {
        auto shiftHz = shiftSmoother.getNextValue();
        const auto currentMix = mixSmoother.getNextValue();
        auto currentBloom = bloomSmoother.getNextValue();
        auto currentBlur = blurSmoother.getNextValue();
        const auto currentLowCut = lowCutSmoother.getNextValue();
        const auto currentHighCut = highCutSmoother.getNextValue();
        auto currentGrainMix = grainMixSmoother.getNextValue();
        const auto currentGrainSize = grainSizeSmoother.getNextValue();
        const auto currentDensity = densitySmoother.getNextValue();
        const auto currentFeedback = feedbackSmoother.getNextValue();
        const auto currentGrainPitch = grainPitchSmoother.getNextValue();
        const auto currentTone = toneSmoother.getNextValue();
        auto currentWidth = widthSmoother.getNextValue();
        const auto currentLfoRate = lfoRateSmoother.getNextValue();
        const auto currentLfoDepth = lfoDepthSmoother.getNextValue();
        const auto currentSpectralEnabled = spectralEnabledSmoother.getNextValue();
        const auto currentGrainEnabled = grainEnabledSmoother.getNextValue();
        const auto currentToneEnabled = toneEnabledSmoother.getNextValue();
        const auto currentOutputDb = outputGainSmoother.getNextValue();

        float currentLfo = 0.0f;
        switch (lfoShape)
        {
            case 0: currentLfo = std::sin(static_cast<float>(juce::MathConstants<double>::twoPi * lfoPhase)); break;
            case 1: currentLfo = 1.0f - 4.0f * std::abs(static_cast<float>(lfoPhase) - 0.5f); break;
            case 2: currentLfo = static_cast<float>(lfoPhase * 2.0 - 1.0); break;
            case 3: currentLfo = static_cast<float>(1.0 - lfoPhase * 2.0); break;
            case 4: currentLfo = lfoPhase < 0.5 ? 1.0f : -1.0f; break;
            case 5: currentLfo = lfoHeldRandom; break;
            default: currentLfo = 0.0f; break;
        }
        lfoPhase += static_cast<double>(currentLfoRate) / juce::jmax(8000.0, getSampleRate());
        if (lfoPhase >= 1.0)
        {
            lfoPhase -= 1.0;
            lfoRandomState ^= lfoRandomState << 13U;
            lfoRandomState ^= lfoRandomState >> 17U;
            lfoRandomState ^= lfoRandomState << 5U;
            lfoHeldRandom = static_cast<float>(lfoRandomState & 0x00ffffffU) / 8388607.5f - 1.0f;
        }
        lfoMeter.store(currentLfo, std::memory_order_relaxed);
        lfoPhaseMeter.store(static_cast<float>(lfoPhase), std::memory_order_relaxed);

        const auto modulation = lfoTarget > 0 && lfoTarget <= 10 ? currentLfo * currentLfoDepth : 0.0f;
        switch (lfoTarget)
        {
            case 1: shiftHz += modulation * 1500.0f; break;
            case 2: currentBloom = juce::jlimit(0.0f, 1.0f, currentBloom + modulation * 0.5f); break;
            case 3: currentBlur = juce::jlimit(0.0f, 1.0f, currentBlur + modulation * 0.5f); break;
            case 4: currentGrainMix = juce::jlimit(0.0f, 1.0f, currentGrainMix + modulation * 0.5f); break;
            case 5: break;
            case 6: break;
            case 7: break;
            case 8: break;
            case 9: break;
            case 10: currentWidth = juce::jlimit(0.0f, 2.0f, currentWidth + modulation); break;
            default: break;
        }
        shiftHz = juce::jlimit(-1500.0f, 1500.0f, shiftHz);
        auto modulatedGrainPitch = currentGrainPitch;
        auto modulatedGrainSize = currentGrainSize;
        auto modulatedDensity = currentDensity;
        auto modulatedFeedback = currentFeedback;
        auto modulatedTone = currentTone;
        switch (lfoTarget)
        {
            case 5: modulatedGrainPitch += modulation * 24.0f; break;
            case 6: modulatedGrainSize += modulation * 107.5f; break;
            case 7: modulatedDensity += modulation * 11.0f; break;
            case 8: modulatedFeedback += modulation * 0.375f; break;
            case 9: modulatedTone += modulation * 9500.0f; break;
            default: break;
        }
        const auto grainPitchRatio = std::exp2(juce::jlimit(-24.0f, 24.0f, modulatedGrainPitch) / 12.0f);
        const auto modulatedToneCoefficient = std::exp(-juce::MathConstants<float>::twoPi
                                                        * juce::jlimit(1000.0f, 20000.0f, modulatedTone)
                                                        / static_cast<float>(juce::jmax(8000.0, getSampleRate())));
        std::array<float, 2> delayedDry{};
        std::array<float, 2> processedWet{};

        for (int channelIndex = 0; channelIndex < channelsToProcess; ++channelIndex)
        {
            auto* samples = buffer.getWritePointer(channelIndex);
            auto& channel = channelProcessors[static_cast<size_t>(channelIndex)];
            const auto input = samples[sampleIndex];
            inputSumSquares += static_cast<double>(input) * static_cast<double>(input);
            const auto channelPosition = static_cast<size_t>(channelIndex);
            const auto spectralOutput = channel.processSample(input, shiftHz, currentBloom, currentBlur,
                                                               currentLowCut, currentHighCut,
                                                               delayedDry[channelPosition]);
            const auto spectralWet = delayedDry[channelPosition]
                                     + (spectralOutput - delayedDry[channelPosition]) * currentSpectralEnabled;
            const auto grainWet = grainProcessors[static_cast<size_t>(channelIndex)].processSample(
                spectralWet, grainPitchRatio, modulatedGrainSize, modulatedDensity, modulatedFeedback);
            const auto granularWet = spectralWet
                                     + (grainWet - spectralWet) * currentGrainMix * currentGrainEnabled;
            auto& filterState = toneFilterState[channelPosition];
            filterState = granularWet * (1.0f - modulatedToneCoefficient)
                          + filterState * modulatedToneCoefficient;
            processedWet[channelPosition] = granularWet + (filterState - granularWet) * currentToneEnabled;
        }

        if (channelsToProcess == 2)
        {
            const auto mid = (processedWet[0] + processedWet[1]) * 0.5f;
            const auto side = (processedWet[0] - processedWet[1]) * 0.5f * currentWidth;
            processedWet[0] = mid + side;
            processedWet[1] = mid - side;
        }

        for (int channelIndex = 0; channelIndex < channelsToProcess; ++channelIndex)
        {
            auto* samples = buffer.getWritePointer(channelIndex);
            const auto channelPosition = static_cast<size_t>(channelIndex);
            const auto mixed = delayedDry[channelPosition]
                               + (processedWet[channelPosition] - delayedDry[channelPosition]) * currentMix;
            samples[sampleIndex] = mixed * juce::Decibels::decibelsToGain(currentOutputDb);
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
    state.setProperty("auraStateVersion", 3, nullptr);
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

void AuraAudioProcessor::SpectralChannel::prepare(double sampleRate, int requestedFftSize) noexcept
{
    currentSampleRate = juce::jmax(8000.0, sampleRate);
    setFftSize(requestedFftSize);
}

void AuraAudioProcessor::SpectralChannel::setFftSize(int requestedFftSize) noexcept
{
    const auto safeSize = requestedFftSize >= 8192 ? 8192 : requestedFftSize >= 4096 ? 4096 : 2048;
    if (activeFftSize == safeSize)
        return;

    activeFftSize = safeSize;
    activeHopSize = activeFftSize / 4;
    activeHalfFftSize = activeFftSize / 2;
    activeFftMask = activeFftSize - 1;
    makeTables();
    inputRing.fill(0.0f);
    shiftPhaseRing.fill(0.0);
    outputQueue.fill(0.0f);
    normalizationQueue.fill(0.0f);
    inputWritePosition = 0;
    outputReadPosition = 0;
    samplesSeen = 0;
    shiftOscillatorPhase = 0.0;
}

void AuraAudioProcessor::SpectralChannel::makeTables() noexcept
{
    for (int index = 0; index < activeFftSize; ++index)
    {
        const auto phase = 2.0f * pi * (static_cast<float>(index) + 0.5f) / static_cast<float>(activeFftSize);
        analysisWindow[static_cast<size_t>(index)] = std::sqrt(0.5f - 0.5f * std::cos(phase));
    }

    for (int index = 0; index < activeHopSize; ++index)
    {
        float overlapPower = 0.0f;
        for (int overlap = 0; overlap < 4; ++overlap)
        {
            const auto windowIndex = index + overlap * activeHopSize;
            const auto window = analysisWindow[static_cast<size_t>(windowIndex)];
            overlapPower += window * window;
        }
        for (int overlap = 0; overlap < 4; ++overlap)
        {
            const auto windowIndex = index + overlap * activeHopSize;
            synthesisWindow[static_cast<size_t>(windowIndex)] =
                analysisWindow[static_cast<size_t>(windowIndex)] / juce::jmax(overlapPower, 1.0e-8f);
        }
    }

    for (int index = 0; index < activeHalfFftSize; ++index)
    {
        const auto phase = -2.0f * pi * static_cast<float>(index) / static_cast<float>(activeFftSize);
        twiddles[static_cast<size_t>(index)] = { std::cos(phase), std::sin(phase) };
    }

    int numberOfBits = 0;
    for (auto size = activeFftSize; size > 1; size >>= 1)
        ++numberOfBits;
    for (int index = 0; index < activeFftSize; ++index)
    {
        auto value = static_cast<unsigned int>(index);
        unsigned int reversed = 0;
        for (int bit = 0; bit < numberOfBits; ++bit)
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
                                                         float bloomAmount, float blurAmount,
                                                         float lowCutHz, float highCutHz,
                                                         float& delayedDry) noexcept
{
    const auto dryReadPosition = (dryWritePosition - activeFftSize) & inputMask;
    delayedDry = dryDelay[static_cast<size_t>(dryReadPosition)];
    dryDelay[static_cast<size_t>(dryWritePosition)] = input;
    dryWritePosition = (dryWritePosition + 1) & inputMask;

    inputRing[static_cast<size_t>(inputWritePosition)] = input;
    shiftPhaseRing[static_cast<size_t>(inputWritePosition)] = shiftOscillatorPhase;
    shiftOscillatorPhase = std::remainder(
        shiftOscillatorPhase + 2.0 * juce::MathConstants<double>::pi
                                * static_cast<double>(shiftHz) / currentSampleRate,
        2.0 * juce::MathConstants<double>::pi);
    inputWritePosition = (inputWritePosition + 1) & inputMask;

    const auto queueIndex = static_cast<size_t>(outputReadPosition);
    const auto normalization = normalizationQueue[queueIndex];
    const auto wetOutput = normalization > 1.0e-8f ? outputQueue[queueIndex] / normalization : 0.0f;
    outputQueue[queueIndex] = 0.0f;
    normalizationQueue[queueIndex] = 0.0f;
    ++samplesSeen;

    if (samplesSeen >= static_cast<std::uint64_t>(activeFftSize)
        && (samplesSeen - static_cast<std::uint64_t>(activeFftSize))
               % static_cast<std::uint64_t>(activeHopSize) == 0)
    {
        processFrame(shiftHz, bloomAmount, blurAmount, lowCutHz, highCutHz);
    }

    outputReadPosition = (outputReadPosition + 1) & outputQueueMask;
    return wetOutput;
}

void AuraAudioProcessor::SpectralChannel::transform(bool inverse) noexcept
{
    for (int index = 0; index < activeFftSize; ++index)
    {
        const auto reversed = static_cast<int>(bitReversed[static_cast<size_t>(index)]);
        if (reversed > index)
            std::swap(fftData[static_cast<size_t>(index)], fftData[static_cast<size_t>(reversed)]);
    }

    for (int size = 2; size <= activeFftSize; size <<= 1)
    {
        const auto halfSize = size / 2;
        const auto twiddleStep = activeFftSize / size;
        for (int start = 0; start < activeFftSize; start += size)
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
        const auto scale = 1.0f / static_cast<float>(activeFftSize);
        for (int index = 0; index < activeFftSize; ++index)
        {
            auto& value = fftData[static_cast<size_t>(index)];
            value.real *= scale;
            value.imag *= scale;
        }
    }
}

void AuraAudioProcessor::SpectralChannel::processFrame(float shiftHz, float bloomAmount,
                                                        float blurAmount, float lowCutHz,
                                                        float highCutHz) noexcept
{
    const auto frameStart = (inputWritePosition - activeFftSize) & inputMask;
    for (int index = 0; index < activeFftSize; ++index)
    {
        const auto inputIndex = (frameStart + index) & inputMask;
        fftData[static_cast<size_t>(index)] = {
            inputRing[static_cast<size_t>(inputIndex)] * analysisWindow[static_cast<size_t>(index)], 0.0f
        };
    }

    transform(false);
    const auto sampleRate = static_cast<float>(currentSampleRate);
    const auto nyquist = sampleRate * 0.5f;
    const auto effectiveHighCut = juce::jmin(highCutHz, nyquist);
    for (int bin = 0; bin <= activeHalfFftSize; ++bin)
    {
        auto value = fftData[static_cast<size_t>(bin)];
        const auto sourceFrequency = static_cast<float>(bin) * sampleRate / static_cast<float>(activeFftSize);
        if (sourceFrequency < lowCutHz || sourceFrequency > effectiveHighCut
            || (shiftHz > 0.0f && sourceFrequency + shiftHz > nyquist))
            value = { 0.0f, 0.0f };
        mappedSpectrum[static_cast<size_t>(bin)] = value;
        magnitudes[static_cast<size_t>(bin)] = std::sqrt(value.real * value.real + value.imag * value.imag);
    }

    const auto blurRadius = juce::roundToInt(juce::jlimit(0.0f, 1.0f, blurAmount) * 18.0f);
    if (blurRadius == 0)
    {
        for (int bin = 0; bin <= activeHalfFftSize; ++bin)
            blurredMagnitudes[static_cast<size_t>(bin)] = magnitudes[static_cast<size_t>(bin)];
    }
    else
    {
        for (int bin = 0; bin <= activeHalfFftSize; ++bin)
        {
            float weightedMagnitude = 0.0f;
            float weightSum = 0.0f;
            for (int offset = -blurRadius; offset <= blurRadius; ++offset)
            {
                const auto sourceBin = bin + offset;
                if (sourceBin < 0 || sourceBin > activeHalfFftSize)
                    continue;
                const auto weight = 1.0f - static_cast<float>(std::abs(offset))
                                                  / static_cast<float>(blurRadius + 1);
                weightedMagnitude += magnitudes[static_cast<size_t>(sourceBin)] * weight;
                weightSum += weight;
            }
            blurredMagnitudes[static_cast<size_t>(bin)] =
                weightSum > 0.0f ? weightedMagnitude / weightSum : magnitudes[static_cast<size_t>(bin)];
        }
    }

    for (int bin = 0; bin <= activeHalfFftSize; ++bin)
    {
        const auto centerMagnitude = magnitudes[static_cast<size_t>(bin)];
        const auto blurredMagnitude = blurredMagnitudes[static_cast<size_t>(bin)];
        const auto baseMagnitude = centerMagnitude + (blurredMagnitude - centerMagnitude) * blurAmount;
        const auto leftMagnitude = blurredMagnitudes[static_cast<size_t>(juce::jmax(0, bin - 1))];
        const auto rightMagnitude = blurredMagnitudes[static_cast<size_t>(juce::jmin(activeHalfFftSize, bin + 1))];
        const auto softenedMagnitude = baseMagnitude * 0.5f + (leftMagnitude + rightMagnitude) * 0.25f;
        const auto targetMagnitude = baseMagnitude + (softenedMagnitude - baseMagnitude) * bloomAmount;
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
            const auto rightBin = juce::jmin(activeHalfFftSize, bin + 1);
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
        const auto sourceFrequency = static_cast<float>(bin) * sampleRate / static_cast<float>(activeFftSize);
        if (sourceFrequency < lowCutHz || sourceFrequency > effectiveHighCut
            || (shiftHz > 0.0f && sourceFrequency + shiftHz > nyquist))
            value = { 0.0f, 0.0f };
        if (bin > 0 && bin < activeHalfFftSize)
        {
            value.real *= 2.0f;
            value.imag *= 2.0f;
        }
        fftData[static_cast<size_t>(bin)] = value;
        if (bin > 0 && bin < activeHalfFftSize)
            fftData[static_cast<size_t>(activeFftSize - bin)] = { 0.0f, 0.0f };
    }

    fftData[0].imag = 0.0f;
    fftData[static_cast<size_t>(activeHalfFftSize)].imag = 0.0f;
    transform(true);

    for (int index = 0; index < activeFftSize; ++index)
    {
        const auto queueIndex = (outputReadPosition + 1 + index) & outputQueueMask;
        const auto inputIndex = (frameStart + index) & inputMask;
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
