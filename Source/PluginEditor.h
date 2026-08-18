#pragma once

#include "EditorWidgets.h"
#include "LfoEditor.h"
#include "PluginProcessor.h"

class EmberlineEditor final : public juce::AudioProcessorEditor,
                              public juce::DragAndDropContainer
{
public:
    explicit EmberlineEditor (EmberlineProcessor&);
    ~EmberlineEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    Panel& addPanel (juce::Component& page, const juce::String& title, juce::Colour accent, int columns = 0);
    void showPage (bool mod);

    EmberlineProcessor& processor;
    EmberLookAndFeel lnf;

    juce::Component soundPage, modPage;
    juce::TextButton soundTab { "SOUND" }, modTab { "MOD" };
    juce::Label sourcesLabel;
    std::vector<std::unique_ptr<SourceChip>> chips;

    std::deque<Panel> panels;
    std::deque<LfoEditor> lfoEditors;
    std::deque<EnvDisplay> envDisplays;
    MatrixPanel matrix;
    std::vector<Panel*> oscRow, lowerRow, lfoRow, envRow;
    Panel* macroPanel = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EmberlineEditor)
};
