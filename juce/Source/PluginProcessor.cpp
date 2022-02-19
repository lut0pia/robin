#include "PluginProcessor.h"
#include "PluginEditor.h"

#define RBN_IMPLEMENTATION
#include "../../robin.h"

//==============================================================================
RobinAudioProcessor::RobinAudioProcessor()
  : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)) {
}

RobinAudioProcessor::~RobinAudioProcessor() {
  valueTree.removeListener(this);
}

//==============================================================================
const juce::String RobinAudioProcessor::getName() const {
  return JucePlugin_Name;
}

bool RobinAudioProcessor::acceptsMidi() const {
  return true;
}

bool RobinAudioProcessor::producesMidi() const {
  return false;
}

bool RobinAudioProcessor::isMidiEffect() const {
  return false;
}

double RobinAudioProcessor::getTailLengthSeconds() const {
  return 0.0;
}

int RobinAudioProcessor::getNumPrograms() {
  return RBN_PROGRAM_COUNT;
}

int RobinAudioProcessor::getCurrentProgram() {
  return currentProgram;
}

void RobinAudioProcessor::setCurrentProgram(int index) {
  currentProgram = index;
}

const juce::String RobinAudioProcessor::getProgramName(int index) {
  return {};
}

void RobinAudioProcessor::changeProgramName(int index, const juce::String& newName) {
}

//==============================================================================
void RobinAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
  rbn_config robinConfig {};
  robinConfig.sample_rate = sampleRate;
  rbn_init(&robinInstance, &robinConfig);

  // Simple sinewave as default program
  robinInstance.programs[0].operators[0].freq_ratio = 1.f;
  robinInstance.programs[0].operators[0].output = 1.f;
  robinInstance.programs[0].operators[0].volume_envelope.points[0].value = 1.f;
  
  rbn_refresh(&robinInstance);

  createValueTreeFromRobin();
}

void RobinAudioProcessor::releaseResources() {
  // When playback stops, you can use this as an opportunity to free up any
  // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool RobinAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
#if JucePlugin_IsMidiEffect
  juce::ignoreUnused(layouts);
  return true;
#else
  // This is the place where you check if the layout is supported.
  // In this template code we only support mono or stereo.
  // Some plugin hosts, such as certain GarageBand versions, will only
  // load plugins that support stereo bus layouts.
  if(layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
    && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
    return false;

  // This checks if the input layout matches the output layout
#if ! JucePlugin_IsSynth
  if(layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
    return false;
#endif

  return true;
#endif
}
#endif

void RobinAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
  juce::ScopedNoDenormals noDenormals;

  buffer.clear();

  rbn_output_config outputConfig;
  outputConfig.left_buffer = buffer.getWritePointer(0);
  outputConfig.right_buffer = buffer.getWritePointer(1);
  outputConfig.stride = 1;
  outputConfig.sample_format = rbn_f32;

  int currentSample = 0;
  for(const auto metadata : midiMessages) {
    outputConfig.sample_count = metadata.samplePosition - currentSample;
    rbn_render(&robinInstance, &outputConfig);
    currentSample = metadata.samplePosition;

    juce::MidiMessage juceMessage = metadata.getMessage();
    rbn_msg robinMessage {};
    // The channel data is omitted so the first byte is offset by one
    memcpy(robinMessage.u8 + 1, juceMessage.getRawData(), 3);
    rbn_send_msg(&robinInstance, robinMessage);
  }

  outputConfig.sample_count = buffer.getNumSamples() - currentSample;
  rbn_render(&robinInstance, &outputConfig);

  midiMessages.clear();
}

//==============================================================================
bool RobinAudioProcessor::hasEditor() const {
  return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* RobinAudioProcessor::createEditor() {
  //return new juce::GenericAudioProcessorEditor(*this);
  return new RobinAudioProcessorEditor(*this);
}

//==============================================================================
void RobinAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
  destData.append(&robinInstance, sizeof(rbn_instance));
}

void RobinAudioProcessor::getCurrentProgramStateInformation(juce::MemoryBlock& destData) {
  destData.append(robinInstance.programs, sizeof(rbn_program));
}

void RobinAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
  if(sizeInBytes == sizeof(rbn_instance)) {
    memcpy(&robinInstance, data, sizeInBytes);
  }
}

void RobinAudioProcessor::setCurrentProgramStateInformation(const void* data, int sizeInBytes) {
  memcpy(robinInstance.programs, data, sizeInBytes);
}

//==============================================================================
juce::UndoManager* RobinAudioProcessor::getUndoManager() {
  return &undoManager;
}
juce::ValueTree RobinAudioProcessor::getValueTree() {
  return valueTree;
}
juce::ValueTree RobinAudioProcessor::getCurrentProgramTree() {
  return valueTree.getChild(getCurrentProgram());
}
juce::ValueTree RobinAudioProcessor::getCurrentProgramOperatorTree(int operatorIndex) {
  return getCurrentProgramTree().getChildWithName("Operators").getChild(operatorIndex);
}
juce::ValueTree RobinAudioProcessor::getCurrentProgramOperatorMatrix(int operatorIndex, int i, int j) {
  return valueTree;
}
void RobinAudioProcessor::valueTreePropertyChanged(juce::ValueTree& treeWhosePropertyHasChanged, const juce::Identifier& property) {
  updateRobinFromValueTree();
}
void RobinAudioProcessor::createValueTreeFromRobin() {
  //  Robin
  //    Program+
  //      Index
  //      Operators+ [FreqOffset, FreqRatio, Noise, Output]
  //        Envelope+ [Name]
  //          Point+ [Time, Value]
  //        Modulations
  //          Modulation+ [Value]
  valueTree = juce::ValueTree("Robin");
  for(int programIndex = 0; programIndex < getNumPrograms(); programIndex++) {
    juce::ValueTree programTree("Program");
    rbn_program* program = robinInstance.programs + programIndex;
    programTree.setProperty("Index", programIndex, &undoManager);

    juce::ValueTree operatorsTree("Operators");
    for(int operatorIndex = 0; operatorIndex < RBN_OPERATOR_COUNT; operatorIndex++) {
      juce::ValueTree operatorTree("Operator");
      rbn_operator* op = program->operators + operatorIndex;
      operatorTree.setProperty("FreqOffset", op->freq_offset, &undoManager);
      operatorTree.setProperty("FreqRatio", op->freq_ratio, &undoManager);
      operatorTree.setProperty("Noise", op->noise, &undoManager);
      operatorTree.setProperty("Output", op->output, &undoManager);

      juce::ValueTree volumeEnvelopeTree("VolumeEnvelope");
      volumeEnvelopeTree.setProperty("ReleaseTime", op->volume_envelope.release_time, &undoManager);
      for(int pointIndex = 0; pointIndex < RBN_ENVPT_COUNT; pointIndex++) {
        juce::ValueTree pointTree("Point");
        pointTree.setProperty("Time", op->volume_envelope.points[pointIndex].time, &undoManager);
        pointTree.setProperty("Value", op->volume_envelope.points[pointIndex].value, &undoManager);
        volumeEnvelopeTree.addChild(pointTree, pointIndex, &undoManager);
      }
      operatorTree.appendChild(volumeEnvelopeTree, &undoManager);

      juce::ValueTree modulationsTree("Modulations");
      for(int modulationIndex = 0; modulationIndex < RBN_OPERATOR_COUNT; modulationIndex++) {
        juce::ValueTree modulation("Modulation");
        modulation.setProperty("Value", program->op_matrix[programIndex][modulationIndex], &undoManager);
        modulationsTree.addChild(modulation, modulationIndex, &undoManager);
      }
      operatorTree.appendChild(modulationsTree, &undoManager);

      operatorsTree.addChild(operatorTree, operatorIndex, &undoManager);
    }
    programTree.appendChild(operatorsTree, &undoManager);

    valueTree.addChild(programTree, programIndex, &undoManager);
  }

  valueTree.addListener(this);
}
void RobinAudioProcessor::updateRobinFromValueTree() {
  for(int programIndex = 0; programIndex < getNumPrograms(); programIndex++) {
    juce::ValueTree programTree = valueTree.getChild(programIndex);
    rbn_program* program = robinInstance.programs + programIndex;

    juce::ValueTree operatorsTree = programTree.getChildWithName("Operators");
    for(int operatorIndex = 0; operatorIndex < RBN_OPERATOR_COUNT; operatorIndex++) {
      juce::ValueTree operatorTree = operatorsTree.getChild(operatorIndex);
      rbn_operator* op = program->operators + operatorIndex;
      op->freq_offset = operatorTree.getProperty("FreqOffset");
      op->freq_ratio = operatorTree.getProperty("FreqRatio");
      op->noise = operatorTree.getProperty("Noise");
      op->output = operatorTree.getProperty("Output");

      juce::ValueTree volumeEnvelopeTree = operatorTree.getChildWithName("VolumeEnvelope");
      op->volume_envelope.release_time = volumeEnvelopeTree.getProperty("ReleaseTime");
      for(int pointIndex = 0; pointIndex < RBN_ENVPT_COUNT; pointIndex++) {
        juce::ValueTree pointTree = volumeEnvelopeTree.getChild(pointIndex);
        op->volume_envelope.points[pointIndex].time = pointTree.getProperty("Time");
        op->volume_envelope.points[pointIndex].value = pointTree.getProperty("Value");
      }

      juce::ValueTree modulationsTree = operatorTree.getChildWithName("Modulations");
      for(int modulationIndex = 0; modulationIndex < RBN_OPERATOR_COUNT; modulationIndex++) {
        juce::ValueTree modulationTree = modulationsTree.getChild(operatorIndex);
        program->op_matrix[operatorIndex][modulationIndex] = modulationTree.getProperty("Value");
      }
    }
  }

  rbn_refresh(&robinInstance);
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
  return new RobinAudioProcessor();
}
