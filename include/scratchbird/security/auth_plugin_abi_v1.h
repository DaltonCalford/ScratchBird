/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SB_AUTH_ABI_MAGIC UINT32_C(0x53424155) /* 'SBAU' */
#define SB_AUTH_ABI_MAJOR UINT32_C(1)
#define SB_AUTH_ABI_MINOR UINT32_C(0)
#define SB_AUTH_MAX_PLUGIN_ID 64
#define SB_AUTH_MAX_METHOD_ID 64
#define SB_AUTH_MAX_ERROR_CODE 40
#define SB_AUTH_UUID_BYTES 16

typedef uint64_t sb_auth_plugin_instance_t;
typedef uint64_t sb_auth_exchange_t;

typedef enum sb_auth_rc_t {
    SB_AUTH_RC_OK = 0,
    SB_AUTH_RC_CONTINUE = 1,
    SB_AUTH_RC_DENY = 2,
    SB_AUTH_RC_ERROR = 3,
    SB_AUTH_RC_UNSUPPORTED = 4,
    SB_AUTH_RC_INVALID_ARGUMENT = 5,
    SB_AUTH_RC_POLICY_VIOLATION = 6,
    SB_AUTH_RC_SIGNATURE_INVALID = 7,
    SB_AUTH_RC_UNAUTHORIZED_PLUGIN = 8
} sb_auth_rc_t;

typedef enum sb_auth_transport_t {
    SB_AUTH_TRANSPORT_LOCAL = 0,
    SB_AUTH_TRANSPORT_IPC = 1,
    SB_AUTH_TRANSPORT_INET = 2
} sb_auth_transport_t;

typedef struct sb_auth_slice_t {
    const uint8_t* ptr;
    uint32_t len;
} sb_auth_slice_t;

typedef struct sb_auth_connection_ctx_v1 {
    uint32_t struct_size;
    uint32_t reserved;
    uint8_t database_uuid[SB_AUTH_UUID_BYTES];
    sb_auth_transport_t transport;
    uint32_t connection_flags;
    sb_auth_slice_t username;
    sb_auth_slice_t client_address;
    sb_auth_slice_t server_address;
    uint32_t peer_uid;
    uint32_t peer_gid;
    uint32_t peer_pid;
} sb_auth_connection_ctx_v1;

typedef struct sb_auth_principal_v1 {
    uint32_t struct_size;
    uint8_t principal_uuid[SB_AUTH_UUID_BYTES];
    sb_auth_slice_t resolved_username;
    sb_auth_slice_t external_subject;
    uint32_t assurance_level;
} sb_auth_principal_v1;

typedef struct sb_auth_step_result_v1 {
    uint32_t struct_size;
    sb_auth_rc_t rc;
    uint32_t plugin_error_numeric;
    char plugin_error_code[SB_AUTH_MAX_ERROR_CODE];
    char sqlstate[6];
    sb_auth_slice_t payload;
    sb_auth_principal_v1 principal;
} sb_auth_step_result_v1;

typedef struct sb_auth_host_api_v1 {
    uint32_t struct_size;

    uint64_t (*now_unix_ms)(void);
    sb_auth_rc_t (*secure_random)(uint8_t* out, uint32_t len);
    int (*const_time_equal)(const uint8_t* a, const uint8_t* b, uint32_t len);

    sb_auth_rc_t (*resolve_user_by_name)(
        sb_auth_slice_t username,
        uint8_t out_user_uuid[SB_AUTH_UUID_BYTES]);
    sb_auth_rc_t (*resolve_user_by_external_subject)(
        sb_auth_slice_t issuer,
        sb_auth_slice_t subject,
        uint8_t out_user_uuid[SB_AUTH_UUID_BYTES]);

    sb_auth_rc_t (*emit_audit_event)(
        sb_auth_slice_t event_name,
        sb_auth_slice_t event_json);

    sb_auth_rc_t (*read_policy_value)(
        sb_auth_slice_t key,
        sb_auth_slice_t* out_value);

    void* (*alloc)(uint32_t size);
    void (*dealloc)(void* ptr);
} sb_auth_host_api_v1;

typedef struct sb_auth_method_descriptor_v1 {
    char method_id[SB_AUTH_MAX_METHOD_ID];
    uint32_t method_flags;
    uint32_t legacy_wire_code;
} sb_auth_method_descriptor_v1;

typedef struct sb_auth_plugin_descriptor_v1 {
    uint32_t struct_size;
    char plugin_id[SB_AUTH_MAX_PLUGIN_ID];
    sb_auth_slice_t plugin_version;
    uint32_t abi_major;
    uint32_t abi_minor;
    uint32_t method_count;
    const sb_auth_method_descriptor_v1* methods;
} sb_auth_plugin_descriptor_v1;

typedef struct sb_auth_plugin_api_v1 {
    uint32_t struct_size;

    sb_auth_rc_t (*create_instance)(sb_auth_plugin_instance_t* out_instance);
    void (*destroy_instance)(sb_auth_plugin_instance_t instance);

    sb_auth_rc_t (*configure_instance)(
        sb_auth_plugin_instance_t instance,
        sb_auth_slice_t config_json);

    sb_auth_rc_t (*begin_auth)(
        sb_auth_plugin_instance_t instance,
        sb_auth_slice_t method_id,
        const sb_auth_connection_ctx_v1* conn,
        sb_auth_slice_t client_payload,
        sb_auth_exchange_t* inout_exchange,
        sb_auth_step_result_v1* out_result);

    sb_auth_rc_t (*continue_auth)(
        sb_auth_plugin_instance_t instance,
        sb_auth_exchange_t exchange,
        sb_auth_slice_t client_payload,
        sb_auth_step_result_v1* out_result);

    void (*abort_auth)(
        sb_auth_plugin_instance_t instance,
        sb_auth_exchange_t exchange);

    sb_auth_rc_t (*health_check)(
        sb_auth_plugin_instance_t instance,
        sb_auth_slice_t* out_json);
} sb_auth_plugin_api_v1;

typedef sb_auth_rc_t (*sb_auth_plugin_get_api_v1_fn)(
    uint32_t requested_abi_major,
    const sb_auth_host_api_v1* host_api,
    const sb_auth_plugin_descriptor_v1** out_descriptor,
    const sb_auth_plugin_api_v1** out_api);

#ifdef __cplusplus
}
#endif
