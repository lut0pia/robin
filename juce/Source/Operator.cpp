#include "Operator.h"

//==============================================================================
RobinOperator::RobinOperator(juce::ValueTree tree, juce::UndoManager* undoManager)
  : volumeEnvelope(tree.getChildWithName("VolumeEnvelope"), undoManager),
  pitchEnvelope(tree.getChildWithName("PitchEnvelope"), undoManager) {
  this->tree = tree;
  this->undoManager = undoManager;

  setText("Operator Settings");

  frequencyOffsetLabel.setText("Frequency Offset", juce::dontSendNotification);
  frequencyOffsetLabel.attachToComponent(&frequencyOffset, true);
  frequencyOffset.setRange(-1024.0, 1024.0, 0.01);
  frequencyOffset.setSkewFactor(0.5, true);
  frequencyOffset.addListener(this);
  addAndMakeVisible(frequencyOffset);

  frequencyRatioLabel.setText("Frequency Ratio", juce::dontSendNotification);
  frequencyRatioLabel.attachToComponent(&frequencyRatio, true);
  frequencyRatio.setRange(0.0, 64.0, 0.01);
  frequencyRatio.setSkewFactor(0.5);
  frequencyRatio.addListener(this);
  addAndMakeVisible(frequencyRatio);

  noiseLabel.setText("Noise", juce::dontSendNotification);
  noiseLabel.attachToComponent(&noise, true);
  noise.setRange(0.0, 1.0, 0.01);
  noise.addListener(this);
  addAndMakeVisible(noise);

  outputLabel.setText("Output", juce::dontSendNotification);
  outputLabel.attachToComponent(&output, true);
  output.setRange(0.0, 1.0, 0.01);
  output.addListener(this);
  addAndMakeVisible(output);

  volumeEnvelopeButton.setButtonText("Volume");
  volumeEnvelopeButton.setClickingTogglesState(true);
  volumeEnvelopeButton.setRadioGroupId(1002);
  volumeEnvelopeButton.onStateChange = [this]() {
    if(volumeEnvelopeButton.getState() == juce::Button::ButtonState::buttonDown) {
      selectEnvelope(RobinEnvelopeType::Volume);
    }
  };
  addAndMakeVisible(volumeEnvelopeButton);

  pitchEnvelopeButton.setButtonText("Pitch");
  pitchEnvelopeButton.setClickingTogglesState(true);
  pitchEnvelopeButton.setRadioGroupId(1002);
  pitchEnvelopeButton.onStateChange = [this]() {
    if(pitchEnvelopeButton.getState() == juce::Button::ButtonState::buttonDown) {
      selectEnvelope(RobinEnvelopeType::Pitch);
    }
  };
  addAndMakeVisible(pitchEnvelopeButton);

  volumeEnvelope.setText("Volume Envelope");
  addAndMakeVisible(volumeEnvelope);

  pitchEnvelope.setText("Pitch Envelope");
  addChildComponent(pitchEnvelope);

  updateFromValueTree();
  volumeEnvelopeButton.setState(juce::Button::ButtonState::buttonDown);
}

RobinOperator::~RobinOperator() {
}

void RobinOperator::selectEnvelope(RobinEnvelopeType type) {
  volumeEnvelope.setVisible(false);
  pitchEnvelope.setVisible(false);
  switch(type) {
    case RobinEnvelopeType::Volume:
      volumeEnvelope.setVisible(true);
      break;
    case RobinEnvelopeType::Pitch:
      pitchEnvelope.setVisible(true);
      break;
  }
}

void RobinOperator::updateFromValueTree() {
  frequencyOffset.setValue(tree.getProperty("FreqOffset"));
  frequencyRatio.setValue(tree.getProperty("FreqRatio"));
  noise.setValue(tree.getProperty("Noise"));
  output.setValue(tree.getProperty("Output"));

  volumeEnvelope.repaint();
  pitchEnvelope.repaint();
}

//==============================================================================
void RobinOperator::resized() {
  { // Operator settings
    auto panelBounds = getLocalBounds().reduced(16).withTrimmedLeft(110);
    frequencyOffset.setBounds(panelBounds.removeFromTop(32));
    frequencyRatio.setBounds(panelBounds.removeFromTop(32));
    noise.setBounds(panelBounds.removeFromTop(32));
    output.setBounds(panelBounds.removeFromTop(32));
  }

  { // Envelope buttons
    auto buttonBounds = getLocalBounds().reduced(2).removeFromBottom(128).translated(0, -20);
    buttonBounds.setHeight(20);
    volumeEnvelopeButton.setBounds(buttonBounds.getProportion(juce::Rectangle<float>{0.f, 0.f, 0.5f, 1.f}));
    pitchEnvelopeButton.setBounds(buttonBounds.getProportion(juce::Rectangle<float>{0.5f, 0.f, 0.5f, 1.f}));
  }

  { // Envelopes
    volumeEnvelope.setBounds(getLocalBounds().removeFromBottom(128).reduced(2));
    pitchEnvelope.setBounds(getLocalBounds().removeFromBottom(128).reduced(2));
  }
}

//==============================================================================
void RobinOperator::sliderValueChanged(juce::Slider* slider) {
  if(slider == &frequencyOffset) {
    tree.setProperty("FreqOffset", slider->getValue(), undoManager);
  } else if(slider == &frequencyRatio) {
    tree.setProperty("FreqRatio", slider->getValue(), undoManager);
  } else if(slider == &noise) {
    tree.setProperty("Noise", slider->getValue(), undoManager);
  } else if(slider == &output) {
    tree.setProperty("Output", slider->getValue(), undoManager);
  }
}