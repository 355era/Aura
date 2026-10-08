#include "PluginEditor.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
const juce::Colour backgroundColour{ 0xff0b0c0e };
const juce::Colour panelColour{ 0xff151719 };
const juce::Colour softLineColour{ 0xff3c4143 };
const juce::Colour leafColour{ 0xffc5c9c6 };
const juce::Colour brightLeafColour{ 0xfff2f1ed };
const juce::Colour aquaColour{ 0xfff06a55 };
const juce::Colour mutedTextColour{ 0xff8f9494 };
const juce::Colour warmAccentColour{ 0xfff04f3b };
const juce::Colour lilyColour{ 0xfff04f3b };
const juce::Colour lilyLightColour{ 0xffffb1a2 };
const juce::Colour cardColour{ 0xffecebe6 };
const juce::Colour cardTextColour{ 0xff25292a };

juce::Font makeAuraFont(float height, bool bold = false)
{
    return juce::Font(juce::Font::getDefaultSansSerifFontName(), height,
                      bold ? juce::Font::bold : juce::Font::plain);
}

struct FactoryPreset
{
    const char* name;
    std::array<float, 22> values;
};

constexpr std::array<FactoryPreset, 5> factoryPresets{{
    { "Botanical Init", { 0.0f, 100.0f, 18.0f, 12.0f, 20.0f, 20000.0f, 22.0f, 120.0f, 0.0f,
                          12.0f, 18.0f, 18000.0f, 110.0f, 0.25f, 0.0f, 0.0f, 1.0f,
                          1.0f, 1.0f, 1.0f, 0.0f, 0.0f } },
    { "Leaf Veil", { 74.0f, 82.0f, 24.0f, 16.0f, 40.0f, 18000.0f, 26.0f, 155.0f, 0.0f,
                     10.0f, 14.0f, 15500.0f, 120.0f, 0.18f, 0.12f, 0.0f, 1.0f,
                     1.0f, 1.0f, 1.0f, -1.0f, 0.0f } },
    { "Pollen Drift", { 185.0f, 78.0f, 34.0f, 28.0f, 90.0f, 16500.0f, 38.0f, 92.0f, 5.0f,
                        18.0f, 24.0f, 12500.0f, 135.0f, 0.32f, 0.22f, 1.0f, 4.0f,
                        1.0f, 1.0f, 1.0f, -1.0f, 1.0f } },
    { "Glass Orchid", { -245.0f, 68.0f, 14.0f, 36.0f, 250.0f, 18000.0f, 44.0f, 72.0f, -7.0f,
                        20.0f, 32.0f, 16500.0f, 125.0f, 0.12f, 0.18f, 2.0f, 1.0f,
                        1.0f, 1.0f, 1.0f, -2.0f, 0.0f } },
    { "Rain Memory", { 38.0f, 90.0f, 42.0f, 48.0f, 45.0f, 9500.0f, 52.0f, 215.0f, 0.0f,
                       7.0f, 48.0f, 9000.0f, 145.0f, 0.08f, 0.3f, 5.0f, 8.0f,
                       1.0f, 1.0f, 1.0f, -3.0f, 2.0f } }
}};

constexpr std::array<const char*, 22> presetParameterIDs{
    "shift", "mix", "bloom", "blur", "lowcut", "highcut", "grain", "grainsize", "grainpitch",
    "density", "feedback", "tone", "width", "lforate", "lfodepth", "lfoshape", "lfotarget",
    "spectral_on", "grain_on", "tone_on", "output", "fft"
};
constexpr int userPresetStartID = 100;
}

void AuraLookAndFeel::drawRotarySlider(juce::Graphics& graphics, int x, int y, int width, int height,
                                       float sliderPosition, float startAngle, float endAngle,
                                       juce::Slider& slider)
{
    juce::ignoreUnused(slider);
    const auto centreX = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
    const auto centreY = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
    const auto radius = juce::jmin(static_cast<float>(width), static_cast<float>(height)) * 0.39f;
    const auto arcThickness = juce::jmax(2.5f, radius * 0.075f);

    juce::Path track;
    track.addCentredArc(centreX, centreY, radius, radius, 0.0f, startAngle, endAngle, true);
    graphics.setColour(softLineColour.withAlpha(0.95f));
    graphics.strokePath(track, juce::PathStrokeType(arcThickness, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));

    const auto angle = startAngle + sliderPosition * (endAngle - startAngle);
    juce::Path activeArc;
    activeArc.addCentredArc(centreX, centreY, radius, radius, 0.0f, startAngle, angle, true);
    graphics.setColour(slider.isMouseOverOrDragging() ? warmAccentColour : aquaColour);
    graphics.strokePath(activeArc, juce::PathStrokeType(arcThickness, juce::PathStrokeType::curved,
                                                         juce::PathStrokeType::rounded));

    for (int tick = 0; tick <= 12; ++tick)
    {
        const auto tickAngle = startAngle + static_cast<float>(tick) / 12.0f * (endAngle - startAngle);
        const auto inner = radius + radius * 0.075f;
        const auto outer = radius + radius * (tick % 3 == 0 ? 0.15f : 0.12f);
        graphics.setColour(leafColour.withAlpha(tick % 3 == 0 ? 0.65f : 0.38f));
        graphics.drawLine(centreX + std::cos(tickAngle) * inner,
                          centreY + std::sin(tickAngle) * inner,
                          centreX + std::cos(tickAngle) * outer,
                          centreY + std::sin(tickAngle) * outer,
                          juce::jmax(0.65f, radius * (tick % 3 == 0 ? 0.018f : 0.013f)));
    }

    const auto innerRadius = radius * 0.74f;
    graphics.setGradientFill(juce::ColourGradient(juce::Colour{ 0xfff8f7f2 },
                                                   centreX - innerRadius, centreY - innerRadius,
                                                   juce::Colour{ 0xffd5d7d2 },
                                                   centreX + innerRadius, centreY + innerRadius, false));
    graphics.fillEllipse(centreX - innerRadius, centreY - innerRadius,
                         innerRadius * 2.0f, innerRadius * 2.0f);
    graphics.setColour(softLineColour.withAlpha(0.34f));
    graphics.drawEllipse(centreX - innerRadius, centreY - innerRadius,
                         innerRadius * 2.0f, innerRadius * 2.0f, 1.0f);

    const auto pointerLength = radius * 0.56f;
    juce::Path pointer;
    pointer.startNewSubPath(centreX, centreY);
    pointer.lineTo(centreX + std::cos(angle) * pointerLength,
                   centreY + std::sin(angle) * pointerLength);
    graphics.setColour(cardTextColour);
    graphics.strokePath(pointer, juce::PathStrokeType(juce::jmax(1.2f, radius * 0.035f), juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
    graphics.setColour(warmAccentColour);
    graphics.fillEllipse(centreX - 3.0f, centreY - 3.0f, 6.0f, 6.0f);
}

AuraDial::AuraDial(juce::AudioProcessorValueTreeState& parameters, AuraLookAndFeel& lookAndFeel,
                   const juce::String& parameterID, const juce::String& title,
                   const juce::String& helper, double defaultValue,
                   const juce::String& displayUnits)
    : units(displayUnits)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 0.75f,
                               juce::MathConstants<float>::pi * 2.25f, true);
    slider.setLookAndFeel(&lookAndFeel);
    slider.setScrollWheelEnabled(false);
    slider.setDoubleClickReturnValue(true, defaultValue);
    addAndMakeVisible(slider);

    titleLabel.setText(title, juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, cardTextColour);
    titleLabel.setFont(makeAuraFont(12.0f, true));
    addAndMakeVisible(titleLabel);

    valueLabel.setJustificationType(juce::Justification::centred);
    valueLabel.setColour(juce::Label::textColourId, cardTextColour);
    valueLabel.setFont(makeAuraFont(16.0f, true));
    addAndMakeVisible(valueLabel);

    helperLabel.setText(helper, juce::dontSendNotification);
    helperLabel.setJustificationType(juce::Justification::centred);
    helperLabel.setColour(juce::Label::textColourId, cardTextColour.withAlpha(0.64f));
    helperLabel.setFont(makeAuraFont(9.5f));
    addAndMakeVisible(helperLabel);
    setTooltip(title + ": " + helper);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        parameters, parameterID, slider);
    refreshValue();
}

void AuraDial::resized()
{
    auto bounds = getLocalBounds();
    titleLabel.setBounds(bounds.removeFromTop(juce::roundToInt(20.0f * uiScale)));
    slider.setBounds(bounds.removeFromTop(juce::roundToInt(86.0f * uiScale))
                         .reduced(juce::roundToInt(4.0f * uiScale), 0));
    valueLabel.setBounds(bounds.removeFromTop(juce::roundToInt(23.0f * uiScale)));
    helperLabel.setBounds(bounds.removeFromTop(juce::roundToInt(17.0f * uiScale)));
}

void AuraDial::setScale(float scale)
{
    uiScale = juce::jlimit(0.7f, 1.5f, scale);
    titleLabel.setFont(makeAuraFont(12.0f * uiScale, true));
    valueLabel.setFont(makeAuraFont(16.0f * uiScale, true));
    helperLabel.setFont(makeAuraFont(9.5f * uiScale));
    resized();
}

void AuraDial::refreshValue()
{
    const auto value = slider.getValue();
    if (units == "shiftHz")
    {
        const auto sign = value > 0.0 ? "+" : "";
        valueLabel.setText(sign + juce::String(value, 1) + " Hz", juce::dontSendNotification);
    }
    else if (units == "Hz")
    {
        valueLabel.setText(juce::String(juce::roundToInt(value)) + " Hz", juce::dontSendNotification);
    }
    else if (units == "kHz")
    {
        valueLabel.setText(juce::String(value / 1000.0, 1) + " kHz", juce::dontSendNotification);
    }
    else if (units == "rate")
    {
        valueLabel.setText(juce::String(value, value < 1.0 ? 2 : 1) + " Hz", juce::dontSendNotification);
    }
    else if (units == "st")
    {
        const auto sign = value > 0.0 ? "+" : "";
        valueLabel.setText(sign + juce::String(value, 1) + " st", juce::dontSendNotification);
    }
    else if (units == "dB")
    {
        const auto sign = value > 0.0 ? "+" : "";
        valueLabel.setText(sign + juce::String(value, 1) + " dB", juce::dontSendNotification);
    }
    else
    {
        const auto formattedValue = units == "gr/s" ? juce::String(value, 1)
                                                     : juce::String(juce::roundToInt(value));
        valueLabel.setText(formattedValue + " " + units, juce::dontSendNotification);
    }
}

AuraGrainPad::AuraGrainPad(AuraAudioProcessor& audioProcessor) : processor(audioProcessor)
{
    setWantsKeyboardFocus(false);
    setTooltip("Drag left or right to set spectral Shift; drag up or down to set Grain mix.");
    startTimerHz(30);
}

void AuraGrainPad::resized()
{
}

void AuraGrainPad::mouseDown(const juce::MouseEvent& event)
{
    updateFromPosition(event.position);
}

void AuraGrainPad::mouseDrag(const juce::MouseEvent& event)
{
    updateFromPosition(event.position);
}

void AuraGrainPad::updateFromPosition(juce::Point<float> position)
{
    const auto area = getLocalBounds().toFloat().reduced(18.0f, 26.0f);
    const auto x = juce::jlimit(0.0f, 1.0f, (position.x - area.getX()) / area.getWidth());
    const auto y = juce::jlimit(0.0f, 1.0f, (position.y - area.getY()) / area.getHeight());
    auto& parameters = processor.getParameters();
    if (auto* shift = parameters.getParameter("shift"))
    {
        const auto value = -1500.0f + x * 3000.0f;
        shift->setValueNotifyingHost(shift->convertTo0to1(value));
    }
    if (auto* grain = parameters.getParameter("grain"))
    {
        const auto value = (1.0f - y) * 100.0f;
        grain->setValueNotifyingHost(grain->convertTo0to1(value));
    }
}

void AuraGrainPad::timerCallback()
{
    repaint();
}

void AuraGrainPad::paint(juce::Graphics& graphics)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto pad = bounds.reduced(1.0f);
    graphics.setColour(panelColour);
    graphics.fillRoundedRectangle(pad, 13.0f);
    graphics.setColour(softLineColour);
    graphics.drawRoundedRectangle(pad, 13.0f, 1.0f);

    const auto area = bounds.reduced(18.0f, 26.0f);
    graphics.setColour(softLineColour.withAlpha(0.62f));
    for (int line = 1; line < 4; ++line)
    {
        const auto x = area.getX() + area.getWidth() * static_cast<float>(line) / 4.0f;
        const auto y = area.getY() + area.getHeight() * static_cast<float>(line) / 4.0f;
        graphics.drawLine(x, area.getY(), x, area.getBottom(), 0.7f);
        graphics.drawLine(area.getX(), y, area.getRight(), y, 0.7f);
    }

    // A fine eight-petal contour gives the granular field a botanical identity
    // without hiding the moving particles or the useful XY control area.
    juce::Path flowerContour;
    const auto flowerCentre = area.getCentre();
    const auto flowerRadius = juce::jmin(area.getWidth(), area.getHeight()) * 0.42f;
    for (int point = 0; point <= 192; ++point)
    {
        const auto angle = juce::MathConstants<float>::twoPi * static_cast<float>(point) / 192.0f;
        const auto radius = flowerRadius * (0.78f + 0.22f * std::cos(8.0f * angle));
        const juce::Point<float> vertex{ flowerCentre.x + std::cos(angle) * radius,
                                         flowerCentre.y + std::sin(angle) * radius };
        if (point == 0)
            flowerContour.startNewSubPath(vertex);
        else
            flowerContour.lineTo(vertex);
    }
    flowerContour.closeSubPath();
    graphics.setColour(lilyColour.withAlpha(0.045f));
    graphics.fillPath(flowerContour);
    graphics.setColour(lilyLightColour.withAlpha(0.34f));
    graphics.strokePath(flowerContour, juce::PathStrokeType(0.9f));

    const auto* shiftParameter = processor.getParameters().getRawParameterValue("shift");
    const auto* grainParameter = processor.getParameters().getRawParameterValue("grain");
    const auto shift = shiftParameter != nullptr ? shiftParameter->load(std::memory_order_relaxed) : 0.0f;
    const auto grain = grainParameter != nullptr ? grainParameter->load(std::memory_order_relaxed) : 0.0f;
    const auto markerX = area.getX() + juce::jmap(shift, -1500.0f, 1500.0f, 0.0f, area.getWidth());
    const auto markerY = area.getBottom() - juce::jlimit(0.0f, 100.0f, grain) * 0.01f * area.getHeight();
    const auto lfo = processor.getLfoValue();
    const auto level = juce::jlimit(0.0f, 1.0f, processor.getInputLevel() * 5.0f);
    const auto phase = processor.getLfoPhase();
    for (int particle = 0; particle < 34; ++particle)
    {
        const auto seed = static_cast<float>(particle) * 0.6180339f;
        const auto x = area.getX() + std::fmod(seed + phase * (0.08f + level * 0.35f), 1.0f) * area.getWidth();
        const auto y = area.getY() + (0.5f + 0.43f * std::sin(seed * 6.2831853f + phase * 6.2831853f + lfo))
                                      * area.getHeight();
        const auto distance = std::hypot(x - markerX, y - markerY);
        const auto glow = juce::jlimit(0.18f, 0.85f, 0.75f - distance / (area.getWidth() + area.getHeight()));
        const auto size = 1.4f + level * 2.0f + (particle % 5 == 0 ? 1.0f : 0.0f);
        graphics.setColour((particle % 4 == 0 ? lilyLightColour : warmAccentColour).withAlpha(glow));
        graphics.fillEllipse(x - size * 0.5f, y - size * 0.5f, size, size);
    }

    graphics.setColour(warmAccentColour.withAlpha(0.2f));
    graphics.fillEllipse(markerX - 11.0f, markerY - 11.0f, 22.0f, 22.0f);
    graphics.setColour(warmAccentColour);
    graphics.drawEllipse(markerX - 6.0f, markerY - 6.0f, 12.0f, 12.0f, 1.7f);
    graphics.fillEllipse(markerX - 2.0f, markerY - 2.0f, 4.0f, 4.0f);

    graphics.setColour(brightLeafColour);
    graphics.setFont(makeAuraFont(9.0f, true));
    graphics.drawText("GRAIN FIELD  ·  SHIFT / GRAIN", 12, 7, getWidth() - 24, 15,
                      juce::Justification::centredLeft);
    graphics.setColour(mutedTextColour);
    graphics.setFont(makeAuraFont(8.0f));
    graphics.drawText("SHIFT (Hz)", 18, getHeight() - 16, 66, 12, juce::Justification::centredLeft);
    graphics.drawText("GRAIN MIX", getWidth() - 86, getHeight() - 16, 68, 12,
                      juce::Justification::centredRight);
}

AuraAudioProcessorEditor::AuraAudioProcessorEditor(AuraAudioProcessor& audioProcessor)
    : AudioProcessorEditor(&audioProcessor),
      processor(audioProcessor),
    shiftDial(audioProcessor.getParameters(), lookAndFeel, "shift", "Shift", "Spectral frequency offset", 0.0, "shiftHz"),
      mixDial(audioProcessor.getParameters(), lookAndFeel, "mix", "Mix", "Dry and processed level", 100.0),
      bloomDial(audioProcessor.getParameters(), lookAndFeel, "bloom", "Bloom", "Adds neighboring spectral energy", 18.0),
      blurDial(audioProcessor.getParameters(), lookAndFeel, "blur", "Blur", "Smooths nearby bins", 12.0),
      lowCutDial(audioProcessor.getParameters(), lookAndFeel, "lowcut", "Low cut", "Lowest shifted frequency", 20.0, "Hz"),
      highCutDial(audioProcessor.getParameters(), lookAndFeel, "highcut", "High cut", "Highest shifted frequency", 20000.0, "kHz"),
      grainDial(audioProcessor.getParameters(), lookAndFeel, "grain", "Grain mix", "Granular layer level", 22.0),
      grainSizeDial(audioProcessor.getParameters(), lookAndFeel, "grainsize", "Grain size", "Length of each grain",
                    120.0, "ms"),
      grainPitchDial(audioProcessor.getParameters(), lookAndFeel, "grainpitch", "Grain pitch", "Pitch per grain",
                     0.0, "st"),
      densityDial(audioProcessor.getParameters(), lookAndFeel, "density", "Density", "Grains per second",
                  12.0, "gr/s"),
      feedbackDial(audioProcessor.getParameters(), lookAndFeel, "feedback", "Feedback", "Amount recirculated",
                   18.0),
      toneDial(audioProcessor.getParameters(), lookAndFeel, "tone", "Tone", "Low-pass cutoff", 18000.0, "kHz"),
      widthDial(audioProcessor.getParameters(), lookAndFeel, "width", "Stereo width", "Width of the stereo image", 110.0),
      lfoRateDial(audioProcessor.getParameters(), lookAndFeel, "lforate", "Rate", "LFO cycles per second", 0.25, "rate"),
      lfoDepthDial(audioProcessor.getParameters(), lookAndFeel, "lfodepth", "Depth", "LFO modulation amount", 0.0),
      outputDial(audioProcessor.getParameters(), lookAndFeel, "output", "Output", "Final output level", 0.0, "dB"),
      grainPad(audioProcessor)
{
    setOpaque(true);
    setBufferedToImage(false);
    setSize(1040, 620);
    setResizable(true, true);
    setResizeLimits(780, 465, 1560, 930);
    getConstrainer()->setFixedAspectRatio(1040.0 / 620.0);
    addAndMakeVisible(shiftDial);
    addAndMakeVisible(mixDial);
    addAndMakeVisible(bloomDial);
    addAndMakeVisible(blurDial);
    addAndMakeVisible(lowCutDial);
    addAndMakeVisible(highCutDial);
    addAndMakeVisible(grainDial);
    addAndMakeVisible(grainSizeDial);
    addAndMakeVisible(grainPitchDial);
    addAndMakeVisible(densityDial);
    addAndMakeVisible(feedbackDial);
    addAndMakeVisible(toneDial);
    addAndMakeVisible(widthDial);
    addAndMakeVisible(lfoRateDial);
    addAndMakeVisible(lfoDepthDial);
    addAndMakeVisible(outputDial);
    addAndMakeVisible(grainPad);

    const auto setupTab = [this](juce::TextButton& button, const juce::String& label, Page page)
    {
        button.setButtonText(label);
        button.setFont(makeAuraFont(10.0f, true));
        button.setColour(juce::TextButton::buttonColourId, panelColour);
        button.setColour(juce::TextButton::buttonOnColourId, warmAccentColour);
        button.setColour(juce::TextButton::textColourOffId, mutedTextColour);
        button.setColour(juce::TextButton::textColourOnId, brightLeafColour);
        button.setClickingTogglesState(true);
        button.onClick = [this, page] { setActivePage(page); };
        addAndMakeVisible(button);
    };
    setupTab(spectralTab, "SPECTRAL", Page::spectral);
    setupTab(grainTab, "GRAIN", Page::grain);
    setupTab(modulationTab, "MOD", Page::modulation);
    setupTab(settingsTab, "SETTINGS", Page::settings);

    fftSelector.addItem("2048", 1);
    fftSelector.addItem("4096", 2);
    fftSelector.addItem("8192", 3);
    lfoShapeSelector.addItemList({ "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Random Hold" }, 1);
    lfoTargetSelector.addItemList({ "Off", "Shift", "Bloom", "Blur", "Grain", "Pitch", "Size", "Density",
                                    "Feedback", "Tone", "Width" }, 1);
    const auto styleCombo = [](juce::ComboBox& combo)
    {
        combo.setFont(makeAuraFont(11.0f));
        combo.setColour(juce::ComboBox::backgroundColourId, cardColour);
        combo.setColour(juce::ComboBox::textColourId, cardTextColour);
        combo.setColour(juce::ComboBox::arrowColourId, warmAccentColour);
        combo.setColour(juce::ComboBox::outlineColourId, softLineColour.withAlpha(0.35f));
    };
    styleCombo(fftSelector);
    styleCombo(lfoShapeSelector);
    styleCombo(lfoTargetSelector);
    addAndMakeVisible(fftSelector);
    addAndMakeVisible(lfoShapeSelector);
    addAndMakeVisible(lfoTargetSelector);
    fftAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getParameters(), "fft", fftSelector);
    lfoShapeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getParameters(), "lfoshape", lfoShapeSelector);
    lfoTargetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getParameters(), "lfotarget", lfoTargetSelector);

    const auto styleToggle = [this](juce::ToggleButton& button)
    {
        button.setColour(juce::ToggleButton::textColourId, cardTextColour);
        button.setColour(juce::ToggleButton::tickColourId, warmAccentColour);
        button.setColour(juce::ToggleButton::tickDisabledColourId, softLineColour);
        addAndMakeVisible(button);
    };
    styleToggle(spectralEnableButton);
    styleToggle(grainEnableButton);
    styleToggle(toneEnableButton);
    spectralEnableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getParameters(), "spectral_on", spectralEnableButton);
    grainEnableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getParameters(), "grain_on", grainEnableButton);
    toneEnableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getParameters(), "tone_on", toneEnableButton);

    presetLabel.setText("PRESET", juce::dontSendNotification);
    presetLabel.setColour(juce::Label::textColourId, mutedTextColour);
    presetLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(presetLabel);

    presetSelector.setTextWhenNothingSelected("Choose preset");
    presetSelector.setColour(juce::ComboBox::backgroundColourId, cardColour);
    presetSelector.setColour(juce::ComboBox::textColourId, cardTextColour);
    presetSelector.setColour(juce::ComboBox::arrowColourId, aquaColour);
    presetSelector.setColour(juce::ComboBox::outlineColourId, softLineColour);
    presetSelector.onChange = [this] { loadSelectedPreset(); };
    addAndMakeVisible(presetSelector);

    savePresetButton.setColour(juce::TextButton::buttonColourId, warmAccentColour);
    savePresetButton.setColour(juce::TextButton::buttonOnColourId, warmAccentColour.brighter(0.12f));
    savePresetButton.setColour(juce::TextButton::textColourOffId, brightLeafColour);
    savePresetButton.setColour(juce::TextButton::textColourOnId, brightLeafColour);
    savePresetButton.setButtonText("Save preset");
    savePresetButton.setFont(makeAuraFont(10.0f, true));
    savePresetButton.onClick = [this] { beginSavingPreset(); };
    addAndMakeVisible(savePresetButton);

    const auto styleBankButton = [this](juce::TextButton& button, const juce::String& text)
    {
        button.setButtonText(text);
        button.setFont(makeAuraFont(10.0f, true));
        button.setColour(juce::TextButton::buttonColourId, panelColour.brighter(0.08f));
        button.setColour(juce::TextButton::buttonOnColourId, panelColour.brighter(0.16f));
        button.setColour(juce::TextButton::textColourOffId, brightLeafColour);
        button.setColour(juce::TextButton::textColourOnId, brightLeafColour);
        addAndMakeVisible(button);
    };
    styleBankButton(loadBankButton, "Load bank");
    loadBankButton.setTooltip("Import an Aura preset bank (.aubank) or a single preset (.aupreset)");
    loadBankButton.onClick = [this] { importPresetBank(); };
    styleBankButton(saveBankButton, "Save bank");
    saveBankButton.setTooltip("Export your saved Aura presets as a shareable .aubank file");
    saveBankButton.onClick = [this] { exportPresetBank(); };

    presetNameEditor.setColour(juce::TextEditor::backgroundColourId, cardColour);
    presetNameEditor.setColour(juce::TextEditor::textColourId, cardTextColour);
    presetNameEditor.setColour(juce::TextEditor::outlineColourId, softLineColour);
    presetNameEditor.setColour(juce::TextEditor::focusedOutlineColourId, aquaColour);
    presetNameEditor.setTextToShowWhenEmpty("Name your preset", mutedTextColour);
    presetNameEditor.setInputRestrictions(48);
    presetNameEditor.onReturnKey = [this] { savePresetFromEditor(); };
    presetNameEditor.onEscapeKey = [this] { cancelSavingPreset(); };
    addAndMakeVisible(presetNameEditor);
    presetNameEditor.setVisible(false);

    confirmPresetButton.setColour(juce::TextButton::buttonColourId, warmAccentColour);
    confirmPresetButton.setColour(juce::TextButton::textColourOffId, brightLeafColour);
    confirmPresetButton.setFont(makeAuraFont(10.0f, true));
    confirmPresetButton.onClick = [this] { savePresetFromEditor(); };
    addAndMakeVisible(confirmPresetButton);
    confirmPresetButton.setVisible(false);

    cancelPresetButton.setColour(juce::TextButton::buttonColourId, softLineColour);
    cancelPresetButton.setColour(juce::TextButton::textColourOffId, brightLeafColour);
    cancelPresetButton.setFont(makeAuraFont(10.0f, true));
    cancelPresetButton.onClick = [this] { cancelSavingPreset(); };
    addAndMakeVisible(cancelPresetButton);
    cancelPresetButton.setVisible(false);

    refreshPresetMenu();
    setActivePage(Page::spectral);
    startTimerHz(30);
}

AuraAudioProcessorEditor::~AuraAudioProcessorEditor()
{
    stopTimer();
    shiftDial.setLookAndFeel(nullptr);
    mixDial.setLookAndFeel(nullptr);
    bloomDial.setLookAndFeel(nullptr);
    blurDial.setLookAndFeel(nullptr);
    lowCutDial.setLookAndFeel(nullptr);
    highCutDial.setLookAndFeel(nullptr);
    grainDial.setLookAndFeel(nullptr);
    grainSizeDial.setLookAndFeel(nullptr);
    grainPitchDial.setLookAndFeel(nullptr);
    densityDial.setLookAndFeel(nullptr);
    feedbackDial.setLookAndFeel(nullptr);
    toneDial.setLookAndFeel(nullptr);
    widthDial.setLookAndFeel(nullptr);
    lfoRateDial.setLookAndFeel(nullptr);
    lfoDepthDial.setLookAndFeel(nullptr);
    outputDial.setLookAndFeel(nullptr);
}

void AuraAudioProcessorEditor::paint(juce::Graphics& graphics)
{
    graphics.fillAll(backgroundColour);
    constexpr float designWidth = 1040.0f;
    constexpr float designHeight = 620.0f;
    const auto scale = juce::jmin(static_cast<float>(getWidth()) / designWidth,
                                  static_cast<float>(getHeight()) / designHeight);
    const auto offsetX = (static_cast<float>(getWidth()) - designWidth * scale) * 0.5f;
    const auto offsetY = (static_cast<float>(getHeight()) - designHeight * scale) * 0.5f;
    graphics.addTransform(juce::AffineTransform(scale, 0.0f, offsetX,
                                                 0.0f, scale, offsetY));

    const juce::Rectangle<float> designBounds{0.0f, 0.0f, designWidth, designHeight};
    const auto panelBounds = designBounds.reduced(8.0f);
    graphics.setColour(panelColour);
    graphics.fillRoundedRectangle(panelBounds, 18.0f);
    graphics.setColour(softLineColour.withAlpha(0.78f));
    graphics.drawRoundedRectangle(panelBounds, 18.0f, 1.0f);

    graphics.setColour(warmAccentColour);
    graphics.fillEllipse(30.0f, 29.0f, 20.0f, 20.0f);
    graphics.setColour(brightLeafColour);
    graphics.setFont(makeAuraFont(13.0f, true));
    graphics.drawText("355ERA", 61, 26, 90, 23, juce::Justification::centredLeft);
    graphics.setColour(mutedTextColour);
    graphics.setFont(makeAuraFont(9.0f));
    graphics.drawText("AURA  /  SPECTRAL TEXTURE PROCESSOR", 159, 28, 330, 20,
                      juce::Justification::centredLeft);

    graphics.setColour(softLineColour);
    graphics.drawLine(28.0f, 62.0f, designWidth - 28.0f, 62.0f, 1.0f);
    graphics.setColour(brightLeafColour);
    graphics.setFont(makeAuraFont(36.0f, true));
    graphics.drawText("AURA", 36, 84, 250, 48, juce::Justification::centredLeft);
    graphics.setColour(aquaColour);
    graphics.setFont(makeAuraFont(10.0f, true));
    graphics.drawText("SPECTRAL SHIFTER", 39, 132, 252, 18, juce::Justification::centredLeft);
    graphics.setColour(mutedTextColour);
    graphics.setFont(makeAuraFont(10.0f));
    graphics.drawText("Shift  ·  Scatter  ·  Shape", 39, 151, 252, 18,
                      juce::Justification::centredLeft);

    graphics.setColour(softLineColour.withAlpha(0.9f));
    graphics.drawLine(306.0f, 76.0f, 306.0f, 550.0f, 1.0f);
    graphics.setColour(aquaColour);
    graphics.setFont(makeAuraFont(11.0f, true));
    juce::String pageTitle;
    juce::String pageDescription;
    switch (activePage)
    {
        case Page::spectral:
            pageTitle = "SPECTRAL ENGINE";
            pageDescription = "Frequency shift, spectral density and band shaping";
            break;
        case Page::grain:
            pageTitle = "GRAIN LAYER";
            pageDescription = "Pitch, window, density and controlled feedback";
            break;
        case Page::modulation:
            pageTitle = "MODULATION";
            pageDescription = "Animate a selected parameter with the internal LFO";
            break;
        case Page::settings:
            pageTitle = "GLOBAL SETTINGS";
            pageDescription = "Resolution, effect routing and output level";
            break;
    }
    graphics.drawText(pageTitle, 322, 107, 300, 18, juce::Justification::centredLeft);
    graphics.setColour(mutedTextColour);
    graphics.setFont(makeAuraFont(9.0f));
    graphics.drawText(pageDescription, 322, 124, 460, 18,
                      juce::Justification::centredLeft);

    const auto drawControlCard = [&graphics](float x, float y, float width, float height = 151.0f)
    {
        const juce::Rectangle<float> card{x, y, width, height};
        graphics.setColour(cardColour);
        graphics.fillRoundedRectangle(card, 9.0f);
        graphics.setColour(softLineColour.withAlpha(0.35f));
        graphics.drawRoundedRectangle(card, 10.0f, 1.0f);
    };
    if (activePage == Page::spectral)
    {
        for (int row = 0; row < 2; ++row)
            for (int column = 0; column < 4; ++column)
                drawControlCard(322.0f + static_cast<float>(column) * 160.0f,
                                170.0f + static_cast<float>(row) * 163.0f, 150.0f);
    }
    else if (activePage == Page::grain)
    {
        for (int column = 0; column < 5; ++column)
            drawControlCard(322.0f + static_cast<float>(column) * 136.0f, 170.0f, 128.0f);
        drawControlCard(322.0f, 333.0f, 672.0f, 151.0f);
        graphics.setColour(cardTextColour.withAlpha(0.7f));
        graphics.setFont(makeAuraFont(9.0f));
        graphics.drawText("A bounded feedback loop keeps repeating grains smooth as density and pitch change.",
                          346, 390, 620, 36, juce::Justification::centredLeft);
    }
    else if (activePage == Page::modulation)
    {
        drawControlCard(322.0f, 170.0f, 128.0f);
        drawControlCard(458.0f, 170.0f, 128.0f);
        drawControlCard(594.0f, 170.0f, 185.0f);
        drawControlCard(790.0f, 170.0f, 204.0f);
        graphics.setColour(cardTextColour.withAlpha(0.72f));
        graphics.setFont(makeAuraFont(9.5f, true));
        graphics.drawText("Waveform", 605, 188, 160, 16, juce::Justification::centredLeft);
        graphics.drawText("LFO destination", 802, 188, 180, 16, juce::Justification::centredLeft);
        drawLfoScope(graphics, { 322.0f, 333.0f, 672.0f, 151.0f });
    }
    else
    {
        drawControlCard(322.0f, 170.0f, 128.0f);
        drawControlCard(458.0f, 170.0f, 185.0f);
        drawControlCard(322.0f, 333.0f, 672.0f, 151.0f);
        graphics.setColour(cardTextColour.withAlpha(0.72f));
        graphics.setFont(makeAuraFont(9.5f, true));
        graphics.drawText("FFT window", 473, 188, 154, 16, juce::Justification::centredLeft);
        graphics.setColour(cardTextColour.withAlpha(0.7f));
        graphics.setFont(makeAuraFont(9.0f));
        graphics.drawText("Switch each layer independently. FFT latency follows the selected window size.",
                          346, 450, 620, 24, juce::Justification::centredLeft);
    }

    graphics.setColour(softLineColour);
    graphics.drawLine(30.0f, 566.0f, designWidth - 30.0f, 566.0f, 1.0f);
    graphics.setColour(mutedTextColour);
    graphics.setFont(makeAuraFont(8.5f));
    graphics.drawText("SPECTRAL SHIFT  ·  GRAIN DELAY  ·  LFO", 36, 578, 430, 19,
                      juce::Justification::centredLeft);
    graphics.setColour(leafColour);
    graphics.drawText("355ERA  /  AURA", 760, 578, 248, 19,
                      juce::Justification::centredRight);
}

void AuraAudioProcessorEditor::drawLfoScope(juce::Graphics& graphics, juce::Rectangle<float> bounds)
{
    graphics.setColour(cardColour);
    graphics.fillRoundedRectangle(bounds, 9.0f);
    graphics.setColour(softLineColour.withAlpha(0.35f));
    graphics.drawRoundedRectangle(bounds, 9.0f, 1.0f);
    auto plot = bounds.reduced(20.0f, 20.0f);
    plot.removeFromTop(8.0f);
    graphics.setColour(cardTextColour.withAlpha(0.12f));
    graphics.drawLine(plot.getX(), plot.getCentreY(), plot.getRight(), plot.getCentreY(), 0.8f);
    graphics.drawLine(plot.getX(), plot.getY(), plot.getX(), plot.getBottom(), 0.8f);

    juce::Path wave;
    const auto shape = juce::jmax(0, lfoShapeSelector.getSelectedId() - 1);
    for (int sample = 0; sample <= 128; ++sample)
    {
        const auto phase = static_cast<float>(sample) / 128.0f;
        float value = 0.0f;
        switch (shape)
        {
            case 0: value = std::sin(phase * juce::MathConstants<float>::twoPi); break;
            case 1: value = 1.0f - 4.0f * std::abs(phase - 0.5f); break;
            case 2: value = phase * 2.0f - 1.0f; break;
            case 3: value = 1.0f - phase * 2.0f; break;
            case 4: value = phase < 0.5f ? 1.0f : -1.0f; break;
            case 5: value = std::sin(phase * 18.0f) * std::cos(phase * 7.0f); break;
            default: break;
        }
        const auto x = plot.getX() + phase * plot.getWidth();
        const auto y = plot.getCentreY() - value * plot.getHeight() * 0.4f;
        if (sample == 0)
            wave.startNewSubPath(x, y);
        else
            wave.lineTo(x, y);
    }
    graphics.setColour(warmAccentColour);
    graphics.strokePath(wave, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
    const auto phase = processor.getLfoPhase();
    const auto lfo = processor.getLfoValue();
    const auto markerX = plot.getX() + phase * plot.getWidth();
    const auto markerY = plot.getCentreY() - lfo * plot.getHeight() * 0.4f;
    graphics.setColour(warmAccentColour.withAlpha(0.2f));
    graphics.fillEllipse(markerX - 7.0f, markerY - 7.0f, 14.0f, 14.0f);
    graphics.setColour(warmAccentColour);
    graphics.fillEllipse(markerX - 3.0f, markerY - 3.0f, 6.0f, 6.0f);
    graphics.setColour(cardTextColour.withAlpha(0.72f));
    graphics.setFont(makeAuraFont(8.0f));
    graphics.drawText("LFO SHAPE  /  LIVE PHASE", 16, 5, 220, 14, juce::Justification::centredLeft);
}

void AuraAudioProcessorEditor::resized()
{
    constexpr float designWidth = 1040.0f;
    constexpr float designHeight = 620.0f;
    const auto scale = juce::jmin(static_cast<float>(getWidth()) / designWidth,
                                  static_cast<float>(getHeight()) / designHeight);
    const auto offsetX = (static_cast<float>(getWidth()) - designWidth * scale) * 0.5f;
    const auto offsetY = (static_cast<float>(getHeight()) - designHeight * scale) * 0.5f;

    const auto scaledBounds = [scale, offsetX, offsetY](int x, int y, int width, int height)
    {
        // Round both edges from the design grid. Rounding the origin and size
        // independently makes adjacent controls drift by a pixel at fractional
        // scales, which is especially visible on the preset and tab buttons.
        const auto left = juce::roundToInt(offsetX + static_cast<float>(x) * scale);
        const auto top = juce::roundToInt(offsetY + static_cast<float>(y) * scale);
        const auto right = juce::roundToInt(offsetX + static_cast<float>(x + width) * scale);
        const auto bottom = juce::roundToInt(offsetY + static_cast<float>(y + height) * scale);
        return juce::Rectangle<int>(left, top, juce::jmax(0, right - left), juce::jmax(0, bottom - top));
    };
    const auto placeDial = [scale, scaledBounds](AuraDial& dial, int x, int y)
    {
        dial.setBounds(scaledBounds(x, y, 128, 151));
        dial.setScale(scale);
    };
    for (int column = 0; column < 4; ++column)
    {
        const auto x = 333 + column * 160;
        const auto lowerX = x;
        switch (column)
        {
            case 0: placeDial(shiftDial, x, 170); placeDial(lowCutDial, lowerX, 333); break;
            case 1: placeDial(mixDial, x, 170); placeDial(highCutDial, lowerX, 333); break;
            case 2: placeDial(bloomDial, x, 170); placeDial(toneDial, lowerX, 333); break;
            case 3: placeDial(blurDial, x, 170); placeDial(widthDial, lowerX, 333); break;
            default: break;
        }
    }
    placeDial(grainDial, 322, 170);
    placeDial(grainSizeDial, 458, 170);
    placeDial(grainPitchDial, 594, 170);
    placeDial(densityDial, 730, 170);
    placeDial(feedbackDial, 866, 170);
    placeDial(lfoRateDial, 322, 170);
    placeDial(lfoDepthDial, 458, 170);
    placeDial(outputDial, 322, 170);
    grainPad.setBounds(scaledBounds(30, 205, 258, 278));

    spectralTab.setBounds(scaledBounds(322, 76, 104, 26));
    spectralTab.setFont(makeAuraFont(10.0f * scale, true));
    grainTab.setBounds(scaledBounds(432, 76, 104, 26));
    grainTab.setFont(makeAuraFont(10.0f * scale, true));
    modulationTab.setBounds(scaledBounds(542, 76, 104, 26));
    modulationTab.setFont(makeAuraFont(10.0f * scale, true));
    settingsTab.setBounds(scaledBounds(652, 76, 112, 26));
    settingsTab.setFont(makeAuraFont(10.0f * scale, true));

    presetLabel.setText("PRESETS", juce::dontSendNotification);
    presetLabel.setBounds(scaledBounds(330, 137, 56, 26));
    presetLabel.setFont(makeAuraFont(9.0f * scale, true));
    presetSelector.setBounds(scaledBounds(390, 135, 270, 28));
    presetSelector.setFont(makeAuraFont(11.0f * scale));
    savePresetButton.setBounds(scaledBounds(670, 135, 108, 28));
    savePresetButton.setFont(makeAuraFont(10.0f * scale, true));
    loadBankButton.setBounds(scaledBounds(786, 135, 100, 28));
    loadBankButton.setFont(makeAuraFont(10.0f * scale, true));
    saveBankButton.setBounds(scaledBounds(894, 135, 100, 28));
    saveBankButton.setFont(makeAuraFont(10.0f * scale, true));
    presetNameEditor.setBounds(scaledBounds(390, 135, 194, 28));
    presetNameEditor.setFont(makeAuraFont(10.0f * scale));
    confirmPresetButton.setBounds(scaledBounds(590, 135, 78, 28));
    confirmPresetButton.setFont(makeAuraFont(10.0f * scale, true));
    cancelPresetButton.setBounds(scaledBounds(674, 135, 88, 28));
    cancelPresetButton.setFont(makeAuraFont(10.0f * scale, true));

    lfoShapeSelector.setBounds(scaledBounds(605, 209, 160, 30));
    lfoShapeSelector.setFont(makeAuraFont(10.5f * scale));
    lfoTargetSelector.setBounds(scaledBounds(802, 209, 180, 30));
    lfoTargetSelector.setFont(makeAuraFont(10.5f * scale));
    fftSelector.setBounds(scaledBounds(473, 208, 154, 30));
    fftSelector.setFont(makeAuraFont(10.5f * scale));
    spectralEnableButton.setBounds(scaledBounds(346, 358, 190, 28));
    spectralEnableButton.setFont(makeAuraFont(10.0f * scale));
    grainEnableButton.setBounds(scaledBounds(552, 358, 180, 28));
    grainEnableButton.setFont(makeAuraFont(10.0f * scale));
    toneEnableButton.setBounds(scaledBounds(748, 358, 180, 28));
    toneEnableButton.setFont(makeAuraFont(10.0f * scale));
}

void AuraAudioProcessorEditor::setActivePage(Page page)
{
    activePage = page;
    const auto spectral = page == Page::spectral;
    const auto grain = page == Page::grain;
    const auto modulation = page == Page::modulation;
    const auto settings = page == Page::settings;
    shiftDial.setVisible(spectral);
    mixDial.setVisible(spectral);
    bloomDial.setVisible(spectral);
    blurDial.setVisible(spectral);
    lowCutDial.setVisible(spectral);
    highCutDial.setVisible(spectral);
    toneDial.setVisible(spectral);
    widthDial.setVisible(spectral);
    grainDial.setVisible(grain);
    grainSizeDial.setVisible(grain);
    grainPitchDial.setVisible(grain);
    densityDial.setVisible(grain);
    feedbackDial.setVisible(grain);
    lfoRateDial.setVisible(modulation);
    lfoDepthDial.setVisible(modulation);
    lfoShapeSelector.setVisible(modulation);
    lfoTargetSelector.setVisible(modulation);
    outputDial.setVisible(settings);
    fftSelector.setVisible(settings);
    spectralEnableButton.setVisible(settings);
    grainEnableButton.setVisible(settings);
    toneEnableButton.setVisible(settings);
    spectralTab.setToggleState(spectral, juce::dontSendNotification);
    grainTab.setToggleState(grain, juce::dontSendNotification);
    modulationTab.setToggleState(modulation, juce::dontSendNotification);
    settingsTab.setToggleState(settings, juce::dontSendNotification);
    repaint();
}

void AuraAudioProcessorEditor::refreshPresetMenu(int preferredItemID)
{
    presetSelector.clear(juce::dontSendNotification);
    userPresetFiles.clear();

    for (size_t index = 0; index < factoryPresets.size(); ++index)
        presetSelector.addItem(factoryPresets[index].name, static_cast<int>(index) + 1);

    auto presetDirectory = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                               .getChildFile("355ERA")
                               .getChildFile("Aura")
                               .getChildFile("Presets");
    juce::Array<juce::File> foundFiles;
    if (presetDirectory.isDirectory())
        presetDirectory.findChildFiles(foundFiles, juce::File::findFiles, false, "*.aupreset");

    std::vector<juce::File> files;
    files.reserve(static_cast<size_t>(foundFiles.size()));
    for (const auto& file : foundFiles)
        files.push_back(file);
    std::sort(files.begin(), files.end(), [](const juce::File& first, const juce::File& second)
    {
        return first.getFileNameWithoutExtension().compareNatural(second.getFileNameWithoutExtension()) < 0;
    });

    if (!files.empty())
        presetSelector.addSeparator();
    for (const auto& file : files)
    {
        const auto itemID = userPresetStartID + static_cast<int>(userPresetFiles.size());
        userPresetFiles.push_back(file);
        presetSelector.addItem(file.getFileNameWithoutExtension(), itemID);
    }

    presetSelector.setSelectedId(preferredItemID, juce::dontSendNotification);
}

void AuraAudioProcessorEditor::setParameterFromPreset(const juce::String& parameterID, float value)
{
    if (auto* parameter = processor.getParameters().getParameter(parameterID))
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

void AuraAudioProcessorEditor::loadSelectedPreset()
{
    const auto selectedID = presetSelector.getSelectedId();
    if (selectedID >= 1 && selectedID <= static_cast<int>(factoryPresets.size()))
    {
        const auto& preset = factoryPresets[static_cast<size_t>(selectedID - 1)];
        for (size_t index = 0; index < presetParameterIDs.size(); ++index)
            setParameterFromPreset(presetParameterIDs[index], preset.values[index]);
        return;
    }

    const auto fileIndex = selectedID - userPresetStartID;
    if (fileIndex < 0 || fileIndex >= static_cast<int>(userPresetFiles.size()))
        return;

    const auto file = userPresetFiles[static_cast<size_t>(fileIndex)];
    const auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                               "Aura Preset", "This preset file could not be read.");
        return;
    }

    auto state = juce::ValueTree::fromXml(*xml);
    if (!state.isValid() || state.getType() != processor.getParameters().state.getType())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                               "Aura Preset", "This file is not a valid Aura preset.");
        return;
    }

    processor.getParameters().replaceState(state);
}

void AuraAudioProcessorEditor::importPresetBank()
{
    bankFileChooser = std::make_unique<juce::FileChooser>(
        "Load Aura preset bank", juce::File{}, "*.aubank;*.aupreset");
    bankFileChooser->launchAsync(juce::FileBrowserComponent::openMode
                                     | juce::FileBrowserComponent::canSelectFiles,
                                 [this](const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();
        if (file.existsAsFile())
            importPresetFile(file);
    });
}

void AuraAudioProcessorEditor::importPresetFile(const juce::File& file)
{
    const auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                               "Aura Bank", "This file is not valid Aura preset data.");
        return;
    }

    const auto presetDirectory = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                                     .getChildFile("355ERA")
                                     .getChildFile("Aura")
                                     .getChildFile("Presets");
    if (presetDirectory.createDirectory().failed())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                               "Aura Bank", "Aura could not create its preset folder.");
        return;
    }

    const auto writePreset = [&presetDirectory, this](juce::ValueTree state, juce::String name)
    {
        if (!state.isValid() || state.getType() != processor.getParameters().state.getType())
            return juce::File{};

        name = name.trim();
        if (name.isEmpty())
            name = "Imported Preset";
        state.setProperty("auraStateVersion", 3, nullptr);
        state.setProperty("presetName", name, nullptr);

        auto safeName = juce::File::createLegalFileName(name);
        if (safeName.isEmpty())
            safeName = "Imported Preset";
        auto destination = presetDirectory.getChildFile(safeName + ".aupreset");
        for (int suffix = 2; destination.existsAsFile(); ++suffix)
            destination = presetDirectory.getChildFile(safeName + " (" + juce::String(suffix) + ").aupreset");

        const auto stateXml = state.createXml();
        if (stateXml == nullptr || !destination.replaceWithText(stateXml->toString()))
            return juce::File{};
        return destination;
    };

    int importedCount = 0;
    juce::File mostRecentPreset;
    if (xml->hasTagName("AURA_BANK"))
    {
        for (auto* preset = xml->getFirstChildElement(); preset != nullptr; preset = preset->getNextElement())
        {
            if (!preset->hasTagName("PRESET"))
                continue;
            const auto* stateXml = preset->getFirstChildElement();
            if (stateXml == nullptr)
                continue;
            const auto state = juce::ValueTree::fromXml(*stateXml);
            const auto name = preset->getStringAttribute("name", state.getProperty("presetName").toString());
            const auto destination = writePreset(state, name);
            if (destination.existsAsFile())
            {
                mostRecentPreset = destination;
                ++importedCount;
            }
        }
    }
    else
    {
        const auto state = juce::ValueTree::fromXml(*xml);
        mostRecentPreset = writePreset(state, file.getFileNameWithoutExtension());
        importedCount = mostRecentPreset.existsAsFile() ? 1 : 0;
    }

    if (importedCount == 0)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                               "Aura Bank", "No compatible Aura presets were found in this file.");
        return;
    }

    refreshPresetMenu();
    for (size_t index = 0; index < userPresetFiles.size(); ++index)
    {
        if (userPresetFiles[index] == mostRecentPreset)
        {
            presetSelector.setSelectedId(userPresetStartID + static_cast<int>(index));
            break;
        }
    }
    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::NoIcon,
                                           "Aura Bank", "Loaded " + juce::String(importedCount)
                                               + (importedCount == 1 ? " preset." : " presets."));
}

void AuraAudioProcessorEditor::exportPresetBank()
{
    if (userPresetFiles.empty())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                                               "Aura Bank", "Save at least one user preset before exporting a bank.");
        return;
    }

    const auto suggestedFile = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                   .getChildFile("Aura User Bank.aubank");
    bankFileChooser = std::make_unique<juce::FileChooser>(
        "Save Aura preset bank", suggestedFile, "*.aubank");
    bankFileChooser->launchAsync(juce::FileBrowserComponent::saveMode
                                     | juce::FileBrowserComponent::canSelectFiles
                                     | juce::FileBrowserComponent::warnAboutOverwriting,
                                 [this](const juce::FileChooser& chooser)
    {
        auto destination = chooser.getResult();
        if (destination == juce::File{})
            return;
        if (!destination.hasFileExtension("aubank"))
            destination = destination.withFileExtension("aubank");

        auto bankXml = std::make_unique<juce::XmlElement>("AURA_BANK");
        bankXml->setAttribute("formatVersion", 1);
        bankXml->setAttribute("plugin", "Aura");
        int exportedCount = 0;
        for (const auto& presetFile : userPresetFiles)
        {
            auto presetXml = juce::XmlDocument::parse(presetFile);
            if (presetXml == nullptr)
                continue;
            const auto state = juce::ValueTree::fromXml(*presetXml);
            if (!state.isValid() || state.getType() != processor.getParameters().state.getType())
                continue;

            auto* preset = bankXml->createNewChildElement("PRESET");
            preset->setAttribute("name", presetFile.getFileNameWithoutExtension());
            preset->addChildElement(presetXml.release());
            ++exportedCount;
        }

        if (exportedCount == 0 || !destination.replaceWithText(bankXml->toString()))
        {
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                   "Aura Bank", "Aura could not write this preset bank.");
            return;
        }
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::NoIcon,
                                               "Aura Bank", "Saved " + juce::String(exportedCount)
                                                   + (exportedCount == 1 ? " preset to bank." : " presets to bank."));
    });
}

void AuraAudioProcessorEditor::beginSavingPreset()
{
    const auto selectedID = presetSelector.getSelectedId();
    const auto selectedUserIndex = selectedID - userPresetStartID;
    const auto initialName = selectedUserIndex >= 0
                             && selectedUserIndex < static_cast<int>(userPresetFiles.size())
                                 ? userPresetFiles[static_cast<size_t>(selectedUserIndex)]
                                       .getFileNameWithoutExtension()
                                 : juce::String{};
    presetNameEditor.setText(initialName, juce::dontSendNotification);
    presetSelector.setVisible(false);
    savePresetButton.setVisible(false);
    loadBankButton.setVisible(false);
    saveBankButton.setVisible(false);
    presetNameEditor.setVisible(true);
    confirmPresetButton.setVisible(true);
    cancelPresetButton.setVisible(true);
    resized();
    presetNameEditor.grabKeyboardFocus();
    presetNameEditor.selectAll();
}

void AuraAudioProcessorEditor::savePresetFromEditor()
{
    auto name = presetNameEditor.getText().trim();
    if (name.isEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                               "Aura Preset", "Enter a name for this preset.");
        return;
    }

    const auto directory = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                               .getChildFile("355ERA")
                               .getChildFile("Aura")
                               .getChildFile("Presets");
    if (directory.createDirectory().failed())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                               "Aura Preset", "Aura could not create its preset folder.");
        return;
    }

    auto safeName = juce::File::createLegalFileName(name);
    if (safeName.isEmpty())
        safeName = "Aura Preset";
    auto file = directory.getChildFile(safeName + ".aupreset");
    const auto selectedID = presetSelector.getSelectedId();
    const auto selectedUserIndex = selectedID - userPresetStartID;
    const auto overwritingSelectedPreset = selectedUserIndex >= 0
                                           && selectedUserIndex < static_cast<int>(userPresetFiles.size())
                                           && userPresetFiles[static_cast<size_t>(selectedUserIndex)] == file;
    for (int suffix = 2; file.existsAsFile() && !overwritingSelectedPreset; ++suffix)
        file = directory.getChildFile(safeName + " (" + juce::String(suffix) + ").aupreset");

    auto state = processor.getParameters().copyState();
    state.setProperty("auraStateVersion", 3, nullptr);
    state.setProperty("presetName", name, nullptr);
    const auto xml = state.createXml();
    if (xml == nullptr || !file.replaceWithText(xml->toString()))
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                               "Aura Preset", "Aura could not write this preset.");
        return;
    }

    cancelSavingPreset();
    refreshPresetMenu();
    for (size_t index = 0; index < userPresetFiles.size(); ++index)
    {
        if (userPresetFiles[index] == file)
        {
            presetSelector.setSelectedId(userPresetStartID + static_cast<int>(index),
                                         juce::dontSendNotification);
            break;
        }
    }
}

void AuraAudioProcessorEditor::cancelSavingPreset()
{
    presetSelector.setVisible(true);
    savePresetButton.setVisible(true);
    loadBankButton.setVisible(true);
    saveBankButton.setVisible(true);
    presetNameEditor.setVisible(false);
    confirmPresetButton.setVisible(false);
    cancelPresetButton.setVisible(false);
    resized();
}

void AuraAudioProcessorEditor::timerCallback()
{
    shiftDial.refreshValue();
    mixDial.refreshValue();
    bloomDial.refreshValue();
    blurDial.refreshValue();
    lowCutDial.refreshValue();
    highCutDial.refreshValue();
    grainDial.refreshValue();
    grainSizeDial.refreshValue();
    grainPitchDial.refreshValue();
    densityDial.refreshValue();
    feedbackDial.refreshValue();
    toneDial.refreshValue();
    widthDial.refreshValue();
    lfoRateDial.refreshValue();
    lfoDepthDial.refreshValue();
    outputDial.refreshValue();
    repaint();
}
