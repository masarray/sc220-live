#pragma once

#include "OrbVisualizer.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace askp::live {

// Compatibility host that keeps LiveMainComponent wiring stable while using
// the approved local sphere visualizer directly with exact bounds.
class SonkupikSphereVisualizer final : public juce::Component
{
public:
    explicit SonkupikSphereVisualizer (askp::Processor& processor);

    void refresh (bool bypassed);
    void resized() override;

private:
    askp::OrbVisualizer orb;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SonkupikSphereVisualizer)
};

} // namespace askp::live
