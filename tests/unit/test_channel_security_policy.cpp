#include <gtest/gtest.h>

#include <cstdio>
#include <memory>
#include <string>
#include <unistd.h>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"

using namespace scratchbird::core;

class ChannelSecurityPolicyTest : public ::testing::Test
{
protected:
    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;

    void SetUp() override
    {
        db_path_ = "/tmp/test_channel_security_policy_" + std::to_string(getpid()) + ".db";
        std::remove(db_path_.c_str());

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_path_, &ctx), Status::OK) << ctx.message;
        catalog_ = db_->catalog_manager();
        ASSERT_NE(catalog_, nullptr);

        ASSERT_EQ(db_->connect(conn_, &ctx), Status::OK) << ctx.message;
        ConnectionContext::setCurrent(conn_.get());
    }

    void TearDown() override
    {
        ConnectionContext::setCurrent(nullptr);
        conn_.reset();
        if (db_)
        {
            db_->close();
            db_.reset();
            catalog_ = nullptr;
        }
        std::remove(db_path_.c_str());
    }

    CatalogManager::CertRegistryCatalogInfo makeCert(CatalogManager::CertKind kind,
                                                     const std::string& subject,
                                                     const std::string& issuer,
                                                     uint8_t thumbprint_fill)
    {
        CatalogManager::CertRegistryCatalogInfo cert{};
        cert.cert_id = generateUuidV7();
        cert.cert_kind = kind;
        cert.subject_name = subject;
        cert.issuer_name = issuer;
        cert.serial_number = subject;
        cert.not_before = 100;
        cert.not_after = 10000;
        cert.public_key_id = generateUuidV7();
        cert.cert_der_id = generateUuidV7();
        cert.signature_algorithm = "sha256WithRSAEncryption";
        cert.thumbprint_sha256.fill(thumbprint_fill);
        cert.status = CatalogManager::CertStatus::ACTIVE;
        return cert;
    }
};

TEST_F(ChannelSecurityPolicyTest, TrustedTlsChannelPassesPolicyGate)
{
    ErrorContext ctx;

    auto anchor_cert = makeCert(CatalogManager::CertKind::CLUSTER, "CN=example-ca", "CN=example-ca", 0x11);
    ASSERT_EQ(catalog_->upsertCertRegistryCatalogEntry(anchor_cert, &ctx), Status::OK) << ctx.message;

    CatalogManager::TrustAnchorCatalogInfo anchor{};
    anchor.anchor_id = generateUuidV7();
    anchor.cert_id = anchor_cert.cert_id;
    anchor.thumbprint_sha256 = anchor_cert.thumbprint_sha256;
    anchor.state = CatalogManager::TrustAnchorState::ACTIVE;
    anchor.activated_time = 200;
    ASSERT_EQ(catalog_->upsertTrustAnchorCatalogEntry(anchor, &ctx), Status::OK) << ctx.message;

    auto server_cert = makeCert(CatalogManager::CertKind::SERVER,
                                "CN=listener.example",
                                "CN=example-ca",
                                0x22);
    ASSERT_EQ(catalog_->upsertCertRegistryCatalogEntry(server_cert, &ctx), Status::OK) << ctx.message;

    CatalogManager::ChannelCertBindingCatalogInfo binding{};
    binding.binding_id = generateUuidV7();
    binding.channel_name = "LISTENER_CONTROL";
    binding.cert_kind = CatalogManager::CertKind::SERVER;
    binding.cert_id = server_cert.cert_id;
    binding.enforce_mtls = true;
    binding.min_tls_version = CatalogManager::TlsVersion::TLS_1_3;
    ASSERT_EQ(catalog_->upsertChannelCertBindingCatalogEntry(binding, &ctx), Status::OK) << ctx.message;

    CatalogManager::PkiDistributionStateCatalogInfo distribution{};
    distribution.distribution_id = generateUuidV7();
    distribution.member_id = generateUuidV7();
    distribution.artifact_kind = CatalogManager::PkiArtifactKind::CHANNEL_BINDING;
    distribution.artifact_id = binding.binding_id;
    distribution.distribution_state = CatalogManager::DistributionState::APPLIED;
    ASSERT_EQ(catalog_->upsertPkiDistributionStateCatalogEntry(distribution, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::ChannelSecurityEvaluationRequest request{};
    request.channel_name = "LISTENER_CONTROL";
    request.cert_kind = CatalogManager::CertKind::SERVER;
    request.is_tls = true;
    request.is_mtls = true;
    request.tls_version = CatalogManager::TlsVersion::TLS_1_3;
    request.tls_cipher_suite = "TLS_AES_256_GCM_SHA384";
    request.has_presented_cert_id = true;
    request.presented_cert_id = server_cert.cert_id;
    request.now_time = 500;

    CatalogManager::ChannelSecurityEvaluationDecision decision{};
    ASSERT_EQ(catalog_->evaluateChannelSecurityPolicy(request, decision, &ctx), Status::OK) << ctx.message;
    EXPECT_TRUE(decision.allowed);
    EXPECT_TRUE(decision.binding_found);
    EXPECT_TRUE(decision.cert_active);
    EXPECT_TRUE(decision.trust_anchor_active);
    EXPECT_FALSE(decision.revocation_stale);
}

TEST_F(ChannelSecurityPolicyTest, StaleDistributionFailsClosed)
{
    ErrorContext ctx;

    auto anchor_cert = makeCert(CatalogManager::CertKind::CLUSTER, "CN=example-ca", "CN=example-ca", 0x33);
    ASSERT_EQ(catalog_->upsertCertRegistryCatalogEntry(anchor_cert, &ctx), Status::OK) << ctx.message;

    CatalogManager::TrustAnchorCatalogInfo anchor{};
    anchor.anchor_id = generateUuidV7();
    anchor.cert_id = anchor_cert.cert_id;
    anchor.thumbprint_sha256 = anchor_cert.thumbprint_sha256;
    anchor.state = CatalogManager::TrustAnchorState::ACTIVE;
    anchor.activated_time = 200;
    ASSERT_EQ(catalog_->upsertTrustAnchorCatalogEntry(anchor, &ctx), Status::OK) << ctx.message;

    auto server_cert = makeCert(CatalogManager::CertKind::SERVER,
                                "CN=parser-control",
                                "CN=example-ca",
                                0x44);
    ASSERT_EQ(catalog_->upsertCertRegistryCatalogEntry(server_cert, &ctx), Status::OK) << ctx.message;

    CatalogManager::ChannelCertBindingCatalogInfo binding{};
    binding.binding_id = generateUuidV7();
    binding.channel_name = "PARSER_CONTROL";
    binding.cert_kind = CatalogManager::CertKind::SERVER;
    binding.cert_id = server_cert.cert_id;
    binding.enforce_mtls = false;
    binding.min_tls_version = CatalogManager::TlsVersion::TLS_1_3;
    ASSERT_EQ(catalog_->upsertChannelCertBindingCatalogEntry(binding, &ctx), Status::OK) << ctx.message;

    CatalogManager::PkiDistributionStateCatalogInfo distribution{};
    distribution.distribution_id = generateUuidV7();
    distribution.member_id = generateUuidV7();
    distribution.artifact_kind = CatalogManager::PkiArtifactKind::CHANNEL_BINDING;
    distribution.artifact_id = binding.binding_id;
    distribution.distribution_state = CatalogManager::DistributionState::IN_PROGRESS;
    distribution.has_last_attempt_time = true;
    distribution.last_attempt_time = 1000;
    ASSERT_EQ(catalog_->upsertPkiDistributionStateCatalogEntry(distribution, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::ChannelSecurityEvaluationRequest request{};
    request.channel_name = "PARSER_CONTROL";
    request.cert_kind = CatalogManager::CertKind::SERVER;
    request.is_tls = true;
    request.is_mtls = false;
    request.tls_version = CatalogManager::TlsVersion::TLS_1_3;
    request.tls_cipher_suite = "TLS_AES_256_GCM_SHA384";
    request.now_time = 5000;
    request.distribution_stale_after_ms = 1000;

    CatalogManager::ChannelSecurityEvaluationDecision decision{};
    EXPECT_EQ(catalog_->evaluateChannelSecurityPolicy(request, decision, &ctx),
              Status::INVALID_AUTHORIZATION);
    EXPECT_EQ(ctx.vnext_code, "SB-PKI-0008");
    EXPECT_TRUE(decision.revocation_stale);
}
