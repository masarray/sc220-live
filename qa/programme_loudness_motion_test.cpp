#include "../live/ProgrammeLoudnessIntegrator.h"

#include <cmath>
#include <iostream>

int main()
{
    constexpr double sampleRate = 48000.0;
    askp::live::ProgrammeLoudnessIntegrator meter;
    meter.prepare (sampleRate);

    const auto quietEnergy = askp::live::ProgrammeLoudnessIntegrator::energyFromLufs (-30.0);
    const auto loudEnergy = askp::live::ProgrammeLoudnessIntegrator::energyFromLufs (-12.0);

    for (int frame = 0; frame < static_cast<int> (sampleRate * 0.6); ++frame)
        meter.processFrame (quietEnergy);
    const float quietMomentary = meter.momentaryLufs();

    for (int frame = 0; frame < static_cast<int> (sampleRate * 0.6); ++frame)
        meter.processFrame (loudEnergy);
    const float loudMomentary = meter.momentaryLufs();

    for (int frame = 0; frame < static_cast<int> (sampleRate * 2.4); ++frame)
        meter.processFrame (loudEnergy);
    const float integrated = meter.integratedLufs();

    std::cout << "Quiet LUFS-M: " << quietMomentary << "\n"
              << "Loud LUFS-M: " << loudMomentary << "\n"
              << "Integrated LUFS-I: " << integrated << "\n";

    const bool momentaryTracks = std::abs (quietMomentary + 30.0f) < 0.15f
                              && std::abs (loudMomentary + 12.0f) < 0.15f
                              && loudMomentary - quietMomentary > 17.5f;
    const bool integratedReady = meter.ready() && integrated > -15.0f && integrated < -11.0f;
    return momentaryTracks && integratedReady ? 0 : 1;
}
