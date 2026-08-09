# P5 authority: final console polish requested from hardware review.
# Keep this pass resilient to earlier generated-source transforms: use unique
# function/context anchors instead of matching broad fragments that may also
# appear in the constructor.

if(NOT DEFINED SC220_ENGINE_GENERATED_SOURCE
   OR NOT DEFINED SC220_UI_MONITOR_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 P5 finalizer requires generated engine and UI sources")
endif()

get_filename_component(_p5_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(_p5_fader_svg "${_p5_root}/assets/ui/sc220_fader_thumb.svg")
if(NOT EXISTS "${_p5_fader_svg}")
    message(FATAL_ERROR "SC220 P5 fader SVG asset is missing: ${_p5_fader_svg}")
endif()

file(READ "${SC220_ENGINE_GENERATED_SOURCE}" _engine)
file(READ "${SC220_UI_MONITOR_GENERATED_SOURCE}" _ui)
file(READ "${_p5_fader_svg}" _fader_svg)

function(sc220_p5_once variable_name label anchor replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${anchor}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 P5 anchor changed: ${label}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _value "${_value}")
    set(${variable_name} "${_value}" PARENT_SCOPE)
endfunction()

function(sc220_p5_range variable_name label begin_marker end_marker replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${begin_marker}" _begin)
    if(_begin EQUAL -1)
        message(FATAL_ERROR "SC220 P5 range begin changed: ${label}")
    endif()
    # Search for the end marker only after the selected begin marker. This avoids
    # accidentally pairing a constructor occurrence with a refresh-function end.
    string(SUBSTRING "${_value}" ${_begin} -1 _tail)
    string(FIND "${_tail}" "${end_marker}" _relative_end)
    if(_relative_end EQUAL -1)
        message(FATAL_ERROR "SC220 P5 range end changed: ${label}")
    endif()
    math(EXPR _end "${_begin} + ${_relative_end}")
    string(SUBSTRING "${_value}" 0 ${_begin} _prefix)
    string(SUBSTRING "${_value}" ${_end} -1 _suffix)
    set(${variable_name} "${_prefix}${replacement}${_suffix}" PARENT_SCOPE)
endfunction()

# Friendly endpoint helper + scalable fader artwork. Internal endpoint identity
# remains untouched; only the user-facing ComboBox text is renamed.
string(CONCAT _namespace_replacement
[=[
juce::String sc220FriendlyEndpointLabel (juce::String name)
{
    return name.replace ("MIXER-01", "SC220", true);
}

static constexpr char kSc220FaderThumbSvg[] = R"SC220FADER(]=]
"${_fader_svg}"
[=[)SC220FADER";
} // namespace

class SC220MainComponent::SurfaceLookAndFeel final]=])

sc220_p5_once(_ui "friendly endpoint helper and SVG fader asset"
[=[
} // namespace

class SC220MainComponent::SurfaceLookAndFeel final]=]
"${_namespace_replacement}")

# Render the supplied SVG as the visible fader cap while native JUCE Slider
# behaviour continues to own hit testing, drag precision and value mapping.
set(_fader_render [=[
        const auto cap = juce::Rectangle<float> (centreX - 20.0f, sliderPos - 34.0f, 40.0f, 68.0f);
        static auto faderThumb = juce::Drawable::createFromImageData (
            kSc220FaderThumbSvg, sizeof (kSc220FaderThumbSvg) - 1);
        if (faderThumb != nullptr)
        {
            faderThumb->drawWithin (g, cap, juce::RectanglePlacement::centred, alpha);
        }
        else
        {
            g.setColour (juce::Colour (0xff343735).withAlpha (alpha));
            g.fillRoundedRectangle (cap, 3.0f);
            g.setColour (juce::Colours::white.withAlpha (0.70f * alpha));
            g.drawHorizontalLine (juce::roundToInt (cap.getCentreY()),
                                  cap.getX() + 3.0f, cap.getRight() - 3.0f);
        }
]=])
sc220_p5_range(_ui "SVG linear-fader thumb"
[=[
        const auto cap = juce::Rectangle<float> (centreX - 20.0f, sliderPos - 34.0f, 40.0f, 68.0f);
]=]
[=[
    }

    int getSliderThumbRadius]=]
"${_fader_render}")

# Constructor setup: display friendly labels but map selected index back to the
# current real endpoint list. Use pcSetupEndpoints so this block cannot collide
# with the refresh-function pcEndpoints anchor below.
set(_pc_setup [=[
    {
        const auto pcSetupEndpoints = audioEngine.getPcAudioEndpointNames();
        for (int i = 0; i < pcSetupEndpoints.size(); ++i)
            pcSourceBox.addItem (sc220FriendlyEndpointLabel (pcSetupEndpoints[i]), i + 1);
    }
    pcSourceBox.setTextWhenNothingSelected ("SELECT PC AUDIO");
    pcSourceBox.onChange = [this]
    {
        const int index = pcSourceBox.getSelectedItemIndex();
        const auto pcSetupEndpoints = audioEngine.getPcAudioEndpointNames();
        if (index < 0 || index >= pcSetupEndpoints.size())
            return;
        const auto error = audioEngine.selectPcAudioEndpoint (pcSetupEndpoints[index]);
        if (error.isNotEmpty())
            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::WarningIcon,
                "Unsafe PC Audio routing", error);
        refreshAllControls();
    };

]=])
sc220_p5_range(_ui "friendly PC Audio combo setup"
[=[
    pcSourceBox.addItemList (audioEngine.getPcAudioEndpointNames(), 1);
]=]
[=[
    auto configureKnob =]=]
"${_pc_setup}")

# Refresh function only. Use the status line as context and stop before the
# separately generated Stream Output refresh block. P4/P3/real-stream passes can
# therefore evolve independently without P5 deleting their code.
set(_pc_refresh [=[
    const auto status = audioEngine.getStatus();

    const auto pcEndpoints = audioEngine.getPcAudioEndpointNames();
    bool endpointListChanged = pcEndpoints.size() != pcSourceBox.getNumItems();
    if (! endpointListChanged)
        for (int i = 0; i < pcSourceBox.getNumItems(); ++i)
            if (pcSourceBox.getItemText (i) != sc220FriendlyEndpointLabel (pcEndpoints[i]))
            {
                endpointListChanged = true;
                break;
            }
    if (endpointListChanged)
    {
        pcSourceBox.clear (juce::dontSendNotification);
        for (int i = 0; i < pcEndpoints.size(); ++i)
            pcSourceBox.addItem (sc220FriendlyEndpointLabel (pcEndpoints[i]), i + 1);
    }
    int pcEndpointIndex = -1;
    for (int i = 0; i < pcEndpoints.size(); ++i)
        if (pcEndpoints[i].equalsIgnoreCase (audioEngine.getPcAudioEndpointName()))
            pcEndpointIndex = i;
    if (pcEndpointIndex >= 0)
        pcSourceBox.setSelectedItemIndex (pcEndpointIndex, juce::dontSendNotification);
    else
        pcSourceBox.setSelectedId (0, juce::dontSendNotification);

]=])
sc220_p5_range(_ui "friendly PC Audio combo refresh"
[=[
    const auto status = audioEngine.getStatus();

    const auto pcEndpoints = audioEngine.getPcAudioEndpointNames();
]=]
[=[
    juce::StringArray streamOutputs;
]=]
"${_pc_refresh}")

# Compact Lucide speaker cue for the PC Audio source strip.
sc220_p5_once(_ui "PC Audio Lucide speaker"
[=[
    g.setColour (palette::graphite);
    g.setFont (interFont (11.5f, true));
    g.drawText ("PC AUDIO", pcStripArea.getX(), pcStripArea.getY() + 27,
                pcStripArea.getWidth(), 24, juce::Justification::centred);
]=]
[=[
    static auto pcSpeakerIcon = createLucideIcon ("volume_2_svg", palette::graphite);
    if (pcSpeakerIcon != nullptr)
        pcSpeakerIcon->drawWithin (
            g, juce::Rectangle<float> (static_cast<float> (pcStripArea.getCentreX() - 9),
                                       static_cast<float> (pcStripArea.getY() + 5), 18.0f, 18.0f),
            juce::RectanglePlacement::centred, 1.0f);
    g.setColour (palette::graphite);
    g.setFont (interFont (11.5f, true));
    g.drawText ("PC AUDIO", pcStripArea.getX(), pcStripArea.getY() + 26,
                pcStripArea.getWidth(), 24, juce::Justification::centred);
]=])

# User-facing DSP wording.
string(REPLACE "MAS ARI SIGNATURE ENGINE"
               "Digital Sound Processor ~ Mas Ari Signature" _ui "${_ui}")
sc220_p5_once(_ui "explicit DSP ON/BYPASS state"
[=[
    g.setColour (active ? palette::green : palette::amber);
    g.fillEllipse (static_cast<float> (enhancerPowerButton.getX() - 56),
                   static_cast<float> (enhancerPowerButton.getBounds().getCentreY() - 4), 8.0f, 8.0f);
    g.setFont (interFont (9.0f, true));
    g.drawText (active ? "ON" : "BYPASS", enhancerPowerButton.getX() - 44,
                enhancerPowerButton.getY(), 42, enhancerPowerButton.getHeight(),
                juce::Justification::centredLeft);
]=]
[=[
    g.setColour (active ? palette::green : palette::amber);
    g.fillEllipse (static_cast<float> (enhancerPowerButton.getX() - 96),
                   static_cast<float> (enhancerPowerButton.getBounds().getCentreY() - 4), 8.0f, 8.0f);
    g.setFont (interFont (9.0f, true));
    g.drawText (active ? "DSP ON" : "DSP BYPASS", enhancerPowerButton.getX() - 82,
                enhancerPowerButton.getY(), 78, enhancerPowerButton.getHeight(),
                juce::Justification::centredLeft);
]=])

# SC220 MUTE uses the same lower console baseline as Stream MUTE.
sc220_p5_once(_ui "SC220 mute baseline"
[=[
    sc220Mute.setBounds (sc220StripArea.getCentreX() - 36,
                         sc220StripArea.getBottom() - 42, 72, 31);
]=]
[=[
    sc220Mute.setBounds (sc220StripArea.getCentreX() - 36,
                         inputArea.getBottom() - 44, 72, 31);
]=])

# Stronger learned-floor expander. The profile remains frozen/non-adaptive and
# normal programme audio still avoids spectral denoise work after Learn.
sc220_p5_once(_engine "stronger learned-floor expander amount"
[=[
    expander.setAmount (0.78f);
    expander.setThresholdOffsetDb (5.5f);
]=]
[=[
    expander.setAmount (0.86f);
    expander.setThresholdOffsetDb (6.0f);
]=])

# Build a P5-tuned copy of the time-domain expander implementation. This changes
# only range/ballistics; no adaptive learner or additional FFT is introduced.
set(_noise_source "${_p5_root}/live/MusicalNoiseClean.cpp")
file(READ "${_noise_source}" _noise)

function(sc220_p5_noise_once label anchor replacement)
    string(FIND "${_noise}" "${anchor}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 P5 expander anchor changed: ${label}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _updated "${_noise}")
    set(_noise "${_updated}" PARENT_SCOPE)
endfunction()

sc220_p5_noise_once("ratio"
    "    const float ratio = juce::jmap (value, 1.0f, 1.75f);"
    "    const float ratio = juce::jmap (value, 1.0f, 2.05f);")
sc220_p5_noise_once("range"
    "    const float rangeDb = juce::jmap (value, 0.0f, 30.0f);"
    "    const float rangeDb = juce::jmap (value, 0.0f, 42.0f);")
sc220_p5_noise_once("knee"
    "    const float kneeDb = 8.0f;"
    "    const float kneeDb = 6.0f;")
sc220_p5_noise_once("open hysteresis"
    "    const float openThresholdDb = thresholdDb + 3.0f;"
    "    const float openThresholdDb = thresholdDb + 4.0f;")
sc220_p5_noise_once("short silence hold"
    "    const int holdLength = static_cast<int> (sampleRate * juce::jmap (value, 0.145f, 0.085f));"
    "    const int holdLength = static_cast<int> (sampleRate * juce::jmap (value, 0.070f, 0.040f));")
sc220_p5_noise_once("faster detector release"
    "    const float detectorRelease = 1.0f - std::exp (-1.0f / static_cast<float> (sampleRate * 0.085));"
    "    const float detectorRelease = 1.0f - std::exp (-1.0f / static_cast<float> (sampleRate * 0.050));")
sc220_p5_noise_once("faster opening"
    "    const float gainOpen = 1.0f - std::exp (-1.0f / static_cast<float> (sampleRate * 0.004));"
    "    const float gainOpen = 1.0f - std::exp (-1.0f / static_cast<float> (sampleRate * 0.003));")
sc220_p5_noise_once("faster close"
    "    const float gainClose = 1.0f - std::exp (-1.0f / static_cast<float> (sampleRate * 0.45));"
    "    const float gainClose = 1.0f - std::exp (-1.0f / static_cast<float> (sampleRate * 0.14));")

if(NOT CMAKE_SCRIPT_MODE_FILE)
    set(_noise_generated "${CMAKE_CURRENT_BINARY_DIR}/generated/sc220-p5/MusicalNoiseClean_P5.cpp")
    get_filename_component(_noise_generated_dir "${_noise_generated}" DIRECTORY)
    file(MAKE_DIRECTORY "${_noise_generated_dir}")
    file(WRITE "${_noise_generated}" "${_noise}")
    set_source_files_properties("${_noise_source}" PROPERTIES HEADER_FILE_ONLY TRUE)
    target_sources(SC220Live PRIVATE "${_noise_generated}")
endif()

foreach(_required IN ITEMS
        "sc220FriendlyEndpointLabel"
        "pcSetupEndpoints"
        "MIXER-01\", \"SC220"
        "volume_2_svg"
        "Digital Sound Processor ~ Mas Ari Signature"
        "DSP ON"
        "DSP BYPASS"
        "kSc220FaderThumbSvg"
        "inputArea.getBottom() - 44"
        "expander.setAmount (0.86f)")
    string(FIND "${_engine}${_ui}" "${_required}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 P5 required console authority missing: ${_required}")
    endif()
endforeach()

foreach(_forbidden IN ITEMS
        "MAS ARI SIGNATURE ENGINE"
        "active ? \"ON\" : \"BYPASS\"")
    string(FIND "${_ui}" "${_forbidden}" _position)
    if(NOT _position EQUAL -1)
        message(FATAL_ERROR "SC220 P5 legacy UI copy remains: ${_forbidden}")
    endif()
endforeach()

file(WRITE "${SC220_ENGINE_GENERATED_SOURCE}" "${_engine}")
file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")
message(STATUS "SC220 P5: faster clean silence + product naming + SVG faders + console polish")