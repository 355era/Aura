#include "PluginEditor.h"

#include <cmath>

namespace
{
const juce::Colour backgroundColour{ 0xffe9efe7 };
const juce::Colour panelColour{ 0xfffaf9f2 };
const juce::Colour softLineColour{ 0xffcbd8c9 };
const juce::Colour leafColour{ 0xff78947c };
const juce::Colour brightLeafColour{ 0xff3d5944 };
const juce::Colour aquaColour{ 0xff6f9884 };
const juce::Colour mutedTextColour{ 0xff758276 };
const juce::Colour warmAccentColour{ 0xffd5aa70 };
const juce::Colour lilyColour{ 0xffb99ac6 };
const juce::Colour lilyLightColour{ 0xffe5d5e8 };
}

void AuraLookAndFeel::drawRotarySlider(juce::Graphics& graphics, int x, int y, int width, int height,
                                       float sliderPosition, float startAngle, float endAngle,
                                       juce::Slider& slider)
{
    juce::ignoreUnused(slider);
    const auto centreX = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
    const auto centreY = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
    const auto radius = juce::jmin(static_cast<float>(width), static_cast<float>(height)) * 0.39f;

    juce::Path track;
    track.addCentredArc(centreX, centreY, radius, radius, 0.0f, startAngle, endAngle, true);
    graphics.setColour(softLineColour.withAlpha(0.95f));
    graphics.strokePath(track, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));

    const auto angle = startAngle + sliderPosition * (endAngle - startAngle);
    juce::Path activeArc;
    activeArc.addCentredArc(centreX, centreY, radius, radius, 0.0f, startAngle, angle, true);
    graphics.setColour(slider.isMouseOverOrDragging() ? brightLeafColour : aquaColour);
    graphics.strokePath(activeArc, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved,
                                                         juce::PathStrokeType::rounded));

    for (int tick = 0; tick <= 12; ++tick)
    {
        const auto tickAngle = startAngle + static_cast<float>(tick) / 12.0f * (endAngle - startAngle);
        const auto inner = radius + 5.0f;
        const auto outer = radius + (tick % 3 == 0 ? 10.0f : 8.0f);
        graphics.setColour(leafColour.withAlpha(tick % 3 == 0 ? 0.65f : 0.38f));
        graphics.drawLine(centreX + std::cos(tickAngle) * inner,
                          centreY + std::sin(tickAngle) * inner,
                          centreX + std::cos(tickAngle) * outer,
                          centreY + std::sin(tickAngle) * outer,
                          tick % 3 == 0 ? 1.2f : 0.8f);
    }

    const auto innerRadius = radius * 0.74f;
    graphics.setColour(juce::Colour{ 0xffe4ece2 });
    graphics.fillEllipse(centreX - innerRadius, centreY - innerRadius,
                         innerRadius * 2.0f, innerRadius * 2.0f);
    graphics.setColour(leafColour.withAlpha(0.26f));
    graphics.drawEllipse(centreX - innerRadius, centreY - innerRadius,
                         innerRadius * 2.0f, innerRadius * 2.0f, 1.0f);

    const auto pointerLength = radius * 0.56f;
    juce::Path pointer;
    pointer.startNewSubPath(centreX, centreY);
    pointer.lineTo(centreX + std::cos(angle) * pointerLength,
                   centreY + std::sin(angle) * pointerLength);
    graphics.setColour(brightLeafColour);
    graphics.strokePath(pointer, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
    graphics.setColour(warmAccentColour);
    graphics.fillEllipse(centreX - 3.0f, centreY - 3.0f, 6.0f, 6.0f);
}

AuraDial::AuraDial(juce::AudioProcessorValueTreeState& parameters, AuraLookAndFeel& lookAndFeel,
                   const juce::String& parameterID, const juce::String& title,
                   const juce::String& helper, bool isSemitoneControl, double defaultValue)
    : displaysSemitones(isSemitoneControl)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 0.75f,
                               juce::MathConstants<float>::pi * 2.25f, true);
    slider.setLookAndFeel(&lookAndFeel);
    slider.setScrollWheelEnabled(false);
    slider.setDoubleClickReturnValue(true, defaultValue);
    addAndMakeVisible(slider);

    titleLabel.setText(title.toUpperCase(), juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, brightLeafColour);
    titleLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    addAndMakeVisible(titleLabel);

    valueLabel.setJustificationType(juce::Justification::centred);
    valueLabel.setColour(juce::Label::textColourId, brightLeafColour);
    valueLabel.setFont(juce::Font(16.0f, juce::Font::bold));
    addAndMakeVisible(valueLabel);

    helperLabel.setText(helper.toUpperCase(), juce::dontSendNotification);
    helperLabel.setJustificationType(juce::Justification::centred);
    helperLabel.setColour(juce::Label::textColourId, mutedTextColour);
    helperLabel.setFont(juce::Font(9.0f));
    addAndMakeVisible(helperLabel);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        parameters, parameterID, slider);
    refreshValue();
}

void AuraDial::resized()
{
    const auto bounds = getLocalBounds();
    titleLabel.setBounds(bounds.removeFromTop(22));
    slider.setBounds(bounds.removeFromTop(132).reduced(5, 0));
    valueLabel.setBounds(bounds.removeFromTop(25));
    helperLabel.setBounds(bounds.removeFromTop(19));
}

void AuraDial::refreshValue()
{
    const auto value = slider.getValue();
    if (displaysSemitones)
    {
        const auto rounded = juce::roundToInt(value);
        const auto sign = rounded > 0 ? "+" : "";
        valueLabel.setText(sign + juce::String(rounded) + " st", juce::dontSendNotification);
    }
    else
    {
        valueLabel.setText(juce::String(juce::roundToInt(value)) + "%", juce::dontSendNotification);
    }
}

AuraAudioProcessorEditor::AuraAudioProcessorEditor(AuraAudioProcessor& audioProcessor)
    : AudioProcessorEditor(&audioProcessor),
      processor(audioProcessor),
      shiftDial(audioProcessor.getParameters(), lookAndFeel, "shift", "Shift", "Semitones", true, 0.0),
      mixDial(audioProcessor.getParameters(), lookAndFeel, "mix", "Mix", "Dry / wet", false, 100.0),
      bloomDial(audioProcessor.getParameters(), lookAndFeel, "bloom", "Bloom", "Spectral softness", false, 18.0)
{
    setOpaque(true);
    setSize(920, 520);
    setResizable(false, false);
    addAndMakeVisible(shiftDial);
    addAndMakeVisible(mixDial);
    addAndMakeVisible(bloomDial);
    startTimerHz(30);
}

AuraAudioProcessorEditor::~AuraAudioProcessorEditor()
{
    stopTimer();
    shiftDial.setLookAndFeel(nullptr);
    mixDial.setLookAndFeel(nullptr);
    bloomDial.setLookAndFeel(nullptr);
}

void AuraAudioProcessorEditor::paint(juce::Graphics& graphics)
{
    graphics.fillAll(backgroundColour);
    const auto panelBounds = getLocalBounds().toFloat().reduced(8.0f);
    graphics.setGradientFill(juce::ColourGradient(panelColour, panelBounds.getX(), panelBounds.getY(),
                                                   backgroundColour, panelBounds.getRight(),
                                                   panelBounds.getBottom(), false));
    graphics.fillRoundedRectangle(panelBounds, 23.0f);
    graphics.setColour(softLineColour.withAlpha(0.78f));
    graphics.drawRoundedRectangle(panelBounds, 23.0f, 1.0f);

    // Fine seed-like marks give the dark field a little depth without competing with the controls.
    graphics.setColour(leafColour.withAlpha(0.10f));
    for (int row = 0; row < 8; ++row)
    {
        for (int column = 0; column < 15; ++column)
        {
            const auto x = 34.0f + static_cast<float>(column) * 58.0f + static_cast<float>(row % 2) * 25.0f;
            const auto y = 93.0f + static_cast<float>(row) * 49.0f;
            if (x < panelBounds.getRight() - 18.0f && y < panelBounds.getBottom() - 34.0f)
                graphics.fillEllipse(x, y, 1.5f, 1.5f);
        }
    }

    // Header mark: a small stem with two leaves.
    juce::Path stem;
    stem.startNewSubPath(31.0f, 43.0f);
    stem.cubicTo(34.0f, 35.0f, 36.0f, 31.0f, 42.0f, 26.0f);
    stem.cubicTo(42.0f, 34.0f, 40.0f, 39.0f, 31.0f, 43.0f);
    graphics.setColour(leafColour);
    graphics.strokePath(stem, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
    juce::Path leaf;
    leaf.startNewSubPath(37.0f, 35.0f);
    leaf.cubicTo(36.0f, 28.0f, 29.0f, 27.0f, 26.0f, 28.0f);
    leaf.cubicTo(27.0f, 35.0f, 31.0f, 38.0f, 37.0f, 35.0f);
    graphics.strokePath(leaf, juce::PathStrokeType(1.3f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));

    graphics.setColour(brightLeafColour);
    graphics.setFont(juce::Font(13.0f, juce::Font::bold));
    graphics.drawText("355ERA", 53, 27, 90, 23, juce::Justification::centredLeft);
    graphics.setColour(mutedTextColour);
    graphics.setFont(juce::Font(9.0f));
    graphics.drawText("BOTANICAL SOUND OBJECTS", 145, 28, 230, 22, juce::Justification::centredLeft);

    graphics.setColour(softLineColour);
    graphics.drawLine(28.0f, 62.0f, static_cast<float>(getWidth() - 28), 62.0f, 1.0f);
    graphics.setColour(warmAccentColour.withAlpha(0.85f));
    graphics.fillEllipse(static_cast<float>(getWidth() - 174), 33.0f, 6.0f, 6.0f);
    graphics.setColour(mutedTextColour);
    graphics.setFont(juce::Font(9.0f));
    graphics.drawText("FFT FIELD  /  2048", getWidth() - 160, 26, 128, 20,
                      juce::Justification::centredRight);

    graphics.setColour(brightLeafColour);
    graphics.setFont(juce::Font(46.0f, juce::Font::bold));
    graphics.drawText("AURA", 38, 90, 248, 58, juce::Justification::centredLeft);
    graphics.setColour(aquaColour);
    graphics.setFont(juce::Font(10.0f, juce::Font::bold));
    graphics.drawText("FFT SPECTRAL SHIFTER", 42, 148, 235, 20, juce::Justification::centredLeft);
    graphics.setColour(mutedTextColour);
    graphics.setFont(juce::Font(11.0f));
    graphics.drawText("Refract a sound into new growth.", 42, 171, 252, 22,
                      juce::Justification::centredLeft);

    graphics.setColour(softLineColour.withAlpha(0.9f));
    graphics.drawLine(315.0f, 90.0f, 315.0f, 426.0f, 1.0f);
    graphics.setColour(aquaColour);
    graphics.setFont(juce::Font(9.0f, juce::Font::bold));
    graphics.drawText("SPECTRAL GARDEN", 342, 99, 200, 18, juce::Justification::centredLeft);
    graphics.setColour(mutedTextColour);
    graphics.setFont(juce::Font(9.0f));
    graphics.drawText("Shift the pitch field, then soften its edges.", 342, 117, 360, 18,
                      juce::Justification::centredLeft);

    drawLilyVisualizer(graphics, { 166.0f, 316.0f }, processor.getInputLevel());
    graphics.setColour(mutedTextColour);
    graphics.setFont(juce::Font(8.5f, juce::Font::bold));
    graphics.drawText("LIVE LEVEL", 97, 418, 138, 16, juce::Justification::centred);

    graphics.setColour(softLineColour);
    graphics.drawLine(30.0f, 457.0f, static_cast<float>(getWidth() - 30), 457.0f, 1.0f);
    graphics.setColour(mutedTextColour);
    graphics.setFont(juce::Font(9.0f));
    graphics.drawText("2048 FFT  ·  4× OVERLAP  ·  LATENCY COMPENSATED", 36, 470, 420, 19,
                      juce::Justification::centredLeft);
    graphics.setColour(leafColour);
    graphics.drawText("FIELD SERIES  /  01", getWidth() - 200, 470, 160, 19,
                      juce::Justification::centredRight);
}

void AuraAudioProcessorEditor::resized()
{
    constexpr int firstDialX = 342;
    constexpr int dialWidth = 166;
    constexpr int dialGap = 11;
    constexpr int dialTop = 174;
    constexpr int dialHeight = 218;
    shiftDial.setBounds(firstDialX, dialTop, dialWidth, dialHeight);
    mixDial.setBounds(firstDialX + dialWidth + dialGap, dialTop, dialWidth, dialHeight);
    bloomDial.setBounds(firstDialX + (dialWidth + dialGap) * 2, dialTop, dialWidth, dialHeight);
}

void AuraAudioProcessorEditor::timerCallback()
{
    shiftDial.refreshValue();
    mixDial.refreshValue();
    bloomDial.refreshValue();
    repaint();
}

void AuraAudioProcessorEditor::drawLilyVisualizer(juce::Graphics& graphics,
                                                  juce::Point<float> centre, float level)
{
    const auto response = juce::jlimit(0.0f, 1.0f, level * 5.0f);
    const auto pulse = response * 10.0f;
    const auto haloRadius = 94.0f + response * 3.0f;
    graphics.setColour(lilyColour.withAlpha(0.19f));
    graphics.fillEllipse(centre.x - 96.0f, centre.y - 96.0f, 192.0f, 192.0f);
    graphics.setColour(softLineColour.withAlpha(0.95f));
    graphics.drawEllipse(centre.x - haloRadius, centre.y - haloRadius,
                         haloRadius * 2.0f, haloRadius * 2.0f, 1.0f);
    graphics.setColour(leafColour.withAlpha(0.46f));
    graphics.drawEllipse(centre.x - 76.0f, centre.y - 76.0f, 152.0f, 152.0f, 1.0f);

    for (int tick = 0; tick < 24; ++tick)
    {
        const auto angle = juce::MathConstants<float>::twoPi * static_cast<float>(tick) / 24.0f;
        const auto innerRadius = 81.0f;
        const auto outerRadius = innerRadius + 3.0f + response * 5.0f;
        graphics.setColour(leafColour.withAlpha(0.32f + response * 0.32f));
        graphics.drawLine(centre.x + std::sin(angle) * innerRadius,
                          centre.y - std::cos(angle) * innerRadius,
                          centre.x + std::sin(angle) * outerRadius,
                          centre.y - std::cos(angle) * outerRadius,
                          tick % 3 == 0 ? 1.4f : 0.8f);
    }

    const auto drawPetal = [&graphics, centre](float angle, float length, float halfWidth,
                                                juce::Colour fill, float alpha)
    {
        juce::Path petal;
        petal.startNewSubPath(0.0f, -8.0f);
        petal.cubicTo(halfWidth, -length * 0.36f, halfWidth * 1.05f, -length * 0.82f,
                      0.0f, -length);
        petal.cubicTo(-halfWidth * 1.05f, -length * 0.82f, -halfWidth, -length * 0.36f,
                      0.0f, -8.0f);
        petal.closeSubPath();
        petal.applyTransform(juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));
        graphics.setColour(fill.withAlpha(alpha));
        graphics.fillPath(petal);
        graphics.setColour(leafColour.withAlpha(0.58f));
        graphics.strokePath(petal, juce::PathStrokeType(0.9f));
    };

    const auto outerLength = 57.0f + pulse;
    for (int petal = 0; petal < 6; ++petal)
    {
        const auto angle = juce::MathConstants<float>::twoPi * static_cast<float>(petal) / 6.0f;
        drawPetal(angle, outerLength, 13.0f + response * 1.5f, lilyColour, 0.68f);
    }
    const auto innerLength = 42.0f + pulse * 0.65f;
    for (int petal = 0; petal < 6; ++petal)
    {
        const auto angle = juce::MathConstants<float>::twoPi * (static_cast<float>(petal) + 0.5f) / 6.0f;
        drawPetal(angle, innerLength, 10.0f, lilyLightColour, 0.92f);
    }

    for (int stamen = 0; stamen < 6; ++stamen)
    {
        const auto angle = juce::MathConstants<float>::twoPi * static_cast<float>(stamen) / 6.0f;
        const auto x1 = centre.x + std::sin(angle) * 4.0f;
        const auto y1 = centre.y - std::cos(angle) * 4.0f;
        const auto x2 = centre.x + std::sin(angle) * 14.0f;
        const auto y2 = centre.y - std::cos(angle) * 14.0f;
        graphics.setColour(warmAccentColour.withAlpha(0.9f));
        graphics.drawLine(x1, y1, x2, y2, 1.2f);
        graphics.fillEllipse(x2 - 2.0f, y2 - 2.0f, 4.0f, 4.0f);
    }

    graphics.setColour(juce::Colour{ 0xfffbf8ed });
    graphics.fillEllipse(centre.x - 9.0f, centre.y - 9.0f, 18.0f, 18.0f);
    graphics.setColour(warmAccentColour);
    graphics.fillEllipse(centre.x - 4.0f, centre.y - 4.0f, 8.0f, 8.0f);
}
