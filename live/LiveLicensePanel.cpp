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
        auto font = juce::Font { juce::FontOptions { size } };
        label.setFont (bold ? font.boldened() : font);
        label.setColour (juce::Label::textColourId, colour);
        label.setJustificationType (justification);
        label.setInterceptsMouseClicks (false, false);
    };

    setLabel (title, 22.0f, true, colours::text, juce::Justification::centredLeft);
    title.setText ("SC220 LIVE FULL VERSION", juce::dontSendNotification);
    setLabel (subtitle, 12.5f, false, colours::textMuted, juce::Justification::centredLeft);
    subtitle.setText ("Full creative control is free for 365 days. Unlock permanently with one optional QRIS payment.",
                      juce::dontSendNotification);
    setLabel (trialBadge, 10.5f, true, colours::cyan, juce::Justification::centredRight);
    setLabel (purchaseLabel, 10.5f, true, colours::cyan, juce::Justification::centredLeft);
    purchaseLabel.setText ("ONLINE PURCHASE", juce::dontSendNotification);
    setLabel (price, 27.0f, true, colours::text, juce::Justification::centredLeft);
    price.setText (formatIdr (fullVersionPriceIdr), juce::dontSendNotification);
    setLabel (priceCaption, 10.5f, false, colours::textMuted, juce::Justification::centredLeft);
    priceCaption.setText ("ONE-TIME PERPETUAL LICENCE / 1 COMPUTER", juce::dontSendNotification);

    setLabel (nameLabel, 10.5f, true, colours::textMuted, juce::Justification::centredLeft);
    setLabel (emailLabel, 10.5f, true, colours::textMuted, juce::Justification::centredLeft);
    nameLabel.setText ("NAME FOR RECEIPT", juce::dontSendNotification);
    emailLabel.setText ("EMAIL FOR RECEIPT", juce::dontSendNotification);
    configureEditor (customerName, "Your name");
    configureEditor (customerEmail, "name@example.com");
    customerName.setText (checkout.savedCustomerName(), false);
    customerEmail.setText (checkout.savedCustomerEmail(), false);

    setLabel (paymentStatus, 11.5f, false, colours::textMuted, juce::Justification::topLeft);
    paymentStatus.setMinimumHorizontalScale (0.75f);

    setLabel (requestLabel, 10.5f, true, colours::cyan, juce::Justification::centredLeft);
    requestLabel.setText ("OFFLINE ACTIVATION  /  COMPUTER REQUEST ID", juce::dontSendNotification);
    configureEditor (requestId, {});
    requestId.setReadOnly (true);
    requestId.setText (license.requestId(), false);

    setLabel (activationLabel, 10.5f, true, colours::textMuted, juce::Justification::centredLeft);
    activationLabel.setText ("SIGNED ACTIVATION CODE", juce::dontSendNotification);
    configureEditor (activationCode, "Paste signed activation code");
    activationCode.setMultiLine (true, true);
    activationCode.setReturnKeyStartsNewLine (true);

    setLabel (privacyNote, 10.5f, false, colours::textMuted, juce::Justification::topLeft);
    privacyNote.setMinimumHorizontalScale (0.82f);
    privacyNote.setText ("MIDTRANS SECURE PAYMENT\nOnly receipt and licence details leave this computer. Audio is never uploaded.",
                         juce::dontSendNotification);

    const std::array<juce::Component*, 22> components {{
        &title, &subtitle, &trialBadge, &purchaseLabel, &price, &priceCaption,
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
    editor.setFont (juce::Font { juce::FontOptions { 12.0f } });
    editor.setColour (juce::TextEditor::backgroundColourId, colours::panelLo);
    editor.setColour (juce::TextEditor::outlineColourId, colours::borderSoft);
    editor.setColour (juce::TextEditor::focusedOutlineColourId, colours::cyan.withAlpha (0.85f));
    editor.setColour (juce::TextEditor::textColourId, colours::text);
    editor.setColour (juce::TextEditor::highlightColourId, colours::cyan.withAlpha (0.22f));
    editor.setColour (juce::TextEditor::highlightedTextColourId, colours::text);
    editor.setTextToShowWhenEmpty (placeholder, colours::textMuted.withAlpha (0.66f));
    editor.setBorder (juce::BorderSize<int> (7, 10, 7, 10));
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
    const int width = juce::jmin (900, area.getWidth());
    const int height = juce::jmin (620, area.getHeight());
    return juce::Rectangle<int> (width, height).withCentre (area.getCentre());
}

void LiveLicensePanel::paint (juce::Graphics& g)
{
    g.setColour (juce::Colours::black.withAlpha (0.74f));
    g.fillAll();

    const auto card = cardBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.30f));
    g.fillRoundedRectangle (card.translated (0.0f, 5.0f), 12.0f);

    juce::ColourGradient panel (colours::panelHi.brighter (0.025f), card.getX(), card.getY(),
                                colours::panelLo, card.getRight(), card.getBottom(), false);
    panel.addColour (0.55, colours::panel);
    g.setGradientFill (panel);
    g.fillRoundedRectangle (card, 12.0f);
    g.setColour (colours::border);
    g.drawRoundedRectangle (card.reduced (0.5f), 12.0f, 1.0f);
    g.setColour (colours::cyan.withAlpha (0.12f));
    g.fillRoundedRectangle ({ card.getX() + 20.0f, card.getY() + 1.0f,
                              card.getWidth() - 40.0f, 2.0f }, 1.0f);

    auto content = cardBounds().reduced (26);
    content.removeFromTop (76);
    content.removeFromBottom (46);
    constexpr int columnGap = 22;
    auto purchasePanel = content.removeFromLeft ((content.getWidth() - columnGap) / 2).toFloat();
    content.removeFromLeft (columnGap);
    const auto offlinePanel = content.toFloat();

    for (const auto panelBounds : { purchasePanel, offlinePanel })
    {
        g.setColour (colours::panelLo.withAlpha (0.72f));
        g.fillRoundedRectangle (panelBounds, 7.0f);
        g.setColour (colours::borderSoft.withAlpha (0.78f));
        g.drawRoundedRectangle (panelBounds.reduced (0.5f), 6.5f, 1.0f);
    }

    const auto priceArea = price.getBounds().getUnion (priceCaption.getBounds())
                                .expanded (8, 5).toFloat();
    g.setColour (colours::cyan.withAlpha (0.055f));
    g.fillRoundedRectangle (priceArea, 6.0f);
    g.setColour (colours::borderSoft);
    g.drawRoundedRectangle (priceArea, 6.0f, 1.0f);
}

void LiveLicensePanel::resized()
{
    auto card = cardBounds().reduced (26);
    auto header = card.removeFromTop (66);
    auto titleRow = header.removeFromTop (31);
    trialBadge.setBounds (titleRow.removeFromRight (250));
    title.setBounds (titleRow);
    subtitle.setBounds (header.removeFromTop (25));
    card.removeFromTop (10);

    auto footer = card.removeFromBottom (36);
    closeButton.setBounds (footer.removeFromRight (112));
    card.removeFromBottom (10);

    constexpr int columnGap = 22;
    auto purchaseColumn = card.removeFromLeft ((card.getWidth() - columnGap) / 2).reduced (16, 14);
    card.removeFromLeft (columnGap);
    auto offlineColumn = card.reduced (16, 14);

    purchaseLabel.setBounds (purchaseColumn.removeFromTop (18));
    purchaseColumn.removeFromTop (8);
    auto priceRow = purchaseColumn.removeFromTop (66);
    price.setBounds (priceRow.removeFromTop (37));
    priceCaption.setBounds (priceRow);
    purchaseColumn.removeFromTop (12);

    nameLabel.setBounds (purchaseColumn.removeFromTop (17));
    customerName.setBounds (purchaseColumn.removeFromTop (38));
    purchaseColumn.removeFromTop (9);
    emailLabel.setBounds (purchaseColumn.removeFromTop (17));
    customerEmail.setBounds (purchaseColumn.removeFromTop (38));
    purchaseColumn.removeFromTop (14);
    buyButton.setBounds (purchaseColumn.removeFromTop (40));
    purchaseColumn.removeFromTop (8);
    auto secondaryActions = purchaseColumn.removeFromTop (34);
    checkButton.setBounds (secondaryActions.removeFromLeft ((secondaryActions.getWidth() - 8) / 2));
    secondaryActions.removeFromLeft (8);
    cancelButton.setBounds (secondaryActions);
    purchaseColumn.removeFromTop (8);
    paymentStatus.setBounds (purchaseColumn);

    requestLabel.setBounds (offlineColumn.removeFromTop (18));
    offlineColumn.removeFromTop (8);
    auto requestRow = offlineColumn.removeFromTop (40);
    copyRequest.setBounds (requestRow.removeFromRight (144));
    requestRow.removeFromRight (8);
    requestId.setBounds (requestRow);
    offlineColumn.removeFromTop (20);
    activationLabel.setBounds (offlineColumn.removeFromTop (18));
    offlineColumn.removeFromTop (8);
    activationCode.setBounds (offlineColumn.removeFromTop (112));
    offlineColumn.removeFromTop (10);
    activateButton.setBounds (offlineColumn.removeFromTop (40));
    offlineColumn.removeFromTop (14);
    privacyNote.setBounds (offlineColumn);
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
