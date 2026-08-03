#include "LiveLicensePanel.h"

namespace askp::live {
namespace {
constexpr int kRefreshHz = 8;

bool looksLikeEmail (const juce::String& text)
{
    const auto value = text.trim();
    const int at = value.indexOfChar ('@');
    const int dot = value.lastIndexOfChar ('.');
    return at > 0 && dot > at + 1 && dot < value.length() - 1;
}
} // namespace

LiveLicensePanel::LiveLicensePanel (EnhancerEngineBridge& engineToUse)
    : engine (engineToUse),
      license (engine.licenseManager()),
      checkout (askp::LicenseCheckoutClient::shared (license.requestId()))
{
    setOpaque (false);
    setInterceptsMouseClicks (true, true);
    setVisible (false);

    auto setLabel = [] (juce::Label& label, float size, bool bold,
                        juce::Colour colour, juce::Justification justification)
    {
        label.setFont (askp::AskpLookAndFeel::get().jakarta (size, bold));
        label.setColour (juce::Label::textColourId, colour);
        label.setJustificationType (justification);
        label.setInterceptsMouseClicks (false, false);
    };

    setLabel (title, 25.0f, true, colours::text, juce::Justification::centredLeft);
    title.setText ("SC220 LIVE FULL VERSION", juce::dontSendNotification);
    setLabel (subtitle, 13.5f, false, colours::textMuted, juce::Justification::centredLeft);
    subtitle.setText ("Full creative control is free for 365 days. Unlock permanently with one optional QRIS payment.",
                      juce::dontSendNotification);
    setLabel (trialBadge, 11.0f, true, colours::cyan, juce::Justification::centred);
    setLabel (price, 31.0f, true, colours::text, juce::Justification::centredLeft);
    price.setText (formatIdr (fullVersionPriceIdr), juce::dontSendNotification);
    setLabel (priceCaption, 11.5f, false, colours::textMuted, juce::Justification::centredLeft);
    priceCaption.setText ("ONE-TIME PERPETUAL LICENCE / 1 COMPUTER", juce::dontSendNotification);

    setLabel (nameLabel, 10.0f, true, colours::textMuted, juce::Justification::centredLeft);
    setLabel (emailLabel, 10.0f, true, colours::textMuted, juce::Justification::centredLeft);
    nameLabel.setText ("NAME FOR RECEIPT", juce::dontSendNotification);
    emailLabel.setText ("EMAIL FOR RECEIPT", juce::dontSendNotification);
    configureEditor (customerName, "Your name");
    configureEditor (customerEmail, "name@example.com");
    customerName.setText (checkout.savedCustomerName(), false);
    customerEmail.setText (checkout.savedCustomerEmail(), false);

    setLabel (paymentStatus, 11.5f, false, colours::textMuted, juce::Justification::topLeft);
    paymentStatus.setMinimumHorizontalScale (0.75f);

    setLabel (requestLabel, 10.0f, true, colours::textMuted, juce::Justification::centredLeft);
    requestLabel.setText ("OFFLINE ACTIVATION / COMPUTER REQUEST ID", juce::dontSendNotification);
    configureEditor (requestId, {});
    requestId.setReadOnly (true);
    requestId.setText (license.requestId(), false);

    setLabel (activationLabel, 10.0f, true, colours::textMuted, juce::Justification::centredLeft);
    activationLabel.setText ("ACTIVATION CODE", juce::dontSendNotification);
    configureEditor (activationCode, "Paste signed activation code");
    activationCode.setMultiLine (true, true);
    activationCode.setReturnKeyStartsNewLine (true);

    setLabel (privacyNote, 10.5f, false, colours::textMuted, juce::Justification::centredLeft);
    privacyNote.setText ("Payment is hosted by Midtrans. SC220 Live sends only receipt details, the order capability and this computer request ID to the licence server. Audio is never uploaded.",
                         juce::dontSendNotification);

    const std::array<juce::Component*, 21> components {{
        &title, &subtitle, &trialBadge, &price, &priceCaption,
        &nameLabel, &emailLabel, &customerName, &customerEmail,
        &buyButton, &checkButton, &cancelButton, &paymentStatus,
        &requestLabel, &requestId, &copyRequest, &activationLabel,
        &activationCode, &activateButton, &closeButton, &privacyNote
    }};

    for (auto* component : components)
        addAndMakeVisible (*component);

    buyButton.onClick = [this] { startOrOpenCheckout(); };
    checkButton.onClick = [this]
    {
        checkout.checkNow();
        setStatus ("Checking the saved order securely with the licence server...", colours::cyan);
    };
    cancelButton.onClick = [this]
    {
        if (checkout.cancelPendingCheckout())
            setStatus ("Unpaid checkout cancelled. You can create a fresh QRIS payment.", colours::green);
        else
            setStatus (checkout.snapshot().message, colours::amber);
        refreshState();
    };
    copyRequest.onClick = [this]
    {
        juce::SystemClipboard::copyTextToClipboard (license.requestId());
        setStatus ("Computer Request ID copied.", colours::green);
    };
    activateButton.onClick = [this] { activateOfflineCode(); };
    closeButton.onClick = [this]
    {
        setVisible (false);
        if (onClose)
            onClose();
    };

    refreshState();
}

LiveLicensePanel::~LiveLicensePanel()
{
    stopTimer();
}

void LiveLicensePanel::configureEditor (juce::TextEditor& editor,
                                        const juce::String& placeholder)
{
    editor.setFont (askp::AskpLookAndFeel::get().jakarta (12.5f, false));
    editor.setColour (juce::TextEditor::backgroundColourId, colours::panelLo);
    editor.setColour (juce::TextEditor::outlineColourId, colours::borderSoft);
    editor.setColour (juce::TextEditor::focusedOutlineColourId, colours::cyan.withAlpha (0.85f));
    editor.setColour (juce::TextEditor::textColourId, colours::text);
    editor.setColour (juce::TextEditor::highlightColourId, colours::cyan.withAlpha (0.22f));
    editor.setColour (juce::TextEditor::highlightedTextColourId, colours::text);
    editor.setTextToShowWhenEmpty (placeholder, colours::textMuted.withAlpha (0.66f));
    editor.setBorder (juce::BorderSize<int> (8, 10, 8, 10));
}

juce::String LiveLicensePanel::formatIdr (int amount)
{
    auto digits = juce::String (amount);
    for (int i = digits.length() - 3; i > 0; i -= 3)
        digits = digits.substring (0, i) + "." + digits.substring (i);
    return "Rp" + digits;
}

juce::String LiveLicensePanel::checkoutStatusLabel (askp::LicenseCheckoutClient::Status status)
{
    using Status = askp::LicenseCheckoutClient::Status;
    switch (status)
    {
        case Status::CreatingCheckout: return "CREATING QRIS";
        case Status::WaitingForPayment: return "WAITING FOR PAYMENT";
        case Status::VerifyingPayment: return "VERIFYING PAYMENT";
        case Status::PaymentConfirmed: return "PAYMENT CONFIRMED";
        case Status::ActivationReady: return "ACTIVATING";
        case Status::Completed: return "LICENSED";
        case Status::Offline: return "OFFLINE";
        case Status::RecoveryRequired: return "RECOVERY REQUIRED";
        case Status::Failed: return "PAYMENT ERROR";
        case Status::Expired: return "CHECKOUT EXPIRED";
        case Status::Idle:
        default: return "READY";
    }
}

void LiveLicensePanel::setStatus (const juce::String& text, juce::Colour colour)
{
    paymentStatus.setText (text, juce::dontSendNotification);
    paymentStatus.setColour (juce::Label::textColourId, colour);
}

void LiveLicensePanel::startOrOpenCheckout()
{
    if (license.isLicensed())
    {
        setStatus (license.statusText(), colours::green);
        return;
    }

    const auto name = customerName.getText().trim();
    const auto email = customerEmail.getText().trim();
    const auto snap = checkout.snapshot();

    if (snap.hasPendingOrder)
    {
        checkout.startCheckout (name, email);
        setStatus (snap.checkoutCanOpen
                     ? "Opening the saved QRIS checkout..."
                     : "Checking the saved purchase before opening another checkout...",
                   colours::cyan);
        return;
    }

    if (name.length() < 2)
    {
        setStatus ("Enter the name that should appear on the payment receipt.", colours::amber);
        customerName.grabKeyboardFocus();
        return;
    }
    if (! looksLikeEmail (email))
    {
        setStatus ("Enter a valid email address for the receipt and purchase recovery.", colours::amber);
        customerEmail.grabKeyboardFocus();
        return;
    }

    checkout.startCheckout (name, email);
    setStatus ("Creating a secure QRIS checkout for " + formatIdr (fullVersionPriceIdr) + "...",
               colours::cyan);
}

void LiveLicensePanel::activateOfflineCode()
{
    const auto result = license.activate (activationCode.getText());
    if (result.ok)
    {
        checkout.markActivated();
        setStatus (result.message, colours::green);
        refreshState();
        if (onLicenseChanged)
            onLicenseChanged();
    }
    else
    {
        checkout.reportActivationFailure (result.message);
        setStatus (result.message, colours::red);
    }
}

void LiveLicensePanel::timerCallback()
{
    license.refresh();

    if (const auto url = checkout.takeCheckoutUrlToOpen(); url.isNotEmpty())
        juce::URL (url).launchInDefaultBrowser();

    if (const auto code = checkout.takeActivationCode(); code.isNotEmpty())
    {
        const auto result = license.activate (code);
        if (result.ok)
        {
            checkout.markActivated();
            setStatus ("Payment verified. SC220 Live Full Version is activated.", colours::green);
            if (onLicenseChanged)
                onLicenseChanged();
        }
        else
        {
            checkout.reportActivationFailure (result.message);
            setStatus ("Payment was confirmed, but local activation failed: " + result.message,
                       colours::red);
        }
    }

    refreshState();
}

void LiveLicensePanel::refreshState()
{
    license.refresh();
    const auto state = license.state();
    const auto snap = checkout.snapshot();

    if (license.isLicensed())
    {
        trialBadge.setText ("FULL VERSION", juce::dontSendNotification);
        trialBadge.setColour (juce::Label::textColourId, colours::green);
        price.setText ("ACTIVATED", juce::dontSendNotification);
        price.setColour (juce::Label::textColourId, colours::green);
        priceCaption.setText (license.statusText().toUpperCase(), juce::dontSendNotification);
        buyButton.setButtonText ("FULL VERSION ACTIVE");
        buyButton.setEnabled (false);
    }
    else
    {
        const bool expired = state == askp::LicenseManager::State::ReadOnly;
        const bool clockIssue = state == askp::LicenseManager::State::ClockNeedsAttention;
        trialBadge.setText (clockIssue ? "CHECK SYSTEM CLOCK"
                                      : expired ? "FREE PERIOD ENDED"
                                                : "FULL FEATURES ACTIVE",
                            juce::dontSendNotification);
        trialBadge.setColour (juce::Label::textColourId,
                              clockIssue || expired ? colours::amber : colours::cyan);
        const int amount = snap.amountIdr > 0 ? snap.amountIdr : fullVersionPriceIdr;
        price.setText (formatIdr (amount), juce::dontSendNotification);
        price.setColour (juce::Label::textColourId, colours::text);
        priceCaption.setText ("ONE-TIME PERPETUAL LICENCE / 1 COMPUTER", juce::dontSendNotification);
        buyButton.setButtonText (snap.checkoutCanOpen ? "OPEN QRIS PAYMENT PAGE"
                                                       : snap.hasPendingOrder ? "VERIFY SAVED PURCHASE"
                                                                              : "BUY FULL VERSION - QRIS");
        buyButton.setEnabled (! snap.busy);
    }

    checkButton.setVisible (snap.hasPendingOrder && ! license.isLicensed());
    checkButton.setEnabled (! snap.busy);
    cancelButton.setVisible (snap.hasPendingOrder && ! license.isLicensed());
    cancelButton.setEnabled (! snap.busy);

    const auto summary = checkoutStatusLabel (snap.status);
    if (snap.message.isNotEmpty()
        && (snap.status != previousCheckoutStatus || snap.message != previousCheckoutMessage))
    {
        juce::Colour colour = colours::textMuted;
        if (snap.status == askp::LicenseCheckoutClient::Status::Completed
            || snap.status == askp::LicenseCheckoutClient::Status::ActivationReady
            || snap.status == askp::LicenseCheckoutClient::Status::PaymentConfirmed)
            colour = colours::green;
        else if (snap.status == askp::LicenseCheckoutClient::Status::Failed
                 || snap.status == askp::LicenseCheckoutClient::Status::RecoveryRequired)
            colour = colours::red;
        else if (snap.status == askp::LicenseCheckoutClient::Status::Offline
                 || snap.status == askp::LicenseCheckoutClient::Status::Expired)
            colour = colours::amber;
        else if (snap.busy)
            colour = colours::cyan;

        setStatus (summary + "\n" + snap.message, colour);
    }

    requestId.setText (license.requestId(), false);
    previousCheckoutStatus = snap.status;
    previousCheckoutMessage = snap.message;
    repaint();
}

juce::Rectangle<int> LiveLicensePanel::cardBounds() const
{
    auto area = getLocalBounds().reduced (24);
    const int width = juce::jmin (760, area.getWidth());
    const int height = juce::jmin (690, area.getHeight());
    return juce::Rectangle<int> (width, height).withCentre (area.getCentre());
}

void LiveLicensePanel::paint (juce::Graphics& g)
{
    g.setColour (juce::Colours::black.withAlpha (0.74f));
    g.fillAll();

    const auto card = cardBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.34f));
    g.fillRoundedRectangle (card.translated (0.0f, 7.0f), 16.0f);

    juce::ColourGradient panel (colours::panelHi.brighter (0.025f), card.getX(), card.getY(),
                                colours::panelLo, card.getRight(), card.getBottom(), false);
    panel.addColour (0.55, colours::panel);
    g.setGradientFill (panel);
    g.fillRoundedRectangle (card, 16.0f);
    g.setColour (colours::border);
    g.drawRoundedRectangle (card.reduced (0.5f), 16.0f, 1.0f);
    g.setColour (colours::cyan.withAlpha (0.12f));
    g.fillRoundedRectangle ({ card.getX() + 20.0f, card.getY() + 1.0f,
                              card.getWidth() - 40.0f, 2.0f }, 1.0f);

    const auto priceArea = juce::Rectangle<float> ((float) price.getX() - 12.0f,
                                                    (float) price.getY() - 7.0f,
                                                    (float) price.getWidth() + 24.0f,
                                                    (float) price.getHeight() + 29.0f);
    g.setColour (colours::cyan.withAlpha (0.045f));
    g.fillRoundedRectangle (priceArea, 10.0f);
    g.setColour (colours::borderSoft);
    g.drawRoundedRectangle (priceArea, 10.0f, 1.0f);
}

void LiveLicensePanel::resized()
{
    auto card = cardBounds().reduced (24);
    auto header = card.removeFromTop (70);
    title.setBounds (header.removeFromTop (34));
    subtitle.setBounds (header.removeFromTop (25));

    trialBadge.setBounds (card.removeFromTop (30).removeFromRight (240));
    card.removeFromTop (8);

    auto priceRow = card.removeFromTop (74);
    price.setBounds (priceRow.removeFromLeft (270).reduced (8, 0));
    priceCaption.setBounds (priceRow.reduced (8, 8));
    card.removeFromTop (12);

    auto customerRow = card.removeFromTop (72);
    auto left = customerRow.removeFromLeft (customerRow.getWidth() / 2).reduced (0, 0);
    customerRow.removeFromLeft (12);
    auto right = customerRow;
    nameLabel.setBounds (left.removeFromTop (18));
    customerName.setBounds (left.removeFromTop (42));
    emailLabel.setBounds (right.removeFromTop (18));
    customerEmail.setBounds (right.removeFromTop (42));
    card.removeFromTop (10);

    auto purchaseRow = card.removeFromTop (42);
    buyButton.setBounds (purchaseRow.removeFromLeft (360));
    purchaseRow.removeFromLeft (10);
    checkButton.setBounds (purchaseRow.removeFromLeft (150));
    purchaseRow.removeFromLeft (10);
    cancelButton.setBounds (purchaseRow);
    card.removeFromTop (10);

    paymentStatus.setBounds (card.removeFromTop (64));
    card.removeFromTop (8);

    requestLabel.setBounds (card.removeFromTop (18));
    auto requestRow = card.removeFromTop (40);
    copyRequest.setBounds (requestRow.removeFromRight (160));
    requestRow.removeFromRight (10);
    requestId.setBounds (requestRow);
    card.removeFromTop (10);

    activationLabel.setBounds (card.removeFromTop (18));
    auto activationRow = card.removeFromTop (76);
    activateButton.setBounds (activationRow.removeFromRight (190));
    activationRow.removeFromRight (10);
    activationCode.setBounds (activationRow);
    card.removeFromTop (10);

    privacyNote.setBounds (card.removeFromTop (43));
    closeButton.setBounds (card.removeFromBottom (38).removeFromRight (120));
}

void LiveLicensePanel::visibilityChanged()
{
    if (isVisible())
    {
        refreshState();
        startTimerHz (kRefreshHz);
        toFront (true);
    }
    else
    {
        stopTimer();
    }
}

} // namespace askp::live
