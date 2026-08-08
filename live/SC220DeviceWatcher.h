#pragma once

#include "LiveAudioEngine.h"

#include <juce_events/juce_events.h>

namespace askp::live {

// Runtime hardware-presence guard for the dedicated SC220 capture endpoint.
//
// Windows/JUCE may keep the last selected input name after a USB audio device
// disappears. SC220 Live therefore cannot treat the saved input name as proof
// that the hardware is still present. This watcher periodically enumerates the
// real recording endpoints, detaches a vanished SC220 while preserving the
// stream output, and automatically reconnects the preferred endpoint when it
// returns.
class SC220DeviceWatcher final : private juce::Timer
{
public:
    explicit SC220DeviceWatcher (LiveAudioEngine& engine)
        : audioEngine (engine)
    {
        pollDevicePresence();
        startTimerHz (2);
    }

    ~SC220DeviceWatcher() override
    {
        stopTimer();
    }

    bool isHardwarePresent() const noexcept { return hardwarePresent; }

private:
    static int inputPreference (const juce::String& name) noexcept
    {
        const auto normalised = name.trim().toUpperCase();
        if (normalised.contains ("MIXER-01"))
            return 100;
        if (normalised.contains ("SC220"))
            return 90;
        if (normalised.contains ("RECORDING TECH"))
            return 80;
        if (normalised == "MIXER")
            return 60;
        if (normalised.startsWith ("MIXER ("))
            return 50;
        return 0;
    }

    static bool looksLikeSc220 (const juce::String& name) noexcept
    {
        return inputPreference (name) > 0;
    }

    static juce::String preferredSc220Input (const juce::StringArray& inputs)
    {
        juce::String preferred;
        int bestScore = 0;
        for (const auto& name : inputs)
        {
            const int score = inputPreference (name);
            if (score > bestScore)
            {
                bestScore = score;
                preferred = name;
            }
        }
        return preferred;
    }

    void timerCallback() override
    {
        pollDevicePresence();
    }

    void pollDevicePresence()
    {
        const auto inputs = audioEngine.getInputDeviceNames();
        const auto preferred = preferredSc220Input (inputs);
        const bool presentNow = preferred.isNotEmpty();

        if (hasPolled && hardwarePresent && ! presentNow)
            reconnectRequired = true;

        hardwarePresent = presentNow;
        hasPolled = true;

        if (! presentNow)
        {
            // Remove only the vanished SC220 input from JUCE's active setup.
            // Keep the current VB-CABLE output alive so PC Audio/stream routing
            // can continue while the hardware interface is powered down.
            auto& manager = audioEngine.getDeviceManager();
            auto setup = manager.getAudioDeviceSetup();
            if (looksLikeSc220 (setup.inputDeviceName))
            {
                reconnectRequired = true;
                setup.inputDeviceName.clear();
                setup.useDefaultInputChannels = false;
                setup.inputChannels.clear();
                manager.setAudioDeviceSetup (setup, true);
            }
            retryTicks = 0;
            return;
        }

        if (retryTicks > 0)
        {
            --retryTicks;
            return;
        }

        const auto status = audioEngine.getStatus();
        const bool preferredSelected = status.inputDevice.equalsIgnoreCase (preferred);
        const bool mustReconnect = reconnectRequired
                                || ! preferredSelected
                                || ! status.deviceOpen;

        if (! mustReconnect)
            return;

        const auto error = audioEngine.selectInputDevice (preferred);
        if (error.isEmpty())
        {
            reconnectRequired = false;
            retryTicks = 0;
        }
        else
        {
            // Avoid hammering a Windows endpoint that is still settling after
            // USB enumeration. At 2 Hz this retries roughly once per 2 seconds.
            reconnectRequired = true;
            retryTicks = 3;
        }
    }

    LiveAudioEngine& audioEngine;
    bool hardwarePresent = false;
    bool hasPolled = false;
    bool reconnectRequired = false;
    int retryTicks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SC220DeviceWatcher)
};

} // namespace askp::live
