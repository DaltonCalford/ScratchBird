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
        std::cerr << "usage: auth_plugin_runbook_selftest <plugin_src_root>\n";
        return 2;
    }

    const std::string root = argv[1];
    const std::string runbook_path = root + "/AUTH_PLUGIN_INCIDENT_ROLLBACK_RUNBOOK.md";
    const std::string dryrun_path = root + "/AUTH_PLUGIN_RUNBOOK_DRYRUN_EVIDENCE_20260304.md";
    const std::string signoff_path = root + "/AUTH_PLUGIN_ONCALL_SIGNOFF_20260304.json";

    std::string runbook;
    std::string dryrun;
    std::string signoff;

    bool ok = true;
    ok = expect(readFile(runbook_path, &runbook), "unable to read runbook artifact") && ok;
    ok = expect(readFile(dryrun_path, &dryrun), "unable to read dry-run evidence artifact") && ok;
    ok = expect(readFile(signoff_path, &signoff), "unable to read signoff artifact") && ok;
    if (!ok) {
        return 1;
    }

    const std::vector<std::string> runbook_tokens{
        "Incident and Rollback Runbook",
        "Severity Model",
        "Detection and Triage",
        "Rollback Procedure",
        "Plugin-Specific Rollback Controls",
        "Communication and Escalation",
        "Post-Incident Actions"
    };
    ok = requireTokens(runbook, runbook_tokens, "runbook") && ok;

    const std::vector<std::string> dryrun_tokens{
        "Dry-run executed: YES",
        "Scenario DR-01",
        "Scenario DR-02",
        "Scenario DR-03",
        "Result: PASS",
        "Passed: 3",
        "Failed: 0"
    };
    ok = requireTokens(dryrun, dryrun_tokens, "dry-run evidence") && ok;

    const std::vector<std::string> signoff_tokens{
        "\"ticket\": \"AUTH-PROD-E03\"",
        "\"role\": \"security_primary_oncall\"",
        "\"role\": \"database_runtime_oncall\"",
        "\"status\": \"approved\"",
        "\"dry_run_evidence\": \"AUTH_PLUGIN_RUNBOOK_DRYRUN_EVIDENCE_20260304.md\""
    };
    ok = requireTokens(signoff, signoff_tokens, "signoff") && ok;

    if (!ok) {
        return 1;
    }

    std::cout << "auth_plugin_runbook_selftest: PASS\n";
    return 0;
}
