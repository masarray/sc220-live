# P5 authority: final console polish requested from hardware review.
# - stronger/faster learned-floor expander without adaptive learning
# - user-facing SC220 endpoint naming
# - Lucide speaker cue for PC Audio
# - aligned SC220/Stream mute buttons
# - explicit DSP ON/BYPASS wording and refined signature subtitle
# - user-provided SVG fader thumb artwork

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
    string(FIND "${_value}" "${end_marker}" _end)
    if(_begin EQUAL -1 OR _end EQUAL -1 OR _end LESS_EQUAL _begin)
        message(FATAL_ERROR "SC220 P5 range changed: ${label}")
    endif()
    string(SUBSTRING "${_value}" 0 ${_begin} _prefix)
    string(SUBSTRING "${_value}" ${_end} -1 _suffix)
    set(${variable_name} "${_prefix}${replacement}${_suffix}" PARENT_SCOPE)
endfunction()

# Embed the reviewed SVG thumb directly into the generated UI. This keeps the
# fader hit-test/drag behaviour native JUCE while making the visible cap follow
# the supplied mixer artwork at every scale factor.
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

# PC Audio keeps the real endpoint identity internally but replaces the hardware
# driver token MIXER-01 with the product name SC220 on the user-facing surface.
set(_pc_setup [=[
    {
        const auto pcEndpoints = audioEngine.getPcAudioEndpointNames();
        for (int i = 0; i < pcEndpoints.size(); ++i)
            pcSourceBox.addItem (sc220FriendlyEndpointLabel (pcEndpoints[i]), i + 1);
    }
    pcSourceBox.setTextWhenNothingSelected ("SELECT PC AUDIO");
    pcSourceBox.onChange = [this]
    {
        const int index = pcSourceBox.getSelectedItemIndex();
        const auto pcEndpoints = audioEngine.getPcAudioEndpointNames();
        if (index < 0 || index >= pcEndpoints.size())
            return;
        const auto error = audioEngine.selectPcAudioEndpoint (pcEndpoints[index]);
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

set(_pc_refresh [=[
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
    const auto pcEndpoints = audioEngine.getPcAudioEndpointNames();
]=]
[=[
    const auto outputIndex]=]
"${_pc_refresh}")

# Add a compact Lucide speaker glyph above the PC Audio title without creating
# per-frame objects. createLucideIcon is already backed by the bundled volume-2 SVG.
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

# User-facing DSP copy: describe the power state and the processor explicitly.
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

# SC220 and Stream mute switches share the same console baseline. sc220StripArea
# is vertically inset, so using its bottom was visibly ~48 px too high.
sc220_p5_once(_ui "SC220 mute baseline"
[=[
    sc220Mute.setBounds (sc220StripArea.getCentreX() - 36,
                         sc220StripArea.getBottom() - 42, 72, 31);
]=]
[=[
    sc220Mute.setBounds (sc220StripArea.getCentreX() - 36,
                         inputArea.getBottom() - 44, 72, 31);
]=])

# P5 strengthens the learned-floor expander while keeping the frozen-profile,
# non-adaptive policy introduced earlier. Active programme stays open/unity.
sc220_p5_once(_engine "stronger learned-floor expander amount"
[=[
    expander.setAmount (0.78f);
    expander.setThresholdOffsetDb (5.5f);
]=]
[=[
    expander.setAmount (0.86f);
    expander.setThresholdOffsetDb (6.0f);
]=])

# Generate a P5-tuned copy of MusicalNoiseClean.cpp so the reviewed base source
# remains readable. Only the downward-expander ballistics/range change here.
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
