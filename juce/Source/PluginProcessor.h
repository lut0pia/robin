#pragma once

#include <JuceHeader.h>
#include "../../robin.h"

class RobinAudioProcessor : public juce::AudioProcessor, public juce::ValueTree::Listener {
public:
  //==============================================================================
  RobinAudioProcessor();
  ~RobinAudioProcessor() override;

  //==============================================================================
  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
  bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

  void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

  //==============================================================================
  juce::AudioProcessorEditor* createEditor() override;
  bool hasEditor() const override;

  //==============================================================================
  const juce::String getName() const override;

  bool acceptsMidi() const override;
  bool producesMidi() const override;
  bool isMidiEffect() const override;
  double getTailLengthSeconds() const override;

  //==============================================================================
  int getNumPrograms() override;
  int getCurrentProgram() override;
  void setCurrentProgram(int index) override;
  const juce::String getProgramName(int index) override;
  void changeProgramName(int index, const juce::String& newName) override;

  //==============================================================================
  virtual void getStateInformation(juce::MemoryBlock& destData) override;
  virtual void setStateInformation(const void* data, int sizeInBytes) override;

  //==============================================================================
  virtual void valueTreePropertyChanged(juce::ValueTree& treeWhosePropertyHasChanged, const juce::Identifier& property) override;
  void createValueTreeFromRobin();
  void updateRobinFromValueTree();
  const rbn_instance& getRobinInstance() const;

protected:
  //==============================================================================
  rbn_instance robinInstance;
  int currentProgram = 0;
  juce::ValueTree valueTree;
  juce::UndoManager undoManager;

private:
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RobinAudioProcessor)
};
