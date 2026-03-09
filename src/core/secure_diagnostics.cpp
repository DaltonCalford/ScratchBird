#include "scratchbird/core/secure_diagnostics.h"

#include <algorithm>
#include <cctype>
#include <regex>

namespace scratchbird::core {

namespace {

std::string normalizeAscii(std::string_view input)
{
    std::string out;
    out.reserve(input.size());
    for (const unsigned char ch : input)
    {
        out.push_back(static_cast<char>(std::tolower(ch)));
    }
    return out;
}

bool containsSensitiveToken(const std::string& normalized_key)
{
    static const char* kTokens[] = {
        "password",
        "passwd",
        "pwd",
        "secret",
        "token",
        "apikey",
        "api_key",
        "api-key",
        "credential",
        "credentials",
        "authorization",
        "auth_header",
        "private_key",
        "client_secret",
        "shared_secret",
        "key_material"
    };
    for (const char* token : kTokens)
    {
        if (normalized_key.find(token) != std::string::npos)
        {
            return true;
        }
    }
    return false;
}

}  // namespace

bool isSensitiveDiagnosticFieldName(std::string_view key)
{
    return containsSensitiveToken(normalizeAscii(key));
}

std::string redactSensitiveDiagnosticText(const std::string& input)
{
    if (input.empty())
    {
        return input;
    }

    std::string redacted = input;
    static const std::regex kUriUserinfoRegex(
        R"(([A-Za-z][A-Za-z0-9+.-]*://[^/@:\s]+:)([^@/\s]+)(@))",
        std::regex::icase);
    static const std::regex kUriAuthorityRegex(
        R"(([A-Za-z][A-Za-z0-9+.-]*://)([^/\s?#]+))",
        std::regex::icase);
    static const std::regex kAssignmentRegex(
        R"(((?:password|passwd|pwd|secret|token|api[_-]?key|credential(?:s)?|authorization|client[_-]?secret|shared[_-]?secret)[ \t]*[:=][ \t]*)(\"[^\"]*\"|'[^']*'|[^,; \t\r\n]+))",
        std::regex::icase);
    static const std::regex kJsonAssignmentRegex(
        R"((\"(?:password|passwd|pwd|secret|token|api[_-]?key|credential(?:s)?|authorization|client[_-]?secret|shared[_-]?secret)\"[ \t]*:[ \t]*)(\"[^\"]*\"|[^,}\s]+))",
        std::regex::icase);
    static const std::regex kQueryParamRegex(
        R"(([?&](?:password|passwd|pwd|secret|token|api[_-]?key|credential(?:s)?|authorization|client[_-]?secret|shared[_-]?secret)=)[^&#\s]*)",
        std::regex::icase);
    static const std::regex kSqlSecretRegex(
        R"(((?:identified[ \t]+by|password|secret|token|api[_-]?key|authorization)[ \t]+)(\"[^\"]*\"|'[^']*'|[^\s,;]+))",
        std::regex::icase);
    static const std::regex kBearerRegex(
        R"(((?:authorization|auth)[ \t]*:[ \t]*(?:bearer|basic)[ \t]+)[A-Za-z0-9._~+\/=-]+)",
        std::regex::icase);

    redacted = std::regex_replace(redacted, kUriUserinfoRegex, "$1<redacted>$3");
    redacted = std::regex_replace(redacted, kBearerRegex, "$1<redacted>");
    redacted = std::regex_replace(redacted, kQueryParamRegex, "$1<redacted>");
    redacted = std::regex_replace(redacted, kAssignmentRegex, "$1<redacted>");
    redacted = std::regex_replace(redacted, kJsonAssignmentRegex, "$1\"<redacted>\"");
    redacted = std::regex_replace(redacted, kSqlSecretRegex, "$1<redacted>");
    redacted = std::regex_replace(redacted, kUriAuthorityRegex, "$1<endpoint>");
    return redacted;
}

std::string redactSensitiveDiagnosticField(std::string_view key,
                                          const std::string& value)
{
    if (value.empty())
    {
        return value;
    }
    if (isSensitiveDiagnosticFieldName(key))
    {
        return "<redacted>";
    }
    return redactSensitiveDiagnosticText(value);
}

}  // namespace scratchbird::core
