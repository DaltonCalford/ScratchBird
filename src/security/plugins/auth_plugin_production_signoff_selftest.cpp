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
        std::cerr << "usage: auth_plugin_production_signoff_selftest <plugin_src_root>\n";
        return 2;
    }

    const std::string root = argv[1];
    const std::string review_path = root + "/AUTH_PLUGIN_PRODUCTION_READINESS_REVIEW_20260304.md";
    const std::string signoff_path = root + "/AUTH_PLUGIN_PRODUCTION_SIGNOFF_20260304.json";
    const std::string tracker_csv_path = root + "/PLUGIN_PRODUCTION_EXECUTION_TRACKER.csv";

    std::string review;
    std::string signoff;
    std::string tracker;

    bool ok = true;
    ok = expect(readFile(review_path, &review), "unable to read readiness review artifact") && ok;
    ok = expect(readFile(signoff_path, &signoff), "unable to read production signoff artifact") &&
         ok;
    ok = expect(readFile(tracker_csv_path, &tracker), "unable to read tracker csv") && ok;
    if (!ok) {
        return 1;
    }

    const std::vector<std::string> review_tokens{
        "Ticket: `AUTH-PROD-E04`",
        "Gate Summary (`AT-E04-01`)",
        "Build gate: PASS",
        "Test gate: PASS",
        "Security/compliance gate: PASS",
        "Rollout/ops readiness gate: PASS",
        "Dependency closure gate: PASS",
        "AT-E04-01`: satisfied",
        "AT-E04-02`: satisfied"
    };
    ok = requireTokens(review, review_tokens, "readiness review") && ok;

    const std::vector<std::string> signoff_tokens{
        "\"ticket\": \"AUTH-PROD-E04\"",
        "\"decision\": \"approved_for_controlled_cutover\"",
        "\"role\": \"security_primary\"",
        "\"role\": \"database_runtime_operations\"",
        "\"role\": \"release_authority\"",
        "\"status\": \"approved\""
    };
    ok = requireTokens(signoff, signoff_tokens, "signoff artifact") && ok;

    const std::vector<std::string> dependency_rows{
        "AUTH-PROD-B01,B,trust_reject",
        "AUTH-PROD-B14,B,kerberos",
        "AUTH-PROD-C01,C,scram",
        "AUTH-PROD-C03,C,factor_chain",
        "AUTH-PROD-D01,D,Threat model",
        "AUTH-PROD-D04,D,Crypto/dependency review",
        "AUTH-PROD-E01,E,Canary rollout",
        "AUTH-PROD-E03,E,Runbooks"
    };
    ok = requireTokens(tracker, dependency_rows, "tracker dependencies") && ok;

    ok = expect(tracker.find("AUTH-PROD-E04,E,Production signoff") != std::string::npos,
                "tracker missing E04 row") &&
         ok;
    ok = expect(tracker.find("AUTH-PROD-E04,E,Production signoff") != std::string::npos &&
                    tracker.find("AUTH-PROD-E04,E,Production signoff") < tracker.size(),
                "tracker E04 row parse check failed") &&
         ok;

    if (tracker.find("AUTH-PROD-E04,E,Production signoff") != std::string::npos) {
        const auto pos = tracker.find("AUTH-PROD-E04,E,Production signoff");
        const auto line_end = tracker.find('\n', pos);
        const std::string line = tracker.substr(pos, line_end == std::string::npos ? std::string::npos
                                                                                    : line_end - pos);
        ok = expect(line.find(",REVIEW") != std::string::npos || line.find(",DONE") != std::string::npos,
                    "tracker E04 status must be REVIEW or DONE") &&
             ok;
    }

    if (!ok) {
        return 1;
    }

    std::cout << "auth_plugin_production_signoff_selftest: PASS\n";
    return 0;
}
