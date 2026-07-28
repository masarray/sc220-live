#pragma once

#include "EnhancerEngineBridge.h"
#include "LicenseCheckoutClient.h"
#include "LiveTheme.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <array>

#ifndef SONKUPIK_LICENSE_PRICE_IDR
 #define SONKUPIK_LICENSE_PRICE_IDR 249000
#endif

namespace askp::live {

class LiveLicensePanel final : public juce::Component,
                               private juce::Timer
{
public:
    static constexpr int fullVersionPriceIdr = SONKUPIK_LICENSE_PRICE_IDR;

    explicit LiveLicensePanel (EnhancerEngineBridge& engine);
    ~LiveLicensePanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;

    std::function<void()> onClose;
    std::function<void()> onLicenseChanged;

private:
    void timerCallback() override;
    void refreshState();
    void configureEditor (juce::TextEditor&, const juce::String& placeholder);
    void setStatus (const juce::String& text, juce::Colour colour = colours::textMuted);
    void startOrOpenCheckout();
    void activateOfflineCode();
    juce::Rectangle<int> cardBounds() const;

    static juce::String formatIdr (int amount);
    static juce::String checkoutStatusLabel (askp::LicenseCheckoutClient::Status status);

    EnhancerEngineBridge& engine;
    askp::LicenseManager& license;
    askp::LicenseCheckoutClient& checkout;

    juce::Label title;
    juce::Label subtitle;
    juce::Label trialBadge;
    juce::Label price;
    juce::Label priceCaption;
    juce::Label nameLabel;
    juce::Label emailLabel;
    juce::TextEditor customerName;
    juce::TextEditor customerEmail;
    juce::TextButton buyButton { "BUY FULL VERSION - QRIS" };
    juce::TextButton checkButton { "CHECK PAYMENT" };
    juce::TextButton cancelButton { "CANCEL UNPAID ORDER" };
    juce::Label paymentStatus;
    juce::Label requestLabel;
    juce::TextEditor requestId;
    juce::TextButton copyRequest { "COPY REQUEST ID" };
    juce::Label activationLabel;
    juce::TextEditor activationCode;
    juce::TextButton activateButton { "ACTIVATE OFFLINE CODE" };
    juce::TextButton closeButton { "CLOSE" };
    juce::Label privacyNote;

    askp::LicenseCheckoutClient::Status previousCheckoutStatus = askp::LicenseCheckoutClient::Status::Idle;
    juce::String previousCheckoutMessage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LiveLicensePanel)
};

} // namespace askp::live
