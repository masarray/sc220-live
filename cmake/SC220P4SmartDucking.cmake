# P4 authority: clean LUFS surface + lightweight speech-aware music ducking.
# Runs after P3 so it only changes the approved ducking detector/control law and
# removes the noisy social-platform target caption from the LUFS dial.

file(READ "${SC220_ENGINE_GENERATED_SOURCE}" _engine)
file(READ "${SC220_UI_MONITOR_GENERATED_SOURCE}" _ui)

function(sc220_p4_once variable_name label anchor replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${anchor}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 P4 anchor changed: ${label}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _value "${_value}")
    set(${variable_name} "${_value}" PARENT_SCOPE)
endfunction()

function(sc220_p4_range variable_name label begin_marker end_marker replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${begin_marker}" _begin)
    string(FIND "${_value}" "${end_marker}" _end)
    if(_begin EQUAL -1 OR _end EQUAL -1 OR _end LESS_EQUAL _begin)
        message(FATAL_ERROR "SC220 P4 range changed: ${label}")
    endif()
    string(SUBSTRING "${_value}" 0 ${_begin} _prefix)
    string(SUBSTRING "${_value}" ${_end} -1 _suffix)
    set(${variable_name} "${_prefix}${replacement}${_suffix}" PARENT_SCOPE)
endfunction()

# Header-only sidechain: no allocation, no FFT, no extra audio latency.
sc220_p4_once(_engine "speech detector include"
    "#include \"SC220P3RuntimeState.h\"\n"
    "#include \"SC220P3RuntimeState.h\"\n#include \"SmartVoiceDuckDetector.h\"\n")

# Keep detector state local to the generated engine translation unit. Only the
# realtime callback touches it, so no lock/atomic handoff is required.
sc220_p4_once(_engine "speech detector instance"
[=[
namespace askp::live {
]=]
[=[
namespace askp::live {

namespace {
SmartVoiceDuckDetector smartVoiceDuckDetector;
}
]=])

# Follow the real device rate so 45 ms confirmation and 300 ms hold remain time
# constants at every supported sample rate/buffer size.
sc220_p4_once(_engine "prepare speech detector"
[=[
    expander.setThresholdOffsetDb (5.5f);
]=]
[=[
    expander.setThresholdOffsetDb (5.5f);
    smartVoiceDuckDetector.prepare (sampleRate);
]=])

# If PC Audio is disabled, forget stale speech/hold state. Re-enabling music in
# silence must always begin at unity rather than inheriting an old duck hold.
sc220_p4_once(_engine "reset duck detector while PC Audio is off"
[=[
        musicBlock.clear();
        duckGainLinear = 1.0f;
        duckGainDb.store (0.0f);
]=]
[=[
        musicBlock.clear();
        smartVoiceDuckDetector.reset();
        duckGainLinear = 1.0f;
        duckGainDb.store (0.0f);
]=])

# Replace raw peak > -36 dB triggering with a speech-band, sustained-confidence
# sidechain. The detector uses the pre-enhancer detectorBlock and can reference
# the frozen Learn profile only as a threshold; it never learns/adapts here.
set(_smart_duck [=[
        const bool voiceSpeaking = smartVoiceDuckDetector.process (
            detectorBlock, numSamples,
            noiseClean.hasProfile(), noiseClean.getEstimatedNoiseFloorDb());
        const float targetDuck = duckingEnabled.load() && voiceSpeaking
                               ? juce::Decibels::decibelsToGain (duckingDepthDb.load())
                               : 1.0f;

        // Natural broadcast ballistics: speech is already confirmed for 45 ms,
        // so use a soft 55 ms fade-down and a long 650 ms musical recovery.
        // The detector itself holds speech for 300 ms across gaps between words.
        const double seconds = targetDuck < duckGainLinear ? 0.055 : 0.650;
        const float coefficient = static_cast<float> (std::exp (
            -static_cast<double> (numSamples)
            / (juce::jmax (1.0, currentSampleRate.load()) * seconds)));
        const float nextDuck = targetDuck + coefficient * (duckGainLinear - targetDuck);

]=])
sc220_p4_range(_engine "speech-aware ducking control law"
    "        const float keyPeak = juce::jmax (sc220PeakL, sc220PeakR);"
    "        pcGainLinear.setTargetValue"
    "${_smart_duck}")

# The live LUFS-M dial is self-explanatory. Remove the social-platform target
# caption rather than carrying a fixed recommendation as permanent visual noise.
set(_target_caption [=[
        g.setColour (palette::orange.withAlpha (0.92f));
        g.setFont (interFont (dial.getWidth() * 0.050f, true));
        g.drawText ("YT / TIKTOK  -14  |  TP <= -1",
                    dial.withTrimmedTop (dial.getHeight() * 0.14f).withHeight (18.0f),
                    juce::Justification::centred);

]=])
string(FIND "${_ui}" "${_target_caption}" _caption_pos)
if(_caption_pos EQUAL -1)
    message(FATAL_ERROR "SC220 P4 LUFS target-caption anchor changed")
endif()
string(REPLACE "${_target_caption}" "" _ui "${_ui}")

# Fail closed against legacy detection and against any second monitor-side duck
# stage. The PC-only confidence monitor must be a plain copy of musicBlock after
# the one shared PC fader + duck ramp has already been applied.
foreach(_forbidden IN ITEMS
        "dbFromPeak (keyPeak) > -36.0f"
        "musicCueBlock.applyGainRamp"
        "YT / TIKTOK  -14  |  TP <= -1")
    string(FIND "${_engine}${_ui}" "${_forbidden}" _position)
    if(NOT _position EQUAL -1)
        message(FATAL_ERROR "SC220 P4 forbidden legacy behaviour remains: ${_forbidden}")
    endif()
endforeach()

string(FIND "${_engine}" "musicBlock.applyGainRamp" _music_duck_pos)
string(FIND "${_engine}" "musicCueBlock.copyFrom" _music_cue_copy_pos)
if(_music_duck_pos EQUAL -1 OR _music_cue_copy_pos EQUAL -1
   OR _music_cue_copy_pos LESS_EQUAL _music_duck_pos)
    message(FATAL_ERROR
        "SC220 P4 monitor routing must copy PC audio only after the shared duck ramp")
endif()

foreach(_required IN ITEMS
        "SmartVoiceDuckDetector"
        "smartVoiceDuckDetector.process"
        "noiseClean.hasProfile(), noiseClean.getEstimatedNoiseFloorDb()"
        "targetDuck < duckGainLinear ? 0.055 : 0.650"
        "musicCueBlock.copyFrom")
    string(FIND "${_engine}${_ui}" "${_required}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 P4 smart-ducking authority missing: ${_required}")
    endif()
endforeach()

file(WRITE "${SC220_ENGINE_GENERATED_SOURCE}" "${_engine}")
file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")
message(STATUS "SC220 P4: clean LUFS UI + sustained speech-aware natural ducking")
