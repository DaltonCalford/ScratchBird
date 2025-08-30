## Error Codes and Results

This document lists error/result enumerations and canonical error messages used across modules, with references to their definitions and typical messages.

### Provider Errors {#provider-errors}

Implementation References:
- `include/scratchbird/engine/provider_dispatch.h`
- `include/scratchbird/engine/database_provider.h`

Enums:
- ProviderResult: Success, Error, NotSupported, ConnectionFailed, AuthenticationFailed, ResourceExhausted, Timeout, InvalidHandle, DatabaseError, TransactionError, StatementError
- ProviderErrorHandler::ErrorSeverity: Info, Warning, Error, Fatal
- ProviderErrorHandler::ErrorCategory: Connection, Authentication, Authorization, Transaction, Statement, Resource, Network, Internal

ErrorInfo structure includes numeric error_code and error_message.

### Authentication Results {#auth-results}

Implementation References:
- `include/scratchbird/engine/authentication.h`

Enums:
- AuthenticationResult: Success, InvalidCredentials, AccountLocked, PasswordExpired, RequiresTwoFactor, RequiresPasswordChange, AccessDenied, InternalError, Timeout, Cancelled

### Protocol Results {#protocol-results}

Implementation References:
- `include/scratchbird/engine/protocol_handler.h`

Enums:
- ProtocolResult: Success, ContinueProcessing, NeedMoreData, ProtocolError, AuthenticationRequired, ConnectionClosed, InternalError

### FDW Error Taxonomy {#fdw-errors}

Implementation References:
- `include/scratchbird/engine/fdw_error_handling.h`

Enums:
- FdwErrorCategory: Connection, Authentication, Network, Query, DataType, Transaction, Configuration, Resource, Security, Internal, Unknown
- FdwErrorSeverity: Info, Warning, Error, Critical, Fatal
- FdwRecoveryAction: None, Retry, RetryWithDelay, Reconnect, Reconfigure, Fallback, Escalate, Abort

Structured FDW error contains `error_code` (e.g., SQLSTATE) and `error_message` with context and recommended actions.

### Remote Protocol Handler: Canonical Messages {#remote-protocol}

Implementation References:
- `src/engine/remote_provider.cpp`

Canonical error codes/messages (negative internal codes with message):

| Code | Message | Context |
| --- | --- | --- |
| -1 | Connection not established | Version negotiation/auth preconditions |
| -2 | Failed to send version negotiation | op_connect send failure |
| -3 | Failed to receive version response | version response read failure |
| -4 | Unexpected response to version negotiation | unexpected op |
| -5 | Connection not established | authenticate precondition |
| -6 | Failed to send authentication | op_attach send failure |
| -7 | Failed to receive authentication response | auth response read failure |
| -8 | Authentication failed | generic auth error |
| -9 | Not authenticated | attach requires auth |
| -10 | Failed to send database attach | attach send error |
| -11 | Failed to receive attach response | attach response read failure |
| -12 | Failed to send database detach | detach send error |
| -13 | Failed to send transaction begin | transaction send error |
| -14 | Failed to send commit | commit send error |
| -15 | Failed to send rollback | rollback send error |
| -16 | Failed to send prepare statement | prepare send error |
| -17 | Failed to send execute statement | execute send error |
| -18 | Failed to send fetch results | fetch send error |
| -19 | Failed to send free statement | free send error |
| -20 | No connection available | protocol send without connection |
| -21 | No connection available | response handling without connection |
| -22 | Failed to receive protocol response | receive failure |
| -999 | Unknown remote error | generic error fallback |

### Trigger SQLSTATE and Exceptions {#triggers}

Implementation References:
- `src/engine/trigger_engine.cpp`

Defaults and behavior:
- Default SQLSTATE for RAISE is `P0001`; default message "Trigger raised error"; thrown as runtime_error with formatted message.

### C API Status Codes {#capi}

Implementation References:
- `include/scratchbird/capi.h`

Enums:
- SB_StatusCode: SB_STATUS_OK (0), SB_STATUS_NOT_IMPLEMENTED (1), SB_STATUS_ERROR (2)

