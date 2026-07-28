#include "LicenseManager.h"
#include "LicensePublicKey.h"

#include <algorithm>
#include <cmath>

namespace askp {

namespace {
constexpr juce::int64 kSecondsPerDay = 24 * 60 * 60;
constexpr juce::int64 kClockToleranceSeconds = 48 * 60 * 60;
constexpr auto kStateSalt = "SONKUPIK-LIVE-LOCAL-STATE-V1-10F3B7C96A5D42E8";
constexpr auto kRegistryRoot = "HKEY_CURRENT_USER\\Software\\MasArray\\Sonkupik Live\\";

juce::String readLineValue(const juce::StringArray& lines, const juce::String& key)
{
    const auto prefix = key + "=";
    for (const auto& line : lines)
        if (line.startsWith(prefix))
            return line.substring(prefix.length()).trim();
    return {};
}

juce::int64 earliestPositive(juce::int64 a, juce::int64 b)
{
    if (a <= 0) return b;
    if (b <= 0) return a;
    return std::min(a, b);
}

juce::String groupedHex(const juce::String& hex)
{
    juce::String result;
    for (int i = 0; i < hex.length(); i += 4) {
        if (result.isNotEmpty())
            result << "-";
        result << hex.substring(i, juce::jmin(i + 4, hex.length()));
    }
    return result;
}

juce::BigInteger pkcs1Sha256Digest(const juce::String& hashHex)
{
    // EMSA-PKCS1-v1_5 for a 2048-bit RSA key and SHA-256 DigestInfo.
    // 00 01 FF..FF 00 3031300D060960864801650304020105000420 || H
    constexpr int rsaBytes = 256;
    constexpr int digestInfoBytes = 19 + 32;
    constexpr auto digestInfoPrefix = "3031300D060960864801650304020105000420";
    const int paddingBytes = rsaBytes - 3 - digestInfoBytes;

    juce::String encoded = "0001";
    for (int i = 0; i < paddingBytes; ++i)
        encoded << "FF";
    encoded << "00" << digestInfoPrefix << hashHex;

    juce::BigInteger result;
    result.parseString(encoded, 16);
    return result;
}
} // namespace

LicenseManager::LicenseManager()
    : machineHash_(computeMachineHash())
{
    const auto file = loadFileState();
    const auto reg = loadRegistryState();

    betaStartUtc_ = earliestPositive(file.betaStartUtc, reg.betaStartUtc);
    lastSeenUtc_ = std::max(file.lastSeenUtc, reg.lastSeenUtc);

    // Prefer a valid activation from either storage location. A copied or
    // tampered code is harmless because every code is signature-verified and
    // bound to this machine hash.
    juce::String customer;
    juce::String error;
    if (validateActivationCode(file.activationCode, &customer, &error)) {
        activationCode_ = file.activationCode;
        licensedTo_ = customer;
        activationValidationKnown_ = true;
        activationVerified_ = true;
    } else if (validateActivationCode(reg.activationCode, &customer, &error)) {
        activationCode_ = reg.activationCode;
        licensedTo_ = customer;
        activationValidationKnown_ = true;
        activationVerified_ = true;
    } else {
        // Neither persisted location contains a usable code. Remember that
        // result; refresh() only needs to update time-based trial state.
        activationValidationKnown_ = true;
        activationVerified_ = false;
    }

    if (betaStartUtc_ <= 0)
        betaStartUtc_ = nowUtcSeconds();

    evaluateState(true);
}

bool LicenseManager::editingAllowed() const noexcept
{
    const auto s = state();
    return s == State::BetaActive || s == State::BetaExpiring || s == State::Licensed;
}

void LicenseManager::refresh()
{
    evaluateState(false);
}

juce::int64 LicenseManager::nowUtcSeconds()
{
    return juce::Time::getCurrentTime().toMilliseconds() / 1000;
}

juce::String LicenseManager::sha256Hex(const juce::String& text)
{
    const auto utf8 = text.toUTF8();
    const auto bytesWithNull = (size_t)utf8.sizeInBytes();
    const auto byteCount = bytesWithNull > 0 ? bytesWithNull - 1 : 0;
    return juce::SHA256(utf8.getAddress(), byteCount).toHexString().toUpperCase();
}

juce::String LicenseManager::textToHex(const juce::String& text)
{
    static constexpr char digits[] = "0123456789ABCDEF";
    const auto utf8 = text.toUTF8();
    const auto* bytes = reinterpret_cast<const unsigned char*>(utf8.getAddress());
    const auto bytesWithNull = (size_t)utf8.sizeInBytes();
    const int count = (int)(bytesWithNull > 0 ? bytesWithNull - 1 : 0);

    juce::String result;
    for (int i = 0; i < count; ++i) {
        result += juce::String::charToString((juce::juce_wchar)digits[(bytes[i] >> 4) & 0x0f]);
        result += juce::String::charToString((juce::juce_wchar)digits[bytes[i] & 0x0f]);
    }
    return result;
}

juce::String LicenseManager::hexToText(const juce::String& hex)
{
    if ((hex.length() & 1) != 0 || !isHex(hex))
        return {};

    juce::MemoryBlock bytes;
    bytes.loadFromHexString(hex);
    return juce::String::fromUTF8(static_cast<const char*>(bytes.getData()), (int)bytes.getSize());
}

bool LicenseManager::isHex(const juce::String& text)
{
    if (text.isEmpty())
        return false;

    for (auto c : text) {
        const bool decimal = c >= '0' && c <= '9';
        const bool lower = c >= 'a' && c <= 'f';
        const bool upper = c >= 'A' && c <= 'F';
        if (!decimal && !lower && !upper)
            return false;
    }
    return true;
}

juce::String LicenseManager::compactCode(const juce::String& text)
{
    return text.removeCharacters(" \t\r\n").trim();
}

juce::String LicenseManager::computeMachineHash()
{
    juce::StringArray parts;
    parts.add("SONKUPIK-LIVE-HARDWARE-FINGERPRINT-V1");

   #if JUCE_WINDOWS
    const auto reg64 = juce::WindowsRegistry::WoW64_64bit;
    parts.add(juce::WindowsRegistry::getValue(
        "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Cryptography\\MachineGuid", {}, reg64));
    parts.add(juce::WindowsRegistry::getValue(
        "HKEY_LOCAL_MACHINE\\HARDWARE\\DESCRIPTION\\System\\BIOS\\SystemManufacturer"));
    parts.add(juce::WindowsRegistry::getValue(
        "HKEY_LOCAL_MACHINE\\HARDWARE\\DESCRIPTION\\System\\BIOS\\SystemProductName"));
    parts.add(juce::WindowsRegistry::getValue(
        "HKEY_LOCAL_MACHINE\\HARDWARE\\DESCRIPTION\\System\\BIOS\\BaseBoardProduct"));
    parts.add(juce::WindowsRegistry::getValue(
        "HKEY_LOCAL_MACHINE\\HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0\\ProcessorNameString"));
   #else
    // Use JUCE's stable machine identifier on macOS and other non-Windows
    // platforms. Do not include the operating-system name: a normal OS update
    // must not invalidate the Computer Request ID or the local state proof.
    const auto machineIds = juce::SystemStats::getMachineIdentifiers(
        juce::SystemStats::MachineIdFlags::uniqueId);
    for (const auto& id : machineIds)
        parts.add(id);

    // The unique device ID is a bounded fallback for unusual systems where the
    // preferred identifier is unavailable. Computer name is intentionally not
    // used because users may rename a Mac without changing the hardware.
    if (machineIds.isEmpty())
        parts.add(juce::SystemStats::getUniqueDeviceID());
   #endif

    juce::String canonical;
    for (auto part : parts) {
        part = part.trim().toUpperCase();
        canonical << (part.isEmpty() ? "UNKNOWN" : part) << "|";
    }
    return sha256Hex(canonical);
}

juce::String LicenseManager::requestId() const
{
    return "ASKP-R1-" + groupedHex(machineHash_);
}

juce::String LicenseManager::normaliseRequestId(const juce::String& request)
{
    auto compact = request.toUpperCase().removeCharacters(" \t\r\n-");
    constexpr int prefixLength = 6; // ASKP + R1
    if (!compact.startsWith("ASKPR1") || compact.length() != prefixLength + 64)
        return {};

    auto hash = compact.substring(prefixLength);
    if (!isHex(hash))
        return {};
    return hash;
}

juce::String LicenseManager::makeActivationPayload(const juce::String& machineHash,
                                                    const juce::String& customerReference,
                                                    juce::int64 issuedUtcSeconds)
{
    auto machine = machineHash.toUpperCase().removeCharacters(" \t\r\n-");
    if (machine.length() != 64 || !isHex(machine))
        return {};

    auto customer = customerReference.trim();
    if (customer.isEmpty())
        customer = "Sonkupik Live Customer";
    customer = customer.substring(0, 80);

    return "ASKP1|" + machine
         + "|" + juce::String(issuedUtcSeconds)
         + "|PERPETUAL|" + textToHex(customer);
}

juce::String LicenseManager::signActivationPayload(const juce::String& payload,
                                                    const juce::RSAKey& privateKey)
{
    if (payload.isEmpty() || !privateKey.isValid())
        return {};

    auto signature = pkcs1Sha256Digest(sha256Hex(payload));
    if (!privateKey.applyToValue(signature))
        return {};
    return signature.toString(16).toUpperCase();
}

juce::String LicenseManager::makeActivationCode(const juce::String& payload,
                                                 const juce::String& signatureHex)
{
    if (payload.isEmpty() || signatureHex.isEmpty() || !isHex(signatureHex))
        return {};
    return "ASKP-A1-" + textToHex(payload) + "-SIG-" + signatureHex.toUpperCase();
}

juce::File LicenseManager::stateFile() const
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("MasArray")
                   .getChildFile("Sonkupik Live");
    dir.createDirectory();
    return dir.getChildFile("licence-state-v1.dat");
}

juce::String LicenseManager::stateProof(const PersistedState& state) const
{
    return sha256Hex(machineHash_ + "|" + juce::String(state.betaStartUtc)
                     + "|" + juce::String(state.lastSeenUtc)
                     + "|" + state.activationCode + "|" + kStateSalt);
}

LicenseManager::PersistedState LicenseManager::loadFileState() const
{
    PersistedState result;
    const auto file = stateFile();
    if (!file.existsAsFile())
        return result;

    juce::StringArray lines;
    lines.addLines(file.loadFileAsString());
    result.betaStartUtc = readLineValue(lines, "start").getLargeIntValue();
    result.lastSeenUtc = readLineValue(lines, "last").getLargeIntValue();
    result.activationCode = readLineValue(lines, "activation");
    const auto proof = readLineValue(lines, "proof");

    if (proof != stateProof(result))
        return {};
    return result;
}

LicenseManager::PersistedState LicenseManager::loadRegistryState() const
{
    PersistedState result;
   #if JUCE_WINDOWS
    result.betaStartUtc = juce::WindowsRegistry::getValue(juce::String(kRegistryRoot) + "BetaStartUtc").getLargeIntValue();
    result.lastSeenUtc = juce::WindowsRegistry::getValue(juce::String(kRegistryRoot) + "LastSeenUtc").getLargeIntValue();
    result.activationCode = juce::WindowsRegistry::getValue(juce::String(kRegistryRoot) + "ActivationCode");
    const auto proof = juce::WindowsRegistry::getValue(juce::String(kRegistryRoot) + "StateProof");
    if (proof != stateProof(result))
        return {};
   #endif
    return result;
}

void LicenseManager::saveState(const PersistedState& state) const
{
    const auto proof = stateProof(state);
    juce::String text;
    text << "start=" << state.betaStartUtc << "\n"
         << "last=" << state.lastSeenUtc << "\n"
         << "activation=" << state.activationCode << "\n"
         << "proof=" << proof << "\n";
    stateFile().replaceWithText(text);

   #if JUCE_WINDOWS
    juce::WindowsRegistry::setValue(juce::String(kRegistryRoot) + "BetaStartUtc", juce::String(state.betaStartUtc));
    juce::WindowsRegistry::setValue(juce::String(kRegistryRoot) + "LastSeenUtc", juce::String(state.lastSeenUtc));
    juce::WindowsRegistry::setValue(juce::String(kRegistryRoot) + "ActivationCode", state.activationCode);
    juce::WindowsRegistry::setValue(juce::String(kRegistryRoot) + "StateProof", proof);
   #endif
}

bool LicenseManager::validateActivationCode(const juce::String& rawCode,
                                            juce::String* customerReference,
                                            juce::String* error) const
{
    auto fail = [&](const juce::String& message) {
        if (error != nullptr) *error = message;
        return false;
    };

    const auto code = compactCode(rawCode);
    if (code.isEmpty())
        return fail("Activation code is empty.");
    if (code.length() > 4096)
        return fail("Activation code is too long.");
    if (!code.startsWithIgnoreCase("ASKP-A1-"))
        return fail("Activation code prefix is not recognised.");

    const int sigPos = code.indexOfIgnoreCase("-SIG-");
    if (sigPos < 0)
        return fail("Activation signature section was not found.");

    const auto payloadHex = code.substring(8, sigPos);
    const auto signatureHex = code.substring(sigPos + 5);
    if ((payloadHex.length() & 1) != 0 || !isHex(payloadHex) || !isHex(signatureHex))
        return fail("Activation code contains invalid characters.");

    const auto payload = hexToText(payloadHex);
    juce::StringArray fields;
    fields.addTokens(payload, "|", "");
    if (fields.size() != 5 || fields[0] != "ASKP1" || fields[3] != "PERPETUAL")
        return fail("Activation payload is not valid for this product.");

    const auto licensedMachine = fields[1].toUpperCase();
    if (licensedMachine != machineHash_)
        return fail("This activation code belongs to a different computer.");

    if (fields[2].getLargeIntValue() <= 0)
        return fail("Activation issue date is invalid.");

    const auto expected = pkcs1Sha256Digest(sha256Hex(payload));

    juce::BigInteger recovered;
    recovered.parseString(signatureHex, 16);
    juce::RSAKey publicKey(licensing::kRsaPublicKey);
    if (!publicKey.isValid() || !publicKey.applyToValue(recovered) || recovered != expected)
        return fail("Activation signature is invalid.");

    const auto customer = hexToText(fields[4]).trim();
    if (customerReference != nullptr)
        *customerReference = customer.isEmpty() ? "Sonkupik Live Customer" : customer;
    if (error != nullptr)
        error->clear();
    return true;
}

LicenseManager::ActivationResult LicenseManager::activate(const juce::String& activationCode)
{
    juce::String customer;
    juce::String error;
    if (!validateActivationCode(activationCode, &customer, &error))
        return { false, error };

    activationCode_ = compactCode(activationCode);
    licensedTo_ = customer;
    activationValidationKnown_ = true;
    activationVerified_ = true;
    evaluateState(true);
    return { true, "Activation successful. Sonkupik Live Full Version is now unlocked." };
}

juce::String LicenseManager::licensedTo() const
{
    return licensedTo_;
}

juce::String LicenseManager::statusText() const
{
    switch (state()) {
        case State::Licensed:
            return licensedTo_.isNotEmpty() ? "Licensed to " + licensedTo_ : "Licensed";
        case State::BetaExpiring:
            return "Full Trial - " + juce::String(daysRemaining()) + " days remaining";
        case State::ReadOnly:
            return "30-day trial ended - Read-only mode";
        case State::ClockNeedsAttention:
            return "Check the Windows date and time";
        case State::BetaActive:
        default:
            return "Full Trial - " + juce::String(daysRemaining()) + " days remaining";
    }
}

void LicenseManager::evaluateState(bool forcePersist)
{
    const auto now = nowUtcSeconds();

    if (!activationValidationKnown_) {
        juce::String customer;
        juce::String activationError;
        activationVerified_ = validateActivationCode(
            activationCode_, &customer, &activationError);
        activationValidationKnown_ = true;
        if (activationVerified_)
            licensedTo_ = customer;
        else
            licensedTo_.clear();
    }

    if (activationVerified_) {
        state_.store(State::Licensed, std::memory_order_release);
        daysRemaining_.store(0, std::memory_order_release);
    } else {
        licensedTo_.clear();
        const bool clockInvalid = betaStartUtc_ > now + kClockToleranceSeconds
                               || lastSeenUtc_ > now + kClockToleranceSeconds;
        if (clockInvalid) {
            state_.store(State::ClockNeedsAttention, std::memory_order_release);
            daysRemaining_.store(0, std::memory_order_release);
        } else {
            const auto expiry = betaStartUtc_ + (juce::int64)betaDurationDays * kSecondsPerDay;
            const auto remainingSeconds = expiry - now;
            const int remainingDays = remainingSeconds > 0
                ? (int)std::ceil((double)remainingSeconds / (double)kSecondsPerDay)
                : 0;
            daysRemaining_.store(remainingDays, std::memory_order_release);
            state_.store(remainingDays <= 0 ? State::ReadOnly
                         : remainingDays <= 7 ? State::BetaExpiring
                                               : State::BetaActive,
                         std::memory_order_release);
        }
    }

    if (now >= lastSeenUtc_)
        lastSeenUtc_ = now;

    if (forcePersist || now - lastPersistUtc_ >= 60 * 60) {
        PersistedState persisted;
        persisted.betaStartUtc = betaStartUtc_;
        persisted.lastSeenUtc = lastSeenUtc_;
        persisted.activationCode = activationCode_;
        saveState(persisted);
        lastPersistUtc_ = now;
    }
}

} // namespace askp
