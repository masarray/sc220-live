#include "AquariumVisualizer.h"

namespace askp::live {

AquariumVisualizer::AquariumVisualizer (askp::Processor& processor)
    : orb (processor)
{
    setOpaque (false);
    setInterceptsMouseClicks (false, false);
    addAndMakeVisible (orb);
}

void AquariumVisualizer::refresh (bool bypassed)
{
    orb.refresh (bypassed);
}

void AquariumVisualizer::resized()
{
    orb.setBounds (getLocalBounds());
}

} // namespace askp::live
