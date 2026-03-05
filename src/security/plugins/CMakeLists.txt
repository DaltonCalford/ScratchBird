set(SCRATCHBIRD_AUTH_PLUGIN_OUTPUT_ROOT "${CMAKE_BINARY_DIR}/auth_plugins" CACHE PATH
    "Output root for auth plugin shared modules")

function(add_scratchbird_auth_plugin_stub plugin_dir plugin_id)
    set(target_name "sb_auth_plugin_${plugin_dir}")
    set(plugin_sources
        ${plugin_dir}/${plugin_dir}_plugin.cpp
    )
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${plugin_dir}/${plugin_dir}_plugin_config.cpp")
        list(APPEND plugin_sources
            ${plugin_dir}/${plugin_dir}_plugin_config.cpp
        )
    endif()

    add_library(${target_name} MODULE
        ${plugin_sources}
    )

    target_include_directories(${target_name} PRIVATE
        ${PROJECT_SOURCE_DIR}/include
    )
    if(TARGET OpenSSL::Crypto)
        target_link_libraries(${target_name} PRIVATE OpenSSL::Crypto)
    endif()

    set_target_properties(${target_name} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        PREFIX ""
        OUTPUT_NAME "libscratchbird_auth_${plugin_dir}"
        LIBRARY_OUTPUT_DIRECTORY "${SCRATCHBIRD_AUTH_PLUGIN_OUTPUT_ROOT}/${plugin_id}"
        RUNTIME_OUTPUT_DIRECTORY "${SCRATCHBIRD_AUTH_PLUGIN_OUTPUT_ROOT}/${plugin_id}"
        ARCHIVE_OUTPUT_DIRECTORY "${SCRATCHBIRD_AUTH_PLUGIN_OUTPUT_ROOT}/_archive"
    )
endfunction()

add_scratchbird_auth_plugin_stub(trust_reject scratchbird.auth.trust_reject)
add_scratchbird_auth_plugin_stub(password_compat scratchbird.auth.password_compat)
add_scratchbird_auth_plugin_stub(scram scratchbird.auth.scram)
add_scratchbird_auth_plugin_stub(token_authkey scratchbird.auth.token_authkey)
add_scratchbird_auth_plugin_stub(peer scratchbird.auth.peer)
add_scratchbird_auth_plugin_stub(certificate_mtls scratchbird.auth.certificate_mtls)
add_scratchbird_auth_plugin_stub(jwt_oidc scratchbird.auth.jwt_oidc)
add_scratchbird_auth_plugin_stub(webauthn scratchbird.auth.webauthn)
add_scratchbird_auth_plugin_stub(factor_chain scratchbird.auth.factor_chain)
add_scratchbird_auth_plugin_stub(workload_identity scratchbird.auth.workload_identity)
add_scratchbird_auth_plugin_stub(oauth_validator scratchbird.auth.oauth_validator)
add_scratchbird_auth_plugin_stub(proxy_assertion scratchbird.auth.proxy_assertion)
add_scratchbird_auth_plugin_stub(ldap scratchbird.auth.ldap)
add_scratchbird_auth_plugin_stub(kerberos scratchbird.auth.kerberos)
add_scratchbird_auth_plugin_stub(ident scratchbird.auth.ident)
add_scratchbird_auth_plugin_stub(radius scratchbird.auth.radius)
add_scratchbird_auth_plugin_stub(pam scratchbird.auth.pam)

add_executable(sb_auth_plugin_challenge_state_store_selftest
    challenge_state_store_selftest.cpp
)
target_include_directories(sb_auth_plugin_challenge_state_store_selftest PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
set_target_properties(sb_auth_plugin_challenge_state_store_selftest PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

add_executable(sb_auth_plugin_secret_provider_selftest
    auth_plugin_secret_provider_selftest.cpp
)
target_include_directories(sb_auth_plugin_secret_provider_selftest PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
set_target_properties(sb_auth_plugin_secret_provider_selftest PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

add_executable(sb_auth_plugin_payload_fuzz_selftest
    auth_plugin_payload_fuzz_selftest.cpp
)
target_include_directories(sb_auth_plugin_payload_fuzz_selftest PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
set_target_properties(sb_auth_plugin_payload_fuzz_selftest PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

add_executable(sb_auth_plugin_secrets_hygiene_selftest
    auth_plugin_secrets_hygiene_selftest.cpp
)
target_include_directories(sb_auth_plugin_secrets_hygiene_selftest PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
set_target_properties(sb_auth_plugin_secrets_hygiene_selftest PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

add_executable(sb_auth_plugin_contract_harness
    auth_plugin_contract_harness.cpp
)
target_include_directories(sb_auth_plugin_contract_harness PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
set_target_properties(sb_auth_plugin_contract_harness PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

add_executable(sb_auth_plugin_dashboard_alerts_selftest
    auth_plugin_dashboard_alerts_selftest.cpp
)
target_include_directories(sb_auth_plugin_dashboard_alerts_selftest PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
set_target_properties(sb_auth_plugin_dashboard_alerts_selftest PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

add_executable(sb_auth_plugin_runbook_selftest
    auth_plugin_runbook_selftest.cpp
)
target_include_directories(sb_auth_plugin_runbook_selftest PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
set_target_properties(sb_auth_plugin_runbook_selftest PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

add_executable(sb_auth_plugin_production_signoff_selftest
    auth_plugin_production_signoff_selftest.cpp
)
target_include_directories(sb_auth_plugin_production_signoff_selftest PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
set_target_properties(sb_auth_plugin_production_signoff_selftest PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

add_executable(sb_auth_plugin_hardening_h1_selftest
    auth_plugin_hardening_h1_selftest.cpp
)
target_include_directories(sb_auth_plugin_hardening_h1_selftest PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
set_target_properties(sb_auth_plugin_hardening_h1_selftest PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

add_executable(sb_auth_plugin_trust_reject_selftest
    trust_reject_plugin_selftest.cpp
)
target_include_directories(sb_auth_plugin_trust_reject_selftest PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
set_target_properties(sb_auth_plugin_trust_reject_selftest PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

add_executable(sb_auth_plugin_peer_selftest
    peer_plugin_selftest.cpp
)
target_include_directories(sb_auth_plugin_peer_selftest PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
set_target_properties(sb_auth_plugin_peer_selftest PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

add_executable(sb_auth_plugin_password_compat_selftest
    password_compat_plugin_selftest.cpp
)
target_include_directories(sb_auth_plugin_password_compat_selftest PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
set_target_properties(sb_auth_plugin_password_compat_selftest PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

add_executable(sb_auth_plugin_scram_selftest
    scram_plugin_selftest.cpp
)
target_include_directories(sb_auth_plugin_scram_selftest PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
set_target_properties(sb_auth_plugin_scram_selftest PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

add_executable(sb_auth_plugin_token_authkey_selftest
    token_authkey_plugin_selftest.cpp
)
target_include_directories(sb_auth_plugin_token_authkey_selftest PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
set_target_properties(sb_auth_plugin_token_authkey_selftest PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

add_executable(sb_auth_plugin_certificate_mtls_selftest
    certificate_mtls_plugin_selftest.cpp
)
target_include_directories(sb_auth_plugin_certificate_mtls_selftest PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
set_target_properties(sb_auth_plugin_certificate_mtls_selftest PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

add_executable(sb_auth_plugin_jwt_oidc_selftest
    jwt_oidc_plugin_selftest.cpp
)
target_include_directories(sb_auth_plugin_jwt_oidc_selftest PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
set_target_properties(sb_auth_plugin_jwt_oidc_selftest PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

add_executable(sb_auth_plugin_webauthn_selftest
    webauthn_plugin_selftest.cpp
)
target_include_directories(sb_auth_plugin_webauthn_selftest PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
set_target_properties(sb_auth_plugin_webauthn_selftest PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

add_executable(sb_auth_plugin_factor_chain_selftest
    factor_chain_plugin_selftest.cpp
)
target_include_directories(sb_auth_plugin_factor_chain_selftest PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
set_target_properties(sb_auth_plugin_factor_chain_selftest PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

add_executable(sb_auth_plugin_oauth_validator_selftest
    oauth_validator_plugin_selftest.cpp
)
target_include_directories(sb_auth_plugin_oauth_validator_selftest PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
set_target_properties(sb_auth_plugin_oauth_validator_selftest PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

add_executable(sb_auth_plugin_proxy_assertion_selftest
    proxy_assertion_plugin_selftest.cpp
)
target_include_directories(sb_auth_plugin_proxy_assertion_selftest PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
set_target_properties(sb_auth_plugin_proxy_assertion_selftest PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

add_executable(sb_auth_plugin_workload_identity_selftest
    workload_identity_plugin_selftest.cpp
)
target_include_directories(sb_auth_plugin_workload_identity_selftest PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
set_target_properties(sb_auth_plugin_workload_identity_selftest PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

add_executable(sb_auth_plugin_ident_selftest
    ident_plugin_selftest.cpp
)
target_include_directories(sb_auth_plugin_ident_selftest PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
set_target_properties(sb_auth_plugin_ident_selftest PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

add_executable(sb_auth_plugin_radius_selftest
    radius_plugin_selftest.cpp
)
target_include_directories(sb_auth_plugin_radius_selftest PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
set_target_properties(sb_auth_plugin_radius_selftest PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

add_executable(sb_auth_plugin_pam_selftest
    pam_plugin_selftest.cpp
)
target_include_directories(sb_auth_plugin_pam_selftest PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
set_target_properties(sb_auth_plugin_pam_selftest PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

add_executable(sb_auth_plugin_ldap_selftest
    ldap_plugin_selftest.cpp
)
target_include_directories(sb_auth_plugin_ldap_selftest PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
set_target_properties(sb_auth_plugin_ldap_selftest PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

add_executable(sb_auth_plugin_kerberos_selftest
    kerberos_plugin_selftest.cpp
)
target_include_directories(sb_auth_plugin_kerberos_selftest PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
set_target_properties(sb_auth_plugin_kerberos_selftest PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

if(TARGET OpenSSL::Crypto)
    set(SCRATCHBIRD_AUTH_PLUGIN_OPENSSL_TARGETS
        sb_auth_plugin_payload_fuzz_selftest
        sb_auth_plugin_secrets_hygiene_selftest
        sb_auth_plugin_contract_harness
        sb_auth_plugin_token_authkey_selftest
        sb_auth_plugin_jwt_oidc_selftest
        sb_auth_plugin_oauth_validator_selftest
        sb_auth_plugin_proxy_assertion_selftest
        sb_auth_plugin_workload_identity_selftest
        sb_auth_plugin_radius_selftest
        sb_auth_plugin_scram_selftest
        sb_auth_plugin_kerberos_selftest
        sb_auth_plugin_webauthn_selftest
        sb_auth_plugin_factor_chain_selftest
    )
    foreach(target_name IN LISTS SCRATCHBIRD_AUTH_PLUGIN_OPENSSL_TARGETS)
        if(TARGET ${target_name})
            target_link_libraries(${target_name} PRIVATE OpenSSL::Crypto)
        endif()
    endforeach()
endif()

if(BUILD_TESTING)
    add_test(
        NAME sb_auth_plugin_challenge_state_store_selftest
        COMMAND sb_auth_plugin_challenge_state_store_selftest
    )
    add_test(
        NAME sb_auth_plugin_secret_provider_selftest
        COMMAND sb_auth_plugin_secret_provider_selftest
    )
    add_test(
        NAME sb_auth_plugin_payload_fuzz_selftest
        COMMAND sb_auth_plugin_payload_fuzz_selftest "${SCRATCHBIRD_AUTH_PLUGIN_OUTPUT_ROOT}"
    )
    add_test(
        NAME sb_auth_plugin_secrets_hygiene_selftest
        COMMAND sb_auth_plugin_secrets_hygiene_selftest "${SCRATCHBIRD_AUTH_PLUGIN_OUTPUT_ROOT}"
    )
    add_test(
        NAME sb_auth_plugin_contract_harness
        COMMAND sb_auth_plugin_contract_harness "${SCRATCHBIRD_AUTH_PLUGIN_OUTPUT_ROOT}"
    )
    add_test(
        NAME sb_auth_plugin_dashboard_alerts_selftest
        COMMAND sb_auth_plugin_dashboard_alerts_selftest "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    add_test(
        NAME sb_auth_plugin_runbook_selftest
        COMMAND sb_auth_plugin_runbook_selftest "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    add_test(
        NAME sb_auth_plugin_production_signoff_selftest
        COMMAND sb_auth_plugin_production_signoff_selftest "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    add_test(
        NAME sb_auth_plugin_hardening_h1_selftest
        COMMAND sb_auth_plugin_hardening_h1_selftest "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    add_test(
        NAME sb_auth_plugin_trust_reject_selftest
        COMMAND sb_auth_plugin_trust_reject_selftest "${SCRATCHBIRD_AUTH_PLUGIN_OUTPUT_ROOT}"
    )
    add_test(
        NAME sb_auth_plugin_peer_selftest
        COMMAND sb_auth_plugin_peer_selftest "${SCRATCHBIRD_AUTH_PLUGIN_OUTPUT_ROOT}"
    )
    add_test(
        NAME sb_auth_plugin_password_compat_selftest
        COMMAND sb_auth_plugin_password_compat_selftest "${SCRATCHBIRD_AUTH_PLUGIN_OUTPUT_ROOT}"
    )
    add_test(
        NAME sb_auth_plugin_scram_selftest
        COMMAND sb_auth_plugin_scram_selftest "${SCRATCHBIRD_AUTH_PLUGIN_OUTPUT_ROOT}"
    )
    add_test(
        NAME sb_auth_plugin_token_authkey_selftest
        COMMAND sb_auth_plugin_token_authkey_selftest "${SCRATCHBIRD_AUTH_PLUGIN_OUTPUT_ROOT}"
    )
    add_test(
        NAME sb_auth_plugin_certificate_mtls_selftest
        COMMAND sb_auth_plugin_certificate_mtls_selftest "${SCRATCHBIRD_AUTH_PLUGIN_OUTPUT_ROOT}"
    )
    add_test(
        NAME sb_auth_plugin_jwt_oidc_selftest
        COMMAND sb_auth_plugin_jwt_oidc_selftest "${SCRATCHBIRD_AUTH_PLUGIN_OUTPUT_ROOT}"
    )
    add_test(
        NAME sb_auth_plugin_webauthn_selftest
        COMMAND sb_auth_plugin_webauthn_selftest "${SCRATCHBIRD_AUTH_PLUGIN_OUTPUT_ROOT}"
    )
    add_test(
        NAME sb_auth_plugin_factor_chain_selftest
        COMMAND sb_auth_plugin_factor_chain_selftest "${SCRATCHBIRD_AUTH_PLUGIN_OUTPUT_ROOT}"
    )
    add_test(
        NAME sb_auth_plugin_oauth_validator_selftest
        COMMAND sb_auth_plugin_oauth_validator_selftest "${SCRATCHBIRD_AUTH_PLUGIN_OUTPUT_ROOT}"
    )
    add_test(
        NAME sb_auth_plugin_proxy_assertion_selftest
        COMMAND sb_auth_plugin_proxy_assertion_selftest "${SCRATCHBIRD_AUTH_PLUGIN_OUTPUT_ROOT}"
    )
    add_test(
        NAME sb_auth_plugin_workload_identity_selftest
        COMMAND sb_auth_plugin_workload_identity_selftest "${SCRATCHBIRD_AUTH_PLUGIN_OUTPUT_ROOT}"
    )
    add_test(
        NAME sb_auth_plugin_ident_selftest
        COMMAND sb_auth_plugin_ident_selftest "${SCRATCHBIRD_AUTH_PLUGIN_OUTPUT_ROOT}"
    )
    add_test(
        NAME sb_auth_plugin_radius_selftest
        COMMAND sb_auth_plugin_radius_selftest "${SCRATCHBIRD_AUTH_PLUGIN_OUTPUT_ROOT}"
    )
    add_test(
        NAME sb_auth_plugin_pam_selftest
        COMMAND sb_auth_plugin_pam_selftest "${SCRATCHBIRD_AUTH_PLUGIN_OUTPUT_ROOT}"
    )
    add_test(
        NAME sb_auth_plugin_ldap_selftest
        COMMAND sb_auth_plugin_ldap_selftest "${SCRATCHBIRD_AUTH_PLUGIN_OUTPUT_ROOT}"
    )
    add_test(
        NAME sb_auth_plugin_kerberos_selftest
        COMMAND sb_auth_plugin_kerberos_selftest "${SCRATCHBIRD_AUTH_PLUGIN_OUTPUT_ROOT}"
    )
endif()
