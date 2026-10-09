#pragma once

#include "EditorWidgets.h"
#include "LfoEditor.h"
#include "MatrixPanel.h"
#include "MeshDisplay.h"
#include "PluginProcessor.h"
#include "PresetBar.h"

class EmberlineEditor final : public juce::AudioProcessorEditor,
                              public juce::DragAndDropContainer,
                              private juce::Timer
{
public:
    explicit EmberlineEditor (EmberlineProcessor&);
    ~EmberlineEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    enum Page { visualizers, lfosAndChaos, matrixPage, fxChain, filtersAndEnvs, numPages };

    Panel& addPanel (juce::Component& parent, const juce::String& title, juce::Colour accent, int columns = 0);
    void showPage (int page);
    void buildTop();
    void buildVisualizers();
    void buildLfosAndChaos();
    void buildFx();
    void buildFiltersAndEnvs();

    void layoutVisualizers();
    void layoutLfosAndChaos();
    void layoutFx();
    void layoutFiltersAndEnvs();
    std::array<int, ember::fx::numEffects> readFxOrder() const;
    void fxDrag (Panel&, const juce::MouseEvent&);
    void timerCallback() override;

    EmberlineProcessor& processor;
    EmberLookAndFeel lnf;
    juce::Image phoenix;

    // fixed header + top region
    std::vector<std::unique_ptr<SourceChip>> chips;
    std::vector<Panel*> oscPanels;
    Panel *masterPanel = nullptr, *macroPanel = nullptr;
    juce::Rectangle<int> contentFrame;

    // tab bar + pages
    std::array<juce::TextButton, numPages> tabs;
    std::array<juce::Component, numPages> pages;

    std::deque<Panel> panels;
    std::deque<LfoEditor> lfoEditors;
    std::deque<LfoPreview> lfoPreviews;
    std::deque<ChaosDisplay> chaosDisplays;
    std::deque<EnvDisplay> envDisplays;
    WavetableMesh mesh;
    PresetBar presetBar;
    MatrixPage matrix;

    std::vector<Panel*> lfoPanels, envPanels, filterRow;
    Panel* chaosPanel = nullptr;
    std::array<Panel*, ember::fx::numEffects> fxPanels {};
    std::array<std::atomic<float>*, ember::fx::numEffects> fxSlot {};
    std::array<int, ember::fx::numEffects> lastFxOrder { 0, 1, 2, 3 };
    juce::Label fxHint;
    juce::String shownMacroNames { "?" }, shownTableNames { "?" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EmberlineEditor)
};
