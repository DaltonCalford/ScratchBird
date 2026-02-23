#coding:utf-8

"""
ID:          session.alter-session-reset-remove-context-vars
ISSUE:       6093
TITLE:       ALTER SESSION RESET: remove all context variables in 'USER_SESSION' namespace
DESCRIPTION:
  Test issue about ALTER SESSION RESET:
  "remove all context variables in 'USER_SESSION' namespace ".

  NOTE. *** SET AUTODDL OFF REQUIRED ***
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
FBTEST:      functional.session.alter_session_reset_remove_context_vars
JIRA:        CORE-5832
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set autoddl off;
    commit;

    set term ^;
    execute block as
        declare c smallint;
    begin
        c = rdb$set_context('USER_SESSION', 'TEST_VAR', '123');
    end
    ^
    set term ;^
    select rdb$get_context('USER_SESSION', 'TEST_VAR') as session_level_context_var_before_reset from rdb$database;

    --------------------
    alter session reset;
    --------------------

    select rdb$get_context('USER_SESSION', 'TEST_VAR') as session_level_context_var_after_reset from rdb$database;

"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    SESSION_LEVEL_CONTEXT_VAR_BEFORE_RESET 123
    SESSION_LEVEL_CONTEXT_VAR_AFTER_RESET <null>
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
