#include "MonitorOutputClient.h"

#include <iostream>

int main()
{
    askp::live::MonitorOutputClient monitor;
    const bool initialised = monitor.initialise();
    const auto devices = monitor.getOutputDeviceNames();
    const auto status = monitor.getStatus();

    std::cout << "backend=" << status.backend << '\n';
    std::cout << "initialised=" << (initialised ? "true" : "false") << '\n';
    std::cout << "render-endpoints=" << devices.size() << '\n';

    monitor.prepareSource (44100.0, 512);
    monitor.setGainDb (-6.0f);
    monitor.setMuted (true);
    monitor.flush();
    monitor.shutdown();

#if JUCE_WINDOWS
    return status.backend == "WASAPI SHARED / OBS-STYLE" ? 0 : 1;
#else
    return 0;
#endif
}
