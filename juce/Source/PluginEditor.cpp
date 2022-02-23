#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "../../util/rbnutil_export.h"

//==============================================================================
RobinAudioProcessorEditor::RobinAudioProcessorEditor(RobinAudioProcessor& processor, juce::ValueTree tree, juce::UndoManager* undoManager)
  : AudioProcessorEditor(&processor), audioProcessor(processor), tree(tree), undoManager(undoManager),
  exportButton("Export") {
  operatorsTree = tree.getChildWithName("Operators");
  for(uintptr_t i = 0; i < RBN_OPERATOR_COUNT; i++) {
    std::shared_ptr<juce::TextButton> opButton = std::make_shared<juce::TextButton>();
    { // Set its title
      char nameBuffer[16];
      sprintf(nameBuffer, "Op%d", i + 1);
      opButton->setButtonText(nameBuffer);
    }
    opButton->setClickingTogglesState(true);
    opButton->setRadioGroupId(1001);
    opButton->onStateChange = [this, i]() {
      if(operatorButtons[i]->getState() == juce::Button::ButtonState::buttonDown) {
        selectOperator(i);
      }
    };

    addAndMakeVisible(*opButton);
    operatorButtons.add(opButton);

    juce::ValueTree operatorTree = operatorsTree.getChild(operatorIndex);
    std::shared_ptr<RobinOperator> op = std::make_shared<RobinOperator>(operatorTree, undoManager);
    addChildComponent(*op);
    operators.add(op);
  }

  for(uintptr_t i = 0; i < RBN_OPERATOR_COUNT * RBN_OPERATOR_COUNT; i++) {
    std::shared_ptr<juce::Slider> slider = std::make_shared<juce::Slider>();
    slider->setLookAndFeel(&getLookAndFeel());
    slider->setSliderStyle(juce::Slider::RotaryVerticalDrag);
    slider->setRange(-8.f, 8.f);
    slider->addListener(this);
    slider->setSkewFactor(0.5f, true);
    slider->setTextBoxStyle(juce::Slider::TextBoxBelow, true, 0, 0);
    slider->setColour(juce::Slider::thumbColourId, juce::Colour(128, 0, 0));
    addAndMakeVisible(*slider);
    operatorMatrix.add(slider);
  }

  updateFromValueTree();
  operatorButtons[0]->setState(juce::Button::ButtonState::buttonDown);

  setSize(1024, 512);
  setResizable(true, true);
}

RobinAudioProcessorEditor::~RobinAudioProcessorEditor() {
}

void RobinAudioProcessorEditor::selectOperator(uint8_t index) {
  operators[operatorIndex]->setVisible(false);
  operatorIndex = index;
  operators[index]->setVisible(true);
}
void RobinAudioProcessorEditor::updateFromValueTree() {
  for(uintptr_t i = 0; i < RBN_OPERATOR_COUNT; i++) {
    operators[i]->updateFromValueTree();
  }
  for(uintptr_t i = 0; i < RBN_OPERATOR_COUNT; i++) {
    for(uintptr_t j = 0; j < RBN_OPERATOR_COUNT; j++) {
      operatorMatrix[i + j * 8]->setValue(operatorsTree.getChild(i).getChildWithName("Modulations").getChild(j).getProperty("Value"));
    }
  }
}

//==============================================================================
void RobinAudioProcessorEditor::parentHierarchyChanged() {
  juce::DocumentWindow* window = findParentComponentOfClass<juce::DocumentWindow>();
  if(window) {
    window->Component::addAndMakeVisible(&exportButton);

    exportButton.addListener(this);
    exportButton.setTriggeredOnMouseDown(true);
    exportButton.setBounds(76, 6, 60, window->getTitleBarHeight() - 8);
  }
}

void RobinAudioProcessorEditor::paint(juce::Graphics& g) {
  g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void RobinAudioProcessorEditor::resized() {
  auto area = getLocalBounds();

  const auto operatorSectionWidth = area.getWidth() - area.getHeight();
  const auto operatorButtonWidth = operatorSectionWidth / RBN_OPERATOR_COUNT;
  const auto operatorButtonHeight = 20;

  for(uintptr_t i = 0; i < RBN_OPERATOR_COUNT; i++) {
    operatorButtons[i]->setBounds(operatorButtonWidth * i, 0, operatorButtonWidth, operatorButtonHeight);
    operators[i]->setBounds(0, operatorButtonHeight, operatorSectionWidth, area.getHeight() - operatorButtonHeight);
  }

  { // Operator matrix grid
    const auto operatorMatrixArea = area.withLeft(area.getRight() - area.getHeight());
    juce::Array<juce::GridItem> gridItems;
    for(std::shared_ptr<juce::Slider>& slider : operatorMatrix) {
      gridItems.add(slider.get());
    }
    juce::Grid operatorMatrixGrid;
    for(uintptr_t i = 0; i < RBN_OPERATOR_COUNT; i++) {
      operatorMatrixGrid.templateRows.add(juce::Grid::TrackInfo(juce::Grid::Fr(1)));
      operatorMatrixGrid.templateColumns.add(juce::Grid::TrackInfo(juce::Grid::Fr(1)));
    }
    operatorMatrixGrid.items = gridItems;
    operatorMatrixGrid.performLayout(operatorMatrixArea);
  }
}

//==============================================================================
void RobinAudioProcessorEditor::buttonClicked(juce::Button* button) {
  if(button == &exportButton) {
    exportFileChooser = std::make_unique<juce::FileChooser>(
      "Please select where you want to export the program...",
      juce::File::getSpecialLocation(juce::File::userHomeDirectory),
      "*.c");

    juce::FileBrowserComponent::FileChooserFlags flags = juce::FileBrowserComponent::saveMode;

    exportFileChooser->launchAsync(flags,
      [this](const juce::FileChooser& chooser) {
        juce::File file = chooser.getResult();
        juce::String fileName = file.getFullPathName(); // .replaceCharacter('\\', '/');
        FILE* stream = fopen(fileName.getCharPointer(), "w");
        if(stream) {
          rbnutil_export(stream, audioProcessor.getRobinInstance().programs + audioProcessor.getCurrentProgram());
          fclose(stream);
        }
      });
  }
}

void RobinAudioProcessorEditor::sliderValueChanged(juce::Slider* slider) {
  for(uintptr_t i = 0; i < RBN_OPERATOR_COUNT; i++) {
    for(uintptr_t j = 0; j < RBN_OPERATOR_COUNT; j++) {
      juce::Slider* operatorSlider = operatorMatrix[i + j * 8].get();
      if(slider == operatorSlider) {
        operatorsTree.getChild(i).getChildWithName("Modulations").getChild(j).setProperty("Value", slider->getValue(), undoManager);
        return;
      }
    }
  }
}