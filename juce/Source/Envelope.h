#pragma once

#include <JuceHeader.h>

class RobinEnvelope : public juce::GroupComponent {
public:
  RobinEnvelope(juce::ValueTree tree, juce::UndoManager* undoManager);
  ~RobinEnvelope() override;
  juce::Rectangle<int> getInnerBounds() const;
  juce::Point<float> getHandleRadius() const;
  juce::Point<float> eventPointToEnvelopePoint(const juce::Point<float>& point) const;
  juce::Point<float> getEnvelopePoint(uintptr_t index) const;
  int getLastEnvelopePoint() const;
  void setEnvelopePoint(uintptr_t index, const juce::Point<float>& point);
  void removeEnvelopePoint(uintptr_t index);
  void addEnvelopePoint(const juce::Point<float>& point);

  //==============================================================================
  void paint(juce::Graphics&) override;
  void resized() override;

  //==============================================================================
  void mouseMove(const juce::MouseEvent& event) override;
  void mouseDown(const juce::MouseEvent& event) override;
  void mouseDrag(const juce::MouseEvent& event) override;
  void mouseUp(const juce::MouseEvent& event) override;
  void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

private:
  juce::ValueTree tree;
  juce::UndoManager* undoManager;

  float windowTimeWidth = 1.f;
  int32_t hoveringPoint = -1;
  int32_t draggingPoint = -1;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RobinEnvelope)
};
