// Independent SC220 Live licence interface; retain existing signed activations.
#pragma once

#include <juce_core/juce_core.h>
#include <juce_cryptography/juce_cryptography.h>

#include <atomic>

namespace askp {

class LicenseManager {
public:
    enum class State {
        BetaActive,
        BetaExpiring,
        ReadOnly,
        Licensed,
        ClockNeedsAttention
    };

    struct ActivationResult {
        bool ok = false;
        juce::String message;
    };

    LicenseManager();

    void refresh();

    State state() const noexcept { return state_.load(std::memory_order_acquire); }
    bool editingAllowed() const noexcept;
    bool isLicensed() const noexcept { return state() == State::Licensed; }
    int daysRemaining() const noexcept { return daysRemaining_.load(std::memory_order_acquire); }

    juce::String requestId() const;
    juce::String machineHash() const { return machineHash_; }
    juce::String licensedTo() const;
    juce::String statusText() const;

    ActivationResult activate(const juce::String& activationCode);

    static constexpr int betaDurationDays = 365;

    // Shared by the private Key Activator. The plugin itself never receives a
    // private signing key.
    static juce::String normaliseRequestId(const juce::String& requestId);
    static juce::String makeActivationPayload(const juce::String& machineHash,
                                              const juce::String& customerReference,
                                              juce::int64 issuedUtcSeconds);
    static juce::String signActivationPayload(const juce::String& payload,
                                              const juce::RSAKey& privateKey);
    static juce::String makeActivationCode(const juce::String& payload,
                                           const juce::String& signatureHex);

private:
    struct PersistedState {
        juce::int64 betaStartUtc = 0;
        juce::int64 lastSeenUtc = 0;
        juce::String activationCode;
    };

    static juce::int64 nowUtcSeconds();
    static juce::String computeMachineHash();
    static juce::String sha256Hex(const juce::String& text);
    static juce::String textToHex(const juce::String& text);
    static juce::String hexToText(const juce::String& hex);
    static bool isHex(const juce::String& text);
    static juce::String compactCode(const juce::String& text);

    juce::File stateFile() const;
    PersistedState loadFileState() const;
    PersistedState loadRegistryState() const;
    void saveState(const PersistedState& state) const;
    juce::String stateProof(const PersistedState& state) const;

    bool validateActivationCode(const juce::String& code,
                                juce::String* customerReference,
                                juce::String* error) const;

    void evaluateState(bool forcePersist);

    juce::String machineHash_;
    juce::String activationCode_;
    juce::String licensedTo_;
    // RSA verification is required only when an activation code is loaded or
    // replaced. Periodic clock/beta refreshes reuse this immutable result so
    // opening the editor cannot cause a cold-start cryptography spike.
    bool activationValidationKnown_ = false;
    bool activationVerified_ = false;
    juce::int64 betaStartUtc_ = 0;
    juce::int64 lastSeenUtc_ = 0;
    juce::int64 lastPersistUtc_ = 0;

    std::atomic<State> state_{State::BetaActive};
    std::atomic<int> daysRemaining_{betaDurationDays};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LicenseManager)
};

} // namespace askp
