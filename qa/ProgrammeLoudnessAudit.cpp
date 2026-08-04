#include "live/ProgrammeLoudnessIntegrator.h"

#include <cmath>
#include <iostream>

namespace {
void feedSeconds (askp::live::ProgrammeLoudnessIntegrator& meter,
                  double lufs, double seconds, double sampleRate)
{
    const auto energy = askp::live::ProgrammeLoudnessIntegrator::energyFromLufs (lufs);
    const auto frames = static_cast<int> (std::lround (seconds * sampleRate));
    for (int frame = 0; frame < frames; ++frame)
        meter.processFrame (energy);
}

bool near (float actual, float expected, float tolerance)
{
    return std::abs (actual - expected) <= tolerance;
}
} // namespace

int main()
{
    constexpr double sampleRate = 48000.0;
    askp::live::ProgrammeLoudnessIntegrator meter;
    meter.prepare (sampleRate);

    feedSeconds (meter, -14.0, 10.0, sampleRate);
    if (! meter.ready() || ! near (meter.integratedLufs(), -14.0f, 0.06f))
    {
        std::cerr << "constant programme failed: " << meter.integratedLufs() << '\n';
        return 1;
    }

    // Material sixty LU below the programme is above the absolute gate but
    // below the relative gate, so it must not drag the integrated result down.
    feedSeconds (meter, -60.0, 10.0, sampleRate);
    if (! near (meter.integratedLufs(), -14.0f, 0.10f))
    {
        std::cerr << "relative gate failed: " << meter.integratedLufs() << '\n';
        return 2;
    }

    meter.reset();
    feedSeconds (meter, -80.0, 5.0, sampleRate);
    if (meter.ready() || meter.blockCount() != 0 || meter.integratedLufs() > -100.0f)
    {
        std::cerr << "absolute gate failed: " << meter.integratedLufs() << '\n';
        return 3;
    }

    std::cout << "SC220 loudness integrator audit passed\n";
    return 0;
}
