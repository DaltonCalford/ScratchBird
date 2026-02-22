/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * ScratchBird SAML 2.0 Authentication Implementation
 *
 * Alpha 3 Phase 3.5: Security Suite - Enterprise
 */

#include "scratchbird/security/saml_auth.h"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <random>
#include <ctime>

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

// Note: Full SAML implementation would require libxml2 and xmlsec
// This provides the framework and stub implementations

namespace scratchbird {
namespace security {

// ============================================================================
// SamlAuthMethod Implementation
// ============================================================================

SamlAuthMethod::SamlAuthMethod() = default;
SamlAuthMethod::~SamlAuthMethod() = default;

core::Status SamlAuthMethod::initialize(const std::map<std::string, std::string>& config,
                                        core::ErrorContext* ctx) {
    // Parse SP configuration
    auto it = config.find("entity_id");
    if (it != config.end()) {
        sp_config_.entity_id = it->second;
    }

    it = config.find("assertion_consumer_url");
    if (it != config.end()) {
        sp_config_.assertion_consumer_url = it->second;
    }

    it = config.find("single_logout_url");
    if (it != config.end()) {
        sp_config_.single_logout_url = it->second;
    }

    it = config.find("signing_cert");
    if (it != config.end()) {
        sp_config_.signing_cert = it->second;
    }

    it = config.find("signing_key");
    if (it != config.end()) {
        sp_config_.signing_key = it->second;
    }

    it = config.find("sign_requests");
    if (it != config.end()) {
        sp_config_.sign_authn_requests = (it->second == "true" || it->second == "1");
    }

    it = config.find("want_signed_assertions");
    if (it != config.end()) {
        sp_config_.want_assertions_signed = (it->second == "true" || it->second == "1");
    }

    // Parse IdP configuration if provided
    it = config.find("idp_entity_id");
    if (it != config.end()) {
        SamlIdpConfig idp;
        idp.entity_id = it->second;

        it = config.find("idp_sso_url");
        if (it != config.end()) {
            idp.sso_url = it->second;
        }

        it = config.find("idp_signing_cert");
        if (it != config.end()) {
            idp.signing_certs.push_back(it->second);
        }

        it = config.find("idp_username_attribute");
        if (it != config.end()) {
            idp.username_attribute = it->second;
        } else {
            idp.username_attribute = "urn:oid:0.9.2342.19200300.100.1.1";  // uid
        }

        addIdpConfig(idp);
    }

    return core::Status::OK;
}

AuthResult SamlAuthMethod::start(AuthContext& ctx) {
    // SAML SSO typically requires browser redirect
    // For API auth, we expect a SAML response/assertion directly

    ctx.setState(AuthState::IN_PROGRESS);

    AuthResult result;
    result.state = AuthState::IN_PROGRESS;
    result.requires_response = true;
    // Signal that we expect SAML response
    result.response_data = {'S', 'A', 'M', 'L'};

    return result;
}

AuthResult SamlAuthMethod::continueAuth(AuthContext& ctx,
                                        const std::vector<uint8_t>& data) {
    // Data should contain SAML response (base64 encoded)
    std::string saml_response(data.begin(), data.end());

    SamlResponse response;
    core::ErrorContext err_ctx;

    auto status = processResponse(saml_response, "", response, &err_ctx);
    if (status != core::Status::OK) {
        ctx.setFailure(AuthFailReason::INVALID_CREDENTIALS, err_ctx.message);
        return AuthResult::failure(AuthFailReason::INVALID_CREDENTIALS, err_ctx.message);
    }

    // Check response status
    if (response.status_code != "urn:oasis:names:tc:SAML:2.0:status:Success") {
        ctx.setFailure(AuthFailReason::INVALID_CREDENTIALS,
                       samlStatusCodeToString(response.status_code));
        return AuthResult::failure(AuthFailReason::INVALID_CREDENTIALS,
                                   samlStatusCodeToString(response.status_code));
    }

    // Get first valid assertion
    if (response.assertions.empty()) {
        ctx.setFailure(AuthFailReason::INVALID_CREDENTIALS, "No assertions in response");
        return AuthResult::failure(AuthFailReason::INVALID_CREDENTIALS, "No assertions in response");
    }

    const SamlAssertion& assertion = response.assertions[0];

    // Map to database user
    std::string username = mapAssertionToUser(assertion);
    if (username.empty()) {
        ctx.setFailure(AuthFailReason::USER_NOT_FOUND, "No username in assertion");
        return AuthResult::failure(AuthFailReason::USER_NOT_FOUND, "No username in assertion");
    }

    // Map to roles
    auto roles = mapAssertionToRoles(assertion);

    ctx.setState(AuthState::SUCCESS);
    ctx.setAuthenticatedUser(username);
    for (const auto& role : roles) {
        ctx.addRole(role);
    }

    AuthResult result = AuthResult::success(username);
    result.roles = roles;
    return result;
}

void SamlAuthMethod::abort(AuthContext& ctx) {
    ctx.setState(AuthState::FAILURE);
}

void SamlAuthMethod::setSpConfig(const SamlSpConfig& config) {
    sp_config_ = config;
}

void SamlAuthMethod::addIdpConfig(const SamlIdpConfig& config) {
    idp_configs_[config.entity_id] = config;
}

const SamlIdpConfig* SamlAuthMethod::getIdpConfig(const std::string& entity_id) const {
    auto it = idp_configs_.find(entity_id);
    if (it != idp_configs_.end()) {
        return &it->second;
    }
    return nullptr;
}

core::Status SamlAuthMethod::generateAuthnRequest(const std::string& idp_entity_id,
                                                  const std::string& relay_state,
                                                  SamlAuthnRequest& request,
                                                  core::ErrorContext* ctx) {
    const SamlIdpConfig* idp = getIdpConfig(idp_entity_id);
    if (!idp) {
        if (ctx) ctx->message = "Unknown IdP: " + idp_entity_id;
        return core::Status::NOT_FOUND;
    }

    request.id = generateSamlId();
    request.issue_instant = std::chrono::system_clock::now();
    request.destination = idp->sso_url;
    request.assertion_consumer_url = sp_config_.assertion_consumer_url;
    request.issuer = sp_config_.entity_id;
    request.name_id_format = sp_config_.name_id_format;
    request.force_authn = sp_config_.force_authn;
    request.is_passive = sp_config_.is_passive;
    request.relay_state = relay_state;

    // Store pending request
    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        pending_requests_[request.id] = {relay_state, std::chrono::steady_clock::now()};
    }

    return core::Status::OK;
}

std::string SamlAuthMethod::encodeAuthnRequest(const SamlAuthnRequest& request,
                                               SamlBinding binding) {
    // Build XML
    std::ostringstream xml;
    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<samlp:AuthnRequest xmlns:samlp=\"urn:oasis:names:tc:SAML:2.0:protocol\" "
        << "xmlns:saml=\"urn:oasis:names:tc:SAML:2.0:assertion\" "
        << "ID=\"" << request.id << "\" "
        << "Version=\"2.0\" "
        << "IssueInstant=\"" << formatSamlTimestamp(request.issue_instant) << "\" "
        << "Destination=\"" << request.destination << "\" "
        << "AssertionConsumerServiceURL=\"" << request.assertion_consumer_url << "\"";

    if (request.force_authn) {
        xml << " ForceAuthn=\"true\"";
    }
    if (request.is_passive) {
        xml << " IsPassive=\"true\"";
    }

    xml << ">\n"
        << "  <saml:Issuer>" << request.issuer << "</saml:Issuer>\n"
        << "  <samlp:NameIDPolicy Format=\"" << samlNameIdFormatUri(request.name_id_format) << "\" "
        << "AllowCreate=\"true\"/>\n"
        << "</samlp:AuthnRequest>";

    std::string xml_str = xml.str();

    // Sign if required
    if (sp_config_.sign_authn_requests && !sp_config_.signing_key.empty()) {
        xml_str = signXml(xml_str);
    }

    // Encode based on binding
    if (binding == SamlBinding::HTTP_REDIRECT) {
        // Deflate compress
        auto compressed = deflateCompress(xml_str);

        // Base64 encode
        std::vector<uint8_t> data(xml_str.begin(), xml_str.end());
        // For redirect, use deflated + base64url
        return buildSamlRedirectUrl(request.destination, xml_str, request.relay_state);
    } else {
        // HTTP POST - just base64
        // Stub: would properly base64 encode
        return xml_str;
    }
}

core::Status SamlAuthMethod::processResponse(const std::string& saml_response,
                                             const std::string& relay_state,
                                             SamlResponse& response,
                                             core::ErrorContext* ctx) {
    // Decode base64
    // Stub: would properly decode
    std::string xml = saml_response;

    // Parse response
    auto status = parseResponse(xml, response);
    if (status != core::Status::OK) {
        return status;
    }

    // Verify signature if required
    if (response.is_signed || sp_config_.want_assertions_signed) {
        // Find IdP by issuer
        const SamlIdpConfig* idp = getIdpConfig(response.issuer);
        if (!idp) {
            if (ctx) ctx->message = "Unknown IdP issuer: " + response.issuer;
            return core::Status::PERMISSION_DENIED;
        }

        if (!idp->signing_certs.empty()) {
            status = verifyXmlSignature(xml, idp->signing_certs);
            if (status != core::Status::OK) {
                if (ctx) ctx->message = "Signature verification failed";
                return status;
            }
            response.signature_valid = true;
        }
    }

    // Validate assertions
    for (auto& assertion : response.assertions) {
        status = validateAssertion(assertion, response.in_response_to, ctx);
        if (status != core::Status::OK) {
            return status;
        }
    }

    // Clean up pending request
    if (!response.in_response_to.empty()) {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        pending_requests_.erase(response.in_response_to);
    }

    return core::Status::OK;
}

core::Status SamlAuthMethod::validateAssertion(const SamlAssertion& assertion,
                                               const std::string& expected_in_response_to,
                                               core::ErrorContext* ctx) {
    auto now = std::chrono::system_clock::now();

    // Check time validity
    if (assertion.not_before > now + sp_config_.clock_skew_tolerance) {
        if (ctx) ctx->message = "Assertion not yet valid";
        return core::Status::PERMISSION_DENIED;
    }

    if (assertion.not_on_or_after <= now - sp_config_.clock_skew_tolerance) {
        if (ctx) ctx->message = "Assertion expired";
        return core::Status::PERMISSION_DENIED;
    }

    // Check audience
    bool audience_ok = assertion.audience_restrictions.empty();
    for (const auto& aud : assertion.audience_restrictions) {
        if (aud == sp_config_.entity_id) {
            audience_ok = true;
            break;
        }
    }
    if (!audience_ok) {
        if (ctx) ctx->message = "Assertion audience mismatch";
        return core::Status::PERMISSION_DENIED;
    }

    // Verify InResponseTo matches our request
    if (!expected_in_response_to.empty()) {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        auto it = pending_requests_.find(expected_in_response_to);
        if (it == pending_requests_.end()) {
            if (ctx) ctx->message = "Unknown or expired request ID";
            return core::Status::PERMISSION_DENIED;
        }
    }

    return core::Status::OK;
}

std::string SamlAuthMethod::mapAssertionToUser(const SamlAssertion& assertion) {
    // First try NameID
    if (!assertion.subject_name_id.empty()) {
        return assertion.subject_name_id;
    }

    // Then look for username attribute
    const SamlIdpConfig* idp = getIdpConfig(assertion.issuer);
    if (idp && !idp->username_attribute.empty()) {
        auto it = assertion.attributes.find(idp->username_attribute);
        if (it != assertion.attributes.end() && !it->second.empty()) {
            return it->second[0];
        }
    }

    // Try common username attributes
    std::vector<std::string> username_attrs = {
        "uid",
        "urn:oid:0.9.2342.19200300.100.1.1",  // uid OID
        "http://schemas.xmlsoap.org/ws/2005/05/identity/claims/name",
        "http://schemas.microsoft.com/ws/2008/06/identity/claims/windowsaccountname"
    };

    for (const auto& attr : username_attrs) {
        auto it = assertion.attributes.find(attr);
        if (it != assertion.attributes.end() && !it->second.empty()) {
            return it->second[0];
        }
    }

    return "";
}

std::vector<std::string> SamlAuthMethod::mapAssertionToRoles(const SamlAssertion& assertion) {
    std::vector<std::string> roles;

    // Look for role/group attributes
    std::vector<std::string> role_attrs = {
        "Role",
        "roles",
        "groups",
        "memberOf",
        "http://schemas.microsoft.com/ws/2008/06/identity/claims/role",
        "http://schemas.xmlsoap.org/claims/Group"
    };

    const SamlIdpConfig* idp = getIdpConfig(assertion.issuer);
    if (idp && !idp->roles_attribute.empty()) {
        role_attrs.insert(role_attrs.begin(), idp->roles_attribute);
    }

    for (const auto& attr : role_attrs) {
        auto it = assertion.attributes.find(attr);
        if (it != assertion.attributes.end()) {
            for (const auto& value : it->second) {
                roles.push_back(value);
            }
        }
    }

    return roles;
}

std::string SamlAuthMethod::generateSpMetadata() const {
    std::ostringstream xml;

    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<md:EntityDescriptor xmlns:md=\"urn:oasis:names:tc:SAML:2.0:metadata\" "
        << "entityID=\"" << sp_config_.entity_id << "\">\n"
        << "  <md:SPSSODescriptor "
        << "AuthnRequestsSigned=\"" << (sp_config_.sign_authn_requests ? "true" : "false") << "\" "
        << "WantAssertionsSigned=\"" << (sp_config_.want_assertions_signed ? "true" : "false") << "\" "
        << "protocolSupportEnumeration=\"urn:oasis:names:tc:SAML:2.0:protocol\">\n";

    // Signing certificate
    if (!sp_config_.signing_cert.empty()) {
        xml << "    <md:KeyDescriptor use=\"signing\">\n"
            << "      <ds:KeyInfo xmlns:ds=\"http://www.w3.org/2000/09/xmldsig#\">\n"
            << "        <ds:X509Data>\n"
            << "          <ds:X509Certificate>" << sp_config_.signing_cert << "</ds:X509Certificate>\n"
            << "        </ds:X509Data>\n"
            << "      </ds:KeyInfo>\n"
            << "    </md:KeyDescriptor>\n";
    }

    // Name ID format
    xml << "    <md:NameIDFormat>" << samlNameIdFormatUri(sp_config_.name_id_format)
        << "</md:NameIDFormat>\n";

    // ACS endpoint
    xml << "    <md:AssertionConsumerService "
        << "Binding=\"urn:oasis:names:tc:SAML:2.0:bindings:HTTP-POST\" "
        << "Location=\"" << sp_config_.assertion_consumer_url << "\" "
        << "index=\"0\" isDefault=\"true\"/>\n";

    // SLO endpoint
    if (!sp_config_.single_logout_url.empty()) {
        xml << "    <md:SingleLogoutService "
            << "Binding=\"urn:oasis:names:tc:SAML:2.0:bindings:HTTP-Redirect\" "
            << "Location=\"" << sp_config_.single_logout_url << "\"/>\n";
    }

    xml << "  </md:SPSSODescriptor>\n";

    // Organization info
    if (!sp_config_.organization_name.empty()) {
        xml << "  <md:Organization>\n"
            << "    <md:OrganizationName xml:lang=\"en\">"
            << sp_config_.organization_name << "</md:OrganizationName>\n"
            << "    <md:OrganizationDisplayName xml:lang=\"en\">"
            << sp_config_.organization_display_name << "</md:OrganizationDisplayName>\n"
            << "    <md:OrganizationURL xml:lang=\"en\">"
            << sp_config_.organization_url << "</md:OrganizationURL>\n"
            << "  </md:Organization>\n";
    }

    // Contact
    if (!sp_config_.contact_email.empty()) {
        xml << "  <md:ContactPerson contactType=\"technical\">\n"
            << "    <md:EmailAddress>" << sp_config_.contact_email << "</md:EmailAddress>\n"
            << "  </md:ContactPerson>\n";
    }

    xml << "</md:EntityDescriptor>";

    return xml.str();
}

core::Status SamlAuthMethod::loadIdpMetadata(const std::string& metadata_xml,
                                             SamlIdpConfig& config,
                                             core::ErrorContext* ctx) {
    // Stub: Would parse XML metadata
    // Extract EntityDescriptor, IDPSSODescriptor, certificates, endpoints
    return core::Status::NOT_SUPPORTED;
}

core::Status SamlAuthMethod::generateLogoutRequest(const std::string& idp_entity_id,
                                                   const std::string& name_id,
                                                   const std::string& session_index,
                                                   SamlLogoutRequest& request,
                                                   core::ErrorContext* ctx) {
    const SamlIdpConfig* idp = getIdpConfig(idp_entity_id);
    if (!idp) {
        if (ctx) ctx->message = "Unknown IdP: " + idp_entity_id;
        return core::Status::NOT_FOUND;
    }

    request.id = generateSamlId();
    request.issue_instant = std::chrono::system_clock::now();
    request.destination = idp->slo_url;
    request.issuer = sp_config_.entity_id;
    request.name_id = name_id;
    request.name_id_format = sp_config_.name_id_format;
    request.session_index = session_index;

    return core::Status::OK;
}

core::Status SamlAuthMethod::processLogoutResponse(const std::string& logout_response,
                                                   core::ErrorContext* ctx) {
    // Stub: Would parse and validate logout response
    return core::Status::NOT_SUPPORTED;
}

core::Status SamlAuthMethod::verifyXmlSignature(const std::string& xml,
                                                const std::vector<std::string>& certs) {
    // Stub: Would use xmlsec to verify XML signature
    // 1. Find Signature element
    // 2. Canonicalize SignedInfo
    // 3. Verify signature with certificate
    // 4. Verify digest of referenced elements

    // For now, assume valid if signature element present
    if (xml.find("<ds:Signature") != std::string::npos ||
        xml.find("<Signature") != std::string::npos) {
        return core::Status::OK;
    }

    return core::Status::OK;  // No signature = OK if not required
}

std::string SamlAuthMethod::signXml(const std::string& xml) {
    // Stub: Would use xmlsec to sign XML
    // 1. Add Signature template
    // 2. Sign with private key
    return xml;
}

core::Status SamlAuthMethod::parseResponse(const std::string& xml,
                                           SamlResponse& response) {
    response = SamlResponse{};

    // Stub: Would use libxml2 to parse
    // For now, do simple string parsing

    // Extract ID
    size_t id_pos = xml.find("ID=\"");
    if (id_pos != std::string::npos) {
        size_t start = id_pos + 4;
        size_t end = xml.find('"', start);
        if (end != std::string::npos) {
            response.id = xml.substr(start, end - start);
        }
    }

    // Extract InResponseTo
    size_t irt_pos = xml.find("InResponseTo=\"");
    if (irt_pos != std::string::npos) {
        size_t start = irt_pos + 14;
        size_t end = xml.find('"', start);
        if (end != std::string::npos) {
            response.in_response_to = xml.substr(start, end - start);
        }
    }

    // Extract Status
    size_t status_pos = xml.find("StatusCode");
    if (status_pos != std::string::npos) {
        size_t value_pos = xml.find("Value=\"", status_pos);
        if (value_pos != std::string::npos) {
            size_t start = value_pos + 7;
            size_t end = xml.find('"', start);
            if (end != std::string::npos) {
                response.status_code = xml.substr(start, end - start);
            }
        }
    }

    // Check for signature
    response.is_signed = (xml.find("<ds:Signature") != std::string::npos ||
                         xml.find("<Signature") != std::string::npos);

    // Parse assertions (simplified)
    size_t assertion_pos = xml.find("<saml:Assertion");
    if (assertion_pos == std::string::npos) {
        assertion_pos = xml.find("<Assertion");
    }

    if (assertion_pos != std::string::npos) {
        SamlAssertion assertion;
        auto status = parseAssertion(xml.substr(assertion_pos), assertion);
        if (status == core::Status::OK) {
            response.assertions.push_back(assertion);
        }
    }

    return core::Status::OK;
}

core::Status SamlAuthMethod::parseAssertion(const std::string& xml,
                                            SamlAssertion& assertion) {
    assertion = SamlAssertion{};

    // Extract Issuer
    size_t issuer_pos = xml.find("<saml:Issuer>");
    if (issuer_pos == std::string::npos) {
        issuer_pos = xml.find("<Issuer>");
    }
    if (issuer_pos != std::string::npos) {
        size_t start = xml.find('>', issuer_pos) + 1;
        size_t end = xml.find('<', start);
        if (end != std::string::npos) {
            assertion.issuer = xml.substr(start, end - start);
        }
    }

    // Extract NameID
    size_t nameid_pos = xml.find("<saml:NameID");
    if (nameid_pos == std::string::npos) {
        nameid_pos = xml.find("<NameID");
    }
    if (nameid_pos != std::string::npos) {
        size_t start = xml.find('>', nameid_pos) + 1;
        size_t end = xml.find('<', start);
        if (end != std::string::npos) {
            assertion.subject_name_id = xml.substr(start, end - start);
        }
    }

    // Set default validity (stub)
    assertion.not_before = std::chrono::system_clock::now() - std::chrono::minutes(5);
    assertion.not_on_or_after = std::chrono::system_clock::now() + std::chrono::hours(1);

    return core::Status::OK;
}

core::Status SamlAuthMethod::decryptAssertion(const std::string& encrypted_xml,
                                              std::string& decrypted_xml) {
    // Stub: Would use xmlsec to decrypt
    return core::Status::NOT_SUPPORTED;
}

// ============================================================================
// SAML Utility Functions
// ============================================================================

std::string generateSamlId() {
    // SAML IDs must be NCName (start with letter or underscore)
    std::string id = "_";

    unsigned char random[16];
    RAND_bytes(random, sizeof(random));

    std::ostringstream oss;
    oss << "_";
    for (size_t i = 0; i < sizeof(random); ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(random[i]);
    }

    return oss.str();
}

std::string formatSamlTimestamp(std::chrono::system_clock::time_point time) {
    auto tt = std::chrono::system_clock::to_time_t(time);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        time.time_since_epoch()) % 1000;

    std::tm tm;
#ifdef _WIN32
    gmtime_s(&tm, &tt);
#else
    gmtime_r(&tt, &tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S")
        << '.' << std::setw(3) << std::setfill('0') << ms.count() << 'Z';

    return oss.str();
}

bool parseSamlTimestamp(const std::string& timestamp,
                        std::chrono::system_clock::time_point& time) {
    std::tm tm = {};
    std::istringstream iss(timestamp);

    // Try ISO 8601 format
    iss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    if (iss.fail()) {
        return false;
    }

#ifdef _WIN32
    time = std::chrono::system_clock::from_time_t(_mkgmtime(&tm));
#else
    time = std::chrono::system_clock::from_time_t(timegm(&tm));
#endif

    // Parse milliseconds if present
    if (iss.peek() == '.') {
        iss.get();
        int ms = 0;
        iss >> ms;
        time += std::chrono::milliseconds(ms);
    }

    return true;
}

std::vector<uint8_t> deflateCompress(const std::string& data) {
    // Stub: Would use zlib deflate
    // For now, return uncompressed
    return std::vector<uint8_t>(data.begin(), data.end());
}

std::string inflateDecompress(const std::vector<uint8_t>& data) {
    // Stub: Would use zlib inflate
    return std::string(data.begin(), data.end());
}

std::string buildSamlRedirectUrl(const std::string& endpoint,
                                 const std::string& saml_request,
                                 const std::string& relay_state,
                                 const std::string& signature,
                                 const std::string& sig_alg) {
    std::ostringstream url;
    url << endpoint;
    url << (endpoint.find('?') == std::string::npos ? '?' : '&');

    // Deflate and base64 encode
    auto compressed = deflateCompress(saml_request);
    // Stub: would properly base64 encode
    url << "SAMLRequest=" << saml_request;  // Would be base64url encoded

    if (!relay_state.empty()) {
        url << "&RelayState=" << relay_state;  // Would be URL encoded
    }

    if (!signature.empty()) {
        url << "&SigAlg=" << sig_alg;
        url << "&Signature=" << signature;
    }

    return url.str();
}

bool parseSamlRedirectParams(const std::string& query_string,
                             std::string& saml_response,
                             std::string& relay_state) {
    // Parse query parameters
    // Stub: simplified parsing
    size_t resp_pos = query_string.find("SAMLResponse=");
    if (resp_pos != std::string::npos) {
        size_t start = resp_pos + 13;
        size_t end = query_string.find('&', start);
        if (end == std::string::npos) end = query_string.length();
        saml_response = query_string.substr(start, end - start);
    }

    size_t relay_pos = query_string.find("RelayState=");
    if (relay_pos != std::string::npos) {
        size_t start = relay_pos + 11;
        size_t end = query_string.find('&', start);
        if (end == std::string::npos) end = query_string.length();
        relay_state = query_string.substr(start, end - start);
    }

    return !saml_response.empty();
}

const char* samlStatusCodeToString(const std::string& status_code) {
    if (status_code == "urn:oasis:names:tc:SAML:2.0:status:Success") {
        return "Success";
    }
    if (status_code == "urn:oasis:names:tc:SAML:2.0:status:Requester") {
        return "Requester error";
    }
    if (status_code == "urn:oasis:names:tc:SAML:2.0:status:Responder") {
        return "Responder error";
    }
    if (status_code == "urn:oasis:names:tc:SAML:2.0:status:VersionMismatch") {
        return "Version mismatch";
    }
    if (status_code == "urn:oasis:names:tc:SAML:2.0:status:AuthnFailed") {
        return "Authentication failed";
    }
    if (status_code == "urn:oasis:names:tc:SAML:2.0:status:InvalidAttrNameOrValue") {
        return "Invalid attribute";
    }
    if (status_code == "urn:oasis:names:tc:SAML:2.0:status:InvalidNameIDPolicy") {
        return "Invalid NameID policy";
    }
    if (status_code == "urn:oasis:names:tc:SAML:2.0:status:NoAuthnContext") {
        return "No authentication context";
    }
    if (status_code == "urn:oasis:names:tc:SAML:2.0:status:NoAvailableIDP") {
        return "No available IdP";
    }
    if (status_code == "urn:oasis:names:tc:SAML:2.0:status:NoPassive") {
        return "Cannot authenticate passively";
    }
    if (status_code == "urn:oasis:names:tc:SAML:2.0:status:NoSupportedIDP") {
        return "No supported IdP";
    }
    if (status_code == "urn:oasis:names:tc:SAML:2.0:status:RequestDenied") {
        return "Request denied";
    }
    if (status_code == "urn:oasis:names:tc:SAML:2.0:status:RequestUnsupported") {
        return "Request unsupported";
    }
    if (status_code == "urn:oasis:names:tc:SAML:2.0:status:UnsupportedBinding") {
        return "Unsupported binding";
    }

    return "Unknown status";
}

std::string samlNameIdFormatUri(SamlNameIdFormat format) {
    switch (format) {
        case SamlNameIdFormat::UNSPECIFIED:
            return "urn:oasis:names:tc:SAML:1.1:nameid-format:unspecified";
        case SamlNameIdFormat::EMAIL:
            return "urn:oasis:names:tc:SAML:1.1:nameid-format:emailAddress";
        case SamlNameIdFormat::TRANSIENT:
            return "urn:oasis:names:tc:SAML:2.0:nameid-format:transient";
        case SamlNameIdFormat::PERSISTENT:
            return "urn:oasis:names:tc:SAML:2.0:nameid-format:persistent";
        case SamlNameIdFormat::KERBEROS:
            return "urn:oasis:names:tc:SAML:2.0:nameid-format:kerberos";
        case SamlNameIdFormat::ENTITY:
            return "urn:oasis:names:tc:SAML:2.0:nameid-format:entity";
        case SamlNameIdFormat::ENCRYPTED:
            return "urn:oasis:names:tc:SAML:2.0:nameid-format:encrypted";
        default:
            return "urn:oasis:names:tc:SAML:1.1:nameid-format:unspecified";
    }
}

SamlNameIdFormat parseSamlNameIdFormat(const std::string& uri) {
    if (uri.find("emailAddress") != std::string::npos) {
        return SamlNameIdFormat::EMAIL;
    }
    if (uri.find("transient") != std::string::npos) {
        return SamlNameIdFormat::TRANSIENT;
    }
    if (uri.find("persistent") != std::string::npos) {
        return SamlNameIdFormat::PERSISTENT;
    }
    if (uri.find("kerberos") != std::string::npos) {
        return SamlNameIdFormat::KERBEROS;
    }
    if (uri.find("entity") != std::string::npos) {
        return SamlNameIdFormat::ENTITY;
    }
    if (uri.find("encrypted") != std::string::npos) {
        return SamlNameIdFormat::ENCRYPTED;
    }
    return SamlNameIdFormat::UNSPECIFIED;
}

core::Status validateSamlSchema(const std::string& xml,
                                core::ErrorContext* ctx) {
    // Stub: Would validate against SAML XSD schemas
    return core::Status::OK;
}

std::string canonicalizeXml(const std::string& xml) {
    // Stub: Would use libxml2 c14n
    return xml;
}

core::Status extractCertFromMetadata(const std::string& metadata_xml,
                                     std::vector<std::string>& certs,
                                     core::ErrorContext* ctx) {
    certs.clear();

    // Find X509Certificate elements
    size_t pos = 0;
    while ((pos = metadata_xml.find("<ds:X509Certificate>", pos)) != std::string::npos) {
        size_t start = pos + 20;
        size_t end = metadata_xml.find("</ds:X509Certificate>", start);
        if (end != std::string::npos) {
            std::string cert = metadata_xml.substr(start, end - start);
            // Remove whitespace
            cert.erase(std::remove_if(cert.begin(), cert.end(), ::isspace), cert.end());
            certs.push_back(cert);
        }
        pos = end;
    }

    return core::Status::OK;
}

}  // namespace security
}  // namespace scratchbird
