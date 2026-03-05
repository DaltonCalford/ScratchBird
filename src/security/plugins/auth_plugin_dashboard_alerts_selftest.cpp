/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 */

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    return true;
}

bool readFile(const std::string& path, std::string* out_text) {
    if (!out_text) {
        return false;
    }
    std::ifstream in(path);
    if (!in.is_open()) {
        return false;
    }
    out_text->assign(std::istreambuf_iterator<char>(in),
                     std::istreambuf_iterator<char>());
    return true;
}

bool requireTokens(const std::string& text,
                   const std::vector<std::string>& tokens,
                   const std::string& label) {
    bool ok = true;
    for (const auto& token : tokens) {
        ok = expect(text.find(token) != std::string::npos,
                    label + " missing token: " + token) &&
             ok;
    }
    return ok;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: auth_plugin_dashboard_alerts_selftest <plugin_src_root>\n";
        return 2;
    }

    const std::string root = argv[1];
    const std::string manifest_path = root + "/auth_plugin_dashboard_alerts_manifest.json";
    const std::string routing_path = root + "/auth_plugin_alert_routing_matrix.json";

    std::string manifest_text;
    std::string routing_text;

    bool ok = true;
    ok = expect(readFile(manifest_path, &manifest_text),
                "unable to read dashboard manifest: " + manifest_path) &&
         ok;
    ok = expect(readFile(routing_path, &routing_text),
                "unable to read alert routing matrix: " + routing_path) &&
         ok;
    if (!ok) {
        return 1;
    }

    const std::vector<std::string> required_panel_tokens{
        "\"id\": \"auth-latency-p95\"",
        "\"id\": \"auth-latency-p99\"",
        "\"id\": \"auth-deny-rate\"",
        "\"id\": \"auth-challenge-completion-rate\"",
        "\"id\": \"auth-provider-error-rate\"",
        "\"metric\": \"auth.plugin.request.duration_ms\"",
        "\"metric\": \"auth.plugin.outcome.count\"",
        "\"expression\": \"auth.plugin.challenge.completed.count / auth.plugin.challenge.started.count\""
    };
    ok = requireTokens(manifest_text, required_panel_tokens, "dashboard manifest") && ok;

    const std::vector<std::string> required_routing_tokens{
        "\"severity\": \"sev1\"",
        "\"severity\": \"sev2\"",
        "\"severity\": \"sev3\"",
        "pagerduty:security-primary",
        "pagerduty:database-runtime",
        "slack:#scratchbird-auth-incidents",
        "slack:#scratchbird-runtime-alerts",
        "slack:#scratchbird-auth-observability",
        "\"id\": \"auth-plugin-sev1-security-regression\"",
        "\"id\": \"auth-plugin-sev2-latency-p99\"",
        "\"id\": \"auth-plugin-sev2-deny-rate\"",
        "\"id\": \"auth-plugin-sev2-provider-error-rate\"",
        "\"id\": \"auth-plugin-sev3-challenge-completion\""
    };
    ok = requireTokens(routing_text, required_routing_tokens, "routing matrix") && ok;

    const std::vector<std::string> required_plugins{
        "trust_reject",
        "peer",
        "password_compat",
        "token_authkey",
        "certificate_mtls",
        "jwt_oidc",
        "oauth_validator",
        "proxy_assertion",
        "workload_identity",
        "ident",
        "radius",
        "pam",
        "ldap",
        "kerberos",
        "scram",
        "webauthn",
        "factor_chain"
    };
    for (const auto& plugin : required_plugins) {
        ok = expect(manifest_text.find("\"" + plugin + "\"") != std::string::npos,
                    "manifest missing plugin scope entry: " + plugin) &&
             ok;
        ok = expect(routing_text.find("\"plugin\": \"" + plugin + "\"") != std::string::npos,
                    "routing matrix missing plugin ownership entry: " + plugin) &&
             ok;
    }

    if (!ok) {
        return 1;
    }

    std::cout << "auth_plugin_dashboard_alerts_selftest: PASS\n";
    return 0;
}
