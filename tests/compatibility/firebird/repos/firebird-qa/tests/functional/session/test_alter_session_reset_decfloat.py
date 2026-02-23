#coding:utf-8

"""
ID:          session.alter-session-reset-decfloat
ISSUE:       6093
TITLE:       ALTER SESSION RESET: DECFLOAT parameters must be returned to default values
DESCRIPTION:
  Test issue about ALTER SESSION RESET:
  "DECFLOAT parameters (BIND, TRAP and ROUND) must be reset to default values".

  We change all three specified parameters and evaluate some expressions.
  Then we run RESET SESSION and evaluate the same expressions.
  Results must differ for all of them.

  NOTE-1.
  firebird-driver does support DEFCFLOAT datatype, so test does *not* need to use only ISQL.

  NOTE-2. *** SET AUTODDL OFF REQUIRED ***
  Following is detailed explanation of this note:

    Default ISQL behaviour is to start always *TWO* transactions (for DML and second for DDL)
    after previous commit / rollback and before *ANY* further satement is to be executed, except
    those which control ISQL itself (e.g. 'SET TERM'; 'IN ...'; 'SET BAIL' etc).
    So, even when statement <S> has nothing to change, ISQL will start TWO transactions
    just before executing <S>.
    This means that these transactions will start even if want to run 'ALTER SESSION RESET'.
    This, in turn, makes one of them (which must perform DDL) be 'active and NOT current'
    from ALTER SESSION point of view (which is run within DML transaction).

    According to description given in #6093, ALTER SESSION throws error isc_ses_reset_err
    "if any open transaction exist in current conneciton, *except of current transaction* and
    prepared 2PC transactions which is allowed and ignored by this check".

    So, we have to prohibit 'autostart' of DDL-transaction because otherwise ALTER SESSION will
    throw: "SQLSTATE = 01002 / Cannot reset user session / -There are open transactions (2 active)".
    This is done by 'SET AUTODDL OFF' statement at the beginning of this test script.

  Thanks to Vlad for explanations (discussed 18.01.2021).
FBTEST:      functional.session.alter_session_reset_decfloat
JIRA:        CORE-5832
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """

    recreate table test(a decfloat, b decfloat, c decfloat);
    insert into test(a,b,c) values(1608.90, 5.00, 100.00);
    commit;

    set list on;
    set autoddl off;
    commit;

    set bind of decfloat to smallint;
    set decfloat traps to;
    set decfloat round ceiling;

    set sqlda_display on;
    select cast(1234.5678 as decfloat(16)) as "before_reset: check datatype" from rdb$database;
    set sqlda_display off;

    -- Should issue: Infinity
    select 1/1e-9999 as "before_reset: check division result" from rdb$database;


    select a * b / c as "before_reset: check round result" from test;

    --------------------
    alter session reset;
    --------------------

    set sqlda_display on;
    select cast(1234.5678 as decfloat(16)) as "after_reset: check datatype" from rdb$database;
    set sqlda_display off;

    -- Should issue:
    -- Statement failed, SQLSTATE = 22012
    -- Decimal float divide by zero.  The code attempted to divide a DECFLOAT value by zero.
    select 1/1e-9999 "after_reset: check division result" from rdb$database;

    select a * b / c as "after_reset: check round result" from test;

"""

act = isql_act('db', test_script, substitutions=[('^((?!(SQLSTATE|divide|sqltype|before_reset|after_reset)).)*$', ''),
                                                 ('[ \t]+', ' '), ('.*alias.*', '')])

expected_stdout = """
    01: sqltype: 500 SHORT scale: 0 subtype: 0 len: 2
    before_reset: check datatype    1235

    before_reset: check division result 0

    before_reset: check round result 81

    01: sqltype: 32760 DECFLOAT(16) scale: 0 subtype: 0 len: 8
    after_reset: check datatype                   1234.5678

    after_reset: check round result                                     80.445
"""

expected_stderr = """
    Statement failed, SQLSTATE = 22012
    Decimal float divide by zero.  The code attempted to divide a DECFLOAT value by zero.
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)
