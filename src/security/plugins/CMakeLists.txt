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
