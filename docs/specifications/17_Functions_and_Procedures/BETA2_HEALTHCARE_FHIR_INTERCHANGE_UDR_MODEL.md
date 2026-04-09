# Beta 2 Healthcare FHIR Interchange UDR Model

## Purpose

This document defines the healthcare-interchange UDR family for FHIR resource
validation, transformation, bundle handling, and profile-aware extraction.

This group is the ScratchBird-native replacement target for the highest-value
operational portions of `FHIR` interoperability toolchains.

## Owning package

- `sb_pkg_health_fhir_udr`

## Dependencies

This package depends on:

- `sb_pkg_contract_udr`
- `sb_pkg_quality_udr`
- `sb_pkg_calendar_udr`
- `sb_pkg_text_udr`

## Mandatory surfaces

The package shall provide:

- FHIR resource validation
- bundle validation
- profile-aware extraction for the admitted resource set
- canonical rowset projection for the admitted resource set
- version-aware transform between admitted FHIR versions where supported

## Required routine families

- `sb_fhir.validate_resource(...)`
- `sb_fhir.validate_bundle(...)`
- `sb_fhir.extract_rows(...)`
- `sb_fhir.transform_version(...)`
- `sb_fhir.profile_check(...)`

## Example contract

```sql
select *
from sb_fhir.validate_resource(:patient_resource_json);
```

## Operational rules

1. Supported FHIR versions and profiles must be explicit and versioned.
2. Validation results must distinguish schema failure, terminology/profile
   failure, and business-rule failure.
3. Protected-health-information handling must follow explicit policy gates and
   audit logging rules.

## Explicit exclusions

- remote terminology servers as a baseline requirement
- clinical decision support engines beyond admitted rule hooks
