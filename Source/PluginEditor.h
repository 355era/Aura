#pragma once

#include "PluginProcessor.h"

#include <vector>

class AuraLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider(juce::Graphics& graphics, int x, int y, int width, int height,
                          float sliderPosition, float startAngle, float endAngle,
                          juce::Slider& slider) override;
    void drawButtonText(juce::Graphics& graphics, juce::TextButton& button,
                       bool isMouseOverButton, bool isButtonDown) override;
    void drawComboBoxTextWhenNothingSelected(juce::Graphics& graphics, juce::ComboBox& box,
                                            juce::Label& label) override;
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

class AuraGrainPad final : public juce::Component, private juce::Timer
{
public:
    explicit AuraGrainPad(AuraAudioProcessor& processor);
    void paint(juce::Graphics& graphics) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;

private:
    void timerCallback() override;
    void updateFromPosition(juce::Point<float> position);

    AuraAudioProcessor& processor;
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
    enum class Page { spectral, grain, modulation, effects, settings };
    void timerCallback() override;
    void setActivePage(Page page);
    void drawLfoScope(juce::Graphics& graphics, juce::Rectangle<float> bounds);
    void refreshPresetMenu(int preferredItemID = 0);
    void loadSelectedPreset();
    void beginSavingPreset();
    void savePresetFromEditor();
    void cancelSavingPreset();
    void importPresetBank();
    void exportPresetBank();
    void importPresetFile(const juce::File& file);
    void setParameterFromPreset(const juce::String& parameterID, float value);

    AuraAudioProcessor& audioProcessor;
    AuraLookAndFeel lookAndFeel;
    AuraDial shiftDial;
    AuraDial mixDial;
    AuraDial bloomDial;
    AuraDial blurDial;
    AuraDial lowCutDial;
    AuraDial highCutDial;
    AuraDial grainDial;
    AuraDial grainSizeDial;
    AuraDial grainPitchDial;
    AuraDial densityDial;
    AuraDial feedbackDial;
    AuraDial toneDial;
    AuraDial widthDial;
    AuraDial lfoRateDial;
    AuraDial lfoDepthDial;
    AuraDial outputDial;
    AuraDial mistDial;
    AuraDial petalDial;
    AuraDial warmthDial;
    AuraDial rippleDial;
    AuraDial echoDial;
    AuraGrainPad grainPad;
    juce::TextButton spectralTab{ "SPECTRAL" };
    juce::TextButton grainTab{ "GRAIN" };
    juce::TextButton modulationTab{ "MOD" };
    juce::TextButton effectsTab{ "EFFECTS" };
    juce::TextButton settingsTab{ "SETTINGS" };
    juce::ComboBox fftSelector;
    juce::ComboBox lfoShapeSelector;
    juce::ComboBox lfoTargetSelector;
    juce::ToggleButton spectralEnableButton{ "SPECTRAL ENGINE" };
    juce::ToggleButton grainEnableButton{ "GRAIN LAYER" };
    juce::ToggleButton toneEnableButton{ "TONE FILTER" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> fftAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lfoShapeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lfoTargetAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> spectralEnableAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> grainEnableAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> toneEnableAttachment;
    juce::Label presetLabel;
    juce::ComboBox presetSelector;
    juce::TextButton savePresetButton{ "SAVE PRESET" };
    juce::TextButton loadBankButton{ "LOAD BANK" };
    juce::TextButton saveBankButton{ "SAVE BANK" };
    juce::TextEditor presetNameEditor;
    juce::TextButton confirmPresetButton{ "SAVE" };
    juce::TextButton cancelPresetButton{ "CANCEL" };
    std::vector<juce::File> userPresetFiles;
    std::unique_ptr<juce::FileChooser> bankFileChooser;
    Page activePage = Page::spectral;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AuraAudioProcessorEditor)
};
