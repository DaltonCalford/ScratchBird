# Admin SET ROLE AND SESSION AUTHORIZATION: Examples
Last modified: 2026-02-21

Back links:
- [Language Guide README](../../../README.md)
- [Admin README](../../README.md)
- [Family README](../README.md)
- [Topic README](README.md)

Series navigation:
- Previous: [Semantics](semantics.md)
- Next: [Runtime](runtime.md)

## Coverage
- Status: Supported

## Role Switch

~~~sql
-- assume current user already has role app_readonly
SET ROLE app_readonly;

-- return to base session role context
RESET ROLE;
~~~

## Role Reset Synonyms

~~~sql
SET ROLE NONE;
SET ROLE DEFAULT;
~~~

## Session Authorization (Superuser)

~~~sql
-- superuser-only operation
SET SESSION AUTHORIZATION app_user;

-- restore original session user
RESET SESSION AUTHORIZATION;
~~~

## Expected Denials

~~~sql
-- denied if role not granted to current user
SET ROLE finance_admin;

-- denied if caller is not superuser
SET SESSION AUTHORIZATION app_user;
~~~

## MFA Step-Up Boundary

~~~sql
-- if MFA policy marks role switch as privileged, step-up may be required
SET ROLE ops_admin;
~~~
