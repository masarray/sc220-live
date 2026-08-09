# P6B hardware polish: stronger frozen-profile hiss cleanup plus a cleaner
# digital-mixer MUTE warning. Learning remains explicit/frozen and the normal
# programme path stays zero-latency: no runtime FFT and no adaptive profile.

if(NOT DEFINED SC220_ENGINE_GENERATED_SOURCE
   OR NOT DEFINED SC220_UI_MONITOR_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 P6B requires generated engine and UI sources")
endif()

file(READ "${SC220_ENGINE_GENERATED_SOURCE}" _engine)
file(READ "${SC220_UI_MONITOR_GENERATED_SOURCE}" _ui)

function(sc220_p6b_once variable_name label anchor replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${anchor}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 P6B anchor changed: ${label}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _value "${_value}")
    set(${variable_name} "${_value}" PARENT_SCOPE)
endfunction()

# Frozen-profile spectral hiss cleanup without an FFT in the programme path.
# A two-pole low/high split isolates the hiss-heavy upper band. The learned
# static-noise floor is frozen after LEARN and only sets the suppression zone.
# Strong programme body and fast HF transients immediately protect/open the
# upper band, so music stays natural. Recombination is sample synchronous and
# therefore adds zero samples of latency.
set(_hiss_anchor [=[
    voiceMidi.clear();
    voiceEnhancerBridge.process (voiceBlock, voiceMidi);

    if (noiseClean.hasProfile() && noiseClean.isEnabled() && expander.isEnabled())
]=])
set(_hiss_replacement [=[
    voiceMidi.clear();
    voiceEnhancerBridge.process (voiceBlock, voiceMidi);

    static float p6bLow1L = 0.0f;
    static float p6bLow2L = 0.0f;
    static float p6bLow1R = 0.0f;
    static float p6bLow2R = 0.0f;
    static float p6bHighEnvelope = 0.0f;
    static float p6bBodyEnvelope = 0.0f;
    static float p6bHissGain = 1.0f;

    if (noiseClean.hasProfile() && noiseClean.isEnabled())
    {
        const float p6bRate = static_cast<float> (
            juce::jmax (8000.0, currentSampleRate.load()));
        const float splitAlpha = 1.0f - std::exp (
            -juce::MathConstants<float>::twoPi * 4800.0f / p6bRate);
        const float detectorAttack = 1.0f - std::exp (-1.0f / (p6bRate * 0.0025f));
        const float detectorRelease = 1.0f - std::exp (-1.0f / (p6bRate * 0.055f));
        const float suppressEngage = 1.0f - std::exp (-1.0f / (p6bRate * 0.028f));
        const float programmeOpen = 1.0f - std::exp (-1.0f / (p6bRate * 0.0035f));
        const float frozenFloorDb = noiseClean.getEstimatedNoiseFloorDb();
        const float hissReferenceDb = frozenFloorDb - 3.0f;
        constexpr float maxHissReductionDb = 6.0f;

        const int p6bChannels = juce::jmin (2, voiceBlock.getNumChannels());
        for (int sample = 0; sample < numSamples; ++sample)
        {
            float highL = 0.0f, highR = 0.0f;
            float bodyL = 0.0f, bodyR = 0.0f;

            if (p6bChannels > 0)
            {
                const float x = voiceBlock.getSample (0, sample);
                p6bLow1L += splitAlpha * (x - p6bLow1L);
                p6bLow2L += splitAlpha * (p6bLow1L - p6bLow2L);
                bodyL = p6bLow2L;
                highL = x - bodyL;
            }
            if (p6bChannels > 1)
            {
                const float x = voiceBlock.getSample (1, sample);
                p6bLow1R += splitAlpha * (x - p6bLow1R);
                p6bLow2R += splitAlpha * (p6bLow1R - p6bLow2R);
                bodyR = p6bLow2R;
                highR = x - bodyR;
            }
            else
            {
                highR = highL;
                bodyR = bodyL;
            }

            const float linkedHigh = juce::jmax (std::abs (highL), std::abs (highR));
            const float linkedBody = juce::jmax (std::abs (bodyL), std::abs (bodyR));
            const float previousHighEnvelope = p6bHighEnvelope;

            const float highAlpha = linkedHigh > p6bHighEnvelope
                                  ? detectorAttack : detectorRelease;
            const float bodyAlpha = linkedBody > p6bBodyEnvelope
                                  ? detectorAttack : detectorRelease;
            p6bHighEnvelope += highAlpha * (linkedHigh - p6bHighEnvelope);
            p6bBodyEnvelope += bodyAlpha * (linkedBody - p6bBodyEnvelope);

            const float highDb = juce::Decibels::gainToDecibels (
                juce::jmax (p6bHighEnvelope, 1.0e-8f), -120.0f);
            const float bodyDb = juce::Decibels::gainToDecibels (
                juce::jmax (p6bBodyEnvelope, 1.0e-8f), -120.0f);

            // Suppression exists only close to the frozen noise floor. It is
            // fully released roughly 10.5 dB above that upper-band reference.
            const float noiseLikelihood = 1.0f - juce::jlimit (
                0.0f, 1.0f, (highDb - (hissReferenceDb + 2.0f)) / 10.5f);
            const float bodyProtect = juce::jlimit (
                0.0f, 1.0f, (bodyDb - (frozenFloorDb + 8.0f)) / 16.0f);
            const bool fastTransient = linkedHigh > 1.0e-5f
                                    && linkedHigh > previousHighEnvelope * 2.15f;
            const float transientProtect = fastTransient ? 0.96f : 0.0f;
            const float programmeProtect = juce::jmax (0.92f * bodyProtect,
                                                        transientProtect);

            const float targetReductionDb = -maxHissReductionDb
                                           * noiseLikelihood
                                           * (1.0f - programmeProtect);
            const float targetGain = juce::Decibels::decibelsToGain (targetReductionDb);
            const float gainAlpha = targetGain < p6bHissGain
                                  ? suppressEngage : programmeOpen;
            p6bHissGain += gainAlpha * (targetGain - p6bHissGain);

            if (p6bChannels > 0)
                voiceBlock.setSample (0, sample, bodyL + highL * p6bHissGain);
            if (p6bChannels > 1)
                voiceBlock.setSample (1, sample, bodyR + highR * p6bHissGain);
        }
    }
    else
    {
        // True bypass means no residual gain state is kept across OFF/profile
        // changes. LEARN is still the only operation that changes the profile.
        p6bLow1L = p6bLow2L = p6bLow1R = p6bLow2R = 0.0f;
        p6bHighEnvelope = p6bBodyEnvelope = 0.0f;
        p6bHissGain = 1.0f;
    }

    if (noiseClean.hasProfile() && noiseClean.isEnabled() && expander.isEnabled())
]=])
sc220_p6b_once(_engine "zero-latency frozen-profile hiss suppressor"
    "${_hiss_anchor}" "${_hiss_replacement}")

# P6A drew an expanded glow outside the Button component. JUCE clips child
# painting to the component rectangle, so the expanded rounded glow appeared as
# a second, chopped rectangular shape. Keep one red button silhouette and move
# the warning energy inside it: bright inner rim + pulsing LED + pulsing fill.
set(_mute_old [=[
        if (muteActive)
        {
            g.setColour (palette::red.withAlpha (0.30f + 0.40f * mutePulse));
            g.drawRoundedRectangle (area.expanded (1.4f), 6.0f, 1.4f + 0.8f * mutePulse);
            g.setColour (juce::Colours::white.withAlpha (0.52f + 0.48f * mutePulse));
            g.fillEllipse (area.getX() + 7.0f, area.getCentreY() - 2.5f, 5.0f, 5.0f);
        }
]=])
set(_mute_new [=[
        if (muteActive)
        {
            const auto muteInner = area.reduced (1.4f);
            g.setColour (juce::Colours::white.withAlpha (0.28f + 0.58f * mutePulse));
            g.drawRoundedRectangle (muteInner, 4.0f, 1.0f + 1.0f * mutePulse);
            g.setColour (juce::Colours::white.withAlpha (0.60f + 0.40f * mutePulse));
            g.fillEllipse (muteInner.getX() + 6.0f,
                           muteInner.getCentreY() - 2.6f, 5.2f, 5.2f);
        }
]=])
sc220_p6b_once(_ui "single-shape digital mixer MUTE blink"
    "${_mute_old}" "${_mute_new}")

foreach(_required IN ITEMS
        "maxHissReductionDb = 6.0f"
        "hissReferenceDb = frozenFloorDb - 3.0f"
        "fastTransient"
        "programmeProtect"
        "bodyL + highL * p6bHissGain"
        "const auto muteInner = area.reduced (1.4f)"
        "g.drawRoundedRectangle (muteInner"
        "g.fillEllipse (muteInner.getX() + 6.0f")
    string(FIND "${_engine}${_ui}" "${_required}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 P6B authority missing: ${_required}")
    endif()
endforeach()

foreach(_forbidden IN ITEMS
        "area.expanded (1.4f)"
        "noiseClean.process (voiceBlock);\n    voiceMidi.clear()")
    string(FIND "${_engine}${_ui}" "${_forbidden}" _position)
    if(NOT _position EQUAL -1)
        message(FATAL_ERROR "SC220 P6B stale/adaptive path remains: ${_forbidden}")
    endif()
endforeach()

file(WRITE "${SC220_ENGINE_GENERATED_SOURCE}" "${_engine}")
file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")
message(STATUS "SC220 P6B: frozen zero-latency hiss cleanup + single-shape MUTE blink")
