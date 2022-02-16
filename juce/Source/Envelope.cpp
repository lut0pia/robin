#include "Envelope.h"

#include "../../robin.h"

//==============================================================================
RobinEnvelope::RobinEnvelope(juce::ValueTree tree, juce::UndoManager* undoManager) {
  setInterceptsMouseClicks(true, true);
  setSize(512, 128);

  this->tree = tree;
  this->undoManager = undoManager;
}

RobinEnvelope::~RobinEnvelope() {
}

juce::Rectangle<int> RobinEnvelope::getInnerBounds() const {
  return getLocalBounds().reduced(12).translated(0, 6);
}
juce::Point<float> RobinEnvelope::getHandleRadius() const {
  return juce::Point<float>(5.f, 5.f);
}
juce::Point<float> RobinEnvelope::eventPointToEnvelopePoint(const juce::Point<float>& point) const {
  const auto innerBounds = getInnerBounds().toFloat();
  const float width = innerBounds.getWidth();
  const float height = innerBounds.getHeight();
  const juce::Point<float> innerPoint = innerBounds.getConstrainedPoint(point);
  const float timeToPixels = width / windowTimeWidth;
  return juce::Point<float>(
    (innerPoint.x - innerBounds.getX()) / timeToPixels,
    1.f - (innerPoint.y - innerBounds.getY()) / height);
}

juce::Point<float> RobinEnvelope::getEnvelopePoint(uintptr_t index) const {
  juce::ValueTree pointTree;
  bool isReleasePoint = false;
  if(index == RBN_ENVPT_COUNT) {
    index = getLastEnvelopePoint();
    isReleasePoint = true;
  }
  pointTree = tree.getChild(index);
  const float time = float(pointTree.getProperty("Time")) + (isReleasePoint ? float(tree.getProperty("ReleaseTime")) : 0.f);
  const float value = isReleasePoint ? 0.f : pointTree.getProperty("Value");
  if(time == 0.f && value == 0.f && !isReleasePoint) {
    return juce::Point<float>(INFINITY, INFINITY);
  }
  const auto innerBounds = getInnerBounds();
  const float width = innerBounds.getWidth();
  const float height = innerBounds.getHeight();
  const float timeToPixels = width / windowTimeWidth;
  return innerBounds.getTopLeft().toFloat() +
    juce::Point<float>(time * timeToPixels, height - value * height);
}
int RobinEnvelope::getLastEnvelopePoint() const {
  juce::ValueTree pointTree;
  for(int i = 0; i < RBN_ENVPT_COUNT; i++) {
    pointTree = tree.getChild(i);
    if(float(pointTree.getProperty("Time")) == 0.f && float(pointTree.getProperty("Value")) == 0.f) {
      return i - 1;
    }
  }
}

void RobinEnvelope::setEnvelopePoint(uintptr_t index, const juce::Point<float>& point) {
  if(index == RBN_ENVPT_COUNT) {
    juce::ValueTree pointTree = tree.getChild(getLastEnvelopePoint());
    tree.setProperty("ReleaseTime", juce::jmax(point.x - float(pointTree.getProperty("Time")), 0.f), undoManager);
  } else {
    juce::ValueTree pointTree = tree.getChild(index);

    // Avoid getting time and value at 0
    const float value = point.x == 0.f ? juce::jmax(point.y, 0.001f) : point.y;
    pointTree.setProperty("Value", value, undoManager);

    const float min_x = index > 0 ? tree.getChild(index - 1).getProperty("Time") : 0.f;
    const float delta_x = juce::jmax(min_x, point.x) - float(pointTree.getProperty("Time"));
    for(int i = index; i < RBN_ENVPT_COUNT; i++) {
      pointTree = tree.getChild(i);
      const float time = float(pointTree.getProperty("Time"));
      const float value = float(pointTree.getProperty("Value"));
      if(time > 0.f || value > 0.f) {
        pointTree.setProperty("Time", time + delta_x, undoManager);
      } else {
        break;
      }
    }
  }
}
void RobinEnvelope::removeEnvelopePoint(uintptr_t index) {
  for(uintptr_t i = index; i < RBN_ENVPT_COUNT; i++) {
    juce::ValueTree prevPointTree = tree.getChild(i);
    if(i == RBN_ENVPT_COUNT - 1) {
      prevPointTree.setProperty("Time", 0.f, undoManager);
      prevPointTree.setProperty("Value", 0.f, undoManager);
    } else {
      juce::ValueTree nextPointTree = tree.getChild(i + 1);
      prevPointTree.setProperty("Time", nextPointTree.getProperty("Time"), undoManager);
      prevPointTree.setProperty("Value", nextPointTree.getProperty("Value"), undoManager);
    }
  }
}
void RobinEnvelope::addEnvelopePoint(const juce::Point<float>& point) {
  juce::ValueTree lastPointTree = tree.getChild(RBN_ENVPT_COUNT - 1);
  if(float(lastPointTree.getProperty("Time")) != 0.f || float(lastPointTree.getProperty("Value")) != 0.f) {
    return; // No more envelope points
  }
  for(int i = 0; i < RBN_ENVPT_COUNT; i++) {
    juce::ValueTree pointTree = tree.getChild(i);
    if(point.x < float(pointTree.getProperty("Time"))) {
      for(int j = RBN_ENVPT_COUNT - 2; j >= i; j--) {
        juce::ValueTree prevPointTree = tree.getChild(j);
        juce::ValueTree nextPointTree = tree.getChild(j + 1);
        nextPointTree.setProperty("Time", prevPointTree.getProperty("Time"), undoManager);
        nextPointTree.setProperty("Value", prevPointTree.getProperty("Value"), undoManager);
      }
      pointTree.setProperty("Time", point.x, undoManager);
      pointTree.setProperty("Value", point.y, undoManager);
      hoveringPoint = i;
      return;
    } else if(float(pointTree.getProperty("Time")) == 0.f && float(pointTree.getProperty("Value")) == 0.f) {
      pointTree.setProperty("Time", point.x, undoManager);
      pointTree.setProperty("Value", point.y, undoManager);
      hoveringPoint = i;
      return;
    }
  }
}

//==============================================================================
void RobinEnvelope::paint(juce::Graphics& g) {
  GroupComponent::paint(g);

  const auto innerBounds = getInnerBounds();
  const float width = innerBounds.getWidth();
  const float height = innerBounds.getHeight();
  const float timeToPixels = width / windowTimeWidth;

  { // Draw time markers
    g.setColour(juce::Colour(0.f, 0.f, 0.f, 0.1f));
    for(float time = 1.f; time < windowTimeWidth; time += 1.f) {
      g.drawRect(innerBounds.withLeft(time * timeToPixels).withWidth(1));
    }
  }

  { // Draw curve
    juce::Point<float> prevPoint = innerBounds.getBottomLeft().toFloat();
    g.setColour(juce::Colour(255, 255, 255));
    for(uintptr_t i = 0; i < RBN_ENVPT_COUNT; i++) {
      juce::Point<float> newPoint = getEnvelopePoint(i);
      if(!newPoint.isFinite()) {
        break; // We've reached the last point of the envelope
      }
      g.drawLine(juce::Line<float>(prevPoint, newPoint));

      prevPoint = newPoint;
    }
    g.setColour(juce::Colour(128, 0, 0));
    const float releaseTime = tree.getProperty("ReleaseTime");
    if(releaseTime >= 0.f) {
      float newX = prevPoint.x + releaseTime * timeToPixels;
      g.drawLine(juce::Line<float>(prevPoint, juce::Point<float>(newX, innerBounds.getBottom())));
    }
  }

  { // Draw point handles
    const juce::Point<float> handleRadius = getHandleRadius();
    for(uintptr_t i = 0; i <= RBN_ENVPT_COUNT; i++) {
      const juce::Point<float> point = getEnvelopePoint(i);
      if(!point.isFinite()) {
        continue;
      }
      juce::Colour colour;
      if(i == RBN_ENVPT_COUNT) { // Release point
        colour = i == hoveringPoint ? juce::Colour(255, 200, 100) : juce::Colour(255, 128, 0);
      } else {
        colour = i == hoveringPoint ? juce::Colour(100, 200, 255) : juce::Colour(0, 128, 255);
      }
      g.setColour(colour);
      g.fillRoundedRectangle(juce::Rectangle<float>(point - handleRadius, point + handleRadius), 5.f);
    }
  }
}

void RobinEnvelope::resized() {

}

void RobinEnvelope::mouseMove(const juce::MouseEvent& event) {
  const int32_t previousHoveringPoint = hoveringPoint;
  hoveringPoint = -1;
  for(uintptr_t i = 0; i <= RBN_ENVPT_COUNT; i++) {
    const juce::Point<float> point = getEnvelopePoint(i);
    if(event.position.getDistanceFrom(point) < getHandleRadius().x) {
      hoveringPoint = i;
    }
  }

  if(hoveringPoint != previousHoveringPoint) {
    repaint();
  }
}

void RobinEnvelope::mouseDown(const juce::MouseEvent& event) {

}

void RobinEnvelope::mouseDrag(const juce::MouseEvent& event) {
  if(hoveringPoint >= 0) {
    setEnvelopePoint(hoveringPoint, eventPointToEnvelopePoint(event.position));
    repaint();
  }
}

void RobinEnvelope::mouseUp(const juce::MouseEvent& event) {
  if(event.mods.isRightButtonDown()) {
    if(hoveringPoint >= 0 && hoveringPoint < RBN_ENVPT_COUNT) {
      removeEnvelopePoint(hoveringPoint);
      hoveringPoint = -1;
      repaint();
    } else {
      addEnvelopePoint(eventPointToEnvelopePoint(event.position));
      repaint();
    }
  }
}

void RobinEnvelope::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) {
  const float timeWidthChange = 0.9f;
  if(wheel.deltaY < 0.f) {
    windowTimeWidth *= timeWidthChange;
  } else {
    windowTimeWidth /= timeWidthChange;
  }

  repaint();
}
