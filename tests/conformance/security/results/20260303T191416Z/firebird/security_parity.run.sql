CREATE DATABASE 'a55_fb_sec_20260303191416_2162';
CONNECT 'a55_fb_sec_20260303191416_2162';
-- A55-031 RLS parity script
SELECT 'SEC_RESULT|SEC-001|NA|rls_allow_requires_multi_principal_harness';
SELECT 'SEC_RESULT|SEC-002|NA|rls_deny_requires_multi_principal_harness';
-- A55-032 column permission parity script
SELECT 'SEC_RESULT|SEC-003|NA|column_allow_requires_principal_grant_harness';
SELECT 'SEC_RESULT|SEC-004|NA|column_deny_requires_principal_grant_harness';
-- A55-033 domain masking/encryption parity script
SELECT 'SEC_RESULT|SEC-005|NA|domain_masking_privileged_requires_security_harness';
SELECT 'SEC_RESULT|SEC-006|NA|domain_masking_unprivileged_requires_security_harness';
SELECT 'SEC_RESULT|SEC-007|NA|domain_encryption_allow_requires_security_harness';
SELECT 'SEC_RESULT|SEC-008|NA|domain_encryption_deny_requires_security_harness';
SELECT 'SEC_RESULT|SEC-009|NA|security_audit_visibility_requires_audit_harness';
