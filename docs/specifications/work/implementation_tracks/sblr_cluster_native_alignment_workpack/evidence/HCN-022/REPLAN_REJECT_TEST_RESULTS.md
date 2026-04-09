# Replan / Reject Test Results

Validated paths:
- `SessionEpochPinsTest.MatchingEpochsPassValidation`
- `SessionEpochPinsTest.MismatchCanForceReplanInsteadOfReject`
- `CatalogSessionEpochPinningTest.SessionPinsPersistAndValidateWithReplanOrReject`

Observed:
- matching tuples pass with no action.
- mismatch can force reject (`INVALID_TRANSACTION_STATE`) or replan (`requires_replan=true`) based on policy flag.
