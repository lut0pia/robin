#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Envelope.h"
#include "Operator.h"
#include "../../robin.h"

class RobinAudioProcessorEditor : public juce::AudioProcessorEditor, public juce::Slider::Listener {
public:
  RobinAudioProcessorEditor(RobinAudioProcessor& processor, juce::ValueTree tree, juce::UndoManager* undoManager);
  ~RobinAudioProcessorEditor() override;
  void selectOperator(uint8_t index);
  void updateFromValueTree();

  //==============================================================================
  void paint(juce::Graphics&) override;
  void resized() override;

  //==============================================================================
  virtual void sliderValueChanged(juce::Slider* slider) override;

private:
  // This reference is provided as a quick way for your editor to
  // access the processor object that created it.
  RobinAudioProcessor& audioProcessor;
  juce::ValueTree tree;
  juce::ValueTree operatorsTree;
  juce::UndoManager* undoManager;

  //==============================================================================
  int operatorIndex = 0;

  //==============================================================================
  juce::Array<std::shared_ptr<RobinOperator>> operators;
  juce::Array<std::shared_ptr<juce::TextButton>> operatorButtons;
  juce::Array<std::shared_ptr<juce::Slider>> operatorMatrix;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RobinAudioProcessorEditor)
};
