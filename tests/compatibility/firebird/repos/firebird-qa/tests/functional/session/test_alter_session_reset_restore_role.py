#coding:utf-8

"""
ID:          session.alter-session-reset-restore-role
ISSUE:       6093
TITLE:       ALTER SESSION RESET: restore ROLE which was passed with DPB and clear all cached security classes (if role was changed)
DESCRIPTION:
  Test issue about ALTER SESSION RESET:
  "restore ROLE which was passed with DPB and clear all cached security classes (if role was changed) ".

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
FBTEST:      functional.session.alter_session_reset_restore_role
JIRA:        CORE-5832
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;

    create role acnt;
    create role boss;
    commit;

    create view v_test as
    select mon$user as who_ami, mon$role as whats_my_role
    from mon$attachments
    where mon$attachment_id = current_connection;
    commit;

    grant acnt to sysdba;
    grant boss to sysdba;
    commit;

    set list on;
    set autoddl off;
    commit;

    connect '$(DSN)' user 'SYSDBA' password 'masterkey' role boss;

    select * from v_test;
    set role acnt;
    commit;
    select * from v_test;

    --------------------
    alter session reset;
    --------------------
    select * from v_test;

"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    WHO_AMI                         SYSDBA
    WHATS_MY_ROLE                   BOSS

    WHO_AMI                         SYSDBA
    WHATS_MY_ROLE                   ACNT

    WHO_AMI                         SYSDBA
    WHATS_MY_ROLE                   BOSS
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
