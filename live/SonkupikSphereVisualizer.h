#pragma once

// Use the approved Sonkupik sphere by an explicit repository-relative path.
// Do not use a generic "OrbVisualizer.h" lookup here: ASK-P v0.5.23 also owns
// a header with that name, and selecting it would pull the original VST
// PluginProcessor.h into the same translation unit as the generated headless
// Live processor header.
#include "../third_party/sonkupik-sphere-v0.10.4/OrbVisualizer.h"

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
