#coding:utf-8

"""
ID:          issue-2171
ISSUE:       2171
TITLE:       Remove the restriction on create/delete, enable/disable the user indexes in system tables
DESCRIPTION:
  Test verifies that one may to:
    * create two indices (common and calculated) and
    * gather statistics on them
    * make some of them inactive and return then to active state
    * drop them.
  This is checked first under SYSDBA account, then under common non-privileged user (and these attempts should raise exception)
  and finally we grant DDL privilege to this user (ALTER ANY TABLE) and check again, and this should pass OK.

  Restictions about create/alter/drop indexes on system tables that are checked by test for CORE-4731 should be removed.
JIRA:        CORE-5746
FBTEST:      bugs.core_5746
NOTES:
    [03.07.2025] pzotov
    1. ::: NB :::
       For ALTER, DROP, and others statements, Firebird searches for the specified object across all schemas in the search path.
       The reference is bound to the first matching object found.
       Because of that, on 6.x following syntax should be used for GRANT ALTER ANY TABLE:
           grant alter any table on schema system to tmp$c5746;
       Explained by Adriano, letter 03.07.2025
    2. Added 'SQL_SCHEMA_PREFIX' and variables - to be substituted in expected_* on FB 6.x
       Separated expected output for FB major versions prior/since 6.x.
       No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
       Checked on 6.0.0.892; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813
"""
import locale
import pytest
from firebird.qa import *

db = db_factory()

tmp_user = user_factory('db', name = 'tmp$c5746', password = '123')
act = isql_act('db', substitutions=[('-Effective user is.*', '')])

expected_stdout_5x = """
    PLAN (RDB$RELATIONS INDEX (SYSTABLE_COMM_IDX))
    SIGN_COUNT                      1
    PLAN (RDB$RELATIONS INDEX (SYSTABLE_CALC_IDX))
    SIGN_COUNT                      1

    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -CREATE INDEX SYSTABLE_COMM_IDX failed
    -no permission for ALTER access to TABLE RDB$RELATIONS

    PLAN (RDB$RELATIONS INDEX (SYSTABLE_COMM_IDX))
    SIGN_COUNT                      1
    PLAN (RDB$RELATIONS INDEX (SYSTABLE_CALC_IDX))
    SIGN_COUNT                      1
"""

expected_stdout_6x = """
    PLAN ("SYSTEM"."RDB$RELATIONS" INDEX ("SYSTEM"."SYSTABLE_COMM_IDX"))
    SIGN_COUNT                      1
    PLAN ("SYSTEM"."RDB$RELATIONS" INDEX ("SYSTEM"."SYSTABLE_CALC_IDX"))
    SIGN_COUNT                      1

    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -CREATE INDEX "SYSTEM"."SYSTABLE_COMM_IDX" failed
    -no permission for ALTER access to TABLE "SYSTEM"."RDB$RELATIONS"

    PLAN ("SYSTEM"."RDB$RELATIONS" INDEX ("SYSTEM"."SYSTABLE_COMM_IDX"))
    SIGN_COUNT                      1
    PLAN ("SYSTEM"."RDB$RELATIONS" INDEX ("SYSTEM"."SYSTABLE_CALC_IDX"))
    SIGN_COUNT                      1
"""

@pytest.mark.version('>=3.0.4')
def test_1(act: Action, tmp_user: User):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else  'ON SCHEMA SYSTEM'
    GRANT_ALTER_ANY_TABLE_EXPR = f'grant alter any table {SQL_SCHEMA_PREFIX} to {tmp_user.name};'
    # grant alter any table to tmp$c5746;
    # grant alter any table on schema system to tmp$c5746;

    test_script = f"""
          set list on;
          set plan on;
          create descending index systable_comm_idx on rdb$relations(rdb$format);
          create descending index systable_calc_idx on rdb$relations computed by ( 1 + rdb$format );
          set statistics index systable_comm_idx;
          set statistics index systable_calc_idx;

          select sign(count(*)) as sign_count from rdb$relations where rdb$format < 65537;
          select sign(count(*)) as sign_count from rdb$relations where 1 + rdb$format < 65537;

          alter index systable_calc_idx inactive;
          alter index systable_calc_idx active;

          drop index systable_comm_idx;
          drop index systable_calc_idx;
          commit;

          connect '{act.db.dsn}' user {tmp_user.name} password '{tmp_user.password}';
          -- this should FAIL:
          create descending index systable_comm_idx on rdb$relations(rdb$format);
          commit;


          connect '{act.db.dsn}' user '{act.db.user}' password '{act.db.password}';

          {GRANT_ALTER_ANY_TABLE_EXPR};
          commit;


          connect '{act.db.dsn}' user {tmp_user.name} password '{tmp_user.password}';
          -- All following statements should PASS:
          create descending index systable_comm_idx on rdb$relations(rdb$format);
          create descending index systable_calc_idx on rdb$relations computed by ( 1 + rdb$format );
          set statistics index systable_comm_idx;
          set statistics index systable_calc_idx;

          select sign(count(*)) as sign_count from rdb$relations where rdb$format < 65537;
          select sign(count(*)) as sign_count from rdb$relations where 1 + rdb$format < 65537;

          alter index systable_calc_idx inactive;
          alter index systable_calc_idx active;

          drop index systable_comm_idx;
          drop index systable_calc_idx;
          commit;

    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.isql(switches=['-q'], input=test_script, combine_output = True, io_enc = locale.getpreferredencoding())
    assert act.clean_stdout == act.clean_expected_stdout
