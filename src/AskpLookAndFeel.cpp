#include "AskpLookAndFeel.h"

#include "BinaryData.h"

namespace askp {

AskpLookAndFeel::AskpLookAndFeel()
{
    jakartaRegular_ = juce::Typeface::createSystemTypefaceFor(
        BinaryData::plus_jakarta_sans_regular_ttf,
        BinaryData::plus_jakarta_sans_regular_ttfSize);
    jakartaSemibold_ = juce::Typeface::createSystemTypefaceFor(
        BinaryData::plus_jakarta_sans_semibold_ttf,
        BinaryData::plus_jakarta_sans_semibold_ttfSize);

    if (jakartaRegular_ != nullptr)
        setDefaultSansSerifTypeface(jakartaRegular_);

    setColour(juce::ResizableWindow::backgroundColourId, palette::chassis);
    setColour(juce::TooltipWindow::backgroundColourId, palette::glassBottom);
}

juce::Font AskpLookAndFeel::jakarta(float height, bool semibold) const
{
    const auto face = semibold && jakartaSemibold_ != nullptr
                    ? jakartaSemibold_
                    : jakartaRegular_;

    if (face != nullptr)
        return juce::Font(juce::FontOptions(face).withHeight(height));

    auto fallback = juce::Font(juce::FontOptions(height));
    return semibold ? fallback.boldened() : fallback;
}

juce::Font AskpLookAndFeel::mono(float height) const
{
    return jakarta(height, false);
}

juce::Typeface::Ptr AskpLookAndFeel::getTypefaceForFont(const juce::Font& font)
{
    if (font.isBold() && jakartaSemibold_ != nullptr)
        return jakartaSemibold_;
    if (jakartaRegular_ != nullptr)
        return jakartaRegular_;
    return juce::LookAndFeel_V4::getTypefaceForFont(font);
}

AskpLookAndFeel& AskpLookAndFeel::get()
{
    static AskpLookAndFeel instance;
    return instance;
}

juce::Path arcPath(juce::Point<float> centre, float radius,
                   float startAngle, float endAngle)
{
    juce::Path p;
    p.addCentredArc(centre.x, centre.y, radius, radius, 0.0f,
                    startAngle, endAngle, true);
    return p;
}

} // namespace askp
