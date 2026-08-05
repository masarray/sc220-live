#include "MonitorOutputClient.h"
#include "PcAudioLoopbackCapture.h"

#include <iostream>

int main()
{
    askp::live::MonitorOutputClient monitor;
    const bool monitorInitialised = monitor.initialise();
    const auto monitorDevices = monitor.getOutputDeviceNames();
    const auto monitorStatus = monitor.getStatus();

    std::cout << "backend=" << monitorStatus.backend << '\n';
    std::cout << "initialised=" << (monitorInitialised ? "true" : "false") << '\n';
    std::cout << "render-endpoints=" << monitorDevices.size() << '\n';

    monitor.prepareSource (44100.0, 512);
    monitor.setGainDb (-6.0f);
    monitor.setMuted (true);
    monitor.flush();

    askp::live::PcAudioLoopbackCapture capture;
    const auto captureEndpoints =
        askp::live::PcAudioLoopbackCapture::availableEndpoints();
    capture.prepare (48000.0);
    const bool captureThreadStarted = capture.start();
    juce::Thread::sleep (75);
    std::cout << "capture-thread-started="
              << (captureThreadStarted ? "true" : "false") << '\n';
    std::cout << "capture-endpoints=" << captureEndpoints.size() << '\n';
    std::cout << "capture-error=" << capture.lastError().toStdString() << '\n';
    capture.stop();

    monitor.shutdown();

#if JUCE_WINDOWS
    return monitorStatus.backend == "WASAPI SHARED / OBS-STYLE"
               && monitorInitialised && captureThreadStarted
        ? 0 : 1;
#else
    return 0;
#endif
}
