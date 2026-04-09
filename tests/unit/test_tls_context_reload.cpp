#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

#include "scratchbird/core/error_context.h"
#include "scratchbird/security/tls_config.h"

namespace
{

auto repoRoot() -> std::filesystem::path
{
    return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
}

auto mysqlTlsFixture(const std::string& filename) -> std::filesystem::path
{
    return repoRoot() /
           "tests/compatibility/mysql/repos/mysql-server/mysql-test/std_data" /
           filename;
}

class TLSContextReloadTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        temp_dir_ = std::filesystem::temp_directory_path() /
                    ("sb_tls_reload_" + std::to_string(static_cast<long long>(::getpid())));
        std::filesystem::remove_all(temp_dir_);
        std::filesystem::create_directories(temp_dir_);

        live_cert_path_ = temp_dir_ / "server-cert.pem";
        live_key_path_ = temp_dir_ / "server-key.pem";

        copyFixturePair("server-cert-verify-pass.pem", "server-key-verify-pass.pem");
    }

    void TearDown() override
    {
        std::filesystem::remove_all(temp_dir_);
    }

    void copyFixturePair(const std::string& cert_name, const std::string& key_name)
    {
        std::filesystem::copy_file(
            mysqlTlsFixture(cert_name),
            live_cert_path_,
            std::filesystem::copy_options::overwrite_existing);
        std::filesystem::copy_file(
            mysqlTlsFixture(key_name),
            live_key_path_,
            std::filesystem::copy_options::overwrite_existing);
    }

    auto makeServerConfig() const -> scratchbird::security::TLSConfig
    {
        scratchbird::security::TLSConfig config;
        config.enabled = true;
        config.cert_file = live_cert_path_.string();
        config.key_file = live_key_path_.string();
        return config;
    }

    std::filesystem::path temp_dir_;
    std::filesystem::path live_cert_path_;
    std::filesystem::path live_key_path_;
};

TEST_F(TLSContextReloadTest, ReloadCertificatesRefreshesCachedCertificateMetadata)
{
    scratchbird::core::ErrorContext ctx;
    auto tls_ctx = scratchbird::security::TLSContext::createServer(makeServerConfig(), &ctx);
    ASSERT_NE(tls_ctx, nullptr) << ctx.message;

    const auto original_info = *tls_ctx->getServerCertInfo();
    ASSERT_FALSE(original_info.serial_number.empty());
    ASSERT_FALSE(original_info.fingerprint_sha256.empty());

    copyFixturePair("server-cert-verify-san.pem", "server-key-verify-san.pem");

    ASSERT_EQ(tls_ctx->reloadCertificates(&ctx), scratchbird::core::Status::OK) << ctx.message;

    const auto reloaded_info = *tls_ctx->getServerCertInfo();
    EXPECT_FALSE(reloaded_info.subject_cn.empty());
    EXPECT_FALSE(reloaded_info.fingerprint_sha256.empty());
    EXPECT_NE(reloaded_info.subject_cn, original_info.subject_cn);
    EXPECT_NE(reloaded_info.fingerprint_sha256, original_info.fingerprint_sha256);

    auto fresh_ctx = scratchbird::security::TLSContext::createServer(makeServerConfig(), &ctx);
    ASSERT_NE(fresh_ctx, nullptr) << ctx.message;
    const auto fresh_info = *fresh_ctx->getServerCertInfo();
    EXPECT_EQ(reloaded_info.subject_cn, fresh_info.subject_cn);
    EXPECT_EQ(reloaded_info.fingerprint_sha256, fresh_info.fingerprint_sha256);
}

} // namespace
