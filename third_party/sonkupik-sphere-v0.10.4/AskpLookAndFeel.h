#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace askp {

// ---------------------------------------------------------------------------
// Palette — dark chassis, silver face, violet nebula core (Galaxy-style).
// ---------------------------------------------------------------------------
namespace palette {
inline const juce::Colour chassis      { 0xFF0D0D11 };
inline const juce::Colour chassisEdge  { 0xFF26262E };
inline const juce::Colour faceHi       { 0xFFF3F4F6 };
inline const juce::Colour faceLo       { 0xFFD9DADF };
inline const juce::Colour inkDark      { 0xFF34353C };
inline const juce::Colour label        { 0xFF8B8C95 };
inline const juce::Colour labelOnDark  { 0xFF9A9AA6 };
inline const juce::Colour violet       { 0xFF8B5CF6 };
inline const juce::Colour violetHot    { 0xFFC77DFF };
inline const juce::Colour violetDeep   { 0xFF31135E };
inline const juce::Colour glassTop     { 0xFF17131F };
inline const juce::Colour glassBottom  { 0xFF0A0810 };
inline const juce::Colour white        { 0xFFF7F7FA };
} // namespace palette

class AskpLookAndFeel : public juce::LookAndFeel_V4 {
public:
    AskpLookAndFeel();

    juce::Font jakarta(float height, bool semibold = false) const;
    juce::Font mono(float height) const;
    juce::Typeface::Ptr getTypefaceForFont(const juce::Font& font) override;

    static AskpLookAndFeel& get();

private:
    juce::Typeface::Ptr jakartaRegular_;
    juce::Typeface::Ptr jakartaSemibold_;
};

// Small helpers shared by paint code.
juce::Path arcPath(juce::Point<float> centre, float radius,
                   float startAngle, float endAngle);

} // namespace askp
