#include "MonitorOutputClient.h"
#include <juce_core/juce_core.h>
#include <iostream>

int main()
{
    using Client = askp::live::MonitorOutputClient;
    Client aux (Client::EndpointPolicy::physicalOnly);
    Client broadcast (Client::EndpointPolicy::virtualCableOnly);
    auto check = [] (bool condition, const char* message)
    {
        if (! condition) { std::cerr << "FAIL: " << message << '\n'; return false; }
        return true;
    };
    bool ok = true;
    ok &= check (aux.acceptsEndpoint ("Speakers (Recording Tech SC220)"),
                 "Aux should accept physical speakers");
    ok &= check (! aux.acceptsEndpoint ("CABLE Input (VB-Audio Virtual Cable)"),
                 "Aux must never select the streaming cable");
    ok &= check (broadcast.acceptsEndpoint ("CABLE Input (VB-Audio Virtual Cable)"),
                 "Broadcast accepts the VB-CABLE playback sink");
    ok &= check (broadcast.acceptsEndpoint ("Voicemeeter Input (VB-Audio)"),
                 "Broadcast accepts a virtual playback sink");
    ok &= check (! broadcast.acceptsEndpoint ("CABLE Output (VB-Audio Virtual Cable)"),
                 "Broadcast rejects the recording/source endpoint");
    ok &= check (! broadcast.acceptsEndpoint ("Speakers (Recording Tech SC220)"),
                 "Broadcast cannot seize the ASIO physical headphones");
    ok &= check (aux.isMuted(), "Optional auxiliary monitor defaults muted");
    ok &= check (! broadcast.isMuted(), "Broadcast is not silently muted by auxiliary default");

    broadcast.prepareSource (48000.0, 256);
    juce::AudioBuffer<float> programme (2, 256);
    programme.clear();
    // No virtual cable or audio device is required for this deterministic
    // source/policy regression. An unattached FIFO push is a nonblocking no-op.
    for (int i = 0; i < 1000; ++i)
        broadcast.push (programme, 256);
    ok &= check (broadcast.getStatus().sourceSampleRate == 48000.0,
                 "Broadcast must retain the ASIO source clock");
    if (ok) std::cout << "PASS: SC220 independent ASIO/WASAPI endpoint policies and offline source\n";
    return ok ? 0 : 1;
}
