/**
 * ScratchBird SCRAM-SHA-256/512 Authentication Implementation
 *
 * Alpha 3 Phase 3.4: Security Suite
 *
 * Implements RFC 5802 (SCRAM) and RFC 7677 (SCRAM-SHA-256)
 */

#include "scratchbird/security/scram_auth.h"
#include "scratchbird/security/auth_manager.h"

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include <cstring>
#include <sstream>
#include <algorithm>
#include <random>

namespace scratchbird {
namespace security {

// ============================================================================
// Base64 Encoding/Decoding
// ============================================================================

static const char BASE64_CHARS[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64Encode(const uint8_t* data, size_t len) {
    std::string result;
    result.reserve(((len + 2) / 3) * 4);

    for (size_t i = 0; i < len; i += 3) {
        uint32_t n = (static_cast<uint32_t>(data[i]) << 16);
        if (i + 1 < len) n |= (static_cast<uint32_t>(data[i + 1]) << 8);
        if (i + 2 < len) n |= static_cast<uint32_t>(data[i + 2]);

        result.push_back(BASE64_CHARS[(n >> 18) & 0x3F]);
        result.push_back(BASE64_CHARS[(n >> 12) & 0x3F]);
        result.push_back((i + 1 < len) ? BASE64_CHARS[(n >> 6) & 0x3F] : '=');
        result.push_back((i + 2 < len) ? BASE64_CHARS[n & 0x3F] : '=');
    }

    return result;
}

std::string base64Encode(const std::vector<uint8_t>& data) {
    return base64Encode(data.data(), data.size());
}

std::vector<uint8_t> base64Decode(const std::string& encoded) {
    static const int DECODE_TABLE[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    };

    std::vector<uint8_t> result;
    result.reserve((encoded.size() * 3) / 4);

    uint32_t accum = 0;
    int bits = 0;

    for (char c : encoded) {
        if (c == '=') break;

        int val = DECODE_TABLE[static_cast<unsigned char>(c)];
        if (val < 0) continue;  // Skip invalid chars

        accum = (accum << 6) | val;
        bits += 6;

        if (bits >= 8) {
            bits -= 8;
            result.push_back(static_cast<uint8_t>((accum >> bits) & 0xFF));
        }
    }

    return result;
}

// ============================================================================
// Utility Functions
// ============================================================================

void xorBytes(std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    for (size_t i = 0; i < a.size() && i < b.size(); i++) {
        a[i] ^= b[i];
    }
}

std::string generateNonce(size_t length) {
    std::vector<uint8_t> random_bytes(length);
    RAND_bytes(random_bytes.data(), static_cast<int>(length));

    // Use only alphanumeric characters
    static const char NONCE_CHARS[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

    std::string nonce;
    nonce.reserve(length);
    for (size_t i = 0; i < length; i++) {
        nonce.push_back(NONCE_CHARS[random_bytes[i] % (sizeof(NONCE_CHARS) - 1)]);
    }

    return nonce;
}

std::string normalizeUsername(const std::string& username) {
    // Simple normalization: replace '=' with '=3D' and ',' with '=2C'
    std::string result;
    result.reserve(username.size());

    for (char c : username) {
        if (c == '=') {
            result += "=3D";
        } else if (c == ',') {
            result += "=2C";
        } else {
            result.push_back(c);
        }
    }

    return result;
}

static std::string denormalizeUsername(const std::string& username) {
    std::string result;
    result.reserve(username.size());

    for (size_t i = 0; i < username.size(); i++) {
        if (username[i] == '=' && i + 2 < username.size()) {
            if (username[i+1] == '3' && username[i+2] == 'D') {
                result.push_back('=');
                i += 2;
                continue;
            } else if (username[i+1] == '2' && username[i+2] == 'C') {
                result.push_back(',');
                i += 2;
                continue;
            }
        }
        result.push_back(username[i]);
    }

    return result;
}

// ============================================================================
// Message Parsing
// ============================================================================

bool parseClientFirst(const std::string& message, ScramClientFirst& parsed) {
    // Format: gs2-header,client-first-message-bare
    // gs2-header: [gs2-cbind-flag],[authzid],
    // client-first-message-bare: n=<username>,r=<nonce>[,extensions]

    size_t pos = 0;

    // Parse GS2 header
    // gs2-cbind-flag: 'n' | 'y' | 'p=<name>'
    if (pos >= message.size()) return false;

    if (message[pos] == 'p') {
        parsed.gs2_flag = 'p';
        // Skip 'p=' and channel binding name
        pos = message.find(',', pos);
        if (pos == std::string::npos) return false;
        parsed.channel_binding = message.substr(2, pos - 2);
    } else if (message[pos] == 'n' || message[pos] == 'y') {
        parsed.gs2_flag = message[pos];
        pos++;
    } else {
        return false;
    }

    // Skip comma
    if (pos >= message.size() || message[pos] != ',') return false;
    pos++;

    // Parse authzid (optional)
    if (pos < message.size() && message[pos] != ',') {
        // authzid present
        if (message.substr(pos, 2) != "a=") return false;
        pos += 2;
        size_t end = message.find(',', pos);
        if (end == std::string::npos) return false;
        parsed.authzid = message.substr(pos, end - pos);
        pos = end;
    }

    // Skip comma
    if (pos >= message.size() || message[pos] != ',') return false;
    pos++;

    // Store client-first-message-bare (everything after GS2 header)
    parsed.client_first_bare = message.substr(pos);

    // Parse n=<username>
    if (message.substr(pos, 2) != "n=") return false;
    pos += 2;
    size_t comma = message.find(',', pos);
    if (comma == std::string::npos) return false;
    parsed.username = denormalizeUsername(message.substr(pos, comma - pos));
    pos = comma + 1;

    // Parse r=<nonce>
    if (message.substr(pos, 2) != "r=") return false;
    pos += 2;
    comma = message.find(',', pos);
    if (comma == std::string::npos) {
        parsed.client_nonce = message.substr(pos);
    } else {
        parsed.client_nonce = message.substr(pos, comma - pos);
    }

    return true;
}

bool parseClientFinal(const std::string& message, ScramClientFinal& parsed) {
    // Format: c=<base64>,r=<nonce>[,extensions],p=<base64-proof>

    // Find proof at end
    size_t proof_pos = message.rfind(",p=");
    if (proof_pos == std::string::npos) return false;

    parsed.without_proof = message.substr(0, proof_pos);
    parsed.proof = message.substr(proof_pos + 3);

    // Parse channel binding
    if (message.substr(0, 2) != "c=") return false;
    size_t comma = message.find(',', 2);
    if (comma == std::string::npos) return false;
    parsed.channel_binding = message.substr(2, comma - 2);

    // Parse nonce
    size_t r_pos = message.find("r=", comma);
    if (r_pos == std::string::npos) return false;
    r_pos += 2;
    comma = message.find(',', r_pos);
    if (comma == std::string::npos) comma = proof_pos;
    parsed.nonce = message.substr(r_pos, comma - r_pos);

    return true;
}

// ============================================================================
// SCRAM Crypto
// ============================================================================

core::Status calculateSaltedPassword(
    const std::string& password,
    const std::vector<uint8_t>& salt,
    uint32_t iterations,
    ScramAlgorithm algorithm,
    std::vector<uint8_t>& salted_password)
{
    const EVP_MD* md = (algorithm == ScramAlgorithm::SHA_256) ?
                       EVP_sha256() : EVP_sha512();
    int key_len = (algorithm == ScramAlgorithm::SHA_256) ? 32 : 64;

    salted_password.resize(key_len);

    // PBKDF2 with HMAC
    if (PKCS5_PBKDF2_HMAC(
            password.c_str(), static_cast<int>(password.size()),
            salt.data(), static_cast<int>(salt.size()),
            static_cast<int>(iterations),
            md,
            key_len, salted_password.data()) != 1) {
        return core::Status::INTERNAL_ERROR;
    }

    return core::Status::OK;
}

static std::vector<uint8_t> hmacHash(
    const std::vector<uint8_t>& key,
    const std::string& data,
    ScramAlgorithm algorithm)
{
    const EVP_MD* md = (algorithm == ScramAlgorithm::SHA_256) ?
                       EVP_sha256() : EVP_sha512();
    int hash_len = (algorithm == ScramAlgorithm::SHA_256) ? 32 : 64;

    std::vector<uint8_t> result(hash_len);
    unsigned int out_len = hash_len;

    HMAC(md,
         key.data(), static_cast<int>(key.size()),
         reinterpret_cast<const unsigned char*>(data.c_str()),
         static_cast<int>(data.size()),
         result.data(), &out_len);

    return result;
}

static std::vector<uint8_t> hmacHash(
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& data,
    ScramAlgorithm algorithm)
{
    const EVP_MD* md = (algorithm == ScramAlgorithm::SHA_256) ?
                       EVP_sha256() : EVP_sha512();
    int hash_len = (algorithm == ScramAlgorithm::SHA_256) ? 32 : 64;

    std::vector<uint8_t> result(hash_len);
    unsigned int out_len = hash_len;

    HMAC(md,
         key.data(), static_cast<int>(key.size()),
         data.data(), static_cast<int>(data.size()),
         result.data(), &out_len);

    return result;
}

static std::vector<uint8_t> hash(
    const std::vector<uint8_t>& data,
    ScramAlgorithm algorithm)
{
    int hash_len = (algorithm == ScramAlgorithm::SHA_256) ? 32 : 64;
    std::vector<uint8_t> result(hash_len);

    if (algorithm == ScramAlgorithm::SHA_256) {
        SHA256(data.data(), data.size(), result.data());
    } else {
        SHA512(data.data(), data.size(), result.data());
    }

    return result;
}

core::Status generateScramCredentials(
    const std::string& password,
    ScramAlgorithm algorithm,
    uint32_t iterations,
    std::vector<uint8_t>& salt,
    std::vector<uint8_t>& stored_key,
    std::vector<uint8_t>& server_key)
{
    // Generate salt if not provided
    if (salt.empty()) {
        salt.resize(16);
        RAND_bytes(salt.data(), 16);
    }

    // Calculate SaltedPassword
    std::vector<uint8_t> salted_password;
    auto status = calculateSaltedPassword(password, salt, iterations,
                                           algorithm, salted_password);
    if (status != core::Status::OK) {
        return status;
    }

    // ClientKey = HMAC(SaltedPassword, "Client Key")
    std::vector<uint8_t> client_key = hmacHash(salted_password, "Client Key", algorithm);

    // StoredKey = H(ClientKey)
    stored_key = hash(client_key, algorithm);

    // ServerKey = HMAC(SaltedPassword, "Server Key")
    server_key = hmacHash(salted_password, "Server Key", algorithm);

    // Clear sensitive data
    std::fill(salted_password.begin(), salted_password.end(), 0);
    std::fill(client_key.begin(), client_key.end(), 0);

    return core::Status::OK;
}

// ============================================================================
// ScramState Implementation
// ============================================================================

ScramState::ScramState(ScramAlgorithm algo)
    : algorithm_(algo)
{}

ScramState::~ScramState() {
    // Clear sensitive data
    std::fill(salt_.begin(), salt_.end(), 0);
    std::fill(stored_key_.begin(), stored_key_.end(), 0);
    std::fill(server_key_.begin(), server_key_.end(), 0);
}

std::string ScramState::getAuthMessage() const {
    // AuthMessage = client-first-message-bare + "," +
    //               server-first-message + "," +
    //               client-final-message-without-proof
    return client_first_bare_ + "," + server_first_msg_ + "," + client_final_without_proof_;
}

// ============================================================================
// ScramSHA256AuthMethod Implementation
// ============================================================================

ScramSHA256AuthMethod::ScramSHA256AuthMethod() {
    config_.algorithm = ScramAlgorithm::SHA_256;
}

ScramSHA256AuthMethod::~ScramSHA256AuthMethod() {
    std::lock_guard<std::mutex> lock(states_mutex_);
    states_.clear();
}

core::Status ScramSHA256AuthMethod::initialize(
    const std::map<std::string, std::string>& config,
    core::ErrorContext* /*ctx*/)
{
    // Parse configuration
    auto it = config.find("iterations");
    if (it != config.end()) {
        config_.iterations = std::stoul(it->second);
        if (config_.iterations < 4096) {
            config_.iterations = 4096;  // Minimum per RFC
        }
    }

    it = config.find("channel_binding");
    if (it != config.end() && it->second == "required") {
        config_.channel_binding_required = true;
    }

    return core::Status::OK;
}

void ScramSHA256AuthMethod::setCredentialStore(std::shared_ptr<CredentialStore> store) {
    credential_store_ = std::move(store);
}

AuthResult ScramSHA256AuthMethod::start(AuthContext& ctx) {
    // SCRAM requires the client to send the first message
    // We just wait for it
    ctx.setState(AuthState::IN_PROGRESS);

    // Create state
    auto state = std::make_unique<ScramState>(ScramAlgorithm::SHA_256);

    std::lock_guard<std::mutex> lock(states_mutex_);
    states_[&ctx] = std::move(state);

    // Return empty response - client sends first
    AuthResult result;
    result.state = AuthState::IN_PROGRESS;
    result.requires_response = true;
    return result;
}

AuthResult ScramSHA256AuthMethod::continueAuth(
    AuthContext& ctx,
    const std::vector<uint8_t>& data)
{
    std::string message(data.begin(), data.end());

    std::lock_guard<std::mutex> lock(states_mutex_);
    auto it = states_.find(&ctx);
    if (it == states_.end()) {
        return AuthResult::failure(AuthFailReason::PROTOCOL_ERROR,
                                   "No SCRAM state found");
    }

    ScramState* state = it->second.get();

    switch (state->phase()) {
        case ScramPhase::INITIAL:
            return processClientFirst(ctx, message);

        case ScramPhase::SERVER_FIRST_SENT:
            return processClientFinal(ctx, message);

        default:
            return AuthResult::failure(AuthFailReason::PROTOCOL_ERROR,
                                       "Unexpected SCRAM phase");
    }
}

AuthResult ScramSHA256AuthMethod::processClientFirst(
    AuthContext& ctx,
    const std::string& message)
{
    auto it = states_.find(&ctx);
    if (it == states_.end()) {
        return AuthResult::failure(AuthFailReason::INTERNAL_ERROR, "No state");
    }
    ScramState* state = it->second.get();

    // Parse client-first message
    ScramClientFirst parsed;
    if (!parseClientFirst(message, parsed)) {
        return AuthResult::failure(AuthFailReason::PROTOCOL_ERROR,
                                   "Invalid client-first message");
    }

    // Check channel binding
    if (config_.channel_binding_required && parsed.gs2_flag == 'n') {
        return AuthResult::failure(AuthFailReason::NOT_ALLOWED,
                                   "Channel binding required but not supported by client");
    }

    // Store parsed data
    state->setClientNonce(parsed.client_nonce);
    state->setClientFirstMessageBare(parsed.client_first_bare);
    state->setChannelBindingFlag(parsed.gs2_flag);
    ctx.setUsername(parsed.username);

    // Look up user credentials
    if (!credential_store_) {
        return AuthResult::failure(AuthFailReason::INTERNAL_ERROR,
                                   "No credential store configured");
    }

    UserCredential cred;
    auto status = credential_store_->getCredential(parsed.username, cred);
    if (status != core::Status::OK) {
        // User not found - still continue to avoid user enumeration
        // Generate fake credentials
        std::vector<uint8_t> fake_salt(16);
        RAND_bytes(fake_salt.data(), 16);
        state->setSalt(fake_salt);
        state->setIterations(config_.iterations);

        // Generate fake stored/server keys
        std::vector<uint8_t> fake_stored(32, 0), fake_server(32, 0);
        state->setStoredKey(fake_stored);
        state->setServerKey(fake_server);
    } else {
        // Use real credentials
        state->setSalt(cred.password_salt);
        state->setIterations(cred.password_iterations);
        state->setStoredKey(cred.scram_stored_key);
        state->setServerKey(cred.scram_server_key);
    }

    // Generate server nonce
    std::string server_nonce = generateNonce(config_.nonce_length);
    state->setServerNonce(parsed.client_nonce + server_nonce);

    // Generate server-first message
    std::string server_first = generateServerFirst(*state);
    state->setServerFirstMessage(server_first);
    state->setPhase(ScramPhase::SERVER_FIRST_SENT);

    return AuthResult::continueAuth(
        std::vector<uint8_t>(server_first.begin(), server_first.end()));
}

AuthResult ScramSHA256AuthMethod::processClientFinal(
    AuthContext& ctx,
    const std::string& message)
{
    auto it = states_.find(&ctx);
    if (it == states_.end()) {
        return AuthResult::failure(AuthFailReason::INTERNAL_ERROR, "No state");
    }
    ScramState* state = it->second.get();

    // Parse client-final message
    ScramClientFinal parsed;
    if (!parseClientFinal(message, parsed)) {
        return AuthResult::failure(AuthFailReason::PROTOCOL_ERROR,
                                   "Invalid client-final message");
    }

    // Verify nonce
    if (parsed.nonce != state->serverNonce()) {
        return AuthResult::failure(AuthFailReason::PROTOCOL_ERROR,
                                   "Nonce mismatch");
    }

    // Store client-final-message-without-proof for auth message
    state->setClientFinalWithoutProof(parsed.without_proof);

    // Decode and verify client proof
    std::vector<uint8_t> client_proof = base64Decode(parsed.proof);
    if (!verifyClientProof(*state, client_proof)) {
        state->setPhase(ScramPhase::FAILED);
        return AuthResult::failure(AuthFailReason::INVALID_CREDENTIALS,
                                   "SCRAM authentication failed");
    }

    // Calculate server signature
    std::vector<uint8_t> server_sig = calculateServerSignature(*state);

    // Generate server-final message
    std::string server_final = generateServerFinal(*state, server_sig);
    state->setPhase(ScramPhase::COMPLETE);

    // Success!
    ctx.setAuthenticatedUser(ctx.username());

    AuthResult result = AuthResult::success(ctx.username());
    result.response_data = std::vector<uint8_t>(server_final.begin(), server_final.end());
    return result;
}

std::string ScramSHA256AuthMethod::generateServerFirst(ScramState& state) {
    // Format: r=<nonce>,s=<base64-salt>,i=<iterations>
    std::ostringstream oss;
    oss << "r=" << state.serverNonce()
        << ",s=" << base64Encode(state.salt())
        << ",i=" << state.iterations();
    return oss.str();
}

std::string ScramSHA256AuthMethod::generateServerFinal(
    ScramState& /*state*/,
    const std::vector<uint8_t>& server_signature)
{
    // Format: v=<base64-server-signature>
    return "v=" + base64Encode(server_signature);
}

bool ScramSHA256AuthMethod::verifyClientProof(
    ScramState& state,
    const std::vector<uint8_t>& client_proof)
{
    // ClientSignature = HMAC(StoredKey, AuthMessage)
    std::vector<uint8_t> client_sig = hmacHash(
        state.storedKey(),
        state.getAuthMessage(),
        state.algorithm());

    // ClientKey = ClientProof XOR ClientSignature
    std::vector<uint8_t> client_key = client_proof;
    xorBytes(client_key, client_sig);

    // StoredKey = H(ClientKey)
    std::vector<uint8_t> computed_stored_key = hash(client_key, state.algorithm());

    // Verify
    return constantTimeCompare(computed_stored_key, state.storedKey());
}

std::vector<uint8_t> ScramSHA256AuthMethod::calculateServerSignature(ScramState& state) {
    // ServerSignature = HMAC(ServerKey, AuthMessage)
    return hmacHash(state.serverKey(), state.getAuthMessage(), state.algorithm());
}

void ScramSHA256AuthMethod::abort(AuthContext& ctx) {
    std::lock_guard<std::mutex> lock(states_mutex_);
    states_.erase(&ctx);
}

bool ScramSHA256AuthMethod::verifyPassword(
    const std::string& username,
    const std::string& password)
{
    if (!credential_store_) {
        return false;
    }

    UserCredential cred;
    if (credential_store_->getCredential(username, cred) != core::Status::OK) {
        return false;
    }

    // Generate credentials from password and compare
    std::vector<uint8_t> salt = cred.password_salt;
    std::vector<uint8_t> stored_key, server_key;

    auto status = generateScramCredentials(
        password,
        ScramAlgorithm::SHA_256,
        cred.password_iterations,
        salt,
        stored_key,
        server_key);

    if (status != core::Status::OK) {
        return false;
    }

    return constantTimeCompare(stored_key, cred.scram_stored_key);
}

// ============================================================================
// ScramSHA512AuthMethod Implementation
// ============================================================================

ScramSHA512AuthMethod::ScramSHA512AuthMethod() {
    config_.algorithm = ScramAlgorithm::SHA_512;
}

ScramSHA512AuthMethod::~ScramSHA512AuthMethod() {
    std::lock_guard<std::mutex> lock(states_mutex_);
    states_.clear();
}

core::Status ScramSHA512AuthMethod::initialize(
    const std::map<std::string, std::string>& config,
    core::ErrorContext* ctx)
{
    // Same as SHA256 but with different algorithm
    auto it = config.find("iterations");
    if (it != config.end()) {
        config_.iterations = std::stoul(it->second);
        if (config_.iterations < 4096) {
            config_.iterations = 4096;
        }
    }

    it = config.find("channel_binding");
    if (it != config.end() && it->second == "required") {
        config_.channel_binding_required = true;
    }

    return core::Status::OK;
}

void ScramSHA512AuthMethod::setCredentialStore(std::shared_ptr<CredentialStore> store) {
    credential_store_ = std::move(store);
}

AuthResult ScramSHA512AuthMethod::start(AuthContext& ctx) {
    ctx.setState(AuthState::IN_PROGRESS);

    auto state = std::make_unique<ScramState>(ScramAlgorithm::SHA_512);

    std::lock_guard<std::mutex> lock(states_mutex_);
    states_[&ctx] = std::move(state);

    AuthResult result;
    result.state = AuthState::IN_PROGRESS;
    result.requires_response = true;
    return result;
}

AuthResult ScramSHA512AuthMethod::continueAuth(
    AuthContext& ctx,
    const std::vector<uint8_t>& data)
{
    std::string message(data.begin(), data.end());

    std::lock_guard<std::mutex> lock(states_mutex_);
    auto it = states_.find(&ctx);
    if (it == states_.end()) {
        return AuthResult::failure(AuthFailReason::PROTOCOL_ERROR,
                                   "No SCRAM state found");
    }

    ScramState* state = it->second.get();

    switch (state->phase()) {
        case ScramPhase::INITIAL:
            return processClientFirst(ctx, message);
        case ScramPhase::SERVER_FIRST_SENT:
            return processClientFinal(ctx, message);
        default:
            return AuthResult::failure(AuthFailReason::PROTOCOL_ERROR,
                                       "Unexpected SCRAM phase");
    }
}

AuthResult ScramSHA512AuthMethod::processClientFirst(
    AuthContext& ctx,
    const std::string& message)
{
    // Same logic as SHA256, just using SHA512 state
    auto it = states_.find(&ctx);
    if (it == states_.end()) {
        return AuthResult::failure(AuthFailReason::INTERNAL_ERROR, "No state");
    }
    ScramState* state = it->second.get();

    ScramClientFirst parsed;
    if (!parseClientFirst(message, parsed)) {
        return AuthResult::failure(AuthFailReason::PROTOCOL_ERROR,
                                   "Invalid client-first message");
    }

    if (config_.channel_binding_required && parsed.gs2_flag == 'n') {
        return AuthResult::failure(AuthFailReason::NOT_ALLOWED,
                                   "Channel binding required");
    }

    state->setClientNonce(parsed.client_nonce);
    state->setClientFirstMessageBare(parsed.client_first_bare);
    state->setChannelBindingFlag(parsed.gs2_flag);
    ctx.setUsername(parsed.username);

    if (!credential_store_) {
        return AuthResult::failure(AuthFailReason::INTERNAL_ERROR,
                                   "No credential store");
    }

    UserCredential cred;
    auto status = credential_store_->getCredential(parsed.username, cred);
    if (status != core::Status::OK) {
        std::vector<uint8_t> fake_salt(16);
        RAND_bytes(fake_salt.data(), 16);
        state->setSalt(fake_salt);
        state->setIterations(config_.iterations);
        std::vector<uint8_t> fake_stored(64, 0), fake_server(64, 0);
        state->setStoredKey(fake_stored);
        state->setServerKey(fake_server);
    } else {
        state->setSalt(cred.password_salt);
        state->setIterations(cred.password_iterations);
        state->setStoredKey(cred.scram_stored_key);
        state->setServerKey(cred.scram_server_key);
    }

    std::string server_nonce = generateNonce(config_.nonce_length);
    state->setServerNonce(parsed.client_nonce + server_nonce);

    std::string server_first = generateServerFirst(*state);
    state->setServerFirstMessage(server_first);
    state->setPhase(ScramPhase::SERVER_FIRST_SENT);

    return AuthResult::continueAuth(
        std::vector<uint8_t>(server_first.begin(), server_first.end()));
}

AuthResult ScramSHA512AuthMethod::processClientFinal(
    AuthContext& ctx,
    const std::string& message)
{
    auto it = states_.find(&ctx);
    if (it == states_.end()) {
        return AuthResult::failure(AuthFailReason::INTERNAL_ERROR, "No state");
    }
    ScramState* state = it->second.get();

    ScramClientFinal parsed;
    if (!parseClientFinal(message, parsed)) {
        return AuthResult::failure(AuthFailReason::PROTOCOL_ERROR,
                                   "Invalid client-final message");
    }

    if (parsed.nonce != state->serverNonce()) {
        return AuthResult::failure(AuthFailReason::PROTOCOL_ERROR, "Nonce mismatch");
    }

    state->setClientFinalWithoutProof(parsed.without_proof);

    std::vector<uint8_t> client_proof = base64Decode(parsed.proof);
    if (!verifyClientProof(*state, client_proof)) {
        state->setPhase(ScramPhase::FAILED);
        return AuthResult::failure(AuthFailReason::INVALID_CREDENTIALS,
                                   "SCRAM authentication failed");
    }

    std::vector<uint8_t> server_sig = calculateServerSignature(*state);
    std::string server_final = generateServerFinal(*state, server_sig);
    state->setPhase(ScramPhase::COMPLETE);

    ctx.setAuthenticatedUser(ctx.username());

    AuthResult result = AuthResult::success(ctx.username());
    result.response_data = std::vector<uint8_t>(server_final.begin(), server_final.end());
    return result;
}

std::string ScramSHA512AuthMethod::generateServerFirst(ScramState& state) {
    std::ostringstream oss;
    oss << "r=" << state.serverNonce()
        << ",s=" << base64Encode(state.salt())
        << ",i=" << state.iterations();
    return oss.str();
}

std::string ScramSHA512AuthMethod::generateServerFinal(
    ScramState& /*state*/,
    const std::vector<uint8_t>& server_signature)
{
    return "v=" + base64Encode(server_signature);
}

bool ScramSHA512AuthMethod::verifyClientProof(
    ScramState& state,
    const std::vector<uint8_t>& client_proof)
{
    std::vector<uint8_t> client_sig = hmacHash(
        state.storedKey(),
        state.getAuthMessage(),
        state.algorithm());

    std::vector<uint8_t> client_key = client_proof;
    xorBytes(client_key, client_sig);

    std::vector<uint8_t> computed_stored_key = hash(client_key, state.algorithm());

    return constantTimeCompare(computed_stored_key, state.storedKey());
}

std::vector<uint8_t> ScramSHA512AuthMethod::calculateServerSignature(ScramState& state) {
    return hmacHash(state.serverKey(), state.getAuthMessage(), state.algorithm());
}

void ScramSHA512AuthMethod::abort(AuthContext& ctx) {
    std::lock_guard<std::mutex> lock(states_mutex_);
    states_.erase(&ctx);
}

bool ScramSHA512AuthMethod::verifyPassword(
    const std::string& username,
    const std::string& password)
{
    if (!credential_store_) {
        return false;
    }

    UserCredential cred;
    if (credential_store_->getCredential(username, cred) != core::Status::OK) {
        return false;
    }

    std::vector<uint8_t> salt = cred.password_salt;
    std::vector<uint8_t> stored_key, server_key;

    auto status = generateScramCredentials(
        password,
        ScramAlgorithm::SHA_512,
        cred.password_iterations,
        salt,
        stored_key,
        server_key);

    if (status != core::Status::OK) {
        return false;
    }

    return constantTimeCompare(stored_key, cred.scram_stored_key);
}

}  // namespace security
}  // namespace scratchbird
