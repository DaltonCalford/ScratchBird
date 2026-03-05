/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 */

#include <filesystem>
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

bool readFile(const std::filesystem::path& path, std::string* out_text) {
    if (!out_text) {
        return false;
    }
    std::ifstream in(path);
    if (!in.is_open()) {
        return false;
    }
    out_text->assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

bool containsAny(std::string_view text, const std::vector<std::string>& needles) {
    for (const auto& needle : needles) {
        if (text.find(needle) != std::string_view::npos) {
            return true;
        }
    }
    return false;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: auth_plugin_hardening_h1_selftest <plugin_src_root>\n";
        return 2;
    }

    const std::filesystem::path root = argv[1];
    const std::vector<std::string> synthetic_tokens{
        "__timeout__",
        "__deny__",
        "__reject__",
        "simulate"
    };

    const std::filesystem::path radius_path = root / "radius" / "radius_plugin.cpp";
    const std::filesystem::path pam_path = root / "pam" / "pam_plugin.cpp";
    const std::filesystem::path ldap_path = root / "ldap" / "ldap_plugin.cpp";

    std::string radius_text;
    std::string pam_text;
    std::string ldap_text;
    bool ok = true;
    ok = expect(readFile(radius_path, &radius_text), "unable to read radius plugin") && ok;
    ok = expect(readFile(pam_path, &pam_text), "unable to read pam plugin") && ok;
    ok = expect(readFile(ldap_path, &ldap_text), "unable to read ldap plugin") && ok;
    if (!ok) {
        return 1;
    }

    ok = expect(radius_text.find("auth.radius.allow_test_directives") != std::string::npos,
                "radius must define allow_test_directives policy gate") &&
         ok;
    ok = expect(radius_text.find("AUTH_RADIUS_TEST_DIRECTIVE_DENIED") != std::string::npos,
                "radius must expose explicit test-directive deny error") &&
         ok;
    ok = expect(pam_text.find("auth.pam.allow_test_directives") != std::string::npos,
                "pam must define allow_test_directives policy gate") &&
         ok;
    ok = expect(pam_text.find("AUTH_PAM_TEST_DIRECTIVE_DENIED") != std::string::npos,
                "pam must expose explicit test-directive deny error") &&
         ok;
    ok = expect(ldap_text.find("auth.ldap.allow_test_directives") != std::string::npos,
                "ldap must define allow_test_directives policy gate") &&
         ok;
    ok = expect(ldap_text.find("AUTH_LDAP_TEST_DIRECTIVE_DENIED") != std::string::npos,
                "ldap must expose explicit test-directive deny error") &&
         ok;

    std::vector<std::filesystem::path> plugin_files;
    for (const auto& entry : std::filesystem::directory_iterator(root)) {
        if (!entry.is_directory()) {
            continue;
        }
        const auto file = entry.path() / (entry.path().filename().string() + "_plugin.cpp");
        if (std::filesystem::exists(file)) {
            plugin_files.push_back(file);
        }
    }

    for (const auto& file : plugin_files) {
        if (file == radius_path || file == pam_path || file == ldap_path) {
            continue;
        }
        std::string text;
        ok = expect(readFile(file, &text), "unable to read plugin source: " + file.string()) && ok;
        if (!ok) {
            continue;
        }
        if (containsAny(text, synthetic_tokens)) {
            ok = expect(false,
                        "unexpected synthetic directive token in plugin source: " +
                            file.filename().string()) &&
                 ok;
        }
    }

    if (!ok) {
        return 1;
    }

    std::cout << "auth_plugin_hardening_h1_selftest: PASS\n";
    return 0;
}
