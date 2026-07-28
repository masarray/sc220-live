#pragma once

#include <juce_core/juce_core.h>
#include <juce_cryptography/juce_cryptography.h>

#include <atomic>

namespace askp {

// One process-wide, low-priority checkout worker. Network work never runs on
// the audio or message thread. A paid order remains recoverable from its saved
// order capability even when the QR page has expired or the app was closed for
// several days. The signed signed activation activation is still verified locally by
// LicenseManager before any editing control is unlocked.
class LicenseCheckoutClient final : private juce::Thread {
public:
    enum class Status {
        Idle,
        CreatingCheckout,
        WaitingForPayment,
        VerifyingPayment,
        PaymentConfirmed,
        ActivationReady,
        Completed,
        Offline,
        RecoveryRequired,
        Failed,
        Expired
    };

    struct Snapshot {
        Status status = Status::Idle;
        juce::String message;
        juce::String orderId;
        juce::String checkoutUrl;
        int amountIdr = 0;
        juce::int64 expiresAt = 0;
        bool hasPendingOrder = false;
        bool checkoutCanOpen = false;
        bool checkoutExpiredLocally = false;
        bool automaticPolling = false;
        bool busy = false;
    };

    static LicenseCheckoutClient& shared(const juce::String& deviceRequestId);
    ~LicenseCheckoutClient() override;

    void startCheckout(const juce::String& customerName,
                       const juce::String& customerEmail);
    void checkNow();
    void requestOpenCheckout();
    void markActivated();
    void reportActivationFailure(const juce::String& message);

    // This is intentionally local-only: it abandons an unpaid checkout after
    // the UI has completed a fresh server verification and received a pending
    // state. Paid/activation-ready orders are never discarded here.
    bool cancelPendingCheckout()
    {
        {
            const juce::ScopedLock lock(stateLock_);
            if (!pending_.hasCredentials()) {
                state_.status = Status::Idle;
                state_.message = "No active checkout needs to be cancelled.";
                state_.busy = false;
                populateSnapshotFromPending(state_);
                return true;
            }

            if (state_.status == Status::PaymentConfirmed
                || state_.status == Status::ActivationReady
                || state_.status == Status::Completed) {
                state_.message = "Payment is already confirmed, so this checkout cannot be cancelled.";
                state_.busy = false;
                populateSnapshotFromPending(state_);
                return false;
            }

            pending_ = {};
            browserUrlReady_.clear();
            activationCodeReady_.clear();
            requestedCustomerName_.clear();
            requestedCustomerEmail_.clear();

            state_ = {};
            state_.status = Status::Idle;
            state_.message = "Checkout cancelled. Close the old QRIS page before creating another payment.";
            state_.busy = false;
        }

        createRequested_.store(false, std::memory_order_release);
        refreshRequested_.store(false, std::memory_order_release);
        autoPoll_.store(false, std::memory_order_release);
        wake_.signal();
        checkoutStateFile().deleteFile();
        return true;
    }

    Snapshot snapshot() const;
    juce::String takeCheckoutUrlToOpen();
    juce::String takeActivationCode();

    juce::String savedCustomerName() const;
    juce::String savedCustomerEmail() const;

private:
    struct PendingOrder {
        juce::String orderId;
        juce::String orderToken;
        juce::String checkoutUrl;
        juce::String customerName;
        juce::String customerEmail;
        int amountIdr = 0;
        juce::int64 createdAt = 0;
        juce::int64 expiresAt = 0;
        juce::int64 lastCheckedAt = 0;

        bool hasCredentials() const noexcept;
        bool checkoutCanOpen() const;
        bool checkoutExpiredLocally() const noexcept;
    };

    struct HttpResult {
        int statusCode = 0;
        juce::String body;
        bool transportOk = false;
        bool responseTooLarge = false;
    };

    explicit LicenseCheckoutClient(juce::String deviceRequestId);

    void run() override;
    void ensureThreadRunning();
    void beginAutomaticPollingWindow();
    int nextAutomaticPollDelayMs(Status status) const;
    void performCreateCheckout(juce::String customerName, juce::String customerEmail);
    void performOrderRefresh();

    HttpResult request(const juce::String& method,
                       const juce::String& path,
                       const juce::String& jsonBody,
                       const juce::String& bearerToken) const;

    void populateSnapshotFromPending(Snapshot& snapshot) const;
    void setState(Status status, const juce::String& message, bool busy);
    void setPendingState(Status status, const juce::String& message, bool busy);
    juce::String readApiError(const juce::String& body, const juce::String& fallback) const;

    juce::File checkoutStateFile() const;
    PendingOrder loadPendingOrder() const;
    bool savePendingOrder(const PendingOrder& order) const;
    void clearPendingOrder();
    juce::String pendingProof(const PendingOrder& order) const;

    static bool isTrustedCheckoutUrl(const juce::String& url);
    static bool isTerminalUnpaidStatus(const juce::String& status);
    static bool isPaidStatus(const juce::String& status);
    static juce::int64 nowUtcSeconds();

    const juce::String deviceRequestId_;

    mutable juce::CriticalSection stateLock_;
    Snapshot state_;
    PendingOrder pending_;
    juce::String requestedCustomerName_;
    juce::String requestedCustomerEmail_;
    juce::String browserUrlReady_;
    juce::String activationCodeReady_;

    juce::WaitableEvent wake_;
    std::atomic<bool> createRequested_{false};
    std::atomic<bool> refreshRequested_{false};
    std::atomic<bool> autoPoll_{false};
    std::atomic<bool> activationCompleted_{false};
    std::atomic<juce::int64> automaticPollingStartedAt_{0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LicenseCheckoutClient)
};

} // namespace askp
