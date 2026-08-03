#include "LicenseCheckoutClient.h"

#ifndef SONKUPIK_LICENSE_API_URL
 #define SONKUPIK_LICENSE_API_URL "https://masarray-license-api.masari.workers.dev"
#endif

#ifndef SONKUPIK_LICENSE_PRODUCT_ID
 #define SONKUPIK_LICENSE_PRODUCT_ID "sonkupik-live"
#endif

#ifndef SONKUPIK_LICENSE_PRICE_IDR
 #define SONKUPIK_LICENSE_PRICE_IDR 399000
#endif

namespace askp {
namespace {
constexpr auto kStateSalt = "SONKUPIK-LIVE-CHECKOUT-V2-4CE8D09A3F7612B5";
constexpr int kRequestTimeoutMs = 8000;
constexpr size_t kMaxApiResponseBytes = 128 * 1024;
constexpr juce::int64 kFastPollWindowSeconds = 2 * 60;
constexpr juce::int64 kNormalPollWindowSeconds = 10 * 60;
constexpr juce::int64 kAutomaticPollWindowSeconds = 30 * 60;

juce::String propertyString(juce::DynamicObject* object, const juce::Identifier& key)
{
    return object != nullptr ? object->getProperty(key).toString().trim() : juce::String{};
}

juce::int64 propertyInt64(juce::DynamicObject* object, const juce::Identifier& key)
{
    return propertyString(object, key).getLargeIntValue();
}

int propertyInt(juce::DynamicObject* object, const juce::Identifier& key)
{
    return propertyString(object, key).getIntValue();
}

juce::String normaliseBaseUrl()
{
    return juce::String(SONKUPIK_LICENSE_API_URL).trim().trimCharactersAtEnd("/");
}
} // namespace

bool LicenseCheckoutClient::PendingOrder::hasCredentials() const noexcept
{
    return orderId.isNotEmpty() && orderToken.isNotEmpty();
}

bool LicenseCheckoutClient::PendingOrder::checkoutCanOpen() const
{
    return hasCredentials()
        && LicenseCheckoutClient::isTrustedCheckoutUrl(checkoutUrl)
        && !checkoutExpiredLocally();
}

bool LicenseCheckoutClient::PendingOrder::checkoutExpiredLocally() const noexcept
{
    return expiresAt > 0 && expiresAt <= LicenseCheckoutClient::nowUtcSeconds();
}

LicenseCheckoutClient& LicenseCheckoutClient::shared(const juce::String& deviceRequestId)
{
    static LicenseCheckoutClient client(deviceRequestId);
    jassert(client.deviceRequestId_ == deviceRequestId);
    return client;
}

LicenseCheckoutClient::LicenseCheckoutClient(juce::String deviceRequestId)
    : juce::Thread("SC220 Live License Checkout"),
      deviceRequestId_(std::move(deviceRequestId))
{
    pending_ = loadPendingOrder();
    if (pending_.hasCredentials()) {
        state_.status = Status::VerifyingPayment;
        state_.message = pending_.checkoutExpiredLocally()
            ? "Saved purchase found. Checking whether payment completed before the QR page expired..."
            : "Saved QRIS checkout restored. Checking payment automatically...";
        populateSnapshotFromPending(state_);
        state_.busy = true;
        beginAutomaticPollingWindow();
        refreshRequested_.store(true, std::memory_order_release);
        ensureThreadRunning();
        wake_.signal();
    } else {
        state_.message = "Unlock SC220 Live Full Version once with QRIS, or use offline activation support.";
    }
}

LicenseCheckoutClient::~LicenseCheckoutClient()
{
    signalThreadShouldExit();
    autoPoll_.store(false, std::memory_order_release);
    wake_.signal();
    stopThread(kRequestTimeoutMs + 1500);
}

void LicenseCheckoutClient::ensureThreadRunning()
{
    if (!isThreadRunning())
        startThread(juce::Thread::Priority::low);
}

void LicenseCheckoutClient::beginAutomaticPollingWindow()
{
    automaticPollingStartedAt_.store(nowUtcSeconds(), std::memory_order_release);
    autoPoll_.store(true, std::memory_order_release);
}

int LicenseCheckoutClient::nextAutomaticPollDelayMs(Status status) const
{
    if (!autoPoll_.load(std::memory_order_acquire))
        return 0;

    const auto started = automaticPollingStartedAt_.load(std::memory_order_acquire);
    const auto elapsed = started > 0 ? nowUtcSeconds() - started : 0;
    if (elapsed >= kAutomaticPollWindowSeconds)
        return 0;

    if (status == Status::PaymentConfirmed)
        return 30000;
    if (status == Status::Offline || status == Status::RecoveryRequired)
        return elapsed < kFastPollWindowSeconds ? 30000 : 120000;
    if (elapsed < kFastPollWindowSeconds)
        return 10000;
    if (elapsed < kNormalPollWindowSeconds)
        return 30000;
    return 120000;
}

void LicenseCheckoutClient::startCheckout(const juce::String& customerName,
                                           const juce::String& customerEmail)
{
    bool restoreExisting = false;
    bool openExisting = false;
    {
        const juce::ScopedLock lock(stateLock_);
        restoreExisting = pending_.hasCredentials();
        openExisting = pending_.checkoutCanOpen();
        if (restoreExisting) {
            if (openExisting)
                browserUrlReady_ = pending_.checkoutUrl;
            state_.status = openExisting ? Status::WaitingForPayment : Status::VerifyingPayment;
            state_.message = openExisting
                ? "Existing QRIS checkout opened. Complete payment on your phone, then return here."
                : "A saved order already exists. Checking it before creating another payment.";
            state_.busy = !openExisting;
            populateSnapshotFromPending(state_);
        } else {
            requestedCustomerName_ = customerName.trim();
            requestedCustomerEmail_ = customerEmail.trim().toLowerCase();
            state_.status = Status::CreatingCheckout;
            state_.message = "Creating a secure Midtrans QRIS checkout...";
            state_.busy = true;
        }
    }

    beginAutomaticPollingWindow();
    if (restoreExisting) {
        if (!openExisting)
            refreshRequested_.store(true, std::memory_order_release);
    } else {
        createRequested_.store(true, std::memory_order_release);
    }
    ensureThreadRunning();
    wake_.signal();
}

void LicenseCheckoutClient::checkNow()
{
    {
        const juce::ScopedLock lock(stateLock_);
        if (!pending_.hasCredentials()) {
            state_.status = Status::Idle;
            state_.message = "No saved checkout is available. Create a QRIS checkout first.";
            state_.busy = false;
            return;
        }
        state_.status = Status::VerifyingPayment;
        state_.message = "Checking the saved payment directly with the licence service...";
        state_.busy = true;
        populateSnapshotFromPending(state_);
    }

    beginAutomaticPollingWindow();
    refreshRequested_.store(true, std::memory_order_release);
    ensureThreadRunning();
    wake_.signal();
}

void LicenseCheckoutClient::requestOpenCheckout()
{
    bool shouldRefresh = false;
    {
        const juce::ScopedLock lock(stateLock_);
        if (pending_.checkoutCanOpen()) {
            browserUrlReady_ = pending_.checkoutUrl;
            state_.message = "QRIS checkout opened in your browser. Payment verification resumed automatically.";
            shouldRefresh = true;
        } else if (pending_.hasCredentials()) {
            shouldRefresh = true;
            state_.status = Status::VerifyingPayment;
            state_.message = "The QR page is no longer openable. Checking the saved payment instead...";
            state_.busy = true;
        } else {
            state_.message = "No active checkout URL is available.";
        }
    }

    if (shouldRefresh) {
        beginAutomaticPollingWindow();
        refreshRequested_.store(true, std::memory_order_release);
        ensureThreadRunning();
        wake_.signal();
    }
}

void LicenseCheckoutClient::markActivated()
{
    activationCompleted_.store(true, std::memory_order_release);
    clearPendingOrder();
    autoPoll_.store(false, std::memory_order_release);
    setState(Status::Completed,
             "Payment verified. Full editing is permanently unlocked on this computer.",
             false);
}

void LicenseCheckoutClient::reportActivationFailure(const juce::String& message)
{
    setPendingState(Status::PaymentConfirmed,
                    "Payment is confirmed, but activation is delayed: " + message
                        + " The saved order will be checked again; do not pay twice.",
                    false);
    beginAutomaticPollingWindow();
    refreshRequested_.store(true, std::memory_order_release);
    ensureThreadRunning();
    wake_.signal();
}

LicenseCheckoutClient::Snapshot LicenseCheckoutClient::snapshot() const
{
    const juce::ScopedLock lock(stateLock_);
    auto result = state_;
    result.automaticPolling = autoPoll_.load(std::memory_order_acquire);
    return result;
}

juce::String LicenseCheckoutClient::takeCheckoutUrlToOpen()
{
    const juce::ScopedLock lock(stateLock_);
    auto result = browserUrlReady_;
    browserUrlReady_.clear();
    return result;
}

juce::String LicenseCheckoutClient::takeActivationCode()
{
    const juce::ScopedLock lock(stateLock_);
    auto result = activationCodeReady_;
    activationCodeReady_.clear();
    return result;
}

juce::String LicenseCheckoutClient::savedCustomerName() const
{
    const juce::ScopedLock lock(stateLock_);
    return pending_.customerName;
}

juce::String LicenseCheckoutClient::savedCustomerEmail() const
{
    const juce::ScopedLock lock(stateLock_);
    return pending_.customerEmail;
}

void LicenseCheckoutClient::run()
{
    while (!threadShouldExit()) {
        if (createRequested_.exchange(false, std::memory_order_acq_rel)) {
            juce::String name;
            juce::String email;
            {
                const juce::ScopedLock lock(stateLock_);
                name = requestedCustomerName_;
                email = requestedCustomerEmail_;
            }
            performCreateCheckout(std::move(name), std::move(email));
            continue;
        }

        const bool shouldRefresh = refreshRequested_.exchange(false, std::memory_order_acq_rel)
                                || autoPoll_.load(std::memory_order_acquire);
        if (shouldRefresh) {
            performOrderRefresh();
            if (threadShouldExit())
                break;

            const auto status = snapshot().status;
            const int delayMs = nextAutomaticPollDelayMs(status);
            if (delayMs > 0 && autoPoll_.load(std::memory_order_acquire)) {
                wake_.wait(delayMs);
            } else {
                autoPoll_.store(false, std::memory_order_release);
                const juce::ScopedLock lock(stateLock_);
                state_.automaticPolling = false;
                if (state_.status == Status::WaitingForPayment && pending_.hasCredentials()) {
                    state_.message = "Payment is still pending. The order is saved; use CHECK PAYMENT later and do not create a duplicate payment.";
                }
            }
            continue;
        }

        wake_.wait(-1);
    }
}

void LicenseCheckoutClient::performCreateCheckout(juce::String customerName,
                                                   juce::String customerEmail)
{
    auto body = juce::DynamicObject::Ptr(new juce::DynamicObject());
    body->setProperty("product_id", SONKUPIK_LICENSE_PRODUCT_ID);
    body->setProperty("device_request_id", deviceRequestId_);
    body->setProperty("customer_name", customerName);
    body->setProperty("customer_email", customerEmail);

    const auto response = request("POST", "/v1/checkout",
                                  juce::JSON::toString(juce::var(body.get()), false), {});
    if (!response.transportOk) {
        setState(Status::Offline,
                 "Licence server is temporarily unreachable. No payment was created; check your internet connection and try again.",
                 false);
        autoPoll_.store(false, std::memory_order_release);
        return;
    }
    if (response.responseTooLarge) {
        setState(Status::Failed, "Licence server returned an oversized response. No checkout was accepted.", false);
        autoPoll_.store(false, std::memory_order_release);
        return;
    }
    if (response.statusCode < 200 || response.statusCode >= 300) {
        setState(Status::Failed, readApiError(response.body, "Could not create the QRIS checkout."), false);
        autoPoll_.store(false, std::memory_order_release);
        return;
    }

    const auto parsed = juce::JSON::parse(response.body);
    auto* root = parsed.getDynamicObject();
    auto orderValue = root != nullptr ? root->getProperty("order") : juce::var{};
    auto* order = orderValue.getDynamicObject();

    PendingOrder created;
    created.orderId = propertyString(order, "id");
    created.orderToken = propertyString(order, "token");
    created.checkoutUrl = propertyString(order, "checkout_url");
    created.amountIdr = propertyInt(order, "amount_idr");
    created.expiresAt = propertyInt64(order, "expires_at");
    created.createdAt = nowUtcSeconds();
    created.customerName = std::move(customerName);
    created.customerEmail = std::move(customerEmail);

    if (!created.hasCredentials() || !isTrustedCheckoutUrl(created.checkoutUrl)
        || created.amountIdr != SONKUPIK_LICENSE_PRICE_IDR
        || created.expiresAt <= created.createdAt) {
        setState(Status::Failed,
                 "Licence server returned an incomplete checkout or a price that does not match Rp399.000. No payment was opened.",
                 false);
        autoPoll_.store(false, std::memory_order_release);
        return;
    }

    if (activationCompleted_.load(std::memory_order_acquire))
        return;

    {
        const juce::ScopedLock lock(stateLock_);
        if (activationCompleted_.load(std::memory_order_acquire))
            return;
        pending_ = created;
        state_.status = Status::WaitingForPayment;
        state_.message = "Securing the checkout reference before opening QRIS...";
        state_.busy = true;
        populateSnapshotFromPending(state_);
    }

    if (!savePendingOrder(created)) {
        setPendingState(Status::RecoveryRequired,
                        "The checkout was created but its recovery reference could not be saved. QRIS was not opened. Keep the Order ID and do not pay or create another order until storage access is fixed.",
                        false);
        autoPoll_.store(false, std::memory_order_release);
        return;
    }

    {
        const juce::ScopedLock lock(stateLock_);
        if (activationCompleted_.load(std::memory_order_acquire))
            return;
        browserUrlReady_ = created.checkoutUrl;
        state_.status = Status::WaitingForPayment;
        state_.message = "QRIS checkout opened. Scan it with your phone; activation will continue automatically.";
        state_.busy = false;
        populateSnapshotFromPending(state_);
    }
    beginAutomaticPollingWindow();
    wake_.signal();
}

void LicenseCheckoutClient::performOrderRefresh()
{
    PendingOrder order;
    {
        const juce::ScopedLock lock(stateLock_);
        order = pending_;
    }

    if (!order.hasCredentials()) {
        autoPoll_.store(false, std::memory_order_release);
        return;
    }

    setPendingState(Status::VerifyingPayment,
                    "Checking the saved order securely with Midtrans...", true);
    const auto path = "/v1/orders/"
        + juce::URL::addEscapeChars(order.orderId, true)
        + "/refresh";
    const auto response = request("POST", path, "{}", order.orderToken);

    order.lastCheckedAt = nowUtcSeconds();
    {
        const juce::ScopedLock lock(stateLock_);
        if (activationCompleted_.load(std::memory_order_acquire)
            || pending_.orderId != order.orderId
            || pending_.orderToken != order.orderToken) {
            return;
        }
        pending_.lastCheckedAt = order.lastCheckedAt;
        order = pending_;
    }
    (void)savePendingOrder(order);

    if (!response.transportOk) {
        setPendingState(Status::Offline,
                        "Payment status is temporarily unavailable. Your order reference is saved. Do not pay again; SC220 Live will retry.",
                        false);
        return;
    }
    if (response.responseTooLarge) {
        setPendingState(Status::RecoveryRequired,
                        "The licence service returned an invalid oversized response. Your order is retained; do not pay again.",
                        false);
        autoPoll_.store(false, std::memory_order_release);
        return;
    }
    if (response.statusCode == 401 || response.statusCode == 404) {
        setPendingState(Status::RecoveryRequired,
                        "This saved order cannot be verified automatically. Keep the Order ID and purchase email; do not pay again. Purchase recovery or support can verify the payment history.",
                        false);
        autoPoll_.store(false, std::memory_order_release);
        return;
    }
    if (response.statusCode >= 400 && response.statusCode < 500
        && response.statusCode != 408 && response.statusCode != 429) {
        setPendingState(Status::RecoveryRequired,
                        readApiError(response.body,
                                     "The licence service rejected this saved order. It has been retained for purchase-history recovery; do not pay again."),
                        false);
        autoPoll_.store(false, std::memory_order_release);
        return;
    }
    if (response.statusCode < 200 || response.statusCode >= 300) {
        setPendingState(Status::Offline,
                        readApiError(response.body,
                                     "Payment verification is temporarily unavailable. The saved order is safe; do not pay again."),
                        false);
        return;
    }

    const auto parsed = juce::JSON::parse(response.body);
    auto* root = parsed.getDynamicObject();
    auto orderValue = root != nullptr ? root->getProperty("order") : juce::var{};
    auto* orderObject = orderValue.getDynamicObject();
    if (root == nullptr || orderObject == nullptr
        || propertyString(orderObject, "id") != order.orderId
        || propertyString(orderObject, "product_id") != juce::String(SONKUPIK_LICENSE_PRODUCT_ID)
        || propertyInt(orderObject, "amount_idr") != order.amountIdr) {
        setPendingState(Status::RecoveryRequired,
                        "The licence service returned an inconsistent order response. The saved purchase is retained; do not pay again.",
                        false);
        autoPoll_.store(false, std::memory_order_release);
        return;
    }
    const auto remoteStatus = propertyString(orderObject, "status").toLowerCase();

    auto activationValue = root != nullptr ? root->getProperty("activation") : juce::var{};
    auto* activation = activationValue.getDynamicObject();
    const auto activationCode = propertyString(activation, "activation_code");

    if (activationCode.isNotEmpty()) {
        {
            const juce::ScopedLock lock(stateLock_);
            activationCodeReady_ = activationCode;
            state_.status = Status::ActivationReady;
            state_.message = "Payment verified. Applying the signed licence automatically...";
            state_.busy = false;
            populateSnapshotFromPending(state_);
        }
        autoPoll_.store(false, std::memory_order_release);
        return;
    }

    if (isPaidStatus(remoteStatus)) {
        setPendingState(Status::PaymentConfirmed,
                        "Payment is confirmed. Licence issuance is still processing; the order is saved and will be retried automatically.",
                        false);
        return;
    }

    if (isTerminalUnpaidStatus(remoteStatus)) {
        const bool expired = remoteStatus.startsWith("expir");
        clearPendingOrder();
        autoPoll_.store(false, std::memory_order_release);
        setState(expired ? Status::Expired : Status::Failed,
                 expired
                     ? "Midtrans confirmed that this checkout expired unpaid. You may create a new QRIS checkout."
                     : "Midtrans confirmed that this checkout was cancelled or denied. No licence was issued.",
                 false);
        return;
    }

    if (remoteStatus == "refunded" || remoteStatus == "refund" || remoteStatus == "chargeback") {
        clearPendingOrder();
        autoPoll_.store(false, std::memory_order_release);
        setState(Status::Failed,
                 "This payment was refunded or charged back, so a commercial activation cannot be issued.",
                 false);
        return;
    }

    if (order.checkoutExpiredLocally()) {
        setPendingState(Status::WaitingForPayment,
                        "The QR payment window has passed, but the order is retained because a completed payment may still be recoverable. Use CHECK PAYMENT; do not pay twice.",
                        false);
    } else {
        setPendingState(Status::WaitingForPayment,
                        "Waiting for QRIS payment. Pay on your phone; SC220 Live checks automatically with adaptive polling.",
                        false);
    }
}

LicenseCheckoutClient::HttpResult LicenseCheckoutClient::request(const juce::String& method,
                                                                 const juce::String& path,
                                                                 const juce::String& jsonBody,
                                                                 const juce::String& bearerToken) const
{
    HttpResult result;
    int statusCode = 0;
    juce::StringPairArray responseHeaders;

    juce::String headers;
    headers << "Accept: application/json\r\n"
            << "Content-Type: application/json\r\n"
            << "User-Agent: SC220-Live/0.1.0\r\n";
    if (bearerToken.isNotEmpty())
        headers << "Authorization: Bearer " << bearerToken << "\r\n";

    auto url = juce::URL(normaliseBaseUrl() + path);
    if (jsonBody.isNotEmpty())
        url = url.withPOSTData(jsonBody);

    const auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
        .withHttpRequestCmd(method)
        .withExtraHeaders(headers)
        .withConnectionTimeoutMs(kRequestTimeoutMs)
        .withResponseHeaders(&responseHeaders)
        .withStatusCode(&statusCode)
        .withNumRedirectsToFollow(1);

    if (auto stream = url.createInputStream(options)) {
        juce::MemoryBlock payload;
        stream->readIntoMemoryBlock(payload, static_cast<int>(kMaxApiResponseBytes + 1));
        result.statusCode = statusCode;
        result.transportOk = true;
        result.responseTooLarge = payload.getSize() > kMaxApiResponseBytes;
        if (!result.responseTooLarge && payload.getSize() > 0) {
            result.body = juce::String::fromUTF8(
                static_cast<const char*>(payload.getData()), (int)payload.getSize());
        }
    } else {
        result.statusCode = statusCode;
        result.transportOk = statusCode > 0;
    }
    return result;
}

void LicenseCheckoutClient::populateSnapshotFromPending(Snapshot& snapshot) const
{
    snapshot.orderId = pending_.orderId;
    snapshot.checkoutUrl = pending_.checkoutUrl;
    snapshot.amountIdr = pending_.amountIdr;
    snapshot.expiresAt = pending_.expiresAt;
    snapshot.hasPendingOrder = pending_.hasCredentials();
    snapshot.checkoutCanOpen = pending_.checkoutCanOpen();
    snapshot.checkoutExpiredLocally = pending_.checkoutExpiredLocally();
    snapshot.automaticPolling = autoPoll_.load(std::memory_order_acquire);
}

void LicenseCheckoutClient::setState(Status status, const juce::String& message, bool busy)
{
    const juce::ScopedLock lock(stateLock_);
    state_.status = status;
    state_.message = message;
    state_.busy = busy;
    populateSnapshotFromPending(state_);
}

void LicenseCheckoutClient::setPendingState(Status status,
                                            const juce::String& message,
                                            bool busy)
{
    const juce::ScopedLock lock(stateLock_);
    state_.status = status;
    state_.message = message;
    state_.busy = busy;
    populateSnapshotFromPending(state_);
}

juce::String LicenseCheckoutClient::readApiError(const juce::String& body,
                                                  const juce::String& fallback) const
{
    const auto parsed = juce::JSON::parse(body);
    auto* root = parsed.getDynamicObject();
    auto errorValue = root != nullptr ? root->getProperty("error") : juce::var{};
    auto* error = errorValue.getDynamicObject();
    const auto message = propertyString(error, "message");
    return message.isNotEmpty() ? message : fallback;
}

juce::File LicenseCheckoutClient::checkoutStateFile() const
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("MasArray")
                   .getChildFile("SC220 Live");
    dir.createDirectory();
    return dir.getChildFile("checkout-state-v2.json");
}

LicenseCheckoutClient::PendingOrder LicenseCheckoutClient::loadPendingOrder() const
{
    PendingOrder result;
    const auto file = checkoutStateFile();
    if (!file.existsAsFile())
        return result;

    const auto parsed = juce::JSON::parse(file.loadFileAsString());
    auto* root = parsed.getDynamicObject();
    if (root == nullptr)
        return result;

    result.orderId = propertyString(root, "order_id");
    result.orderToken = propertyString(root, "order_token");
    result.checkoutUrl = propertyString(root, "checkout_url");
    result.customerName = propertyString(root, "customer_name");
    result.customerEmail = propertyString(root, "customer_email");
    result.amountIdr = propertyInt(root, "amount_idr");
    result.createdAt = propertyInt64(root, "created_at");
    result.expiresAt = propertyInt64(root, "expires_at");
    result.lastCheckedAt = propertyInt64(root, "last_checked_at");

    const auto storedDevice = propertyString(root, "device_request_id");
    const auto storedProduct = propertyString(root, "product_id");
    const auto proof = propertyString(root, "proof");
    if (storedDevice != deviceRequestId_
        || storedProduct != juce::String(SONKUPIK_LICENSE_PRODUCT_ID)
        || result.amountIdr != SONKUPIK_LICENSE_PRICE_IDR
        || proof != pendingProof(result)) {
        return {};
    }
    return result;
}

bool LicenseCheckoutClient::savePendingOrder(const PendingOrder& order) const
{
    if (!order.hasCredentials())
        return false;

    auto object = juce::DynamicObject::Ptr(new juce::DynamicObject());
    object->setProperty("version", 2);
    object->setProperty("product_id", SONKUPIK_LICENSE_PRODUCT_ID);
    object->setProperty("order_id", order.orderId);
    object->setProperty("order_token", order.orderToken);
    object->setProperty("checkout_url", order.checkoutUrl);
    object->setProperty("customer_name", order.customerName);
    object->setProperty("customer_email", order.customerEmail);
    object->setProperty("amount_idr", order.amountIdr);
    object->setProperty("created_at", order.createdAt);
    object->setProperty("expires_at", order.expiresAt);
    object->setProperty("last_checked_at", order.lastCheckedAt);
    object->setProperty("device_request_id", deviceRequestId_);
    object->setProperty("proof", pendingProof(order));
    return checkoutStateFile().replaceWithText(
        juce::JSON::toString(juce::var(object.get()), true));
}

void LicenseCheckoutClient::clearPendingOrder()
{
    {
        const juce::ScopedLock lock(stateLock_);
        pending_ = {};
        browserUrlReady_.clear();
        activationCodeReady_.clear();
        state_.orderId.clear();
        state_.checkoutUrl.clear();
        state_.amountIdr = 0;
        state_.expiresAt = 0;
        state_.hasPendingOrder = false;
        state_.checkoutCanOpen = false;
        state_.checkoutExpiredLocally = false;
    }
    checkoutStateFile().deleteFile();
}

juce::String LicenseCheckoutClient::pendingProof(const PendingOrder& order) const
{
    const auto text = deviceRequestId_ + "|" + juce::String(SONKUPIK_LICENSE_PRODUCT_ID)
                    + "|" + order.orderId + "|" + order.orderToken
                    + "|" + order.checkoutUrl + "|" + order.customerEmail.toLowerCase()
                    + "|" + juce::String(order.amountIdr)
                    + "|" + juce::String(order.createdAt)
                    + "|" + juce::String(order.expiresAt)
                    + "|" + kStateSalt;
    const auto utf8 = text.toUTF8();
    const auto bytesWithNull = (size_t)utf8.sizeInBytes();
    return juce::SHA256(utf8.getAddress(), bytesWithNull > 0 ? bytesWithNull - 1 : 0)
        .toHexString().toUpperCase();
}

bool LicenseCheckoutClient::isTrustedCheckoutUrl(const juce::String& url)
{
    if (!url.startsWithIgnoreCase("https://"))
        return false;
    const auto domain = juce::URL(url).getDomain().toLowerCase();
    return domain == "app.midtrans.com" || domain == "app.sandbox.midtrans.com";
}

bool LicenseCheckoutClient::isTerminalUnpaidStatus(const juce::String& status)
{
    return status == "failed" || status == "cancelled" || status == "canceled"
        || status == "denied" || status == "deny"
        || status == "expire" || status == "expired";
}

bool LicenseCheckoutClient::isPaidStatus(const juce::String& status)
{
    return status == "paid" || status == "settlement" || status == "capture"
        || status == "paid_but_unlicensed";
}

juce::int64 LicenseCheckoutClient::nowUtcSeconds()
{
    return juce::Time::getCurrentTime().toMilliseconds() / 1000;
}

} // namespace askp
