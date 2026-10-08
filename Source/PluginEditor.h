#pragma once

#include "PluginProcessor.h"

#include <vector>

class AuraLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider(juce::Graphics& graphics, int x, int y, int width, int height,
                          float sliderPosition, float startAngle, float endAngle,
                          juce::Slider& slider) override;
};

class AuraDial final : public juce::Component
{
public:
    AuraDial(juce::AudioProcessorValueTreeState& parameters, AuraLookAndFeel& lookAndFeel,
             const juce::String& parameterID, const juce::String& title,
             const juce::String& helper, double defaultValue,
             const juce::String& displayUnits = "%");

    void resized() override;
    void refreshValue();
    void setScale(float scale);

private:
    juce::Slider slider;
    juce::Label titleLabel;
    juce::Label valueLabel;
    juce::Label helperLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    juce::String units;
    float uiScale = 1.0f;
};

class AuraAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                       private juce::Timer
{
public:
    explicit AuraAudioProcessorEditor(AuraAudioProcessor& processor);
    ~AuraAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawLilyVisualizer(juce::Graphics& graphics, juce::Point<float> centre, float level);
    void refreshPresetMenu(int preferredItemID = 0);
    void loadSelectedPreset();
    void beginSavingPreset();
    void savePresetFromEditor();
    void cancelSavingPreset();
    void setParameterFromPreset(const juce::String& parameterID, float value);

    AuraAudioProcessor& processor;
    AuraLookAndFeel lookAndFeel;
    AuraDial shiftDial;
    AuraDial mixDial;
    AuraDial bloomDial;
    AuraDial grainDial;
    AuraDial grainSizeDial;
    AuraDial densityDial;
    AuraDial feedbackDial;
    juce::Label presetLabel;
    juce::ComboBox presetSelector;
    juce::TextButton savePresetButton{ "SAVE PRESET" };
    juce::TextEditor presetNameEditor;
    juce::TextButton confirmPresetButton{ "SAVE" };
    juce::TextButton cancelPresetButton{ "CANCEL" };
    std::vector<juce::File> userPresetFiles;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AuraAudioProcessorEditor)
};
