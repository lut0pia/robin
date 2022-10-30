#pragma once

#include <JuceHeader.h>
#include "Envelope.h"

enum class RobinEnvelopeType {
  Volume,
  Pitch,
};

class RobinOperator : public juce::GroupComponent, public juce::Slider::Listener {
public:
  RobinOperator(juce::ValueTree tree, juce::UndoManager* undoManager);
  ~RobinOperator() override;
  void selectEnvelope(RobinEnvelopeType type);
  void updateFromValueTree();

  //==============================================================================
  void resized() override;

  //==============================================================================
  virtual void sliderValueChanged(juce::Slider* slider) override;

private:
  juce::ValueTree tree;
  juce::UndoManager* undoManager;

  juce::Slider frequencyOffset;
  juce::Label frequencyOffsetLabel;

  juce::Slider frequencyRatio;
  juce::Label frequencyRatioLabel;

  juce::Slider noise;
  juce::Label noiseLabel;

  juce::Slider output;
  juce::Label outputLabel;

  juce::TextButton volumeEnvelopeButton;
  juce::TextButton pitchEnvelopeButton;

  RobinEnvelope volumeEnvelope;
  RobinEnvelope pitchEnvelope;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RobinOperator)
};
