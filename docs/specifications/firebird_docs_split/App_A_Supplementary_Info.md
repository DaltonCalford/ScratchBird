# App A Supplementary Info



<!-- Original PDF Page: 641 -->


• The context variables CURRENT_USER and CURRENT_CONNECTION will not be changed.
• As isql starts multiple transactions for a single connection, ALTER SESSION RESET
cannot be executed in isql.
Error Handling
Any error raised by ON DISCONNECT triggers aborts the session reset and leaves the session state
unchanged. Such errors are reported using primary error code isc_session_reset_err (335545206) and
error text "Cannot reset user session".
Any error raised after ON DISCONNECT triggers (including the ones raised by ON CONNECT triggers)
aborts both the session reset and the connection itself. Such errors are reported using primary
error code isc_ses_reset_failed (335545272) and error text "Reset of user session failed. Connection is
shut down." . Subsequent operations on the connection (except detach) will fail with error
isc_att_shutdown (335544856).
15.8. Debugging
15.8.1. SET DEBUG OPTION
Sets debug options
Syntax
SET DEBUG OPTION option-name = value
Table 272. Supported options
Option name Value Type Description
DSQL_KEEP_BLR BOOLEAN Stores statement BLR for retrieval with
isc_info_sql_exec_path_blr_bytes and
isc_info_sql_exec_path_blr_text.
SET DEBUG OPTION configures debug information for the current connection.

Debug options are closely tied to engine internals and their usage is discouraged if
you do not understand how these internals are subject to change between
versions.
Chapter 15. Management Statements
640

<!-- Original PDF Page: 642 -->

Appendix A: Supplementary Information
In this Appendix are topics that developers may wish to refer to, to enhance understanding of
features or changes.
The RDB$VALID_BLR Field
The field RDB$VALID_BLR in system tables RDB$PROCEDURES, RDB$FUNCTIONS and RDB$TRIGGERS signal
possible invalidation of a PSQL module after alteration of a domain or table column on which the
module depends. RDB$VALID_BLR is set to 0 for any procedure or trigger whose code is made invalid
by such a change.
The field RDB$VALID_BODY_FLAG in RDB$PACKAGES serves a similar purpose for packages.
How Invalidation Works
In PSQL modules, dependencies arise on the definitions of table columns accessed and also on any
parameter or variable that has been defined in the module using the TYPE OF clause.
After the engine has altered any domain, including the implicit domains created internally behind
column definitions and output parameters, the engine internally recompiles all of its dependencies.
Any module that fails to recompile because of an incompatibility arising from a domain change is
marked as invalid (“invalidated” by setting the RDB$VALID_BLR in its system record (in
RDB$PROCEDURES, RDB$FUNCTIONS or RDB$TRIGGERS, as appropriate) to zero.
Revalidation (setting RDB$VALID_BLR to 1) occurs when
1. the domain is altered again and the new definition is compatible with the previously
invalidated module definition, or
2. the previously invalidated module is altered to match the new domain definition
The following query will find the modules that depend on a specific domain and report the state of
their RDB$VALID_BLR fields:
SELECT * FROM (
  SELECT
    'Procedure',
    rdb$procedure_name,
    rdb$valid_blr
  FROM rdb$procedures
  UNION ALL
  SELECT
    'Function',
    rdb$function_name,
    rdb$valid_blr
  FROM rdb$functions
  UNION ALL
Appendix A: Supplementary Information
641

<!-- Original PDF Page: 643 -->

SELECT
    'Trigger',
    rdb$trigger_name,
    rdb$valid_blr
  FROM rdb$triggers
) (type, name, valid)
WHERE EXISTS
  (SELECT * from rdb$dependencies
   WHERE rdb$dependent_name = name
     AND rdb$depended_on_name = 'MYDOMAIN')
/* Replace MYDOMAIN with the actual domain name.
   Use all-caps if the domain was created
   case-insensitively. Otherwise, use the exact
   capitalisation. */
The following query will find the modules that depend on a specific table column and report the
state of their RDB$VALID_BLR fields:
SELECT * FROM (
  SELECT
    'Procedure',
    rdb$procedure_name,
    rdb$valid_blr
  FROM rdb$procedures
  UNION ALL
  SELECT
    'Function',
    rdb$function_name,
    rdb$valid_blr
  FROM rdb$functions
  UNION ALL
  SELECT
    'Trigger',
    rdb$trigger_name,
    rdb$valid_blr
  FROM rdb$triggers) (type, name, valid)
WHERE EXISTS
  (SELECT *
   FROM rdb$dependencies
   WHERE rdb$dependent_name = name
     AND rdb$depended_on_name = 'MYTABLE'
     AND rdb$field_name = 'MYCOLUMN')

All PSQL invalidations caused by domain/column changes are reflected in the
RDB$VALID_BLR field. However, other kinds of changes, such as the number of input
or output parameters, called routines and so on, do not affect the validation field
even though they potentially invalidate the module. A typical such scenario might
be one of the following:
Appendix A: Supplementary Information
642