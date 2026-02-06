/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/*
 * ScratchBird Database Engine
 * GitConfigParser Unit Tests
 * Copyright (c) 2025 ScratchBird Project
 */

#include <gtest/gtest.h>
#include <fstream>
#include "scratchbird/git/GitConfigParser.h"
#include "scratchbird/git/GitTypes.h"

using namespace scratchbird::git;

//=============================================================================
// Canonical Key Tests (YAML)
//=============================================================================

TEST(GitConfigParserCanonicalTest, ParseCanonicalRepoUrl) {
    GitConfigParser parser;
    std::string yaml = R"(
repository:
  repo_url: "https://github.com/example/repo.git"
  repo_branch: main
)";
    EXPECT_TRUE(parser.parseString(yaml));
    
    auto config = parser.getGitConfig();
    EXPECT_EQ(config.repo_url, "https://github.com/example/repo.git");
    EXPECT_EQ(config.repo_branch, "main");
}

TEST(GitConfigParserCanonicalTest, ParseAllCanonicalKeys) {
    GitConfigParser parser;
    std::string yaml = R"(
repository:
  repo_type: git
  repo_url: "https://github.com/example/repo.git"
  repo_branch: develop
  repo_path: /var/lib/scratchbird/repos/example
  repo_mode: full_sync
  sign_commits: true
  commit_template: "[SCHEMA] {message}"
  gpg_key_id: ABC123DEF
)";
    EXPECT_TRUE(parser.parseString(yaml));
    
    auto config = parser.getGitConfig();
    EXPECT_EQ(config.repo_type, "git");
    EXPECT_EQ(config.repo_url, "https://github.com/example/repo.git");
    EXPECT_EQ(config.repo_branch, "develop");
    EXPECT_EQ(config.repo_path, "/var/lib/scratchbird/repos/example");
    EXPECT_EQ(config.repo_mode, "full_sync");
    EXPECT_TRUE(config.sign_commits);
    EXPECT_EQ(config.commit_template, "[SCHEMA] {message}");
    EXPECT_EQ(config.gpg_key_id, "ABC123DEF");
    
    // Verify integration mode
    EXPECT_EQ(config.getIntegrationMode(), GitIntegrationMode::FULL_SYNC);
    EXPECT_TRUE(config.getAutoCommit());
    EXPECT_TRUE(config.getAutoPush());
    EXPECT_TRUE(config.getAutoPull());
}

TEST(GitConfigParserCanonicalTest, ParseIntegrationModes) {
    struct TestCase {
        std::string mode;
        GitIntegrationMode expected;
        bool auto_commit;
        bool auto_push;
        bool auto_pull;
    };
    
    std::vector<TestCase> test_cases = {
        {"manual", GitIntegrationMode::MANUAL, false, false, false},
        {"auto_commit", GitIntegrationMode::AUTO_COMMIT, true, false, false},
        {"auto_push", GitIntegrationMode::AUTO_PUSH, true, true, false},
        {"full_sync", GitIntegrationMode::FULL_SYNC, true, true, true},
    };
    
    for (const auto& tc : test_cases) {
        GitConfigParser parser;
        std::string yaml = "repository:\n  repo_url: \"https://example.com/repo.git\"\n  repo_mode: " + tc.mode + "\n";
        
        EXPECT_TRUE(parser.parseString(yaml)) << "Failed for mode: " << tc.mode;
        
        auto config = parser.getGitConfig();
        EXPECT_EQ(config.getIntegrationMode(), tc.expected) << "Mode mismatch for: " << tc.mode;
        EXPECT_EQ(config.getAutoCommit(), tc.auto_commit) << "AutoCommit mismatch for: " << tc.mode;
        EXPECT_EQ(config.getAutoPush(), tc.auto_push) << "AutoPush mismatch for: " << tc.mode;
        EXPECT_EQ(config.getAutoPull(), tc.auto_pull) << "AutoPull mismatch for: " << tc.mode;
    }
}

TEST(GitConfigParserCanonicalTest, ValidateRequiresRepoUrl) {
    GitConfigParser parser;
    std::string yaml = "repository:\n  repo_branch: main\n";
    
    EXPECT_TRUE(parser.parseString(yaml));
    auto errors = parser.validate();
    
    EXPECT_FALSE(errors.empty());
    EXPECT_EQ(errors[0], "repository.repo_url is required");
}

TEST(GitConfigParserCanonicalTest, ValidateUrlFormat) {
    GitConfigParser parser;
    
    // Valid URLs
    std::vector<std::string> valid_urls = {
        "https://github.com/user/repo.git",
        "http://gitlab.com/user/repo.git",
        "git@github.com:user/repo.git",
        "ssh://git@example.com/repo.git"
    };
    
    for (const auto& url : valid_urls) {
        GitConfigParser p;
        std::string yaml = "repository:\n  repo_url: \"" + url + "\"\n";
        EXPECT_TRUE(p.parseString(yaml));
        auto errors = p.validate();
        EXPECT_TRUE(errors.empty()) << "URL should be valid: " << url;
    }
    
    // Invalid URL
    std::string yaml = "repository:\n  repo_url: \"not-a-valid-url\"\n";
    EXPECT_TRUE(parser.parseString(yaml));
    auto errors = parser.validate();
    EXPECT_FALSE(errors.empty());
    EXPECT_EQ(errors[0], "repository.repo_url must be a valid Git URL");
}

TEST(GitConfigParserCanonicalTest, ValidateRepoMode) {
    GitConfigParser parser;
    
    // Valid modes
    std::vector<std::string> valid_modes = {"manual", "auto_commit", "auto_push", "full_sync"};
    for (const auto& mode : valid_modes) {
        GitConfigParser p;
        std::string yaml = "repository:\n  repo_url: \"https://example.com/repo.git\"\n  repo_mode: " + mode + "\n";
        EXPECT_TRUE(p.parseString(yaml));
        auto errors = p.validate();
        EXPECT_TRUE(errors.empty()) << "Mode should be valid: " << mode;
    }
    
    // Invalid mode
    std::string yaml = R"(
repository:
  repo_url: "https://example.com/repo.git"
  repo_mode: invalid_mode
)";
    EXPECT_TRUE(parser.parseString(yaml));
    auto errors = parser.validate();
    EXPECT_FALSE(errors.empty());
    EXPECT_EQ(errors[0], "repository.repo_mode must be one of: manual, auto_commit, auto_push, full_sync");
}

//=============================================================================
// Legacy Alias Tests (YAML)
//=============================================================================

TEST(GitConfigParserLegacyTest, ParseLegacyKeys) {
    GitConfigParser parser;
    std::string yaml = R"(
repository:
  type: git
  url: "https://github.com/example/repo.git"
  branch: main
  path: /var/lib/repo
)";
    EXPECT_TRUE(parser.parseString(yaml));
    
    auto config = parser.getGitConfig();
    EXPECT_EQ(config.repo_type, "git");
    EXPECT_EQ(config.repo_url, "https://github.com/example/repo.git");
    EXPECT_EQ(config.repo_branch, "main");
    EXPECT_EQ(config.repo_path, "/var/lib/repo");
    
    // Should have deprecation warnings
    EXPECT_TRUE(parser.hasLegacyKeys());
    EXPECT_FALSE(parser.hasCanonicalKeys());
    
    auto warnings = parser.getDeprecationWarnings();
    EXPECT_FALSE(warnings.empty());
}

TEST(GitConfigParserLegacyTest, LegacyModeBooleanFlags) {
    GitConfigParser parser;
    std::string yaml = R"(
repository:
  url: "https://github.com/example/repo.git"
  auto_commit: true
  auto_push: true
  auto_pull: false
)";
    EXPECT_TRUE(parser.parseString(yaml));
    
    auto config = parser.getGitConfig();
    EXPECT_EQ(config.getIntegrationMode(), GitIntegrationMode::AUTO_PUSH);
    EXPECT_TRUE(config.getAutoCommit());
    EXPECT_TRUE(config.getAutoPush());
    EXPECT_FALSE(config.getAutoPull());
}

TEST(GitConfigParserLegacyTest, LegacyAllBooleanFlagsTrue) {
    GitConfigParser parser;
    std::string yaml = R"(
repository:
  url: "https://github.com/example/repo.git"
  auto_commit: true
  auto_push: true
  auto_pull: true
)";
    EXPECT_TRUE(parser.parseString(yaml));
    
    auto config = parser.getGitConfig();
    EXPECT_EQ(config.getIntegrationMode(), GitIntegrationMode::FULL_SYNC);
    EXPECT_TRUE(config.getAutoCommit());
    EXPECT_TRUE(config.getAutoPush());
    EXPECT_TRUE(config.getAutoPull());
}

TEST(GitConfigParserLegacyTest, ValidateAllowsLegacyUrl) {
    GitConfigParser parser;
    std::string yaml = "repository:\n  url: \"https://example.com/repo.git\"\n";
    
    EXPECT_TRUE(parser.parseString(yaml));
    auto errors = parser.validate();
    
    // Should validate successfully (legacy url maps to repo_url)
    EXPECT_TRUE(errors.empty()) << "Legacy url should be accepted";
}

//=============================================================================
// Precedence Tests (Canonical vs Legacy)
//=============================================================================

TEST(GitConfigParserPrecedenceTest, CanonicalTakesPrecedenceOverLegacy) {
    GitConfigParser parser;
    std::string yaml = R"(
repository:
  repo_url: "https://canonical.example.com/repo.git"
  url: "https://legacy.example.com/repo.git"
  repo_branch: canonical-branch
  branch: legacy-branch
)";
    EXPECT_TRUE(parser.parseString(yaml));
    
    auto config = parser.getGitConfig();
    EXPECT_EQ(config.repo_url, "https://canonical.example.com/repo.git");
    EXPECT_EQ(config.repo_branch, "canonical-branch");
    
    // Should have both canonical and legacy warnings
    EXPECT_TRUE(parser.hasCanonicalKeys());
    EXPECT_TRUE(parser.hasLegacyKeys());
    
    // Should have warning about legacy being ignored
    auto warnings = parser.getDeprecationWarnings();
    bool found_ignored_warning = false;
    for (const auto& w : warnings) {
        if (w.find("ignored") != std::string::npos) {
            found_ignored_warning = true;
            break;
        }
    }
    EXPECT_TRUE(found_ignored_warning) << "Should warn that legacy key was ignored";
}

TEST(GitConfigParserPrecedenceTest, RepoModePrecedenceOverBooleanFlags) {
    GitConfigParser parser;
    std::string yaml = R"(
repository:
  repo_url: "https://example.com/repo.git"
  repo_mode: manual
  auto_commit: true
  auto_push: true
)";
    EXPECT_TRUE(parser.parseString(yaml));
    
    auto config = parser.getGitConfig();
    EXPECT_EQ(config.repo_mode, "manual");
    EXPECT_EQ(config.getIntegrationMode(), GitIntegrationMode::MANUAL);
    EXPECT_FALSE(config.getAutoCommit());
    EXPECT_FALSE(config.getAutoPush());
}

TEST(GitConfigParserPrecedenceTest, CanonicalOnlyNoWarnings) {
    GitConfigParser parser;
    std::string yaml = R"(
repository:
  repo_url: "https://example.com/repo.git"
  repo_branch: main
  repo_mode: auto_push
)";
    EXPECT_TRUE(parser.parseString(yaml));
    
    EXPECT_TRUE(parser.hasCanonicalKeys());
    EXPECT_FALSE(parser.hasLegacyKeys());
    
    auto warnings = parser.getDeprecationWarnings();
    EXPECT_TRUE(warnings.empty()) << "No warnings expected for canonical-only config";
}

//=============================================================================
// INI Parsing Tests
//=============================================================================

TEST(GitConfigParserINITest, ParseGitRepositorySection) {
    GitConfigParser parser;
    std::string ini = R"(
[git.repository]
repo_type = git
repo_url = https://github.com/example/repo.git
repo_branch = main
repo_path = /var/lib/repo
repo_mode = full_sync
sign_commits = true
commit_template = [SCHEMA] {message}
gpg_key_id = ABC123
)";
    EXPECT_TRUE(parser.parseINI(ini));
    
    auto config = parser.getGitConfig();
    EXPECT_EQ(config.repo_type, "git");
    EXPECT_EQ(config.repo_url, "https://github.com/example/repo.git");
    EXPECT_EQ(config.repo_branch, "main");
    EXPECT_EQ(config.repo_path, "/var/lib/repo");
    EXPECT_EQ(config.repo_mode, "full_sync");
    EXPECT_TRUE(config.sign_commits);
    EXPECT_EQ(config.commit_template, "[SCHEMA] {message}");
    EXPECT_EQ(config.gpg_key_id, "ABC123");
}

TEST(GitConfigParserINITest, ParseGitSchemaSection) {
    GitConfigParser parser;
    std::string ini = R"(
[git.schema]
include_grants = true
include_comments = false
include_defaults = true
separate_indexes = true
file_per_object = false
)";
    EXPECT_TRUE(parser.parseINI(ini));
    
    auto options = parser.getSchemaOptions();
    EXPECT_TRUE(options.include_grants);
    EXPECT_FALSE(options.include_comments);
    EXPECT_TRUE(options.include_defaults);
    EXPECT_TRUE(options.separate_indexes);
    EXPECT_FALSE(options.file_per_object);
}

TEST(GitConfigParserINITest, ParseGitMigrationsSection) {
    GitConfigParser parser;
    std::string ini = R"(
[git.migrations]
table = SYS$MIGRATIONS
naming = timestamp
generate_down = false
transaction_per_file = true
checksum_validation = true
)";
    EXPECT_TRUE(parser.parseINI(ini));
    
    auto config = parser.getMigrationConfig();
    EXPECT_EQ(config.table_name, "SYS$MIGRATIONS");
    EXPECT_EQ(config.naming, MigrationNaming::TIMESTAMP);
    EXPECT_FALSE(config.generate_down);
    EXPECT_TRUE(config.transaction_per_file);
    EXPECT_TRUE(config.checksum_validation);
}

TEST(GitConfigParserINITest, ParseLegacyKeysInINI) {
    GitConfigParser parser;
    std::string ini = R"(
[git]
type = git
url = https://github.com/example/repo.git
branch = main
path = /var/lib/repo
mode = auto_push
ssh_key = /home/user/.ssh/id_rsa
)";
    EXPECT_TRUE(parser.parseINI(ini));
    
    auto config = parser.getGitConfig();
    EXPECT_EQ(config.repo_type, "git");
    EXPECT_EQ(config.repo_url, "https://github.com/example/repo.git");
    EXPECT_EQ(config.repo_branch, "main");
    EXPECT_EQ(config.repo_path, "/var/lib/repo");
    EXPECT_EQ(config.repo_mode, "auto_push");
    EXPECT_EQ(config.ssh_key_path, "/home/user/.ssh/id_rsa");
}

TEST(GitConfigParserINITest, INICanonicalPrecedenceOverLegacy) {
    GitConfigParser parser;
    std::string ini = R"(
[git.repository]
repo_url = https://canonical.example.com/repo.git
url = https://legacy.example.com/repo.git
repo_mode = manual
mode = full_sync
)";
    EXPECT_TRUE(parser.parseINI(ini));
    
    auto config = parser.getGitConfig();
    EXPECT_EQ(config.repo_url, "https://canonical.example.com/repo.git");
    EXPECT_EQ(config.repo_mode, "manual");
    
    // Should have deprecation warnings for legacy keys
    EXPECT_TRUE(parser.hasLegacyKeys());
}

TEST(GitConfigParserINITest, INIBooleanParsing) {
    GitConfigParser parser;
    
    struct TestCase {
        std::string value;
        bool expected;
    };
    
    std::vector<TestCase> test_cases = {
        {"true", true},
        {"yes", true},
        {"1", true},
        {"false", false},
        {"no", false},
        {"0", false},
    };
    
    for (const auto& tc : test_cases) {
        GitConfigParser p;
        std::string ini = "[git.schema]\ninclude_grants = " + tc.value + "\n";
        EXPECT_TRUE(p.parseINI(ini)) << "Failed for value: " << tc.value;
        
        auto options = p.getSchemaOptions();
        EXPECT_EQ(options.include_grants, tc.expected) << 
            "Value mismatch for: " << tc.value;
    }
}

TEST(GitConfigParserINITest, INICommentsIgnored) {
    GitConfigParser parser;
    std::string ini = R"(
# This is a comment
[git.repository]  ; inline comment
repo_url = https://example.com/repo.git
; another comment
repo_branch = main
)";
    EXPECT_TRUE(parser.parseINI(ini));
    
    auto config = parser.getGitConfig();
    EXPECT_EQ(config.repo_url, "https://example.com/repo.git");
    EXPECT_EQ(config.repo_branch, "main");
}

TEST(GitConfigParserINITest, INIFileAutoDetection) {
    // Create a temporary INI file
    const char* ini_path = "/tmp/test_git_config.ini";
    {
        std::ofstream file(ini_path);
        file << "[git.repository]\nrepo_url = https://example.com/repo.git\n";
    }
    
    GitConfigParser parser;
    EXPECT_TRUE(parser.parseFile(ini_path));
    
    auto config = parser.getGitConfig();
    EXPECT_EQ(config.repo_url, "https://example.com/repo.git");
    
    // Cleanup
    std::remove(ini_path);
}

//=============================================================================
// Serialization Tests (toYAML)
//=============================================================================

TEST(GitConfigParserSerializationTest, ToYAMLEmitsCanonicalKeys) {
    GitConfigParser parser;
    std::string yaml = R"(
repository:
  url: "https://github.com/example/repo.git"
  branch: main
)";
    EXPECT_TRUE(parser.parseString(yaml));
    
    // Re-serialize
    std::string output = parser.toYAML();
    
    // Should use canonical keys
    EXPECT_NE(output.find("repo_url:"), std::string::npos);
    EXPECT_NE(output.find("repo_branch:"), std::string::npos);
    EXPECT_NE(output.find("repo_type:"), std::string::npos);
    EXPECT_NE(output.find("repo_mode:"), std::string::npos);
    
    // Should NOT have legacy keys
    EXPECT_EQ(output.find("\n  url:"), std::string::npos);
    EXPECT_EQ(output.find("\n  branch:"), std::string::npos);
}

TEST(GitConfigParserSerializationTest, ToYAMLRoundTrip) {
    GitConfigParser parser;
    std::string yaml = R"(
repository:
  repo_url: "https://github.com/example/repo.git"
  repo_branch: develop
  repo_path: /var/lib/repo
  repo_mode: full_sync
  sign_commits: true
  commit_template: "[SCHEMA] {message}"
schema:
  include_grants: true
  include_comments: false
migrations:
  table: CUSTOM_MIGRATIONS
  naming: timestamp
)";
    EXPECT_TRUE(parser.parseString(yaml));
    
    // Serialize
    std::string output = parser.toYAML();
    
    // Parse the output
    GitConfigParser parser2;
    EXPECT_TRUE(parser2.parseString(output));
    
    // Compare configs
    auto config1 = parser.getGitConfig();
    auto config2 = parser2.getGitConfig();
    
    EXPECT_EQ(config1.repo_url, config2.repo_url);
    EXPECT_EQ(config1.repo_branch, config2.repo_branch);
    EXPECT_EQ(config1.repo_path, config2.repo_path);
    EXPECT_EQ(config1.repo_mode, config2.repo_mode);
    EXPECT_EQ(config1.sign_commits, config2.sign_commits);
    EXPECT_EQ(config1.commit_template, config2.commit_template);
    
    auto options1 = parser.getSchemaOptions();
    auto options2 = parser2.getSchemaOptions();
    EXPECT_EQ(options1.include_grants, options2.include_grants);
    EXPECT_EQ(options1.include_comments, options2.include_comments);
}

//=============================================================================
// Environment Variable Tests
//=============================================================================

TEST(GitConfigParserEnvVarTest, ResolveEnvVars) {
    GitConfigParser parser;
    parser.setEnvVar("REPO_URL", "https://github.com/user/repo.git");
    parser.setEnvVar("REPO_BRANCH", "feature-branch");
    
    std::string yaml = R"(
repository:
  repo_url: "${REPO_URL}"
  repo_branch: "${REPO_BRANCH}"
)";
    EXPECT_TRUE(parser.parseString(yaml));
    
    auto config = parser.getGitConfig();
    EXPECT_EQ(config.repo_url, "https://github.com/user/repo.git");
    EXPECT_EQ(config.repo_branch, "feature-branch");
}

TEST(GitConfigParserEnvVarTest, ResolveEnvVarsInPath) {
    GitConfigParser parser;
    parser.setEnvVar("HOME", "/home/testuser");
    
    std::string yaml = R"(
repository:
  repo_url: "https://example.com/repo.git"
  repo_path: "${HOME}/scratchbird/repos"
)";
    EXPECT_TRUE(parser.parseString(yaml));
    
    auto config = parser.getGitConfig();
    EXPECT_EQ(config.repo_path, "/home/testuser/scratchbird/repos");
}

TEST(GitConfigParserEnvVarTest, KeepUnresolvedVars) {
    GitConfigParser parser;
    
    std::string yaml = R"(
repository:
  repo_url: "${UNDEFINED_VAR}/repo.git"
)";
    EXPECT_TRUE(parser.parseString(yaml));
    
    auto config = parser.getGitConfig();
    EXPECT_EQ(config.repo_url, "${UNDEFINED_VAR}/repo.git");
}

//=============================================================================
// Raw Value Access Tests
//=============================================================================

TEST(GitConfigParserRawAccessTest, GetStringValue) {
    GitConfigParser parser;
    std::string yaml = R"(
repository:
  repo_url: "https://example.com/repo.git"
)";
    EXPECT_TRUE(parser.parseString(yaml));
    
    auto value = parser.getString("repository.repo_url");
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(*value, "https://example.com/repo.git");
}

TEST(GitConfigParserRawAccessTest, GetLegacyKeyMappedToCanonical) {
    GitConfigParser parser;
    std::string yaml = R"(
repository:
  url: "https://example.com/repo.git"
)";
    EXPECT_TRUE(parser.parseString(yaml));
    
    // Access via canonical key should work
    auto canonical = parser.getString("repository.repo_url");
    EXPECT_TRUE(canonical.has_value());
    EXPECT_EQ(*canonical, "https://example.com/repo.git");
}

TEST(GitConfigParserRawAccessTest, GetBoolValue) {
    GitConfigParser parser;
    std::string yaml = R"(
repository:
  repo_url: "https://example.com/repo.git"
schema:
  include_grants: true
)";
    EXPECT_TRUE(parser.parseString(yaml));
    
    auto value = parser.getBool("schema.include_grants");
    EXPECT_TRUE(value.has_value());
    EXPECT_TRUE(*value);
}

TEST(GitConfigParserRawAccessTest, MissingValueReturnsNullopt) {
    GitConfigParser parser;
    std::string yaml = "repository:\n  repo_url: \"https://example.com/repo.git\"\n";
    EXPECT_TRUE(parser.parseString(yaml));
    
    auto value = parser.getString("repository.nonexistent");
    EXPECT_FALSE(value.has_value());
}

//=============================================================================
// Schema Options Tests
//=============================================================================

TEST(GitConfigParserSchemaTest, ParseSchemaOptions) {
    GitConfigParser parser;
    std::string yaml = R"(
repository:
  repo_url: "https://example.com/repo.git"
schema:
  include_grants: true
  include_comments: false
  include_defaults: true
  separate_indexes: false
  file_per_object: true
)";
    EXPECT_TRUE(parser.parseString(yaml));
    
    auto options = parser.getSchemaOptions();
    EXPECT_TRUE(options.include_grants);
    EXPECT_FALSE(options.include_comments);
    EXPECT_TRUE(options.include_defaults);
    EXPECT_FALSE(options.separate_indexes);
    EXPECT_TRUE(options.file_per_object);
}

//=============================================================================
// Migration Config Tests
//=============================================================================

TEST(GitConfigParserMigrationTest, ParseMigrationConfig) {
    GitConfigParser parser;
    std::string yaml = R"(
repository:
  repo_url: "https://example.com/repo.git"
migrations:
  table: CUSTOM_MIGRATIONS
  naming: sequential
  generate_down: false
  transaction_per_file: false
  checksum_validation: true
)";
    EXPECT_TRUE(parser.parseString(yaml));
    
    auto config = parser.getMigrationConfig();
    EXPECT_EQ(config.table_name, "CUSTOM_MIGRATIONS");
    EXPECT_EQ(config.naming, MigrationNaming::SEQUENTIAL);
    EXPECT_FALSE(config.generate_down);
    EXPECT_FALSE(config.transaction_per_file);
    EXPECT_TRUE(config.checksum_validation);
}

//=============================================================================
// Environment Config Tests
//=============================================================================

TEST(GitConfigParserEnvironmentTest, ParseEnvironments) {
    GitConfigParser parser;
    std::string yaml = R"(
repository:
  repo_url: "https://example.com/repo.git"
environments:
  development:
    database: dev_db
    host: localhost
    port: 3092
    approval_required: false
  production:
    database: prod_db
    host: prod.example.com
    port: 3092
    approval_required: true
    backup_before_apply: true
)";
    EXPECT_TRUE(parser.parseString(yaml));
    
    auto dev = parser.getEnvironment("development");
    ASSERT_TRUE(dev.has_value());
    EXPECT_EQ(dev->database, "dev_db");
    EXPECT_EQ(dev->host, "localhost");
    EXPECT_EQ(dev->port, 3092);
    EXPECT_FALSE(dev->approval_required);
    
    auto prod = parser.getEnvironment("production");
    ASSERT_TRUE(prod.has_value());
    EXPECT_EQ(prod->database, "prod_db");
    EXPECT_EQ(prod->host, "prod.example.com");
    EXPECT_TRUE(prod->approval_required);
    EXPECT_TRUE(prod->backup_before_apply);
    
    auto envs = parser.listEnvironments();
    EXPECT_EQ(envs.size(), 2);
    EXPECT_TRUE(std::find(envs.begin(), envs.end(), "development") != envs.end());
    EXPECT_TRUE(std::find(envs.begin(), envs.end(), "production") != envs.end());
}

//=============================================================================
// Default Config Template Test
//=============================================================================

TEST(GitConfigParserTemplateTest, DefaultTemplateParsable) {
    std::string tmpl = getDefaultConfigTemplate();
    
    GitConfigParser parser;
    EXPECT_TRUE(parser.parseString(tmpl));
    
    // Should have default values
    auto config = parser.getGitConfig();
    EXPECT_EQ(config.repo_type, "git");
    EXPECT_EQ(config.repo_branch, "main");
    EXPECT_EQ(config.repo_mode, "manual");
    EXPECT_FALSE(config.sign_commits);
    
    // Should use canonical keys
    EXPECT_TRUE(parser.hasCanonicalKeys());
    EXPECT_FALSE(parser.hasLegacyKeys());
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST(GitConfigParserEdgeCaseTest, EmptyConfig) {
    GitConfigParser parser;
    EXPECT_TRUE(parser.parseString(""));
    
    auto errors = parser.validate();
    EXPECT_FALSE(errors.empty());
    EXPECT_EQ(errors[0], "repository.repo_url is required");
}

TEST(GitConfigParserEdgeCaseTest, OnlyVersionLine) {
    GitConfigParser parser;
    EXPECT_TRUE(parser.parseString("version: 1\n"));
    
    auto errors = parser.validate();
    EXPECT_FALSE(errors.empty());
}

TEST(GitConfigParserEdgeCaseTest, WhitespaceHandling) {
    GitConfigParser parser;
    std::string yaml = R"(
repository:
  repo_url:    "  https://example.com/repo.git  "   
  repo_branch :    main   
)";
    EXPECT_TRUE(parser.parseString(yaml));
    
    auto config = parser.getGitConfig();
    // Note: Quotes are stripped, but internal whitespace is preserved
    EXPECT_EQ(config.repo_url, "  https://example.com/repo.git  ");
    EXPECT_EQ(config.repo_branch, "main");
}

TEST(GitConfigParserEdgeCaseTest, SingleQuotes) {
    GitConfigParser parser;
    std::string yaml = R"(
repository:
  repo_url: 'https://example.com/repo.git'
)";
    EXPECT_TRUE(parser.parseString(yaml));
    
    auto config = parser.getGitConfig();
    EXPECT_EQ(config.repo_url, "https://example.com/repo.git");
}

TEST(GitConfigParserEdgeCaseTest, NoQuotes) {
    GitConfigParser parser;
    std::string yaml = R"(
repository:
  repo_url: https://example.com/repo.git
  repo_branch: main
)";
    EXPECT_TRUE(parser.parseString(yaml));
    
    auto config = parser.getGitConfig();
    EXPECT_EQ(config.repo_url, "https://example.com/repo.git");
    EXPECT_EQ(config.repo_branch, "main");
}

TEST(GitConfigParserEdgeCaseTest, LegacyAccessorsWork) {
    GitConfig config;
    config.repo_url = "https://example.com/repo.git";
    config.repo_branch = "main";
    
    EXPECT_EQ(config.getUrl(), "https://example.com/repo.git");
    EXPECT_EQ(config.getBranch(), "main");
    
    config.setUrl("https://new.example.com/repo.git");
    EXPECT_EQ(config.repo_url, "https://new.example.com/repo.git");
}

TEST(GitConfigParserEdgeCaseTest, IntegrationModeTransitions) {
    GitConfig config;
    
    config.setIntegrationMode(GitIntegrationMode::MANUAL);
    EXPECT_EQ(config.getIntegrationMode(), GitIntegrationMode::MANUAL);
    EXPECT_FALSE(config.getAutoCommit());
    EXPECT_FALSE(config.getAutoPush());
    EXPECT_FALSE(config.getAutoPull());
    
    config.setIntegrationMode(GitIntegrationMode::AUTO_COMMIT);
    EXPECT_EQ(config.getIntegrationMode(), GitIntegrationMode::AUTO_COMMIT);
    EXPECT_TRUE(config.getAutoCommit());
    EXPECT_FALSE(config.getAutoPush());
    EXPECT_FALSE(config.getAutoPull());
    
    config.setIntegrationMode(GitIntegrationMode::AUTO_PUSH);
    EXPECT_EQ(config.getIntegrationMode(), GitIntegrationMode::AUTO_PUSH);
    EXPECT_TRUE(config.getAutoCommit());
    EXPECT_TRUE(config.getAutoPush());
    EXPECT_FALSE(config.getAutoPull());
    
    config.setIntegrationMode(GitIntegrationMode::FULL_SYNC);
    EXPECT_EQ(config.getIntegrationMode(), GitIntegrationMode::FULL_SYNC);
    EXPECT_TRUE(config.getAutoCommit());
    EXPECT_TRUE(config.getAutoPush());
    EXPECT_TRUE(config.getAutoPull());
}



