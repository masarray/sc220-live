#include "SonkupikSphereVisualizer.h"

namespace askp::live {

SonkupikSphereVisualizer::SonkupikSphereVisualizer (askp::Processor& processor)
    : orb (processor)
{
    setOpaque (false);
    setInterceptsMouseClicks (false, false);
    addAndMakeVisible (orb);
}

void SonkupikSphereVisualizer::refresh (bool bypassed)
{
    orb.refresh (bypassed);
}

void SonkupikSphereVisualizer::resized()
{
    orb.setBounds (getLocalBounds());
}

} // namespace askp::live
